# Makefile:
# $Id$

CXXFLAGS	= -w2 -I.. $(OFLAGS) -I/usr/local/include
OFLAGS		= -O -g
LDFLAGS		= -T1 -M $(OFLAGS)
LDLIBS		=  -L.. $(LIB) -lsocket++ -lunix -L/usr/local/lib

INC = 
SRC = $(wildcard *.cc *.c)
OBJ = 
EXE = vf_pop mkbig mount_pop
DEP = depends.mak
ALL = $(LIB) $(EXE)

# default target
all: $(ALL)

# custom targets

mount_pop: vf_pop
	ln -fs $< $@

# special targets

vf_pop: vf_pop.o vf_popfile.o ../vf.lib

# standard targets

-include $(DEP)

deps:
	@makedepend -f - -I /usr/local/include -- $(CXXFLAGS) -- $(SRC) > $(DEP)

clean:
	rm -f *.o *.err core *.dmp *.log errlist

empty: clean
	rm -f $(ALL) *.dbg *.map *.map.sort $(DEP)

# development targets

URL = guest:guest@localhost

run: stop vf_pop
	vf_pop -v $(URL)

noisy: noisy3

noisy3: stop vf_pop
	vf_pop -vvv $(URL)

noisy4: stop vf_pop
	vf_pop -vvvv $(URL)

test:
	for f in guest@localhost/*; do cat $$f | tail & done

debug: stop vf_pop
	wd vf_pop -vvvv -d $(URL)

stop:
	-slay -f vf_pop || true

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
	$(LINK.cc) -o $@ $^ $(LDLIBS)
	$(RM) $@.o
	-usemsg -c $@ $<
	chown root $@
	chmod u+s $@


# $Log$
# Revision 1.3  2012/02/05 06:15:24  sam
# more utility targets for testing
#
# Revision 1.2  1999/07/21 13:44:02  sam
# usemsg was missing -c option
#
# Revision 1.1  1999/07/19 15:33:32  sam
# Initial revision
#
