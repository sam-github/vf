# Makefile: build vf.lib and virtual filesystems
# $Id$

CXXFLAGS = -w3 -g -T1	# privity 1 to make kernel calls __get_fd,__init_fd
OFLAGS = -O
LDFLAGS = -M $(CXXFLAGS) -l vf.lib

INC = vf.h vf_mgr.h vf_dir.h vf_file.h
SRC = $(wildcard *.cc)
OBJ = vf_mgr.o vf_dir.o vf_file.o vf_log.o vf.o
EXE = vf_test
LIB = vf.lib
ALL = depends.mak $(LIB) $(EXE)
DEP = depends.mak

# default target
all: $(ALL)

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
	rm -f *.o *.err core *.dmp

empty: clean
	rm -f $(ALL) *.dbg *.map *.map.sort

export: all

# development targets

run: stop vf_test
	vf_test -vv &

log: stop vf_test
	vf_test -vv > vf_test.log &

debug: stop vf_test
	wd vf_test -vvvv &

stop:
	-slay -f vf_test || true

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
# Revision 1.7  1998/04/28 01:53:13  sroberts
# implimented read, fstat, rewinddir, lseek; seems to be a problem untaring
# a directory tree into the virtual filesystem though, checking in anyhow
#
# Revision 1.6  1998/04/06 06:50:55  sroberts
# added .map.sort to files to be emptied
#
# Revision 1.5  1998/04/05 17:35:52  sroberts
# added debug target
#
# Revision 1.4  1998/03/19 07:48:07  sroberts
# make makedeps run automatically when any file changed
#
# Revision 1.3  1998/03/19 07:41:25  sroberts
# implimented dir stat, open, opendir, readdir, rewinddir, close
#
# Revision 1.2  1998/03/15 22:06:12  sroberts
# added dependency generation
#
# Revision 1.1  1998/03/09 06:07:25  sroberts
# Initial revision
#
