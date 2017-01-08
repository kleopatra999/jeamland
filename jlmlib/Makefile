# This line is needed on some machines
MAKE=make

CC=gcc
CFLAGS=-g -Wall

all:
	@echo
	@echo "Please run make with one of the following arguments."
	@echo "lib           -- Compile static library."
	@echo "shlib         -- Compile shared library (On ELF linux)."
	@echo "demos         -- Compile the demo programs."
	@echo "                 (Must compile a library first!)"
	@echo "clean         -- Removes all .o, core and binary files."
	@echo "aclean        -- Minimise disk usage, remove unecessary files."
	@echo

.SUFFIXES:	.jlm
.c.jlm:
	@echo "         $< -> $@"
	@${CC} -I. -L. ${CFLAGS} -o $*.jlm $< -ljlm

.c.o:
	@echo "         $< -> $@"
	@${CC} ${CFLAGS} -c $<

demos: mirror.jlm calc.jlm crash.jlm doom.jlm

shlib:
	@echo "Creating Shared Library."
	@${CC} ${CFLAGS} -fPIC -c jlm.c -o shjlm.o
	@${CC} -shared -Wl,-soname,`pwd`/libjlm.so -o libjlm.so shjlm.o
	@echo "Done.."

lib: jlm.o
	@echo "Creating Static Library."
	@ar rcs libjlm.a jlm.o
	@-ranlib libjlm.a
	@echo "Done.."

aclean:
	@echo "Cleaning directory jlmlib.."
	@-rm -f *.o core

clean: aclean
	@-rm -f libjlm.a libjlm.so *.o *.jlm

jlm.o: jlm.h

