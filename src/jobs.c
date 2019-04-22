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

void addjob(struct env *env, pid_t pid)
{
	for (int i = 0; i < MAXJOBS; i++) {
		if (env->jobs[i] != 0)
			continue;

		env->jobs[i] = pid;
		env->lastjob = i;
//		printf("[%d] Running            %s\n", i, job.cmdline);
		printf("[%d] %d\n", i, pid);
		break;
	}
}

void deljob(struct env *env, pid_t pid)
{
	for (int i = 0; i < MAXJOBS; i++) {
		if (env->jobs[i] != pid)
			continue;

		env->jobs[i] = 0;
		printf("[%d] Done\n", i);
		break;
	}
}

void jobs(struct env *env)
{
	for (int i = 0; i < MAXJOBS; i++) {
		if (env->jobs[i] == 0)
			continue;

		printf("[%d] %d\n", i, env->jobs[i]);
	}
}
