# Makefile: RAM file system
#
# $Id$

CXXFLAGS	= -w2 -I.. $(OFLAGS)
OFLAGS		= -O -g
LDFLAGS		= -T1 -M $(OFLAGS)
LDLIBS		= -L.. -lvf

INC = 
SRC = $(wildcard *.cc)
OBJ = 
EXE = vf_ram
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

run: stop vf_ram
	vf_ram -v &

noisy: stop vf_ram
	vf_ram -vvv &

debug: stop vf_ram
	wd vf_ram -vvvv &

stop:
	-slay -f vf_tar || true

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
# Revision 1.1  1999/08/03 06:14:55  sam
# Initial revision
#
