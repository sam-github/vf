# Makefile:
# $Id$

TAR = tar/dir.tar

CXXFLAGS = -w2 -g -I.. -T1 # privity 1 to make kernel calls __get_fd,__init_fd
OFLAGS   = -O
LDFLAGS  = -M $(CXXFLAGS) -l vf.lib -L..

INC = 
SRC = $(wildcard *.cc)
OBJ = vf_tarfile.o
EXE = tart vf_tar
LIB = vf_tar.lib
DEP = depends.mak
ALL = $(DEP) $(LIB) $(EXE)

# default target
all: $(ALL)

# custom targets

# special targets
$(LIB): $(OBJ)
	wlib -n -q -b $@ $^

$(EXE): $(LIB)

$(OBJ): $(INC)

# standard targets

-include $(DEP)

$(DEP): $(SRC) $(INC)
	@makedeps -f - -I /usr/local/include -- $(CXXFLAGS) -- $(SRC) > $@

clean:
	rm -f *.o *.err core *.dmp *.log

empty: clean
	rm -f $(ALL) *.dbg *.map *.map.sort

# development targets

run: stop vf_tar
	vf_tar -u -vvv $(TAR) &

noisy: stop vf_tar
	vf_tar -u -vvvv $(TAR) &

debug: stop vf_tar
	wd vf_tar -u -vvvv $(TAR) &

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
	$(LINK.c) -o $@ $< $(LDFLAGS)
	$(RM) $@.o
	-usemsg $@ $<
	chown root $@
	chmod u+s $@

%: %.cc
	$(LINK.cc) -o $@ $< $(LDFLAGS)
	$(RM) $@.o
	-usemsg $@ $<
	chown root $@
	chmod u+s $@

# $Log$
# Revision 1.3  1999/06/18 17:04:25  sam
# vf_tar now gueses name of directory, so don't specify
#
# Revision 1.2  1999/04/28 03:44:42  sam
# numerous improvements
#
# Revision 1.1  1999/04/11 01:57:12  sam
# Initial revision
#
