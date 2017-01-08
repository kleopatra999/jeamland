# This line is needed on some machines
MAKE=make

SRCS=	access.c \
	alias.c \
	backend.c \
	board.c \
	cmd.table.c \
	cmd.visitor.c \
	cmd.resident.c \
	cmd.citizen.c \
	cmd.warden.c \
	cmd.consul.c \
	cmd.overseer.c \
	cmd.test.c \
	comm.c \
	crash.c \
	ed.c \
	event.c \
	file.c \
	grupe.c \
	hash.c \
	inetd.c \
	jlm.c \
	login.c \
	master.c \
	mbs.c \
	more.c \
	object.c \
	parse.c \
	room.c \
	sent.c \
	socket.c \
	stack.c \
	string.c \
	soul.c \
	user.c \
	vector.c \
	xalloc.c

OBJS=	$(SRCS:.c=.o)

INCS=	../include/access.h \
	../include/alias.h \
	../include/board.h \
	../include/command.h \
	../include/config.h \
	../include/erq.h \
	../include/event.h \
	../include/files.h \
	../include/grupe.h \
	../include/hash.h \
	../include/inetd.h \
	../include/jeamland.h \
	../include/jlm.h \
	../include/lint.h \
	../include/macro.h \
	../include/master.h \
	../include/mbs.h \
	../include/object.h \
	../include/room.h \
	../include/sent.h \
	../include/soul.h \
	../include/jlstring.h \
	../include/system.h \
	../include/termcap.h \
	../include/user.h

CFLAGS=${MFLAGS} ${DEFS}

all: jeamland

# Test stuff
lint:
	@echo "Linting code."
	lint ${CFLAGS} *.c

aclean:
	@echo "Cleaning directory jeamland."
	@rm -f ${OBJS} core
	@rm -f mkcmdprot.c mkcmdprot cmd.table.h lex.yy.c

clean: aclean
	@rm -f jeamland

jeamland: ${OBJS}
	@echo "Linking jeamland."
	@${CC} -o $@ ${OBJS} ${LIBS}
	@echo "Done.."

# $(OBJS): $(INCS)
# Experimental dependencies... there may be mistakes

GLOBAL= ../include/system.h ../include/jlstring.h ../include/jeamland.h
CMD_GLOBAL= $(GLOBAL) ../include/user.h ../include/room.h

alias.o: $(GLOBAL) ../include/alias.h ../include/user.h
access.o: $(GLOBAL) ../include/access.h
backend.o: $(GLOBAL) ../include/user.h ../include/room.h ../include/event.h \
	../include/config.h ../include/inetd.h ../include/object.h
board.o: $(GLOBAL) ../include/board.h ../include/user.h
cmd.visitor.o: $(CMD_GLOBAL) ../include/event.h ../include/command.h \
	../include/alias.h  ../include/board.h ../include/termcap.h \
	../include/soul.h
cmd.resident.o: $(CMD_GLOBAL) ../include/command.h ../include/alias.h \
	../include/board.h ../include/mbs.h ../include/grupe.h
cmd.citizen.o: $(CMD_GLOBAL)
cmd.warden.o: $(CMD_GLOBAL) ../include/board.h ../include/mbs.h
cmd.consul.c: $(CMD_GLOBAL) ../include/event.h ../include/board.h \
	../include/mbs.h
cmd.overseer.o: $(CMD_GLOBAL) ../include/event.h ../include/command.h
cmd.test.o: $(CMD_GLOBAL)
comm.o: $(GLOBAL) ../include/user.h ../include/ed.h ../include/object.h \
	../include/room.h ../include/termcap.h
ed.o: $(GLOBAL) ../include/ed.h ../include/user.h
event.o: $(GLOBAL) ../include/event.h
file.o: $(GLOBAL) ../include/user.h
grupe.o: $(GLOBAL) ../include/grupe.h
inetd.o: $(GLOBAL) ../include/inetd.h ../include/user.h ../include/event.h
jlm.o: $(GLOBAL) ../include/jlm.h
login.o: $(GLOBAL) ../include/access.h ../include/user.h
master.o: $(GLOBAL) ../include/master.h ../include/user.h ../include/room.h
mbs.o: $(GLOBAL) ../include/mbs.h ../include/user.h ../include/room.h
more.o: $(GLOBAL) ../include/user.h ../include/more.h
object.o: $(GLOBAL) ../include/object.h ../include/room.h ../include/user.h \
	../include/sent.h
parse.o: $(GLOBAL) ../include/command.h ../include/user.h
room.o: $(GLOBAL) ../include/room.h ../include/board.h
sent.o: $(GLOBAL) ../include/sent.h ../include/object.h
socket.o: $(GLOBAL) ../include/user.h ../include/event.h ../include/erq.h \
	../include/room.h
stack.o: $(GLOBAL)
string.o: $(GLOBAL) ../include/user.h ../include/room.h ../include/vector.h
soul.o: $(GLOBAL) ../include/soul.h ../include/user.h
user.o: $(GLOBAL) ../include/user.h ../include/room.h ../include/more.h \
	../include/ed.h
vector.o: $(GLOBAL) ../include/vector.h
xalloc.o: $(GLOBAL)

cmd.table.h: cmd.table.c

cmd.table.o: cmd.table.h

cmd.table.h: mkcmdprot
	@echo "         Generating cmd.table.h (using lex)"
	@./mkcmdprot < cmd.table.c > cmd.table.h

mkcmdprot: mkcmdprot.l
	@echo "         mkcmdprot.l -> mkcmdprot"
	@$(LEX) mkcmdprot.l
	@$(CC) -o mkcmdprot lex.yy.c

# Example dependency to replace the above two if you want/have to use the awk
# version of mkcmdprot.
#cmd.table.h: mkcmdprot.awk
#	@echo "         Generating cmd.table.h (using awk)"
#	@$(AWK) -fmkcmdprot.awk cmd.table.c > cmd.table.h

# Example dependency if you want/have to use the sed version of mkcmdprot
#cmd.table.h: mkcmdprot.sed
#	@echo "         Generating cmd.table.h (using sed)"
#	@sed -n -fmkcmdprot.sed cmd.table.c > cmd.table.h

.c.o:
	@echo "         $< -> $@"
	@$(CC) $(CFLAGS) -c $<

