# Makefile: build vf.lib and virtual ram filesystem
#
# $Id$

# privity 1 to make kernel calls (__get_fd, __init_fd, etc.)
CXXFLAGS = -w2 -g -T1
OFLAGS = -O
LDFLAGS = -M $(CXXFLAGS) -l vf.lib

INC = vf.h vf_mgr.h vf_dir.h vf_file.h vf_syml.h
SRC = $(wildcard *.cc)
OBJ = vf_mgr.o vf_dir.o vf_file.o vf_syml.o vf_log.o vf.o
EXE = vf_ram
VF	= tarfs popfs
LIB = vf.lib
DEP = depends.mak
ALL = deps $(LIB) $(EXE)

# default targets
.PHONY: all vf deps

all: build vf

build: deps $(LIB) $(EXE)

vf: $(LIB)
	for d in $(VF); do $(MAKE) -C $$d; done

deps: $(DEP)

# special targets

$(LIB): $(OBJ)
	wlib -n -q -b $@ $^

$(EXE): $(LIB)

$(OBJ): $(INC)

# standard targets
.PHONY: clean empty export

-include $(DEP)

$(DEP): $(SRC) $(INC)
	@makedeps -f - -I /usr/local/include -- $(CXXFLAGS) -- $(SRC) > $@

clean:
	rm -f *.o *.err core *.dmp *.log

empty: clean
	rm -f $(ALL) *.dbg *.map *.map.sort

export: all

#.PHONY: clean empty export

# development targets
.PHONY: run noisy debug stop

run: stop vf_ram
	vf_ram -vv &

noisy: stop vf_ram
	vf_ram -vvvvv &

debug: stop vf_ram
	wd vf_ram -vvvv &

stop:
	-slay -f vf_ram || true

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

# release targets
.PHONY: release

release:
	cd ..; pax -w -f ../www/vf.tar < vf/Manifest
	cp vf_ram tarfs/vf_tar ../../www/
	wstrip ../../www/vf_ram
	wstrip ../../www/vf_tar
	gzip ../../www/vf_* ../../www/*.tar

# $Log$
# Revision 1.10  1999/06/21 10:27:28  sam
# added targets for the vf sub-dirs
#
# Revision 1.9  1999/06/14 14:27:30  sam
# changed vf_test to vf_ram, and added release target
#
# Revision 1.8  1998/04/28 07:26:43  sroberts
# changed log target to noisy target
#
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
