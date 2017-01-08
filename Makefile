# This line is needed on some machines.
MAKE=make

# Set optimisation level
OPTIMISE=-g
#OPTIMISE=-g3
#OPTIMISE=-g3 -pg
#OPTIMISE=-O
#OPTIMISE=-O3 -g
#OPTIMISE=-O3

# Enable warnings from the compiler (if wanted!)
#WARN=-Wall
#WARN=-pedantic -Wall -Wpointer-arith -Werror
#WARN=-pedantic -Wall -Wpointer-arith -W -Wno-unused

# Define your lex program
#LEX=flex
LEX=lex

#Define your awk program
#AWK=gawk
AWK=awk

# Define any extra options here.
COPTS=

#
#
# You shouldn't have to touch anything below!
#
#

MFLAGS=	${OPTIMISE} ${WARN} \
	${COPTS} ${STR} -I../include \
	-Dgets=DONT_USE_GETS -Dscanf=DONT_USE_SCANF
BASE=MFLAGS="${MFLAGS}" AWK="${AWK}" LEX="${LEX}"
GCCBASE= CC="gcc" ${BASE} MAKE="${MAKE}"
CCBASE= CC="cc" ${BASE} MAKE="${MAKE}"

all: 
	@echo
	@echo "Please run make with one of the following arguments."
	@echo "aix3	-- aix 3.x systems."
	@echo "aix4	-- aix 4.x systems."
	@echo "freebsd	-- FreeBSD systems with gcc."
	@echo "hpux	-- hpux systems."
	@echo "hpuxb	-- hpux systems with old (bad) select prototype."
	@echo "irix	-- sgi systems with IRIX."
	@echo "linux	-- linux systems."
	@echo "solaris	-- Solaris systems. (SunOS 5.x) with gcc."
	@echo "solcc	-- Solaris systems with cc (NOT TESTED)."
	@echo "sun	-- sun systems."
	@echo "sunb	-- sun system with buggy inet_ntoa() call."
	@echo "cc	-- Other systems using cc."
	@echo "gcc	-- Other systems using gcc."
	@echo
	@echo "tags	-- Creates a tags file (for use with vi)"
	@echo "clean	-- Removes all .o, core and jeamland binary files."
	@echo "aclean	-- Minimise disk usage, run after compilation."
	@echo "cxref	-- Creates cross-reference files using cxref."
	@echo "patch	-- Patch code from a diffs file."
	@echo
	@echo 'Use any other arguments at your own risk'
	@echo

# Stuff that I use when testing

ld: tags
	@echo '********'
	@echo 'WARNING: Compiling in memory debug mode.'
	@echo '********'
	@cd src; ${MAKE} ${GCCBASE} \
	    DEFS="-DDEBUG_MALLOC -DDEBUG_MALLOC_FUDGE -D__USE_BSD_SIGNAL \
	    -Dlinux" jeamland
	@cd util; ${MAKE} ${GCCBASE} \
	    DEFS="-D__USE_BSD_SIGNAL -Dlinux" utils
	@echo '********'
	@echo 'WARNING: Compiled in memory debug mode.'
	@echo '********'

lint:
	@cd src; ${MAKE} lint MFLAGS="-I../include" \
	    DEFS="-D__USE_BSD_SIGNAL -Dlinux" jeamland

# Dummies .. people tend to feel better if their machine has an entry ;-)
sun: gcc

linux:
	@cd src; ${MAKE} ${GCCBASE} DEFS="-D__USE_BSD_SIGNAL -Dlinux" jeamland
	@cd util; ${MAKE} ${GCCBASE} DEFS="-D__USE_BSD_SIGNAL -Dlinux" utils
	@scripts/mkdone

aix3:
	@cd src; ${MAKE} ${CCBASE} DEFS="-D_AIX" jeamland
	@cd util; ${MAKE} ${CCBASE} DEFS="-D_AIX" utils
	@scripts/mkdone

aix4:
	@cd src; ${MAKE} ${CCBASE} DEFS="-D_AIX4" jeamland
	@cd util; ${MAKE} ${CCBASE} DEFS="-D_AIX4" utils
	@scripts/mkdone

irix:
	@cd src; ${MAKE} ${CCBASE} DEFS="-Dsgi" LIBS="-lmalloc" jeamland
	@cd util; ${MAKE} ${CCBASE} DEFS="-Dsgi" utils
	@scripts/mkdone

