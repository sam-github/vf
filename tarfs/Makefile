# Makefile:
# $Id$
# $Log$
# Revision 1.1  1999/04/11 01:57:12  sam
# Initial revision
#

CXXFLAGS = -w3 -g
OFLAGS   = -O
LDFLAGS  = -M $(CXXFLAGS)

INC = 
SRC = $(wildcard *.cc)
OBJ = 
EXE = main
LIB = 
DEP = depends.mak
ALL = $(DEP) $(LIB) $(EXE)

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
	rm -f *.o *.err core *.dmp *.log

empty: clean
	rm -f $(ALL) *.dbg *.map *.map.sort

# $Log$
# Revision 1.1  1999/04/11 01:57:12  sam
# Initial revision
#
