# This line is needed on some machines.
MAKE=make

# Set optimisation level
OPTIMISE=-g
#OPTIMISE=-g3
#OPTIMISE=-g3 -pg
#OPTIMISE=-O
#OPTIMISE=-O2

# Enable warnings from the compiler (if wanted!)
#WARN=-Wall
#WARN=-Wall -Wtraditional
#WARN=-pedantic -Wall -Wtraditional -Wpointer-arith -Werror
#WARN=-pedantic -Wall -Wtraditional -Wpointer-arith -W

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
	${COPTS} ${STR} -I../include
BASE=MFLAGS="${MFLAGS}" AWK="${AWK}" LEX="${LEX}"
GCCBASE= CC="gcc" ${BASE}
CCBASE= CC="cc" ${BASE}

all: 
	@echo
	@echo "Please run make with one of the following arguments."
	@echo "aix3	-- aix 3.x systems."
	@echo "aix4	-- aix 4.x systems."
	@echo "hpux	-- hpux systems."
	@echo "hpuxb	-- hpux systems with a bad select prototype."
	@echo "linux	-- linux systems."
	@echo "sgi	-- sgi systems."
	@echo "solaris	-- Solaris systems. (SunOS 5.x)"
	@echo "sun	-- sun systems."
	@echo "sunb	-- sun system with buggy inet_ntoa() call."
	@echo "cc	-- Other systems using cc."
	@echo "gcc	-- Other systems using gcc."
	@echo "tags	-- Creates a tags file."
	@echo "clean	-- Removes all .o, core and jeamland binary files."
	@echo "aclean	-- Minimise disk usage, remove unecessary files."
	@echo "backup	-- Tar's and gzip's source into ../jeamland.tar.gz"
	@echo "pbackup	-- As backup, but preserves .o files."
	@echo "tidy	-- Runs backup then deletes this directory."
	@echo "cxref	-- Creates cross-reference files using cxref."
	@echo "patch    -- Patch code from a diffs file."
	@echo

# Stuff that I use when testing

td:
	@cd src; make ${CCBASE} DEFS="-Dsgi -DDEBUG_MALLOC" LIBS="-lmalloc"
	@cd util; make ${CCBASE} DEFS="-Dsgi"

sund:
	@cd src; make ${GCCBASE} DEFS="-DBUGGY_INET_NTOA -DDEBUG_MALLOC"
	@cd util; make ${GCCBASE} DEFS="-DBUGGY_INET_NTOA -DDEBUG_MALLOC"

linuxd ld:
	@cd src; make ${GCCBASE} \
	    DEFS="-DDEBUG_MALLOC -DDEBUG_MALLOC_FUDGE -D__USE_BSD_SIGNAL \
	    -Dlinux"
	@cd util; make ${GCCBASE} \
	    DEFS="-D__USE_BSD_SIGNAL -Dlinux"

lint:
	@cd src; make lint MFLAGS="-I../include" \
	    DEFS="-D__USE_BSD_SIGNAL -Dlinux"

hpd:
	@cd src; make ${GCCBASE} DEFS="-DHPUX -DDEBUG_MALLOC"
	@cd util; make ${GCCBASE} DEFS="-DHPUX -DDEBUG_MALLOC"

# Dummies .. people tend to feel better if their machine has an entry ;-)
sun: gcc

linux:
	@cd src; make ${GCCBASE} DEFS="-D__USE_BSD_SIGNAL -Dlinux"
	@cd util; make ${GCCBASE} DEFS="-D__USE_BSD_SIGNAL -Dlinux"
	@scripts/mkdone

aix3:
	@cd src; make ${CCBASE} DEFS="-D_AIX"
	@cd util; make ${CCBASE} DEFS="-D_AIX"
	@scripts/mkdone

aix4:
	@cd src; make ${CCBASE} DEFS="-D_AIX4"
	@cd util; make ${CCBASE} DEFS="-D_AIX4"
	@scripts/mkdone

