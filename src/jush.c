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
	if (fd < 0)
		return;

	args[i] = NULL;
	close(stdio);
	if (dup(fd) < 0) {
		close(fd);
		err(1, "Failed redirecting %s", stdio == STDIN_FILENO ? "stdin" : "stdout");
	}
	close(fd);
}

static void run(char *args[], struct env *env)
{
	char *cmd;

	redirect(args);

	cmd = args[0];
	if (cmd[0] == '&' || cmd[0] == '|') {
		if (!WIFEXITED(env->status))
			_exit(1);

		if (cmd[0] == '&') {
			if (WEXITSTATUS(env->status))
				_exit(WEXITSTATUS(env->status));
		} else {
			if (!WEXITSTATUS(env->status))
				_exit(WEXITSTATUS(env->status));
		}

		cmd++;
		while (*cmd && *cmd == ' ')
			cmd++;
		if (*cmd == 0)
			args++;
		else
			args[0] = cmd;
	}

	execvp(args[0], args);
	err(1, "Failed executing %s", args[0]);
}

static void pipeit(char *args[], struct env *env)
{
	pid_t pid;
	int fd[2];

	if (!env->pipes)
		run(args, env);

	if (pipe(fd))
		err(1, "Failed creating pipe");

	pid = fork();
	if (!pid) {
		if (dup2(fd[STDOUT_FILENO], STDOUT_FILENO) < 0)
			err(1, "Failed redirecting stdout");

		run(args, env);
	}

	close(fd[STDOUT_FILENO]);
	if (dup2(fd[STDIN_FILENO], STDIN_FILENO) < 0)
		err(1, "Failed redirecting stdin");

	waitpid(pid, &env->status, 0);

	env->pipes--;
	while (*args)
		args++;
	args++;

	pipeit(args, env);
}

static int parse(char *line, char *args[], struct env *env)
{
	char *ptr, *token;
	char *sep1 = " \t";
	char  sep2[2] = { 0 };
	char *sep = sep1;
	int num = 0;

	env->pipes = 0;
	env->cmds = 1;
	env->bg = 0;

	ptr = strpbrk(line, sep);
	if (ptr)
		*ptr++ = 0;
	token = line;

	while (token) {
		size_t len;
		int pipes = 0;
		int cmds = 0;
		int bg = 0;

		len = strlen(token) - 1;

		if (token[0] == '|') {
			args[num++] = NULL;
			token++;
			if (token[0] == '|')
				env->cmds++;
			else
				env->pipes++;
		}
		if (token[len] == '|') {
			if (token[len - 1] == '|') {
				token[len - 1] = 0;
				cmds++;
			} else {
				token[len] = 0;
				pipes++;
			}
		}

		if (token[0] == ';') {
			args[num++] = NULL;
			env->cmds++;
			token++;
		}
		if (token[len] == ';') {
			token[len] = 0;
			cmds++;
		}

		if (token[0] == '&') {
			args[num++] = NULL;
			token++;
			if (token[0] == '&')
				env->cmds++;
			else
				env->bg++;
		}
		if (token[len] == '&') {
			if (token[len - 1] == '&') {
				token[len - 1] = 0;
				cmds++;
			} else {
				token[len] = 0;
				bg++;
			}
		}

		if (*token)
			args[num++] = expand(token, env);

		if (pipes || cmds || bg) {
			args[num++] = NULL;

			if (pipes)
				env->pipes++;
			if (cmds)
				env->cmds++;
			if (bg)
				env->bg++;

			if (cmds && token[len] == '&')
				args[num++] = strdup("&");
			if (cmds && token[len] == '|')
				args[num++] = strdup("|");
		}

		if (!ptr)
		    break;
		line = ptr;
		if (*ptr == '\'' || *ptr == '"') {
			sep2[0] = *ptr;
			sep = sep2;
			line++;
		}

		ptr = strpbrk(line, sep);
		if (ptr) {
			if (*ptr == sep2[0])
				sep = sep1;
			*ptr++ = 0;
		}
		token = line;
	}
	args[num++] = NULL;

	return args[0] == NULL;
}

static void eval(char *line, struct env *env)
{
	pid_t pid;
	char *argv[strlen(line)];
	char **args;

	memset(argv, 0, sizeof(argv));
	if (parse(line, argv, env))
		goto cleanup;

	if (builtin(argv, env))
		goto cleanup;

	args = argv;
	do {
		pid = fork();
		if (!pid) {
			if (env->pipes)
				pipeit(args, env);
			else
				run(args, env);
		}

		if (env->bg)
			addjob(env, pid);
		else
			waitpid(pid, &env->status, 0);

		env->cmds--;
		while (*args)
			args++;
		args++;
	} while (env->cmds);

cleanup:
	for (size_t i = 0; i < NELEMS(argv); i++) {
		if (!argv[i])
			continue;
		free(argv[i]);
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
