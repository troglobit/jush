/* just give me a shell prompt
 *
 * Copyright (c) 2019  Joachim Nilsson <troglobit@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"
#include <err.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wordexp.h>
#include <editline.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define HISTFILE "~/.jush_history"
#define MAXJOBS  100

/* From The Practice of Programming, by Kernighan and Pike */
#ifndef NELEMS
#define NELEMS(array) (sizeof(array) / sizeof(array[0]))
#endif

struct env {
	char prevcwd[PATH_MAX];
	int pipes;
	int bg;
	int status;
	pid_t jobs[MAXJOBS];
	int lastjob;
	int exit;
};

static void addjob(struct env *env, pid_t pid)
{
	for (int i = 0; i < MAXJOBS; i++) {
		if (env->jobs[i] != 0)
			continue;

		env->jobs[i] = pid;
//		printf("[%d] Running            %s\n", i, job.cmdline);
		printf("[%d] %d\n", i, pid);
		break;
	}
}

static void deljob(struct env *env, pid_t pid)
{
	for (int i = 0; i < MAXJOBS; i++) {
		if (env->jobs[i] != pid)
			continue;

		env->jobs[i] = 0;
		printf("[%d] Done\n", i);
		break;
	}
}

static void jobs(struct env *env)
{
	for (int i = 0; i < MAXJOBS; i++) {
		if (env->jobs[i] == 0)
			continue;

		printf("[%d] %d\n", i, env->jobs[i]);
	}
}

static int compare(const char *arg1, const char *arg2)
{
	if (!arg1 || !arg2)
		return 0;

	return !strcmp(arg1, arg2);
}

/*
 * ~username/path
 * ~/path
 */
static char *tilde_expand(char *arg)
{
	struct passwd *pw;
	size_t len;
	char *tilde, *ptr, *buf;

	if (arg[0] != '~')
		return arg;

	ptr = strchr(arg, '/');
	if (ptr)
		*ptr++ = 0;
	else
		ptr = strchr(arg, 0);

	if (arg[1]) {
		pw = getpwnam(&arg[1]);
		if (!pw)
			return arg;

		tilde = pw->pw_dir;
	} else {
		tilde = getenv("HOME");
	}

	len = strlen(tilde) + strlen(ptr) + 2;
	buf = malloc(len);
	if (!buf) {
		free(arg);
		return NULL;
	}

	snprintf(buf, len, "%s/%s", tilde, ptr);
	free(arg);

	return buf;
}

static char *recompose(char *fmt, ...)
{
	va_list ap;
	size_t len;
	char *buf, *a = NULL;

	/* Initial length, takes / and \0 into account */
	len = strlen(fmt);
	buf = calloc(1, len);
	if (!buf)
		return NULL;

	va_start(ap, fmt);
	while (*fmt) {
		char tmp[sizeof(int) + 1];
		char *s;

		if (*fmt == '%') {
			fmt++;
			switch (*fmt) {
			case 'a':
				s = a = va_arg(ap, char *);
				goto str;
			case 's':
				s = va_arg(ap, char *);
			str:
				len += strlen(s);
				buf = realloc(buf, len);
				if (!buf)
					goto done;
				strncat(buf, s, len);
				break;

			case 'd':
				len += sizeof(int);
				buf = realloc(buf, len);
				if (!buf)
					goto done;
				snprintf(tmp, sizeof(tmp), "%d", va_arg(ap, int));
				strncat(buf, tmp, len);
				break;
			}
		} else {
			snprintf(tmp, sizeof(tmp), "%c", *fmt);
			strncat(buf, tmp, len);
		}
		fmt++;
	}
done:
	if (a)
		free(a);
	va_end(ap);

	return buf;
}

static char *env_expand(char *arg, struct env *env)
{
	char *ptr, *var;

	ptr = strchr(arg, '$');
	if (!ptr)
		return arg;

	var = getenv(&ptr[1]);
	if (var) {
		*ptr++ = 0;
		return recompose("%a%s", arg, var);
	}

	if (ptr[1] == '?') {
		*ptr++ = 0;
		return recompose("%a%d", arg, WEXITSTATUS(env->status));
	}

	return arg;
}

