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
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <editline.h>
#include <sys/wait.h>

struct env {
	int pipes;
};

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

	memset(env, 0, sizeof(*env));
	args[num++] = strtok(line, sep);
	do {
		token = strtok(NULL, sep);
		args[num++] = token;
	} while (token);

	for (int i = 0; args[i]; i++) {
		if (!strcmp(args[i], "|")) {
			args[i] = NULL;
			env->pipes++;
		}
	}

	return args[0] == NULL;
}

static void eval(char *line)
{
	struct env env;
	char *args[strlen(line)];

	if (parse(line, args, &env))
		return;

	if (!fork()) {
		if (env.pipes)
			pipeit(args, &env);
		else
			run(args);
	}

	wait(NULL);
}

int main(int argc, char *argv[])
{
	char *line;

	while ((line = readline("$ ")))
		eval(line);

	return 0;
}
