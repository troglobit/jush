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

static int parse(char *line, char *args[])
{
	const char *sep = " \t";
	char *token;
	int i = 0;
	
	do {
		token = strtok(line, sep);
		args[i++] = token;
		line = NULL;
	} while (token);
	
	return args[0] == NULL;
}

static void run(char *line)
{
	char *args[strlen(line)];

	if (parse(line, args))
		return;

	if (!fork())
		execvp(args[0], args);
	else
		wait(NULL);
}

int main(int argc, char *argv[])
{
	char *line;

	while ((line = readline("$ ")))
		run(line);
}
