/* $NetBSD: cat.c,v 1.54 2013/12/08 08:32:13 spz Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kevin Fall.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(lint)
__COPYRIGHT(
"@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#if 0
static char sccsid[] = "@(#)cat.c	8.2 (Berkeley) 4/27/95";
#else
__RCSID("$NetBSD: cat.c,v 1.54 2013/12/08 08:32:13 spz Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t bsize;
static int rval;
static const char *filename;

void raw_args(char *argv[]);
void raw_cat(int);

int
main(int argc, char *argv[])
{
	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	raw_args(argv);
	if (fclose(stdout))
		err(EXIT_FAILURE, "stdout");
	return rval;
}

void
raw_args(char **argv)
{
	int fd;
	argv++;

	fd = fileno(stdin);
	filename = "stdin";
	do {
		if (*argv) {
			if ((fd = open(*argv, O_RDONLY, 0)) < 0) {
				rval = EXIT_FAILURE;
				++argv;
				continue;
			}
			filename = *argv++;
		} else if (fd < 0) {
			err(EXIT_FAILURE, "stdin");
		}
		raw_cat(fd);
		if (fd != fileno(stdin))
			(void)close(fd);
	} while (*argv);
}

/*
 * バッファの大きさを決める。
 */
void
raw_cat(int rfd)
{
	static char *buf;
	static char fb_buf[BUFSIZ];

	ssize_t nr, nw, off;
	int wfd;

	wfd = fileno(stdout);
	if (wfd < 0)
		err(EXIT_FAILURE, "stdout");
	if (buf == NULL) {
		struct stat sbuf;

		if (bsize == 0) {
			if (fstat(wfd, &sbuf) == 0 && sbuf.st_blksize > 0 &&
			    (size_t)sbuf.st_blksize > sizeof(fb_buf))
				bsize = sbuf.st_blksize;
		}
		if (bsize > sizeof(fb_buf)) {
			buf = malloc(bsize);
			if (buf == NULL)
				warnx("malloc, using %zu buffer", bsize);
		}
		if (buf == NULL) {
			bsize = sizeof(fb_buf);
			buf = fb_buf;
		}
	}
	while ((nr = read(rfd, buf, bsize)) > 0)
		for (off = 0; nr; nr -= nw, off += nw)
			if ((nw = write(wfd, buf + off, (size_t)nr)) < 0)
				err(EXIT_FAILURE, "stdout");
	if (nr < 0) {
		warn("%s", filename);
		rval = EXIT_FAILURE;
	}
}
