//
// vf_log.cc
//
// Copyright (c) 1998, Sam Roberts
// 
// $Log$
// Revision 1.1  1998/03/15 22:10:32  sroberts
// Initial revision
//

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "vf_log.h"

static int vf_level = 0;
static char* vf_tag = 0;

void VFLog(int level, const char* format, ...)
{
	if(level <= vf_level)
	{
		va_list args;
		va_start(args, format);

		char buffer[BUFSIZ];
		int index = 0;
		index += sprintf(buffer, "%s: ", vf_tag);

		index += vsprintf(&buffer[index], format, args);
		index += sprintf(&buffer[index], "\n");

		va_end(args);

		fprintf(stdout, "%s", buffer);
	}
}

void VFLevel(const char* tag, int level)
{
	assert(tag);
	assert(level >= 0);

	if(vf_tag) { free(vf_tag); vf_tag = 0; }

	vf_tag = strdup(tag);
	vf_level = level;

	assert(vf_tag);
}

