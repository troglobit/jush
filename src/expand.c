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

	if (ptr[1] == '$') {
		*ptr++ = 0;
		return recompose("%d", getpid());
	}

	if (ptr[1] == '!') {
		int id;

		*ptr++ = 0;
		id = env->lastjob;
		if (id < 0 || id >= MAXJOBS)
			return strdup("");

		return recompose("%d", env->jobs[id]);
	}

	return arg;
}

char *expand(char *token, struct env *env)
{
	char *arg;

	arg = strdup(token);
	if (!token || !arg)
		return NULL;

	arg = tilde_expand(arg);
	arg = env_expand(arg, env);

	return arg;
}

