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
#include <vf_file.h>
#include <vf_log.h>
#include <vf_fifo.h>

#include "url.h"

class VFPop;

class VFPopFile : public VFFileEntity
{
public:
	VFPopFile(int msg, VFPop& pop, int size);
	~VFPopFile();

	// These aren't implemented yet, I'm going to try and have the files
	// look like links to lstat(), but not to cat and vi, so I can reply
	// to readlink() with a subject/from field...
	int Stat(pid_t pid, const String* path, int lstat); // XXX
	int ReadLink(pid_t pid, const String& path); // XXX

	int Read(pid_t pid, size_t nbytes, off_t* offset);

	void Data(istream* data);

private:
	int QueueRead(pid_t pid, int nbytes, off_t* offset);
	int ReplyRead(pid_t pid, int nbytes, off_t* offset);

	VFPop&		pop_;
	int			msg_;
	istream*	data_;
	int			size_;

	char*		description_;

	struct ReadRequest : public VFFifo<ReadRequest>::Link
	{
		ReadRequest(pid_t pid, int nbytes, off_t* offset) :
			pid(pid), nbytes(nbytes), offset(offset) {}

		pid_t	pid;
		int		nbytes;
		off_t*	offset;
	};

	VFFifo<ReadRequest>	readq_;
};

class VFPop : public VFManager
{
public:
	VFPop(const char* host, const char* user, const char* pass, int mbuf, int sync);

	void Run(const char* mount, int verbosity);

	int Service(pid_t pid, VFMsg* msg);

	int Retr(int msg, VFPopFile* file, istream** str);

private:
	VFDirEntity root_;

	String	host_;
	String	user_;
	String	pass_;
	int		mbuf_;
	int		sync_;
	pid_t	child_;

	static pid_t sigproxy_;

	struct RetrRequest : public VFFifo<RetrRequest>::Link
	{
		RetrRequest(int msg, VFPopFile* file, iostream* mail) :
			msg(msg), file(file), mail(mail) {}

		int			msg;
		VFPopFile*	file;
		iostream*	mail;
	};

	VFFifo<RetrRequest>	retrq_;

	int SyncRetr	(int msg, istream** str);
	int AsyncRetr	();
	int	GetMail		(int msg, ostream* mail);

	iostream* Stream() const;

	int Connect(pop3& pop);
	int Disconnect(pop3& pop);

	static void SigHandler(int signo);


};

#endif

