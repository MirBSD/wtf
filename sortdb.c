/*-
 * Copyright © 2019
 *	mirabilos <m@mirbsd.org>
 *
 * Provided that these terms and disclaimer and all copyright notices
 * are retained or reproduced in an accompanying document, permission
 * is granted to deal in this work without restriction, including un‐
 * limited rights to use, publicly perform, distribute, sell, modify,
 * merge, give away, or sublicence.
 *
 * This work is provided “AS IS” and WITHOUT WARRANTY of any kind, to
 * the utmost extent permitted by applicable law, neither express nor
 * implied; without malicious intent or gross negligence. In no event
 * may a licensor, author or contributor be held liable for indirect,
 * direct, other damage, loss, or other issues arising in any way out
 * of dealing in the work, even if advised of the possibility of such
 * damage or existence of a defect, except proven that it results out
 * of said person’s immediate fault when using the work as intended.
 */

#define _ALL_SOURCE
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <mbfun.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

__RCSID("$MirOS: wtf/sortdb.c,v 1.1 2019/08/15 01:49:59 tg Exp $");

#define MAXCASECONV 512
struct cconv {
	wchar_t lower;
	wchar_t upper;
} caseconv[MAXCASECONV];
size_t ncaseconv = 0;

#define MAXLINES 1048576
struct line {
	wchar_t *literal;
	char *compare;
} lines[MAXLINES];
size_t nlines = 0;

wchar_t *ilines[MAXLINES];
size_t nilines = 0;

#define MAXACRO 128
wchar_t acro[MAXACRO];

#define MAXTAGS 1024
wchar_t tags[MAXTAGS];

#define get(ofs) __extension__({			\
	size_t get_ofs = (ofs);				\
							\
	(get_ofs >= len ? (uint8_t)0 : ibuf[get_ofs]);	\
})

#define xwcsdup(p) __extension__({			\
	wchar_t *xwcsdup_res = wcsdup(p);		\
							\
	if (!xwcsdup_res)				\
		err(1, "out of memory");		\
	(xwcsdup_res);					\
})

static int
line_compar(const void *aa, const void *bb)
{
	const struct line *a = (const struct line *)aa;
	const struct line *b = (const struct line *)bb;

	return (strcmp(a->compare, b->compare));
}

static int
cconv_compar(const void *aa, const void *bb)
{
	const struct cconv *a = (const struct cconv *)aa;
	const struct cconv *b = (const struct cconv *)bb;

	if (a->lower < b->lower)
		return (-1);
	if (a->lower > b->lower)
		return (1);
	if (!a->upper || !b->upper)
		return (0);
	if (a->upper < b->upper)
		return (-1);
	if (a->upper > b->upper)
		return (1);
	return (0);
}

static wchar_t
acro_toupper(wchar_t wc)
{
	struct cconv *match;
	struct cconv test;

	if (wc < 32 && wc != 9 && wc != 10)
		errx(2, "acronym contains control character %02X", wc);
	if (wc >= L'a' && wc <= L'z')
		return (wc + L'A' - L'a');
	test.lower = wc;
	test.upper = 0;
	if ((match = (struct cconv *)bsearch(&test, caseconv, ncaseconv,
	    sizeof(struct cconv), cconv_compar)) == NULL)
		return (wc);
	if (!match->upper)
		errx(99, "match.upper for %04X (%lc) is WNUL",
		    (unsigned int)wc, wc);
	return (match->upper);
}

