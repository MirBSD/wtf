# $MirBSD: Makefile,v 1.1 2003/03/23 22:50:13 tg Exp $
# Public domain.

SCRIPTS=	wtf
NOOBS=		noobj
MAN=		wtf.1

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    ${SCRIPTS} ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
