//
// untar.cc: a tar unarchiver, decompressor, and test driver.
//
// Copyright (C) 1999 Sam Roberts
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
// $Id$
// $Log$
// Revision 1.1  1999/11/24 03:43:12  sam
// Initial revision
//

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <strstream.h>

#include "tararch.h"

#ifdef __USAGE
usage: untar [-qvh] [-g|-z|-f|-F filter] [-l|-x|-s] archive [member]
  -q   set verbosity to 0
  -v   increment verbosity
  -h   print this helpful message

Filters (default is '-g'):
  -g   guess a filter type using the same techniques as the "file"
          command (default is "cat" if the file type is unidentifiable)
  -t   don't filter 'archive', open as a tar file
  -c   filter 'archive' through cat (a kindof null op)
  -z   filter 'archive' through zcat (gunzip it)
  -f   filter 'archive' through fcat (melt it)
  -F   filter 'archive' through 'filter'

Actions (default is '-l'):
  -l   list name and type in a structured format (the default)
  -x   extract file data to stdout
  -s   dump stat information (intended for debugging)
 (applied to 'member' if specified, defaults to all members):
This utility is used primarily to test the tar file reader, though may
be useful for other purposes.
#endif

// options
int		vOpt = 1;
char	zOpt = 'g';
char*	fOpt = 0;
char	cOpt = 'l';
char*	aOpt = 0;
char*	mOpt = 0;

void ERR(const char* format, ...)
{
	if(vOpt > 0)
	{
		va_list args;
		va_start(args, format);
		vfprintf(stdout, format, args);
		va_end(args);
	}

	exit(1);
}

void DBG(const char* format, ...)
{
	if(vOpt > 1)
	{
		va_list args;
		va_start(args, format);
		vfprintf(stdout, format, args);
		va_end(args);
	}
}

int main(int argc, char* argv[])
{
	for(int opt; (opt = getopt(argc, argv, "qvhgtczfF:lxs")) != -1; )
	{
		switch(opt)
		{
			case 'q':
				vOpt = 0;
				break;

			case 'v':
				vOpt++;
				break;

			case 'h':
				print_usage(argv);
				break;

			case 'F':
				fOpt = optarg;
				// fall-through
			case 'g':
			case 't':
			case 'c':
			case 'z':
			case 'f':
				zOpt = opt;
				break;

			case 'l':
			case 'x':
			case 's':
				cOpt = opt;
				break;

			default:
				exit(1);
		}
	}

	if((aOpt = argv[optind++]) == 0) {
		ERR("no archive specified\n");
	}

	mOpt = argv[optind++];

	Tar::Reader tar(aOpt, mOpt, Tar::Reader::Method(zOpt), fOpt);
	if(!tar.Open(mOpt)) {
		ERR("tar.Open() - %s failed: [%d] %s\n",
			tar.ErrorInfo(), tar.ErrorNo(), tar.ErrorStr());
	}

	do
	{
		DBG("next: '%s'\n", tar.Path());

		switch(cOpt)
		{
/*
		case 's': {
			const struct stat* stat = it.Stat();
			printf("untar: it path %s size %d user %s group %s link %s\n",
				it.Path(), stat->st_size, it.User(), it.Group(), it.Link());
			printf("untar: it mode %#x perm %#o uid %d gid %d\n",
				stat->st_mode, 0777&stat->st_mode, stat->st_uid, stat->st_gid);
		  }	break;
*/
		case 'l': {
			struct stat stat;
			tar.Record()->Stat(&stat);

		 	int type;
		 	if(S_ISREG(stat.st_mode)) {
		 		type = 'f';
		 	} else if(S_ISDIR(stat.st_mode)) {
		 		type = 'd';
		 	} else if(S_ISLNK(stat.st_mode)) {
		 		type = 'l';
		 	} else {
		 		DBG("untar: type is %#x\n", stat.st_mode);
		 		continue;
		 	}
		 	printf("%c: %s\n", type, tar.Path());
		 	if(type == 'l') {
		 		printf("-> %s\n", tar.LinkTo());
		 	}
		  }	break;

		case 'x': {
			Tar::Member* mp = tar.Member();

			char b[BUFSIZ];
			int e = 0;
			do {
				if(fwrite(b, sizeof(char), e, stdout) != e) {
					ERR("fwrite failed: [%d] %s\n", errno, strerror(errno));
				}
				e = mp->Read(b, sizeof(b));
			} while(e > 0);

			if(e == -1) {
				ERR("read - %s failed: [%d] %s\n",
					mp->ErrorInfo(), mp->ErrorNo(), mp->ErrorStr());
			}
		  }	break;

		}
	}
	while(tar.Next());

	return 0;
}

