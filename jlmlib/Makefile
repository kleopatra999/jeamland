# This line is needed on some machines
MAKE=make

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

demos: mirror.jlm calc.jlm

shlib:
	@echo "Creating Shared Library."
	@${CC} ${CFLAGS} -fPIC -c jlm.c
	@${CC} -shared -Wl,-soname,`pwd`/libjlm.so -o libjlm.so jlm.o
	@echo "Done.."

lib: jlm.o
	@echo "Creating Static Library."
	@ar rcs libjlm.a jlm.o
	@echo "Done.."

mirror.jlm: mirror.o
	@echo "Linking mirror module."
	@cc -I. -L. -o mirror.jlm mirror.o -ljlm
	@echo "Done.."

calc.jlm: calc.o
	@echo "Linking calc module."
	@cc -I. -L. -o calc.jlm calc.o -ljlm
	@echo "Done.."

aclean:
	@echo "Cleaning directory jlmlib.."
	@-rm -f ${OBJS} test.o calc.o mirror.o core

clean: aclean
	@-rm -f libjlm.a test.jlm calc.jlm mirror.jlm libjlm.so jlm.o

jlm.o: jlm.h

.c.o:
	@echo "         $< -> $@"
	@$(CC) $(CFLAGS) -c $<

