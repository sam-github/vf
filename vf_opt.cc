//
// vf_opt.cc
//
// Copyright (c) 1998, Sam Roberts
// 
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 1, or (at your option)
//  any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  I can be contacted as sroberts@uniserve.com, or sam@cogent.ca.
//
// $Log$
// Revision 1.1  1999/12/05 01:50:24  sam
// Initial revision
//

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include "vf_opt.h"

static char* getnext(char* o)
{
	// skip ws
	while(isspace(*o))
		o++;


}

static char* optcurr;
static char* optnext;

int vf_strgetopt(char* opt, cons char* optstring)
{
	if(opt != optcurrent) {
		optcurr = opt;
		optnext = opt;

		// move optnext up to first non-white space
		while(*optnext && isspace(*optnext))
			optnext++;
	}

	if(*optnext == '\0')
		return -1;

	char* begin = optnext;

	// increment optnext

	optnext = getnext(begin);


}

