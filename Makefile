# $MirBSD: Makefile,v 1.3 2004/06/03 16:32:30 tg Stab $

SCRIPTS=	wtf
MAN=		wtf.1

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${SCRIPTS} ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
