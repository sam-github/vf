//
// url.h: routines to parse URLs
//
// $Id$
// $Log$
// Revision 1.2  1999/06/20 17:21:53  sam
// added multiple inclusion protection
//
// Revision 1.1  1999/05/17 04:37:40  sam
// Initial revision
//

#ifndef URL_H
#define URL_H

int UrlParseAccount(char* account, char** user, char** pass, char** host)
{
	*user = account;
	*pass = 0;
	*host = 0;

	char* p = strpbrk(*user, ":@");
	if(!p) { return 0; }

	if(*p == ':') {
		*p = '\0';
		*pass = p + 1;
		p = strpbrk(*pass, "@");
		if(!p) { return 0; }
	}

	if(*p == '@') {
		*p = '\0';
		*host = p + 1;
	} else {
		return 0;
	}

	// user, pass (if supplied), and host must all have a non-zero length
	if(!strlen(*user) || (*pass && !strlen(*pass)) || !strlen(*host)) {
		return 0;
	}

	return 1;
}

#endif