static char *expand(char *token, struct env *env)
{
	char *arg;

	arg = strdup(token);
	if (!token || !arg)
		return NULL;

	arg = tilde_expand(arg);
	arg = env_expand(arg, env);

	return arg;
}

static void help(void)
{
	puts("This is jush version " VERSION ", a barely usable UNIX shell.\n");

	puts("Features:");
	puts("   ls | wc  Pipes work                   cmd &    Backgrounding works\n");

	puts("Built-in commands:");
	puts("   help     This command                 exit     Exit jush, Ctrl-d also works");
	puts("   jobs     List background jobs         fg [ID]  Put job (ID) in foreground");
	puts("   cd [DIR] Change directory\n");

	puts("Helpful keybindings for line editing:");
	puts("   Ctrl-a   Beginning of line            Ctrl-p   Previous line");
	puts("   Ctrl-e   End of line                  Ctrl-n   Next line");
	puts("   Ctrl-b   Backward one character       Ctrl-l   Redraw line if garbled");
	puts("   Ctrl-f   Forward one character        Meta-d   Delete word");
	puts("   Ctrl-k   Cut text to end of line      Ctrl-r   Search history");
	puts("   Ctrl-y   Paste previously cut text    Ctrl-c   Abort current command");
}

static int builtin(char *args[], struct env *env)
{
	if (compare(args[0], "cd")) {
		char *arg, *path;

		arg = args[1];
		if (!arg || compare(arg, "~"))
			arg = getenv("HOME");
		if (arg && compare(arg, "-"))
			arg = env->prevcwd;

		path = realpath(arg, NULL);
		if (!path) {
			warn("cd");
			return 1;
		}

		if (!getcwd(env->prevcwd, sizeof(env->prevcwd)))
			warn("Cannot find current working directory");

		if (chdir(path))
			warn("Failed cd %s", path);
		free(path);
	} else if (compare(args[0], "exit")) {
		env->exit = 1;
	} else if (compare(args[0], "help")) {
		help();
	} else if (compare(args[0], "jobs")) {
		jobs(env);
	} else if (compare(args[0], "fg")) {
		int id;

		if (!args[1])
			id = env->lastjob;
		else
			id = atoi(args[1]);

		if (id < 0 || id >= MAXJOBS) {
		fg_fail:
			warnx("invalid job id");
		} else {
			pid_t pid;

			pid = env->jobs[id];
			if (pid <= 0)
				goto fg_fail;

			while (waitpid(pid, NULL, 0) < 0 && EINTR == errno)
				kill(pid, SIGINT);
		}
	} else
		return 0;

	return 1;
}

static void redirect(char *args[])
{
	char *arg = NULL;
	int i, flags, fd;
	int mode = S_IRWXU | S_IRWXG | S_IRWXO;
	int stdio = -1;

	for (i = 0; args[i]; i++) {
		arg = args[i];

		if (*arg == '>') {
			arg++;
			if (*arg == '>')
				flags = O_WRONLY | O_APPEND;
			else
				flags = O_WRONLY | O_CREAT | O_TRUNC;
			stdio = STDOUT_FILENO;
			break;
		}

		if (*arg == '<') {
			arg++;
			flags = O_RDONLY;
			stdio = STDIN_FILENO;
			break;
		}
	}

	if (!arg || stdio < 0)
		return;
	if (!*arg)
		arg = args[i + 1];
	if (!arg)
		return;

	fd = open(arg, flags, mode);
	args[i] = NULL;
	close(stdio);
	if (dup(fd) < 0)
		err(1, "Failed redirecting %s", stdio == STDIN_FILENO ? "stdin" : "stdout");
	close(fd);
}

static void run(char *args[])
{
	redirect(args);
	execvp(args[0], args);
	err(1, "Failed executing %s", args[0]);
}

