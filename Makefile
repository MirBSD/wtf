# $MirBSD: Makefile,v 1.2 2003/09/22 18:14:08 tg Exp $
# Public domain.

SCRIPTS=	wtf
MAN=		wtf.1

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    ${SCRIPTS} ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
