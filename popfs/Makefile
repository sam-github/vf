# Makefile:
# $Id$

URL = guest:guest@localhost

CXXFLAGS	= -w2 -I.. $(OFLAGS)
OFLAGS		= -O -g
LDFLAGS		= -T1 -M $(OFLAGS)
LDLIBS		= -L.. -lvf -lsocket++

INC = 
SRC = $(wildcard *.cc *.c)
OBJ = 
EXE = vf_pop
LIB = 
DEP = depends.mak
ALL = $(LIB) $(EXE)

# default target
all: $(ALL)

# custom targets

# special targets
$(LIB): $(OBJ)
	wlib -n -q -b $@ $^

$(EXE): $(LIB) ../vf.lib

$(OBJ): $(INC)

# standard targets

-include $(DEP)

deps:
	@makedeps -f - -I /usr/local/include -- $(CXXFLAGS) -- $(SRC) > $(DEP)

clean:
	rm -f *.o *.err core *.dmp *.log

empty: clean
	rm -f $(ALL) *.dbg *.map *.map.sort $(DEP)

# development targets

run: stop $(EXE)
	$(EXE) -v $(URL)

noisy: stop $(EXE)
	$(EXE) -vvvv $(URL)

debug: stop $(EXE)
	wd $(EXE) -d -vvvv $(URL)

stop:
	-slay -f $(EXE) || true

# rule forcing run of usemsg after linking
%: %.o
	$(LINK.o) -o $@ $< $(LDLIBS)
	-usemsg -c $@ $@.c*
	chown root $@
	chmod u+s $@
	-sort $@.map > $@.map.sort

%: %.c
	$(LINK.c) -o $@ $< $(LDLIBS)
	$(RM) $@.o
	-usemsg $@ $<
	chown root $@
	chmod u+s $@

%: %.cc
	$(LINK.cc) -o $@ $< $(LDLIBS)
	$(RM) $@.o
	-usemsg $@ $<
	chown root $@
	chmod u+s $@


# $Log$
# Revision 1.1  1999/07/19 15:33:32  sam
# Initial revision
#
