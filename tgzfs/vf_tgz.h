//
// vf_tgz.h
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
// Revision 1.1  1999/11/24 03:54:01  sam
// Initial revision
//

#ifndef VF_TGZ_H
#define VF_TGZ_H

#include <vf_mgr.h>
#include <vf_dir.h>
#include <vf_log.h>
#include <vf_wt.h>

class VFTgzFile;

//
// Request, Info, and Task used by the WorkTeam framework.
//

struct TgzRequest
{
	char	path[PATH_MAX + 1];
};

struct TgzInfo
{
	VFTgzFile*  file;
};

struct TgzTask
{
public:
	TgzTask(const char* cmd) :
		cmd(cmd)
	{
	}
	int operator () (const TgzRequest& request, VFDataIfx& dh);

private:
	const char* cmd;
};

//
// VFTgz: implements the complete interface for the WorkTeam callbacks,
//	as well as being the manager.
//

class VFTgz : public VFManager, public VFCompleteIfx<TgzInfo>
{
public:
	VFTgz(const char* host, const char* user, const char* pass, int mbuf);

	void Run(const char* mount, int verbosity);

	int Service(pid_t pid, VFMsg* msg);

	int Retr(int msg, VFTgzFile* file);

	void Complete(int status, const TgzInfo& info, iostream* data);

private:
	VFDirEntity root_;

	String	archive_;
	int		inmem_;

	VFWorkTeam<TgzRequest, TgzInfo, TgzTask>* team_;

	iostream* Stream() const;

	static void SigHandler(int signo);
};

#endif

