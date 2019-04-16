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
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <editline.h>
#include <sys/wait.h>

struct env {
	char prevcwd[PATH_MAX];
	int pipes;
	int exit;
};

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
	int fd[2];

	if (!env->pipes)
		run(args);

	if (pipe(fd))
		err(1, "Failed creating pipe");

	if (!fork()) {
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

	wait(NULL);

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

	args[num++] = strtok(line, sep);
	do {
		token = strtok(NULL, sep);
		if (token) {
			if (token[0] == '|') {
				args[num++] = NULL;
				env->pipes++;
				token++;
				if (token[0] == 0)
					continue;
			}
		}

		args[num++] = token;
	} while (token);

	return args[0] == NULL;
}

static void eval(char *line, struct env *env)
{
	char *args[strlen(line)];

	if (parse(line, args, env))
		return;

	if (builtin(args, env))
		return;

	if (!fork()) {
		if (env->pipes)
			pipeit(args, env);
		else
			run(args);
	}

	wait(NULL);
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
	while (!env.exit) {
		line = readline(prompt(ps1, sizeof(ps1)));
		if (!line) {
			puts("");
			break;
		}

		eval(line, &env);
	}

	return 0;
}
