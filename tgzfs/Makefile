# Makefile:
# $Id$

TAR = test/dir.tar.gz
TARFLAGS = #-e

CXXFLAGS	= -w2 -I.. $(OFLAGS) -I /usr/local/include
OFLAGS		= -O -g
LDFLAGS		= -T1 -M $(OFLAGS)
LDLIBS		= -lsocket++ -L /usr/local/lib

INC = 
SRC = $(wildcard *.cc)
OBJ = vf_tgzfile.o
EXE = vf_tgz
LIB = 
DEP = depends.mak
ALL = $(LIB) $(EXE)

# default target
all: $(ALL)

# custom targets

# special targets

$(EXE): $(OBJ) ../vf.lib

$(OBJ): $(INC)

# standard targets

-include $(DEP)

deps:
	@makedepend -f - -- $(CXXFLAGS) -- $(SRC) > $(DEP)

clean:
	rm -f *.o *.err core *.dmp *.log

empty: clean
	rm -f $(ALL) *.dbg *.map *.map.sort $(DEP)

# development targets

run: stop vf_tgz
	vf_tgz $(TARFLAGS) $(TAR) &

loud: stop vf_tgz
	vf_tgz $(TARFLAGS) -v $(TAR) &

noisy: stop vf_tgz
	vf_tgz $(TARFLAGS) -vv $(TAR) &

debug: stop vf_tgz
	wd vf_tgz $(TARFLAGS) -d -vvvv $(TAR) &

stop:
	-slay -f vf_tgz || true

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
# Revision 1.1  1999/11/24 03:54:01  sam
# Initial revision
#
