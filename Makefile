# Makefile: build vf.lib and virtual filesystems
# $Id$

CXXFLAGS = -w3 -g
OFLAGS = -O
LDFLAGS = -M $(CXXFLAGS) -l vf.lib

INC = vf.h vf_fsys.h vf_dir.h vf_file.h
SRC = $(wildcard *.cc)
OBJ = vf_fsys.o vf_dir.o vf_file.o vf_log.o
EXE = vf_test
LIB = vf.lib
ALL = $(LIB) $(EXE)

# default target
all: $(ALL)

# special targets
$(LIB): $(OBJ)
	wlib -n -q -b $@ $^

$(EXE): $(LIB)

$(OBJ): $(INC)

# standard targets

deps:
	makedeps -f - -I /usr/local/include -- $(CXXFLAGS) -- $(SRC) > depends.mak

clean:
	rm -f *.o *.err core *.dmp *.map

empty: clean
	rm -f $(ALL)

export: all

# rule forcing run of usemsg after linking
%: %.o
	$(LINK.o) -o $@ $^ $(LDLIBS)
	-usemsg -c $@ $@.c*
	-sort $@.map > $@.map.sort

%: %.c
	$(LINK.c) -o $@ $< $(LDFLAGS)
	$(RM) $@.o
	-usemsg $@ $<

%: %.cc
	$(LINK.cc) -o $@ $< $(LDFLAGS)
	$(RM) $@.o
	-usemsg $@ $<

# $Log$
# Revision 1.2  1998/03/15 22:06:12  sroberts
# added dependency generation
#
# Revision 1.1  1998/03/09 06:07:25  sroberts
# Initial revision
#
