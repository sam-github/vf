# Makefile: build vf.lib and virtual ram filesystem
#
# $Id$

VERSION=vf-$(shell cat Version)

# customizable installation directory
prefix	= /usr/local

CXXFLAGS	= -w2 $(OFLAGS)
OFLAGS		= -Oeilrsx #-g
LDFLAGS 	= -M -T1 $(OFLAGS)
LDLIBS		= -l vf.lib
POD2HTMLFLAGS = --title="A C++ Framework for QNX Virtual Filesystems"

INC = vf.h vf_mgr.h vf_dir.h vf_file.h vf_syml.h
SRC = $(wildcard *.cc)
OBJ = vf_mgr.o vf_fdmap.o vf_dir.o vf_file.o vf_syml.o vf_path.o vf_log.o vf.o
EXE = stat fifot patht
VF	= tar ramfs tgzfs popfs tarfs
LIB = vf.lib
DOC = vf.txt vf.html
DEP = depends.mak
ALL = $(LIB) $(EXE) $(DOC)

# default targets
.PHONY: all vf deps

build: all vfs

all: $(ALL)

docs: $(DOC)

vfs: $(LIB)
	for d in $(VF); do $(MAKE) -C $$d; done

# special targets

$(LIB): $(OBJ)
	wlib -n -q -b $@ $^

$(EXE): $(LIB)

$(OBJ): $(INC)

# standard targets
.PHONY: clean empty export

-include $(DEP)

deps:
	makedepend -f - -I /usr/local/include -- $(CXXFLAGS) -- $(SRC) > $(DEP)

clean:
	rm -f *.o *.err core *.dmp *.log

empty: clean
	rm -f $(ALL) *.dbg *.map *.map.sort $(DEP)

install: build
	mkdir -p $(prefix)/bin
	cp tar/untar $(prefix)/bin
	cp ramfs/vf_ram $(prefix)/bin; cd $(prefix)/bin; ln -fs vf_ram mount_ram
	cp tarfs/vf_tar $(prefix)/bin; cd $(prefix)/bin; ln -fs vf_tar mount_tar
	cp tgzfs/vf_tgz $(prefix)/bin; cd $(prefix)/bin; ln -fs vf_tgz mount_tgz
	cp popfs/vf_pop $(prefix)/bin; cd $(prefix)/bin; ln -fs vf_pop mount_pop
	cd $(prefix)/bin; chown root:users vf_*; chmod u+s vf_*

docs: vf.txt vf.html

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

%.txt: %.pod
	pod2text $< > $@

#	perl -np -e's/A<([^\|]*)\|(.*)>/I<\1> (\2)/g' $< | pod2text > $@

%.html: %.pod
	pod2html $(POD2HTMLFLAGS) $< > _$@
	podfix.pl < _$@ > $@
	rm -f pod2html-*cache _$@

#	perl -np -e's/A<([^\|]*)\|(.*)>/<A href="\2">\1E<60>A>/g' $< > _$<

# release targets
.PHONY: release

release: docs
	mkdir -p $(VERSION)
	pax -w -f - < Manifest | (cd $(VERSION); tar -xf-)
	tar -cvf release/$(VERSION).tar $(VERSION)
	gzip release/$(VERSION).tar
	mv release/$(VERSION).tar.gz release/$(VERSION).tgz
	rm -Rf $(VERSION)
	use mount_ram > release/mount_ram.usage
	use mount_pop > release/mount_pop.usage
	use mount_tar > release/mount_tar.usage
	use mount_tgz > release/mount_tgz.usage
	cp vf.html release/

# $Log$
# Revision 1.20  2000/01/13 02:28:03  sam
#  path now has a custom allocator
#
# Revision 1.19  1999/12/16 04:51:48  sam
# now does recursive build by default, and does mkdir before the install
#
# Revision 1.18  1999/12/05 01:51:40  sam
# added install target, and tweaked release target
#
# Revision 1.17  1999/10/17 16:24:05  sam
# changed name of makedeps to X11's name, and including usage messages
# in the released website
#
# Revision 1.16  1999/08/09 15:12:51  sam
# To allow blocking system calls, I refactored the code along the lines of
# QSSL's iomanager2 example, devolving more responsibility to the entities,
# and having the manager and ocbs do less work.
#
# Revision 1.15  1999/08/03 06:13:38  sam
# moved ram filesystem into its own subdirectory
#
# Revision 1.14  1999/07/21 13:42:38  sam
# fixing the URLs output by pod2html, and usemsg was missing -c option
#
# Revision 1.13  1999/07/19 15:42:32  sam
# think I've worked out the kinks in the build...
#
# Revision 1.12  1999/07/14 17:17:47  sam
# documentation built into the release now
#
# Revision 1.11  1999/07/11 11:27:33  sam
# decreased vf_ram verbosity for run, and tweaked build defaults
#
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
