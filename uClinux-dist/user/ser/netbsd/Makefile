# New ports collection makefile for:    ser
# Date created:        6 November 2002
# Whom:                andrei
#
# $NetBSD$
#
# $Id: Makefile,v 1.4.2.5 2005/12/15 17:11:20 andrei Exp $
#

COMMENT= "SIP Express Router"

PORTNAME= ser
PORTVERSION= 0.9.5
CATEGORIES= net
MASTER_SITES= ftp://ftp.berlios.de/pub/ser/0.9.5/src/

MAINTAINER= andrei@iptel.org

MAN8=ser.8
MAN5=ser.cfg.5
# MANCOMPRESSED must not be specified (if defined is assumed to be yes)

# netbsd 2.0
USE_TOOLS+=	gmake
#netbsd 1.6
USE_GMAKE= yes     
# we override the DISTFILE NAME
DISTNAME= ${PORTNAME}-${PORTVERSION}
DISTFILES= ${DISTNAME}_src.tar.gz
WRKSRC= ${WRKDIR}/${PORTNAME}-${PORTVERSION}

# build by default mysql and jabber
MAKE_ENV= include_modules="mysql jabber cpl-c pa"

# depends on expat, mysqlclient and libxml2 because of the above modules
DEPENDS= expat-[0-9]*:../../textproc/expat \
         mysql-client>=3.23.35:../../databases/mysql-client \
         libxml2*:../../textproc/libxml2

#.include "/usr/pkgsrc/mk/bsd.prefs.mk"

pre-fetch:

.include "../../textproc/expat/buildlink3.mk"
.include "../../databases/mysql-client/buildlink3.mk"
.include "../../textproc/libxml2/buildlink3.mk"

.include "../../mk/bsd.pkg.mk"


