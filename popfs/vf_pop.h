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
#include <vf_file.h>
#include <vf_log.h>

#include "url.h"

class PopDir;

class PopFile : public VFFileEntity
{
public:
	PopFile(int msg, PopDir& dir, int size);
	~PopFile();

	int ReadLink(const String& path, _fsys_readlink* req, _fsys_readlink_reply* reply);

	int Write(pid_t pid, size_t nbytes, off_t offset);
	int Read(pid_t pid, size_t nbytes, off_t offset);

private:
	PopDir&		dir_;
	int			msg_;
	istream*	data_;
	int			size_;

	char*		description_;
};

class PopDir : public VFDirEntity
{
public:
	PopDir(const char* host, const char* user, const char* pass, int mbuf);

	istream* Retr(int msg);

private:
	iostream* Stream() const;

	int Connect(pop3& pop);
	int Disconnect(pop3& pop);

	String	host_;
	String	user_;
	String	pass_;
	int		mbuf_;
};

#endif

