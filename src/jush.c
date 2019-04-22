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

#include "jush.h"

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
		size_t len = strlen(token) - 1;
		int pipes = 0;
		int bg = 0;

		if (token[0] == '|') {
			args[num++] = NULL;
			env->pipes++;
			token++;
		}
		if (token[len] == '|') {
			token[len] = 0;
			pipes++;
		}

		if (token[0] == '&') {
			args[num++] = NULL;
			env->bg++;
			token++;
		}
		if (token[len] == '&') {
			token[len] = 0;
			bg++;
		}

		if (*token)
			args[num++] = expand(token, env);

		if (pipes || bg) {
			args[num++] = NULL;

			if (pipes)
				env->pipes++;
			if (bg)
				env->bg++;
		}

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
	char hostname[MAXHOSTNAMELEN];
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
	char ps1[2 * MAXHOSTNAMELEN];
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
	env.lastjob = -1;

	hist = histfile();
	rl_initialize();
	rl_set_complete_func(complete);
	rl_set_list_possib_func(list_possible);
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
