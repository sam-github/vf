//
// $Id$
//
// $Log$
// Revision 1.1  2000/01/13 02:28:03  sam
// Initial revision
//

#ifdef __USAGE
%C	 
#endif

#define _QNX_SOURCE
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/sched.h>
#include <sys/vc.h>
#include <sys/qnx_glob.h>
#include <unix.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define VF_PATH_MALLOC

#include "vf_path.h"

/* global variables */
int nops = 0;
int	tsec = 10;

void sig_handler (int signo) {
	int	bytes = 0;
	int bps   = 0;

	switch(signo) {
	case SIGALRM:
		printf ("%s: %d ops / %d secs = %d ops / second\n",
			"patht", nops, tsec, nops/tsec);
		break;
	default:
		assert(0);
	}
	fflush(stdout);
	exit (0);
}


void main ()
{
	/* sig handling */
	signal(SIGALRM, sig_handler);
	
	alarm (tsec);
	while (1) {
		Path path1	= "hello";	nops++;
		Path path2	= path1;	nops++;
		Path path3	= path2;	nops++;
		Path path4	= path3;	nops++;
		Path path5	= path4;	nops++;

		Path path6	= path5;	nops++;
		Path path7	= path6;	nops++;
		Path path8	= path7;	nops++;
		Path path9	= path8;	nops++;
		Path path10	= path9;	nops++;
	}
}


