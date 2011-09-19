/*** dadd.c -- perform simple date arithmetic, date plus duration
 *
 * Copyright (C) 2011 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of datetools.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **/
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include "date-core.h"
#include "date-io.h"


/* we parse durations ourselves so we can cope with the
 * non-commutativity of duration addition:
 * 2000-03-30 +1m -> 2000-04-30 +1d -> 2000-05-01
 * 2000-03-30 +1d -> 2000-03-31 +1m -> 2000-04-30 */
static struct dt_dur_s
dadd_strpdur(const char *str)
{
/* at the moment we allow only one format */
	struct dt_dur_s res = {DT_DUR_UNK};
	char *sp = ((union {char *p; const char *c;}){.c = str}).p;
	int tmp;
	int y = 0;
	int m = 0;
	int w = 0;
	int d = 0;

	if (str == NULL) {
		goto out;
	}
	/* read the year */
	do {
		tmp = strtol(sp, &sp, 10);
		switch (*sp++) {
		case '\0':
			/* must have been day then */
			d = tmp;
			goto assess;
		case 'd':
		case 'D':
			d = tmp;
			goto assess;
		case 'y':
		case 'Y':
			y = tmp;
			goto assess;
		case 'm':
		case 'M':
			m = tmp;
			goto assess;
		case 'w':
		case 'W':
			w = tmp;
			goto assess;
		default:
			goto out;
		}
	} while (*sp);
assess:
	if (LIKELY((m && d) ||
		   (y == 0 && m == 0 && w == 0) ||
		   (y == 0 && w == 0 && d == 0))) {
		res.typ = DT_DUR_MD;
		res.md.m = m;
		res.md.d = d;
	} else if (w) {
		res.typ = DT_DUR_WD;
		res.wd.w = w;
		res.wd.d = d;
	} else if (y) {
		res.typ = DT_DUR_YM;
		res.ym.y = y;
		res.ym.m = m;
	}
out:
	return res;
}


#define SECRET_BIT	(1U << 31)

union __d_or_dur_u {
	uint32_t typ;
	struct dt_d_s d;
	struct dt_dur_s dur;
};

static union __d_or_dur_u
strp_d_or_dur(const char *input, const char *const *fmt, size_t nfmt)
{
	union __d_or_dur_u res = {.typ = DT_UNK};

	if ((res.dur = dadd_strpdur(input)).typ > DT_DUR_UNK) {
		res.typ |= SECRET_BIT;
	} else if ((res.d = dt_io_strpd(input, fmt, nfmt)).typ > DT_UNK) {
		;
	}
	return res;
}

static inline int
__dp(union __d_or_dur_u d)
{
	return (d.typ & SECRET_BIT) == 0;
}

static inline int
__durp(union __d_or_dur_u d)
{
	return (d.typ & SECRET_BIT) == SECRET_BIT;
}

static int
dadd_addprnt(struct dt_d_s d, struct dt_dur_s dur, const char *fmt)
{
	struct dt_d_s da;
	char buf[256];

	da = dt_add(d, dur);
	dt_strfd(buf, sizeof(buf), fmt, da);
	fputs(buf, stdout);
	return 0;
}

static int
dadd_proc(
	const char *inp, union __d_or_dur_u ddur,
	const char *const *fmt, size_t nfmt, const char *ofmt)
{
	int res = -1;

	if (__durp(ddur)) {
		struct dt_d_s d;
		if ((d = dt_io_strpd(inp, fmt, nfmt)).typ > DT_UNK) {
			ddur.typ &= ~SECRET_BIT;
			res = dadd_addprnt(d, ddur.dur, ofmt);
		}
	} else if (__dp(ddur)) {
		struct dt_dur_s dur;
		if ((dur = dt_strpdur(inp)).typ > DT_DUR_UNK) {
			res = dadd_addprnt(ddur.d, dur, ofmt);
		}
	}
	return res;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "dadd-clo.h"
#include "dadd-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct gengetopt_args_info argi[1];
	union __d_or_dur_u d;
	const char *inp;
	const char *ofmt;
	char **fmt;
	size_t nfmt;
	int res = 0;

	/* fixup negative numbers, A -1 B for dates A and B */
	fixup_argv(argc, argv);
	if (cmdline_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num == 0) {
		fputs("Error: DATE or DURATION must be specified\n\n", stderr);
		cmdline_parser_print_help();
		res = 1;
		goto out;
	}
	/* unescape sequences, maybe */
	if (argi->backslash_escapes_given) {
		dt_io_unescape(argi->format_arg);
	}

	ofmt = argi->format_arg;
	fmt = argi->input_format_arg;
	nfmt = argi->input_format_given;

	/* check first arg */
	inp = unfixup_arg(argi->inputs[0]);
	if ((d = strp_d_or_dur(inp, fmt, nfmt)).typ == 0) {
		fprintf(stderr, "Error: neither date nor duration `%s'\n", inp);
		res = 1;
		goto out;
	}
	if (argi->inputs_num == 1) {
		/* read from stdin */
		FILE *fp = stdin;
		char *line;
		size_t lno = 0;

		/* no threads reading this stream */
		__fsetlocking(fp, FSETLOCKING_BYCALLER);

		for (line = NULL; !feof_unlocked(fp); lno++) {
			ssize_t n;
			size_t len;

			n = getline(&line, &len, fp);
			if (n < 0) {
				break;
			}
			/* terminate the string accordingly */
			line[n - 1] = '\0';
			/* perform addition now */
			dadd_proc(line, d, fmt, nfmt, ofmt);
		}
		/* get rid of resources */
		free(line);
		goto out;

	} else if (argi->inputs_num == 2) {
		/* special case for people that need the exit code */
		inp = unfixup_arg(argi->inputs[1]);
		if (dadd_proc(inp, d, fmt, nfmt, ofmt) < 0) {
			res = 1;
		}
	} else {
		cmdline_parser_print_help();
		res = 1;
	}

out:
	cmdline_parser_free(argi);
	return res;
}

/* dcal.c ends here */