int
main(int argc, char *argv[])
{
	wchar_t *cwp, cw, *dwp, *twp;
	uint8_t *ibuf, c;
	size_t len, bp, cp, tp;
	int fd, rv = 0;
	struct stat sb;

	if (argc != 2) {
		fprintf(stderr, "Syntax: %s acronyms\n",
		    argv[0] ? argv[0] : "sortdb");
		return (1);
	}

	if ((fd = open(argv[1], O_RDONLY | O_SHLOCK)) < 0)
		err(1, "open");
	if (fstat(fd, &sb))
		err(1, "stat");
	if (sb.st_size > (off_t)SSIZE_MAX)
		errx(1, "input too large");
	len = (size_t)sb.st_size;
	if ((ibuf = mmap(NULL, len, PROT_READ, MAP_FILE, fd,
	    (off_t)0)) == MAP_FAILED)
		err(1, "mmap");

	cp = 0;
 nextiline:
	if (nilines == MAXLINES)
		errx(2, "raise %s and recompile", "MAXLINES");
	bp = cp;
	while ((c = get(cp++)) && c != '\n')
		/* nothing */;
	if (!c)
		errx(2, "NUL at offset %zu", cp - 1);
	if (cp - 1 == bp)
		errx(2, "empty line at offset %zu", cp - 1);
	switch (get(cp - 1)) {
	case 0x09:
	case 0x0C:
	case 0x0D:
	case 0x20:
		warnx("line %zu ends with whitespace at offset %zu",
		    nilines + 1, cp - 1);
		rv = 3;
		break;
	}
	ilines[nilines++] = ambsntowcs((void *)(ibuf + bp), cp - bp - 1);
	if (cp < len)
		goto nextiline;
	fprintf(stderr, "I: %zu input lines\n", nilines);
	munmap(ibuf, len);
	close(fd);
	if (nilines < 3)
		errx(2, "file likely too short");

	cwp = ilines[0];
	if (*cwp++ != L' ')
		errx(2, "first line does not start with a space: %ls",
		    ilines[0]);
	do {
		wchar_t cl, cu, clu, cul;

		if (cwp[0] != L' ' || !cwp[1] || cwp[2] != L'/' || !cwp[3])
			errx(2, "error in caseconv pair: %ls", cwp);
		cl = cwp[1];
		cu = cwp[3];
		if (cl == L'ℒ' && cu == L'ℓ')
			goto caseconv_checks_done;
		clu = towupper(cl);
		cul = towlower(cu);

		if (!iswlower(cl))
			errx(2, "caseconv pair %lc/%lc lower is not lower",
			    cl, cu);
		if (!iswupper(cu))
			errx(2, "caseconv pair %lc/%lc upper is not upper",
			    cl, cu);
		if (clu != cu)
			errx(2, "caseconv pair %lc/%lc LOWER %lc is not upper",
			    cl, cu, clu);
		if (cul != cl &&
		    !(cl == L'ς' && cu == L'Σ' && cul == L'σ'))
			warnx("caseconv pair %lc/%lc upper %lc is not lower",
			    cl, cu, cul);
 caseconv_checks_done:

		caseconv[ncaseconv].lower = cl;
		caseconv[ncaseconv].upper = cu;
		if (++ncaseconv == MAXCASECONV)
			errx(2, "raise %s and recompile", "MAXCASECONV");
		cwp += 4;
	} while (*cwp);

	if (mergesort(caseconv, ncaseconv, sizeof(struct cconv), cconv_compar))
		err(1, "mergesort caseconv");
	if ((cwp = calloc(1 + 4 * ncaseconv + 1, sizeof(wchar_t))) == NULL)
		err(1, "out of memory");
	cwp[0] = L' ';
	for (bp = 0; bp < ncaseconv; ++bp) {
		cwp[1 + bp * 4] = L' ';
		cwp[1 + bp * 4 + 1] = caseconv[bp].lower;
		cwp[1 + bp * 4 + 2] = L'/';
		cwp[1 + bp * 4 + 3] = caseconv[bp].upper;
	}
	/* NUL already there from calloc */
	goto firstline;

	while (nlines < nilines) {
		if ((cwp = wcschr(ilines[nlines], L'\t')) == NULL) {
			cwp = ilines[nlines];
			/* comment line (no TAB) */
			if (cwp[0] != L' ') {
				warnx("comment line %zu does not begin with space: %ls",
				    nlines + 1, cwp);
				rv = 3;
			}
 firstline:
			lines[nlines].literal = cwp;
			lines[nlines].compare = awcstombs(cwp);
			++nlines;
			continue;
		}
		if (wcschr(cwp + 1, L'\t') != NULL) {
			warnx("line %zu tab in expansion: %ls",
			    nlines + 1, ilines[nlines]);
			rv = 3;
		}
		cwp = ilines[nlines];
		cp = 0;
		while ((cw = *cwp++) != L'\t') {
			if (cw == L'.' && cp > 0 &&
			    acro[cp - 1] >= L'A' && acro[cp - 1] <= L'Z') {
				/* skip period after upper-cased latin */
				continue;
			}
			acro[cp++] = acro_toupper(cw);
			if (cp == MAXACRO)
				errx(2, "raise %s and recompile", "MAXACRO");
		}
		acro[cp] = L'\0';
		tp = 0;
 parse_line:
		if (!(cw = *cwp++))
			goto end_of_line;
		if (iswspace(cw))
			goto parse_line;
		if (cw == L'[' && wcschr(cwp, L']')) {
			/* leading tag */
			if (tp) {
				/* space stuffing between tags */
				--cwp;
				cw = L' ';
			}
 stuff_tag:
			tags[tp++] = cw;
			if (tp == MAXTAGS)
				errx(2, "raise %s and recompile", "MAXTAGS");
			if (cw == L']')
				goto parse_line;
			if (!(cw = *cwp++))
				errx(2, "EOL inmidst a tag? line %zu",
				    nlines + 1);
			goto stuff_tag;
		}
		/* not a leading tag nor whitespace nor EOL */
		--cwp;
		/* find end of string handling trailing tags and whitespace */
		twp = cwp + wcslen(cwp);
		dwp = twp - 1;

 check_trailing:
		while (dwp > cwp && iswspace(*dwp))
			--dwp;
		if (dwp > cwp && *dwp == L']' && wcschr(cwp, L'[')) {
			while (dwp > cwp && *dwp != L'[')
				--dwp;
			twp = dwp--;
			goto check_trailing;
		}
		if (*twp) {
			if (tp) {
 stuff_trailing_tag:
				cw = L' ';
			} else {
 stuff_trt_content:
				cw = *twp++;
			}
			tags[tp++] = cw;
			if (tp == MAXTAGS)
				errx(2, "raise %s and recompile", "MAXTAGS");
			if (cw != L']')
				goto stuff_trt_content;
			while (iswspace(*twp))
				++twp;
			if (*twp /* == L'[' */)
				goto stuff_trailing_tag;
		}
		/* no trailing tags or whitespace */
		*++dwp = L'\0';
		bp = dwp - cwp;
		if (0)
 end_of_line:
		  bp = wcslen(cwp);
		tags[tp] = L'\0';
		if (!bp) {
			warnx("line %zu has no content, only tags: %ls",
			    nlines + 1, ilines[nlines]);
			rv = 3;
		}

		lines[nlines].literal = calloc(cp + 1 + bp + 1 + tp + 1,
		    sizeof(wchar_t));
		dwp = calloc(cp + 1 + bp + 1 + tp + 1,
		    sizeof(wchar_t));
		memcpy(lines[nlines].literal, acro, cp * sizeof(wchar_t));
		memcpy(dwp, acro, cp * sizeof(wchar_t));
		lines[nlines].literal[cp] = L'\t';
		dwp[cp] = L'\t';
		++cp;
		memcpy((twp = dwp + cp), cwp, bp * sizeof(wchar_t));
		if (tp) {
			dwp[cp + bp] = L' ';
			memcpy(dwp + cp + bp + 1, tags,
			    tp * sizeof(wchar_t));
			memcpy(lines[nlines].literal + cp, tags,
			    tp * sizeof(wchar_t));
			cp += tp;
			lines[nlines].literal[cp++] = L' ';
		}
		memcpy(lines[nlines].literal + cp, cwp, bp * sizeof(wchar_t));

		while ((cw = *twp))
			*twp++ = towupper(cw);
		lines[nlines++].compare = awcstombs(dwp);
		free(dwp);
	}

	if (mergesort(lines, nlines, sizeof(struct line), line_compar))
		err(1, "mergesort lines");

	for (nlines = 0; nlines < nilines; ++nlines)
		printf("%ls\n", lines[nlines].literal);

	return (rv);
}
