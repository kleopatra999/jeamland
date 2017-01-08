# This line is needed on some machines
MAKE=make

SRCS=	erqd.c angel.c sign.c rquery.c sortsnoops.c
OBJS=	$(SRCS:.c=.o)
INCS=	../include/erq.h
CFLAGS=${MFLAGS} ${DEFS}

# If you want sortsnoops or rquery, add them to the following line.
#all: erqd angel sign rquery sortsnoops
all: erqd angel sign

aclean:
	@echo "Cleaning directory util.."
	@rm -f ${OBJS} core

clean: aclean
	@rm -f erqd angel sign rquery sortsnoops

erqd: erqd.o
	@echo "Linking erqd."
	@${CC} -o $@ erqd.o ${ELIBS}
	@echo "Done.."

angel: angel.o
	@echo "Linking angel."
	@${CC} -o $@ angel.o ${ALIBS}
	@echo "Done.."

sign: sign.o
	@echo "Linking sign."
	@${CC} -o $@ sign.o ${SLIBS}
	@echo "Done.."

rquery: rquery.o
	@echo "Linking rquery."
	@${CC} -o $@ rquery.o ${ELIBS}
	@echo "Done.."

sortsnoops: sortsnoops.o
	@echo "Linking sortsnoops."
	@${CC} -o $@ sortsnoops.o ${SNLIBS}
	@echo "Done.."

erqd.o: ../include/erq.h
angel.o:
sign.o:

.c.o:
	@echo "         $< -> $@"
	@$(CC) $(CFLAGS) -c $<

