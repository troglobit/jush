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

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wordexp.h>
#include <editline.h>
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

static void run(char *args[])
{
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
			args[num++] = strdup(token);
		token = strtok(NULL, sep);
	}
	args[num++] = token;

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
		waitpid(pid, NULL, 0);

cleanup:
	for (size_t i = 0; i < NELEMS(args); i++) {
		if (args[i])
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
	char *line;
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
	read_history(histfile());
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
	write_history(histfile());

	return 0;
}
