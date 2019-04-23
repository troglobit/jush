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

static void help(void)
{
	puts("This is jush version " VERSION ", a barely usable UNIX shell.\n");

	puts("Features:");
	puts("   ls | wc  UNIX pipes                     cmd &  Backgrounding jobs");
	puts("   ls;  ps  Sequential command       true && cmd  Logical conditions");
	puts("                                    false || cmd  Logical conditions");

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

int builtin(char *args[], struct env *env)
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