sunb:
	@cd src; ${MAKE} ${GCCBASE} DEFS="-DBUGGY_INET_NTOA" jeamland
	@cd util; ${MAKE} ${GCCBASE} DEFS="-DBUGGY_INET_NTOA" utils
	@scripts/mkdone

solaris:
	@cd src; ${MAKE} ${GCCBASE} DEFS="-Dsunc" \
	    LIBS="-lmalloc -lsocket -lnsl" jeamland
	@cd util; ${MAKE} ${GCCBASE} DEFS="-Dsunc" ELIBS="-lsocket -lnsl" \
	    SLIBS="-lsocket -lnsl" utils
	@scripts/mkdone

solcc:
	@cd src; ${MAKE} ${CCBASE} DEFS="-Dsunc" \
	    LIBS="-lmalloc -lsocket -lnsl" jeamland
	@cd util; ${MAKE} ${CCBASE} DEFS="-Dsunc" ELIBS="-lsocket -lnsl" \
	    SLIBS="-lsocket -lnsl" utils
	@scripts/mkdone

hpuxb:
	@cd src; ${MAKE} ${GCCBASE} DEFS="-DHPUX -DHPUX_SB" jeamland
	@cd util; ${MAKE} ${GCCBASE} DEFS="-DHPUX -DHPUX_SB" utils
	@scripts/mkdone

hpux:
	@cd src; ${MAKE} ${GCCBASE} DEFS="-DHPUX" jeamland
	@cd util; ${MAKE} ${GCCBASE} DEFS="-DHPUX" utils
	@scripts/mkdone

freebsd:
	@cd src; ${MAKE} ${GCCBASE} DEFS="-DFREEBSD" jeamland LIBS="-lcrypt"
	@cd util; ${MAKE} ${GCCBASE} DEFS="-DFREEBSD" utils
	@scripts/mkdone

cc:
	@cd src; ${MAKE} ${CCBASE} jeamland
	@cd util; ${MAKE} ${CCBASE} utils
	@scripts/mkdone

gcc:
	@cd src; ${MAKE} ${GCCBASE} jeamland
	@cd util; ${MAKE} ${GCCBASE} utils
	@scripts/mkdone

rmtags:
	@-rm -f tags

tags: rmtags
	@echo Building tags file
	@-touch src/lex.yy.c
	@-rm src/lex.yy.c
	@ctags -S src/*.c include/*.h

cxref:
	@echo Building cross-reference files.
	@-rm -rf cxref
	@cd src; ${MAKE} cmd.table.h; rm -f lex.yy*
	@mkdir cxref
	@cxref -Iinclude \
	    -Dtime_t=int -DFILE=int -Dsize_t=int \
	    -xref-all -html -Ocxref \
	    src/*.c include/*.h
	@echo Done.

tidy: backup
	@(cd ..; rm -rf jeamland)

backup: clobber
	@(cd ..; tar -cf - jeamland | gzip -9v > jeamland.tar.gz; cd jeamland)

clean:
	@-rm -f lib/core
	@-rm -f tags
	@-rm -rf cxref
	@cd src; ${MAKE} clean
	@cd util; ${MAKE} clean
	@cd jlmlib; ${MAKE} clean

aclean:
	@cd src; ${MAKE} aclean
	@cd util; ${MAKE} aclean
	@cd jlmlib; ${MAKE} aclean
	@echo 'Stripping binaries'
	@scripts/stripall util/* bin/* src/jeamland jlmlib/*

distclean:
	-rm -f lib/secure/audit/* lib~/secure/audit/*
	-mv lib/secure/snoops/snooped lib/secure/snooped
	-rm -f lib/secure/snoops/*
	-mv lib/secure/snooped lib/secure/snoops/snooped
	-rm -f lib/log/* lib/log/secure/*
	-rm -f lib~/log/* lib~/log/secure/*
	-rm -f lib/log/channel/*/*
	-rm -f lib~/log/channel/*/*
	-rm -f lib/secure/email/* lib~/secure/email/*
	-rm -f lib/dead_ed/* lib~/dead_ed/*
	-rm -f lib/etc/pid.* lib~/etc/pid.*
	-rm -f include/.config.org src/.Makefile.orig
	-rm -f lib/help/sendme/rnclient
	-rm -f lib/help/spod

clobber realclean: clean distclean
	-cd nodist;${MAKE} clean

distrib: clobber
	@nodist/dodist

patch: clean
	@-rm -f install.sh
	@scripts/patch

rn:
	@(cd ..; tar -cvf - rnclient | gzip -9vc | uuencode rnclient.tar.gz) > \
	  lib/help/sendme/rnclient

