//
// vf_log.cc
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
// Revision 1.3  1999/09/27 02:53:15  sam
// put the process id in the log message format, and made some abortive
// attempts to make setting up verbosity and string a little easier.
//
// Revision 1.2  1999/04/28 03:27:49  sam
// Stamped sources with the GPL.
//
// Revision 1.1  1998/03/15 22:10:32  sroberts
// Initial revision
//

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "vf_log.h"

static int vf_level = 0;
static char* vf_tag = "vf";

void VFLog(int level, const char* format, ...)
{
	if(level <= vf_level)
	{
		va_list args;
		va_start(args, format);

		char buffer[BUFSIZ];
		int index = 0;
		index += sprintf(buffer, "%s[%d]: ", vf_tag, getpid());

		index += vsprintf(&buffer[index], format, args);
		index += sprintf(&buffer[index], "\n");

		va_end(args);

		fprintf(stdout, "%s", buffer);
	}
}

void VFLevel(const char* tag, int level)
{
	if(tag)			// change tag
	{
		if(vf_tag) { free(vf_tag); vf_tag = 0; }

		vf_tag = strdup(tag);

		assert(vf_tag);
	}

	if(level >= 0)	// change level
	{
		vf_level = level;
	}
}

const char* VFGetTag() { return vf_tag; }
int VFGetLevel() { return vf_level; }

