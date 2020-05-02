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

#include <ctype.h>
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

static char *stripit(char *arg)
{
	int got = 0;
	char *end;

	while (isspace(*arg))
		arg++;

	if (*arg == '"') {
		got++;
		arg++;
	}

	end = arg;
	while (*end != 0)
		end++;
	end--;

	while (isspace(*end)) {
		*end = 0;
		end--;
	}

	if (got && *end == '"')
		*end = 0;

	return arg;
}

static char *env_set(char *arg, struct env *env)
{
	struct var *var;
	char *value;

	value = strchr(arg, '=');
	if (!value)
		return arg;
	*value++ = 0;

	arg   = stripit(arg);
	value = stripit(value);

	LIST_FOREACH(var, &env->variables, link) {
		if (!strcmp(var->key, arg)) {
			char *tmp;

			tmp = strdup(value);
			if (!tmp)
				goto nomem;

			free(var->value);
			var->value = tmp;
			goto done;
		}
	}

	var = malloc(sizeof(*var));
	if (!var) {
	nomem:
		warn("Out of memory creating new environment variable");
		goto done;
	}

	var->key = strdup(arg);
	if (!var->key) {
		free(var);
		goto nomem;
	}

	var->value = strdup(value);
	if (!var->value) {
		free(var->key);
		free(var);
		goto nomem;
	}

	LIST_INSERT_HEAD(&env->variables, var, link);
done:
	return NULL;
}

static char *env_get(char *name, struct env *env)
{
	struct var *var;

	LIST_FOREACH(var, &env->variables, link) {
		if (strcmp(var->key, name))
			continue;

		return var->value;
	}

	return getenv(name);
}

static char *env_expand(char *arg, struct env *env)
{
	char *ptr, *var, *rest;

	while ((ptr = strchr(arg, '$'))) {
	next:
		/* Handle \$, for literal $ character */
		if (ptr > arg) {
			rest = ptr;
			if (*--rest == '\\') {
				ptr = strchr(&ptr[1], '$');
				if (!ptr)
					break;
				goto next;
			}
		}
		*ptr++ = 0;

		rest = strchr(ptr, ' '); /* $IFS */
		if (rest)
			*rest++ = 0;
		else
			rest = "";

		var = env_get(ptr, env);
		if (var)
			arg = recompose("%a%s%s", arg, var, rest);
		else if (*ptr == '?')
			arg = recompose("%a%d%s", arg, WEXITSTATUS(env->status), rest);
		else if (*ptr == '$')
			arg = recompose("%a%d%s", arg, getpid(), rest);
		else if (*ptr == '!') {
			int id;

			id = env->lastjob;
			if (id < 0 || id >= MAXJOBS)
				arg = recompose("%a%s", arg, rest);
			else
				arg = recompose("%a%d%s", arg, env->jobs[id], rest);
		} else
			arg = recompose("%a%s", arg, rest);
	}

	return arg;
}

char *expand(char *token, struct env *env)
{
	char *arg;

	if (!token)
		return NULL;

	arg = strdup(token);
	if (!arg)
		return NULL;

	arg = tilde_expand(arg);
	arg = env_expand(arg, env);
	arg = env_set(arg, env);

	return arg;
}

