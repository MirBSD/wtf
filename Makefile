# $MirOS: wtf/Makefile,v 1.2 2005/03/13 18:34:07 tg Exp $

SCRIPTS=	wtf
MAN=		wtf.1

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${SCRIPTS} ${DESTDIR}${BINDIR}/

.include <bsd.prog.mk>
