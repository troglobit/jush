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

char *recompose(char *fmt, ...)
{
	va_list ap;
	size_t len;
	char *buf, *a = NULL;

	/* Initial length, takes / and \0 into account */
	len = strlen(fmt);
	buf = calloc(1, len);
	if (!buf)
		return NULL;

	va_start(ap, fmt);
	while (*fmt) {
		char tmp[64];
		char *s, *ptr;

		if (*fmt == '%') {
			fmt++;
			switch (*fmt) {
			case 'a':
				s = a = va_arg(ap, char *);
				goto str;
			case 's':
				s = va_arg(ap, char *);
			str:
				len += strlen(s);
				ptr = realloc(buf, len);
				if (!ptr) {
					free(buf);
					buf = NULL;
					goto done;
				}
				buf = ptr;
				strlcat(buf, s, len);
				break;

			case 'd':
				len += sizeof(int);
				ptr = realloc(buf, len);
				if (!ptr) {
					free(buf);
					buf = NULL;
					goto done;
				}
				buf = ptr;
				snprintf(tmp, sizeof(tmp), "%d", va_arg(ap, int));
				strlcat(buf, tmp, len);
				break;
			}
		} else {
			snprintf(tmp, sizeof(tmp), "%c", *fmt);
			strlcat(buf, tmp, len);
		}
		fmt++;
	}
done:
	if (a)
		free(a);
	va_end(ap);

	return buf;
}

