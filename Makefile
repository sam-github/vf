# Makefile: build vf.lib and virtual filesystems
# $Id$

CFLAGS = -w3 -g
OFLAGS = -O
LDFLAGS = -M $(CFLAGS) -l vf.lib

INC = vf.h vf_fsys.h vf_dir.h vf_file.h
OBJ = vf_fsys.o vf_dir.o vf_file.o
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

clean:
	rm -f *.o *.err core *.dmp *.map

empty: clean
	rm -f $(ALL)

export: all

# rule forcing run of usemsg after linking
%: %.o
	$(LINK.o) -o $@ $^ $(LDLIBS)
	-usemsg -c $@ $@.c*

%: %.c
	$(LINK.c) -o $@ $< $(LDFLAGS)
	$(RM) $@.o
	-usemsg $@ $<

%: %.cc
	$(LINK.cc) -o $@ $< $(LDFLAGS)
	$(RM) $@.o
	-usemsg $@ $<

# $Log$
# Revision 1.1  1998/03/09 06:07:25  sroberts
# Initial revision
#
