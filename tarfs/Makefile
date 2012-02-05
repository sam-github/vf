# Makefile:
# $Id$

TAR = test/mc-4.5.9.tar
TARFLAGS = 

CXXFLAGS	= -w2 -I.. $(OFLAGS) -I /usr/local/include
OFLAGS		= -O -g
LDFLAGS		= -T1 -M $(OFLAGS)
LDLIBS		= #-lsocket++ -L /usr/local/lib

INC = 
SRC = $(wildcard *.cc)
OBJ = vf_tarfile.o
EXE = vf_tar
LIB = 
DEP = depends.mak
ALL = $(LIB) $(EXE)

# default target
all: $(ALL)

# custom targets

# special targets
$(EXE): $(OBJ) ../vf.lib

# standard targets

-include $(DEP)

deps:
	@makedepend -f - -- $(CXXFLAGS) -- $(SRC) > $(DEP)

clean:
	rm -f *.o *.err core *.dmp *.log *.map *.map.sort

empty: clean
	rm -f $(ALL) *.dbg *.map *.map.sort $(DEP)

# development targets

run: stop vf_tar
	vf_tar $(TARFLAGS) $(TAR)

loud: stop vf_tar
	vf_tar $(TARFLAGS) -v $(TAR)

noisy: stop vf_tar
	vf_tar $(TARFLAGS) -vvv $(TAR)

debug: stop vf_tar
	wd vf_tar $(TARFLAGS) -vvvv $(TAR) -d &

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
	$(LINK.c) -o $@ $^ $(LDLIBS)
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
# Revision 1.7  2012/02/05 06:27:36  sam
# increase verbosity and other tweeks
#
# Revision 1.6  1999/07/21 13:44:02  sam
# usemsg was missing -c option
#
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
