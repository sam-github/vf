# Makefile:
# $Id$

TAR = test/mc-4.5.9.tar
TARFLAGS = -e

CXXFLAGS	= -w2 -I.. $(OFLAGS)
OFLAGS		= -O -g
LDFLAGS		= -T1 -M $(OFLAGS)
LDLIBS		= -L.. -ltarfs -lvf

INC = 
SRC = $(wildcard *.cc)
OBJ = vf_tarfile.o
EXE = tart vf_tar
LIB = tarfs.lib
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

run: stop vf_tar
	vf_tar $(TARFLAGS) -v $(TAR) &

noisy: stop vf_tar
	vf_tar $(TARFLAGS) -vvv $(TAR) &

debug: stop vf_tar
	wd vf_tar $(TARFLAGS) -vvvv $(TAR) &

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
# Revision 1.5  1999/07/19 15:19:52  sam
# found some bugs while testing the distribution, fixed them
#
# Revision 1.4  1999/07/02 15:27:31  sam
# decreased verbosity of run command, and made executables depend on vf.lib
#
# Revision 1.3  1999/06/18 17:04:25  sam
# vf_tar now gueses name of directory, so don't specify
#
# Revision 1.2  1999/04/28 03:44:42  sam
# numerous improvements
#
# Revision 1.1  1999/04/11 01:57:12  sam
# Initial revision
#