static void pipeit(char *args[], struct env *env)
{
	pid_t pid;
	int fd[2];

	if (!env->pipes)
		run(args);

	if (pipe(fd))
		err(1, "Failed creating pipe");

	pid = fork();
	if (!pid) {
		close(STDOUT_FILENO);
		if (dup(fd[STDOUT_FILENO]) < 0)
			err(1, "Failed redirecting stdout");
		close(fd[STDOUT_FILENO]);

		run(args);
	}

	close(fd[STDOUT_FILENO]);
	close(STDIN_FILENO);
	if (dup(fd[STDIN_FILENO]) < 0)
		err(1, "Failed redirecting stdin");
	close(fd[STDIN_FILENO]);

	waitpid(pid, NULL, 0);

	env->pipes--;
	while (*args)
		args++;
	args++;

	pipeit(args, env);
}

static int parse(char *line, char *args[], struct env *env)
{
	const char *sep = " \t";
	char *token;
	int num = 0;

	env->pipes = 0;
	env->bg = 0;

	token = strtok(line, sep);
	while (token) {
		if (token[0] == '|') {
			args[num++] = NULL;
			env->pipes++;
			token++;
		}

		if (token[0] == '&') {
			args[num++] = NULL;
			env->bg = 1;
			token++;
		}

		if (*token)
			args[num++] = expand(token, env);
		token = strtok(NULL, sep);
	}
	args[num++] = NULL;

	return args[0] == NULL;
}

static void eval(char *line, struct env *env)
{
	pid_t pid;
	char *args[strlen(line)];

	memset(args, 0, sizeof(args));
	if (parse(line, args, env))
		goto cleanup;

	if (builtin(args, env))
		goto cleanup;

	pid = fork();
	if (!pid) {
		if (env->pipes)
			pipeit(args, env);
		else
			run(args);
	}

	if (env->bg)
		addjob(env, pid);
	else
		waitpid(pid, &env->status, 0);

cleanup:
	for (size_t i = 0; i < NELEMS(args); i++) {
		if (!args[i])
			continue;
		free(args[i]);
	}
}

static void reaper(struct env *env)
{
	pid_t pid;

	while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
		deljob(env, pid);
}

static void breakit(int signo)
{
}

static char *histfile(void)
{
	static char *ptr = NULL;

	if (!ptr) {
		wordexp_t we;

		wordexp(HISTFILE, &we, 0);
		ptr = strdup(we.we_wordv[0]);
		wordfree(&we);
	}

	return ptr;
}

static char *prompt(char *buf, size_t len)
{
	size_t hlen;
	char flag, *home, *who, *cwd;
	char hostname[HOST_NAME_MAX];
	char tmp[80];

	home = getenv("HOME");
	hlen = strlen(home);
	cwd = getcwd(tmp, sizeof(tmp));
	if (!strncmp(home, cwd, hlen)) {
		cwd = &tmp[hlen - 1];
		cwd[0] = '~';
	}
	gethostname(hostname, sizeof(hostname));

	who = getenv("LOGNAME");
	if (compare(who, "root"))
		flag = '#';
	else
		flag = '$';

	snprintf(buf, len, "\033[01;32m%s@%s\033[00m:\033[01;34m%s\033[00m%c ", who, hostname, cwd, flag);

	return buf;
}

int main(int argc, char *argv[])
{
	struct env env;
	char *line, *hist;
	char ps1[2 * HOST_NAME_MAX];
	int cmd = 0;
	int c;

	while ((c = getopt(argc, argv, "ch")) != EOF) {
		switch (c) {
		case 'c':
			cmd = 1;
			break;

		case 'h':
			puts("Usage: jush [-ch] [CMD]");
			return 0;

		default:
			return 1;
		}
	}

	signal(SIGINT, breakit);

	if (cmd) {
		size_t len = 1;

		for (int i = optind; i < argc; i++)
			len += strlen(argv[i]) + 1;

		line = calloc(1, len);
		if (!line)
			err(1, "Not enough memory");

		for (int i = optind; i < argc; i++) {
			strcat(line, argv[i]);
			strcat(line, " ");
		}

		eval(line, &env);
		free(line);

		return 0;
	}

	memset(&env, 0, sizeof(env));
	hist = histfile();
	rl_initialize();
	read_history(hist);

	while (!env.exit) {
		line = readline(prompt(ps1, sizeof(ps1)));
		if (!line) {
			puts("");
			break;
		}

		eval(line, &env);
		free(line);

		reaper(&env);
	}

	write_history(hist);
	rl_uninitialize();
	free(hist);

	return 0;
}
