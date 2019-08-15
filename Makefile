# $MirOS: wtf/Makefile,v 1.5 2019/08/15 01:49:59 tg Exp $

SCRIPTS=	wtf
MAN=		wtf.1

PROG=		sortdb
DPADD+=		${LIBMBFUN}
LDADD+=		-lmbfun

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${SCRIPTS} ${DESTDIR}${BINDIR}/

.include <bsd.prog.mk>
