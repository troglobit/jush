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

#include <dirent.h>
#include "jush.h"

extern int el_filename_list_possib(char *pathname, char ***av);
extern char *el_filename_complete(char *pathname, int *match);

static int first_arg(void)
{
	if (rl_point > 0) {
		char *ptr = strchr(rl_line_buffer, ' ');

		if (ptr && (ptr - rl_line_buffer < rl_point))
			return 0;
	}

	return 1;
}

static char **get_exec(const char *text)
{
	size_t len, num = 0;
	char *paths, *path;
	char **matches;

	paths = getenv("PATH");
	if (!paths)
		return NULL;

	len = strlen(text);

	paths = strdup(paths);
	if (!paths)
		return NULL;

	matches = calloc(num + 1, sizeof(char *));
	if (!matches)
		return NULL;

	path = strtok(paths, ":");
	while (path) {
		struct dirent *d;
		DIR *dir;

		dir = opendir(path);
		while (dir && (d = readdir(dir))) {
			struct stat st;
			char file[MAXPATHLEN];
			char **ptr;

			if (d->d_name[strlen(d->d_name) - 1] == '~')
				continue;

			snprintf(file, sizeof(file), "%s/%s", path, d->d_name);
			if (access(file, X_OK) || stat(file, &st))
				continue;

			if (!S_ISREG(st.st_mode))
				continue;

			if (len > 0 && strncmp(text, d->d_name, len))
				continue;

			matches[num++] = strdup(d->d_name);
			ptr = realloc(matches, (num + 1) * sizeof(char *));
			if (!ptr) {
				if (dir)
					closedir(dir);
				return NULL;
			}
			matches = ptr;
		}
		if (dir)
			closedir(dir);

		path = strtok(NULL, ":");
	}
	matches[num] = NULL;

	if (!num) {
		free(matches);
		matches = NULL;
	}

	return matches;
}

char *complete(char *text, int *match)
{
	size_t i, j, len, end, ac;
	char **av;
	char *word = NULL;

	if (!first_arg())
		return el_filename_complete(text, match);

	ac = (size_t)list_possible(text, &av);
	if (ac == 0)
		return NULL;

	if (ac == 1)
		*match = 1;
	else
		*match = 0;

	/* Find largest matching substring. */
	len = strlen(text);
	for (i = len, end = strlen(av[0]); i < end; i++) {
		for (j = 1; j < ac; j++) {
			if (av[0][i] != av[j][i])
				goto done;
		}
	}
	done:
	if (i >= len) {
		j = i - len + 1;
		word = malloc(sizeof(char) * j + 2);
		if (word) {
			memcpy(word, av[0] + len, j);
			word[j - 1] = '\0';
		}

		if (ac == 1)
			strcat(word, " ");
	}

	for (i = 0; i < ac; i++)
		free(av[i]);
	free(av);

	return word;
}

int list_possible(char *text, char ***av)
{
	size_t ac;
	char **matches;

	if (!first_arg())
		return el_filename_list_possib(text, av);

	matches = get_exec((const char *)text);
	if (!matches)
		return 0;

	for (ac = 0; matches[ac]; ac++)
		;
	*av = matches;

	return (int)ac;
}