sgi:
	@cd src; make ${CCBASE} DEFS="-Dsgi" LIBS="-lmalloc"
	@cd util; make ${CCBASE} DEFS="-Dsgi"
	@scripts/mkdone

sunb:
	@cd src; make ${GCCBASE} DEFS="-DBUGGY_INET_NTOA"
	@cd util; make ${GCCBASE} DEFS="-DBUGGY_INET_NTOA"
	@scripts/mkdone

solaris:
	@cd src; make ${GCCBASE} DEFS="-Dsunc" LIBS="-lmalloc -lsocket -lnsl"
	@cd util; make ${GCCBASE} DEFS="-Dsunc" ELIBS="-lsocket -lnsl" \
	    SLIBS="-lsocket -lnsl"
	@scripts/mkdone

hpuxb:
	@cd src; make ${GCCBASE} DEFS="-DHPUX -DHPUX_SB"
	@cd util; make ${GCCBASE} DEFS="-DHPUX -DHPUX_SB"
	@scripts/mkdone

hpux:
	@cd src; make ${GCCBASE} DEFS="-DHPUX"
	@cd util; make ${GCCBASE} DEFS="-DHPUX"
	@scripts/mkdone

cc:
	@cd src; make ${CCBASE}
	@cd util; make ${CCBASE}
	@scripts/mkdone

gcc:
	@cd src; make ${GCCBASE}
	@cd util; make ${GCCBASE}
	@scripts/mkdone

tags:
	@echo Building tags file
	@-rm -f tags
	@-rm src/lex.yy.c
	@ctags src/*.c include/*.h

cxref:
	@echo Building cross-reference files.
	@-rm -rf cxref
	@cd src; make cmd.table.h; rm -f lex.yy*
	@mkdir cxref
	@cxref -Iinclude \
	    -Dtime_t=int -DFILE=int -Dsize_t=int \
	    -xref-all -html -Ocxref \
	    src/*.c include/*.h
	@echo Done.

tidy: clean backup
	@(cd ..; rm -rf jeamland)

backup: clean
	@(cd ..; tar -cf - jeamland | gzip -9 > jeamland.tar.gz; cd jeamland)

pbackup: saveos backup restoreos

saveos:
	@mkdir ../jltmp
	@mkdir ../jltmp/uttmp
	@-mv src/*.o ../jltmp
	@-mv src/mkcmdprot ../jltmp
	@-mv src/cmd.table.h ../jltmp
	@-mv util/*.o ../jltmp/uttmp
	@-mv tags ../jltmp

restoreos:
	@-mv ../jltmp/tags tags
	@-mv ../jltmp/*.o src
	@-mv ../jltmp/mkcmdprot src
	@-mv ../jltmp/cmd.table.h src
	@-mv ../jltmp/uttmp/*.o util
	@-rm -rf ../jltmp

clean:
	@-rm -f lib/core
	@-rm -f tags
	@-rm -rf cxref
	@cd src; make clean
	@cd util; make clean
	@cd jlmlib; make clean

aclean:
	@cd src; make aclean
	@cd util; make aclean
	@cd jlmlib; make aclean
	@-strip bin/*

distclean:
	-rm -f lib/secure/audit/* lib~/secure/audit/*
	-mv lib/secure/snoops/snooped lib/secure/snooped
	-rm -f lib/secure/snoops/*
	-mv lib/secure/snooped lib/secure/snoops/snooped
	-rm -f lib/log/* lib/log/secure/* lib/log/channel/*
	-rm -f lib~/log/* lib~/log/secure/* lib~/log/channel/*
	-rm -f lib/secure/email/* lib~/secure/email/*
	-rm -f lib/dead_ed/* lib~/dead_ed/*
	-rm -f lib/etc/pid.* lib~/etc/pid.*
	-rm -f include/.config.org

clobber: clean distclean
	-cd nodist;make clean

VERSION=cat config.h | grep '^#define VERSION' | cut -d\" -f2

distrib: clobber
	@nodist/dodist

patch: clean
	@-rm -f install.sh
	@scripts/patch

