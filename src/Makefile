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
	imud.c \
	imud3.c \
	inetd.c \
	jlm.c \
	login.c \
	mapping.c \
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
	../include/imud.h \
	../include/imud3.h \
	../include/inetd.h \
	../include/jeamland.h \
	../include/jlm.h \
	../include/lint.h \
	../include/macro.h \
	../include/mapping.h \
	../include/master.h \
	../include/mbs.h \
	../include/object.h \
	../include/room.h \
	../include/sent.h \
	../include/socket.h \
	../include/soul.h \
	../include/jlstring.h \
	../include/system.h \
	../include/termcap.h \
	../include/user.h

CFLAGS=${MFLAGS} ${DEFS}

def:
	@echo
	@echo "Run make from the top level JeamLand directory."
	@echo

# Test stuff
lint:
	@echo "Linting code."
	lint ${CFLAGS} *.c

aclean:
	@echo "Cleaning directory jeamland."
	@touch core
	@rm -f ${OBJS} core jeamland~
	@touch core
	@rm -f mkcmdprot.c mkcmdprot cmd.table.h lex.yy.c core

clean: aclean
	@rm -f jeamland jeamland~

jeamland: ${OBJS}
	@echo "Linking jeamland."
	@-touch $@
	@-mv $@ $@~
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
	../include/soul.h ../include/config.h
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
imud.c: $(GLOBAL) ../include/imud.h
imud3.c: $(GLOBAL) ../include/imud3.h
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

.c.o:
	@echo "         $< -> $@"
	@$(CC) $(CFLAGS) -c $<

# The following comment lines are required as they are searched for by the
# Configure script.

#MKCMDPROT_LEX_BEGIN
cmd.table.h: mkcmdprot
	@echo "         Generating cmd.table.h (using lex)"
	@./mkcmdprot < cmd.table.c > cmd.table.h

mkcmdprot: mkcmdprot.l
	@echo "         mkcmdprot.l -> mkcmdprot"
	@$(LEX) mkcmdprot.l
	@$(CC) -o mkcmdprot lex.yy.c
#MKCMDPROT_LEX_END

##MKCMDPROT_AWK_BEGIN
#cmd.table.h: mkcmdprot.awk
#	@echo "         Generating cmd.table.h (using awk)"
#	@$(AWK) -fmkcmdprot.awk cmd.table.c > cmd.table.h
##MKCMDPROT_AWK_END

##MKCMDPROT_SED_BEGIN
#cmd.table.h: mkcmdprot.sed
#	@echo "         Generating cmd.table.h (using sed)"
#	@sed -n -fmkcmdprot.sed cmd.table.c > cmd.table.h
##MKCMDPROT_SED_END

