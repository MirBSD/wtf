/*-
 * Copyright © 2019, 2020, 2022, 2023
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

__RCSID("$MirOS: wtf/sortdb.c,v 1.26 2023/02/28 20:01:19 tg Exp $");

#define MAXCASECONV 512
struct cconv {
	wchar_t lower;
	wchar_t upper;
} caseconv[MAXCASECONV];
size_t ncaseconv = 0;

const char null[] = "";

#define MAXLINES 1048576
struct line {
	/* null for comment lines (not dup-checked) */
	const char *acronym;
	/* string as output to the acronyms data file */
	const char *literal;
	/* string used during sorting (uppercased literal, tags at end) */
	const char *sorting;
	/* string used for dup checking (sorting minus tags, cf. parens) */
	const char *dupbase;
} lines[MAXLINES];
size_t nlines = 0;

wchar_t *ilines[MAXLINES];
size_t nilines = 0;

#define MAXACRO 128
wchar_t acro[MAXACRO];

#define MAXTAGS 1024
wchar_t atags[MAXTAGS];
wchar_t etags[MAXTAGS];

/* fix if running on not MirBSD */
uint8_t saw_upper[sizeof(wchar_t) == 2 ? 65536 : -1];

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
	int rv;

	if ((rv = strcmp(a->sorting, b->sorting)))
		return rv;
	return (strcmp(a->literal, b->literal));
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
	wchar_t *cwp, cw, *dwp, *twp, skipdots, *asp;
	uint8_t *ibuf, c;
	size_t len, bp, cp, asplen, atp, etp;
	int fd, rv = 0;
	struct stat sb;
	size_t nacro = 0, nexpn = 0, noutl = 0;

	memset(saw_upper, 0, sizeof(saw_upper));
	for (fd = 'A'; fd <= 'Z'; ++fd)
		++saw_upper[fd];
	/* whitelist */
	++saw_upper[0x0130]; /* capital turkish dotted i */
	++saw_upper[0x212A]; /* Kelvin sign */
	for (fd = 0x2160; fd <= 0x216F; ++fd)
		++saw_upper[fd]; /* roman numeral */
	++saw_upper[0x2183]; /* roman numeral */
	for (fd = 0x24B6; fd <= 0x24CF; ++fd)
		++saw_upper[fd]; /* circled letter */

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
	ilines[nilines++] = xambsntowcs((void *)(ibuf + bp), cp - bp - 1);
	if (cp < len)
		goto nextiline;
	fprintf(stderr, "I: %zu input lines (before deduplication)\n", nilines);
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
		if (cl == L'ß' && cu == L'ẞ')
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
		++saw_upper[cu];
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
			lines[nlines].acronym = null;
			lines[nlines].literal = lines[nlines].sorting =
			    xawcstombs(cwp);
			lines[nlines].dupbase = null;
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
		c = 0;
		/* .* */
		skipdots = *cwp == L'.';
		while ((cw = acro_toupper(cwp[cp])) != L'\t') {
			if (cw == L'.') {
				if (!c && cp > 0 &&
				    cwp[cp - 1] >= L'A' &&
				    cwp[cp - 1] <= L'Z') {
					/* *[A-Z].* */
					c |= 1;
				}
			} else if (cw != L'-') {
				/* *[!.-]* */
				c |= skipdots;
			}
			cwp[cp++] = cw;
		}
		skipdots = c ? L'.' : L'\0';
		cp = 0;
		while ((cw = *cwp++) != L'\t') {
			if (cw == skipdots)
				continue;
			acro[cp++] = cw;
			if (!saw_upper[cw]) {
				if (iswupper(cw)) {
					warnx("line %zu ucase %04X not handled",
					    nlines + 1, cw);
					rv = 3;
				}
				++saw_upper[cw];
			}
			if (cp == MAXACRO)
				errx(2, "raise %s and recompile", "MAXACRO");
		}
		acro[cp] = L'\0';
		asp = NULL;
		asplen = 0;
		atp = 0;
		etp = 0;
 parse_line:
		if (!(cw = *cwp++))
			goto end_of_line;
		if (iswspace(cw))
			goto parse_line;
		if (!atp && !asp && cw == L'{' && *cwp != L'}' &&
		    (twp = wcschr(cwp, /*{*/ L'}'))) {
			/* acronym casespelling */
			asp = cwp;
			asplen = twp - asp;
			*twp++ = L'\0';
			cwp = twp;
			/* check match */
			if (skipdots == L'.')
				goto have_skipdots;
			c = 0;
			bp = 0;
			skipdots = *asp == L'.';
			while ((cw = asp[bp])) {
				if (cw == L'.') {
					if (!c && bp > 0 &&
					    acro_toupper(asp[bp - 1]) >= L'A' &&
					    acro_toupper(asp[bp - 1]) <= L'Z') {
						/* *[A-Z].* */
						c |= 1;
					}
				} else if (cw != L'-') {
					/* *[!.-]* */
					c |= skipdots;
				}
				++bp;
			}
			skipdots = c ? L'.' : L'\0';
 have_skipdots:
			/* now we figured skipdots from both acro and asp */
			twp = asp;
			bp = 0;
			while ((cw = *twp++)) {
				if (cw == skipdots)
					continue;
				cw = acro_toupper(cw);
				if (cw != acro[bp++])
					goto asp_mismatch;
			}
			if (acro[bp] != L'\0') {
 asp_mismatch:
				fprintf(stderr, "W: #%zu spelling {%ls} "
				    " does not match acronym key {%ls}\n",
				    nlines + 1, asp, acro);
				rv = 3;
			}
			goto parse_line;
		}
		if (cw == L'[' && wcschr(cwp, L']')) {
			/* leading tag */
			if (atp) {
				/* space stuffing between tags */
				--cwp;
				cw = L' ';
			}
 stuff_tag:
			atags[atp++] = cw;
			if (atp == MAXTAGS)
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
			if (etp) {
 stuff_trailing_tag:
				cw = L' ';
			} else {
 stuff_trt_content:
				cw = *twp++;
			}
			etags[etp++] = cw;
			if (etp == MAXTAGS)
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
		atags[atp] = L'\0';
		etags[etp] = L'\0';
		if (!bp) {
			warnx("line %zu has no content, only tags: %ls",
			    nlines + 1, ilines[nlines]);
			rv = 3;
		}

		lines[nlines].acronym = xawcstombs(acro);
		len = cp + 1 + atp + 1 + bp + 1 + etp + 1;
		if (asp)
			len += asplen + 3;
		if ((dwp = calloc(len, sizeof(wchar_t))) == NULL)
			err(1, "out of memory");
		/* construct the literal */
		memcpy(dwp, acro, cp * sizeof(wchar_t));
		dwp[cp++] = L'\t';
		/* for recall of divergence point */
		twp = dwp + cp;
		if (asp) {
			*twp++ = L'{';
			memcpy(twp, asp, asplen * sizeof(wchar_t));
			twp += asplen;
			*twp++ = L'}';
			*twp++ = L' ';
		}
		if (atp) {
			memcpy(twp, atags, atp * sizeof(wchar_t));
			twp += atp;
			*twp++ = L' ';
		}
		memcpy(twp, cwp, bp * sizeof(wchar_t));
		twp += bp;
		if (etp) {
			*twp++ = L' ';
			memcpy(twp, etags, etp * sizeof(wchar_t));
			twp += etp;
		}
		*twp = L'\0';
		lines[nlines].literal = xawcstombs(dwp);
		/* now the other order for sorting */
		twp = dwp + cp;
		memcpy(twp, cwp, bp * sizeof(wchar_t));
		cwp = twp;
		twp += bp;
		c = 0;	/* do not drop dashes later if morse */
		while (cwp < twp) {
			cw = *cwp++;
			if (cw != L'-' && cw != L'.') {
				c = 1;
				break;
			}
		}
		if (asp) {
			*twp++ = L' ';
			*twp++ = L'{';
			cwp = twp + asplen;
			while (twp < cwp)
				*twp++ = L'~'; /* filler, see below */
			*twp++ = L'}';
		}
		if (atp) {
			*twp++ = L' ';
			memcpy(twp, atags, atp * sizeof(wchar_t));
			twp += atp;
		}
		if (etp) {
			*twp++ = L' ';
			memcpy(twp, etags, etp * sizeof(wchar_t));
			twp += etp;
		}
		*twp = L'\0';
		/* uppercase for case-insensitive sorting */
		twp = dwp + cp;
		cwp = twp;
		while ((cw = *cwp++))
			if (c && (cw == L'-' || cw == L'‑')) {
				if (cwp < (dwp + cp + bp))
					--bp;
			} else
				*twp++ = towupper(cw);
		*twp = L'\0';
		if (asp)
			/* asp is case-sensitive — overwrite again */
			memcpy(dwp + cp + bp + 2, asp, asplen * sizeof(wchar_t));
		lines[nlines].sorting = xawcstombs(dwp);
		/* back up to remove tags */
		twp = dwp + cp;
		cwp = twp + bp;
 do_dupbase:
		/* and parenthesis-surrounded links */
		do {
			*cwp-- = L'\0';
		} while (cwp > twp && iswspace(*cwp));
		while (cwp > twp && *cwp == /*(*/L')') {
			wchar_t *pwp = wcsrchr(twp, '('/*)*/);
			if (pwp == NULL || wcsncmp(pwp, L"(CF. "/*)*/, 5))
				/* not a cf. reference — ignore */
				break;
			if (pwp == twp) {
				/* expansion consists only of (cf. XXX) */
				/* kinda bad, we’d really like to keep all */
				/* keep only first, for now… */
#if 0
				fprintf(stderr, "I: #%zu empty <%s>\n",
				    nlines + 1, lines[nlines].literal);
#endif
				break;
			}
			cwp = pwp;
			goto do_dupbase;
		}
		--cp;
		if (asp) {
			/* length difference? */
			if (asplen != cp)
				memmove(dwp + asplen, dwp + cp,
				    (wcslen(dwp + cp) + 1) * sizeof(*dwp));
			/* use spelling for dupbase */
			memcpy(dwp, asp, asplen * sizeof(*dwp));
		}
		lines[nlines].dupbase = xawcstombs(dwp);
#if 0
		fprintf(stderr, "D: #%zu acronym<%s>\nliteral<%s>\nsorting<%s>\ndupbase<%s>\n",
		    nlines + 1, lines[nlines].acronym, lines[nlines].literal,
		    lines[nlines].sorting, lines[nlines].dupbase);
#endif
		++nlines;
		free(dwp);
	}

	if (mergesort(lines, nlines, sizeof(struct line), line_compar))
		err(1, "mergesort lines");

	nlines = 0;
	goto into_the_loop;
	while (++nlines < nilines) {
		if (!strcmp(lines[nlines - 1].literal, lines[nlines].literal)) {
			fprintf(stderr, "I: dup: %s\n", lines[nlines].literal);
			continue;
		}
		if (lines[nlines - 1].dupbase != null &&
		    lines[nlines].dupbase != null &&
		    !strcmp(lines[nlines - 1].dupbase, lines[nlines].dupbase)) {
			fprintf(stderr, "W: duplicate base string <%s>:\n"
			    "N: #%zu <%s>\nN: #%zu <%s>\n",
			    lines[nlines].dupbase,
			    nlines - 1 + 1, lines[nlines - 1].literal,
			    nlines + 1, lines[nlines].literal);
			rv = 3;
		}
		if (lines[nlines].acronym != null) {
			if (strcmp(lines[nlines - 1].acronym,
			    lines[nlines].acronym))
				++nacro;
			++nexpn;
		}
 into_the_loop:
		printf("%s\n", lines[nlines].literal);
		++noutl;
	}

	fprintf(stderr, "I: %zu lines, %zu acronyms with %zu expansions\n",
	    noutl, nacro, nexpn);
	return (rv);
}
