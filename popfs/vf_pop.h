//
// vf_pop.h
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
// $Log$
// Revision 1.9  1999/09/26 23:40:21  sam
// moved the popfile into it's own module
//
// Revision 1.8  1999/09/26 22:50:27  sam
// reorganized the templatization, checking in prior to last cleanup
//
// Revision 1.7  1999/09/23 01:39:22  sam
// first cut at templatization running ok
//
// Revision 1.6  1999/09/19 22:24:47  sam
// implemented threading with a work team
//
// Revision 1.5  1999/08/09 15:19:38  sam
// Multi-threaded downloads! Pop servers usually lock the maildrop, so concurrent
// downloads are not possible, but stats and reads of already downloaded mail
// do not block on an ongoing download!
//
// Revision 1.4  1999/08/09 09:27:43  sam
// started to multi-thread, but fw didn't allow blocking, checking in to fix fw
//
// Revision 1.3  1999/07/21 16:00:17  sam
// the symlink extension wasn't working, commented it out
//
// Revision 1.2  1999/07/19 15:23:20  sam
// can make aribtrary info show up in the link target, now just need to get
// that info...
//
// Revision 1.1  1999/06/20 17:21:53  sam
// Initial revision
//

#ifndef VF_POP_H
#define VF_POP_H

#include <pop3.h>

#include <vf_mgr.h>
#include <vf_dir.h>
#include <vf_log.h>
#include <vf_wt.h>

#include "url.h"

class VFPopFile;

//
// Request, Info, and Task used by the WorkTeam framework.
//

struct PopRequest
{
	int     msgid;
};

struct PopInfo
{
	VFPopFile*  file;
};

struct PopTask
{
public:
	PopTask(const String& host, const String& user, String& pass) :
		host(host), user(user), pass(pass)
	{
	}
	int operator () (const PopRequest& request, VFDataIfx& dh);

private:
	const String host;
	const String user;
	const String pass;
};

//
// VFPop: implements the complete interface for the WorkTeam callbacks,
//	as well as being the manager.
//

class VFPop : public VFManager, public VFCompleteIfx<PopInfo>
{
public:
	VFPop(const char* host, const char* user, const char* pass, int mbuf);

	void Run(const char* mount, int verbosity);

	int Service(pid_t pid, VFMsg* msg);

	int Retr(int msg, VFPopFile* file);

	void Complete(int status, const PopInfo& info, iostream* data);

private:
	VFDirEntity root_;

	String	host_;
	String	user_;
	String	pass_;
	int		inmem_;

	VFWorkTeam<PopRequest, PopInfo, PopTask>* team_;

	iostream* Stream() const;

	int Connect(pop3& pop);
	int Disconnect(pop3& pop);

	static void SigHandler(int signo);
};


#endif

