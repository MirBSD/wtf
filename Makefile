# $MirBSD: wtf/Makefile,v 1.2 2005/01/07 01:49:10 tg Exp $

SCRIPTS=	wtf
MAN=		wtf.1

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${SCRIPTS} ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
