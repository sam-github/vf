# Makefile:
# $Id$

TAR = test/file.tar.gz
TARFLAGS = -g

CXXFLAGS	= -w2 $(OFLAGS)
OFLAGS		= -O -g
LDFLAGS		= -M $(OFLAGS)
LDLIBS		= 

INC = 
SRC = $(wildcard *.cc)
OBJ = 
EXE = untar
LIB = 
DEP = depends.mak
ALL = $(EXE)

# default target
all: $(ALL)

# custom targets

# special targets
$(LIB): $(OBJ)
	wlib -n -q -b $@ $^

# standard targets

-include $(DEP)

deps:
	@makedepend -f - -- $(CXXFLAGS) -- $(SRC) > $(DEP)

clean:
	rm -f *.o *.err core *.dmp *.log *.map

empty: clean
	rm -f $(ALL) *.dbg *.map *.map.sort $(DEP)

# development targets

l: stop untar
	untar -vv -l $(TAR)

x: stop untar
	untar -vv -x $(TAR)

ldebug: stop untar
	wd untar $(TARFLAGS) -l -vvvv $(TAR) &

xdebug: stop untar
	wd untar $(TARFLAGS) -x -vvvv $(TAR) &

stop:
	-slay -f vf_tar || true

# rule forcing run of usemsg after linking
%: %.o
	$(LINK.o) -o $@ $^ $(LDLIBS)
	-usemsg -c $@ $@.c*
	chown root $@
	chmod u+s $@
	-sort $@.map > $@.map.sort

%: %.c
	$(LINK.c) -o $@ $< $(LDLIBS)
	$(RM) $@.o
	-usemsg -c $@ $<
	chown root $@
	chmod u+s $@

%: %.cc
	$(LINK.cc) -o $@ $< $(LDLIBS)
	$(RM) $@.o
	-usemsg -c $@ $<
	chown root $@
	chmod u+s $@

# $Log$
# Revision 1.1  1999/11/24 03:43:12  sam
# Initial revision
#
