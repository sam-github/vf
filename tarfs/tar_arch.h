//
// tar_arch.h: 
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
// Revision 1.7  1999/10/28 04:08:46  sam
// fixed bug in resetting of seek_ for iterators, and now works on
// pipes if the archive is read from start to finish continuously
//
// Revision 1.6  1999/08/09 15:17:56  sam
// Ported framework modifications down.
//
// Revision 1.5  1999/07/19 15:18:48  sam
// now reads GNU archives
//
// Revision 1.4  1999/04/28 03:27:28  sam
// Stamped sources with the GPL.
//
// Revision 1.3  1999/04/24 04:41:46  sam
// added support for symbolic links, and assorted bugfixes
//
// Revision 1.2  1999/04/11 06:44:23  sam
// fixed various small bugs that turned up during implementation of a tar fsys
//
// Revision 1.1  1999/04/11 01:56:54  sam
// Initial revision
//

#ifndef TAR_ARCH_H
#define TAR_ARCH_H

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <tar.h>
#include <unistd.h>

/*
 * Header block on tape.
 *
 * I'm going to use traditional DP naming conventions here.
 * A "block" is a big chunk of stuff that we do I/O on.
 * A "record" is a piece of info that we care about.
 * Typically many "record"s fit into a "block".
 */
#define	BLOCKSZ		512
#define	RECORDSIZE	512
#define	NAMSIZ		100
#define	TUNMLEN		32
#define	TGNMLEN		32
#define SPARSE_EXT_HDR  21
#define SPARSE_IN_HDR	4

/* The checksum field is filled with this while the checksum is computed. */
#define	CHKBLANKS	"        "	/* 8 blanks, no null */

/* The magic field is filled with this if uname and gname are valid. */
//#define TMAGIC		"ustar  "	/* 7 chars and a null */
#define TMAGIC		"ustar"		/* QNX doesn't have the spaces... but GNU does */

struct sparse {
	char offset[12];
	char numbytes[12];
};

struct sp_array {
	int offset;
	int numbytes;
};

union TarRecord {
	char		charptr[RECORDSIZE];
	struct /*header*/ {
		char	name[NAMSIZ];
		char	mode[8];	/* octal */
		char	uid[8];		/* octal */
		char	gid[8];		/* octal */
		char	size[12];
		char	mtime[12];
		char	chksum[8];
		char	linkflag;
		char	linkname[NAMSIZ];
		char	magic[8];
		char	uname[TUNMLEN];
		char	gname[TGNMLEN];
		char	devmajor[8];
		char	devminor[8];
		/* these following fields were added by JF for gnu */
		/* and are NOT standard */
		char	atime[12];
		char	ctime[12];
		char	offset[12];
		char	longnames[4];
#ifdef NEEDPAD
		char    pad;
#endif
		struct	sparse sp[SPARSE_IN_HDR];
		char	isextended;
		char	realsize[12];		/* 1 size of the sparse file */
/*		char	ending_blanks[12];*/	/* number of nulls at the
						   end of the file, if any */
	} /*header*/;
	struct extended_header { // the position of this is probably wrong
		struct sparse sp[21];
		char isextended;
	} ext_hdr;

	int Int(const char* str) const {	// assume octal, hope this is right!
		return strtol(str, 0, 8);
	} 

	int Stat(struct stat* stat) const
	{
		if(!stat) { return 0; }

		memset(stat, 0, sizeof(*stat));
		stat->st_ino	= 0;
		stat->st_dev	= 0;
		stat->st_size	= Int(size);
		stat->st_rdev	= 0;
		stat->st_uid	= Int(uid);
		stat->st_gid	= Int(gid);
		stat->st_ftime	= 0;
		stat->st_mtime	= Int(mtime);
		stat->st_atime	= Int(atime);
		stat->st_ctime	= Int(ctime);
		stat->st_mode	= 0777 & (mode_t) Int(mode);
		stat->st_nlink	= 0;
		stat->st_status	= 0;

		switch(linkflag) {
			case REGTYPE:	// new and old name for regular files
			case AREGTYPE:	stat->st_mode |= S_IFREG;	break;
			case LNKTYPE:	stat->st_mode |= S_IFREG;	break; // hard link...
			case SYMTYPE:	stat->st_mode |= S_IFLNK;	break;
			case CHRTYPE:	stat->st_mode |= S_IFCHR;	break;
			case BLKTYPE:	stat->st_mode |= S_IFBLK;	break;
			case DIRTYPE:	stat->st_mode |= S_IFDIR;	break;
			case FIFOTYPE:	stat->st_mode |= S_IFIFO;	break;
		}
		return 0;
	}
};

class TarArchive
{
public:
	class Iterator
	{
	private:
		static inline void Dup(char*& dst, const char* src);
	public:
		int ChUidGid(uid_t uid, gid_t gid = -1)
		{
			if(!valid_) { return 0; }

			if(uid != -1) { stat_.st_uid = uid; }
			if(gid != -1) { stat_.st_gid = gid; }
			return 1;
		}
		int Seek(off_t seek)
		{
			if(seek < 0) {
				errno_ = EINVAL;
				return -1;
			}
			seek_ = seek;
			return seek_;
		}
		int Read(void* buf, size_t nbytes)
		{
			if(seek_ > size_) {
				nbytes = 0;
			} else if(nbytes + seek_ > size_) {
				nbytes = size_ - seek_;
			}
			off_t off = (offset_ + 1 /*header*/) * BLOCKSZ + seek_;

			int ret = dir_->Read(off, buf, nbytes);
			if(ret == -1) {
				errno_ = dir_->ErrorNo();
			} else {
				seek_ += ret;
			}
			return ret;
		}
		const struct stat* Stat() const
		{
			if(!valid_) { return 0; }

			return &stat_;
		}
		int Stat(struct stat* stat) const
		{
			if(!valid_) { return 0; }

			if(stat) {
				*stat = stat_;
			}
			return 1;
		}
		int Record(TarRecord* r)
		{
			int ret = dir_->Read(BLOCKSZ*offset_, r, sizeof(*r));
			if(ret == -1) { errno_ = dir_->ErrorNo(); }
			return ret;
		}

		Iterator(const Iterator& it)
		{
			Clear();
			Copy(it);
		}
		~Iterator()
		{
			Debug("\tit::~it()\n");
			Delete();
		}
		Iterator& operator = (const Iterator& it)
		{
			Debug("\tit::op = ()\n");
			Delete();
			Clear();
			Copy(it);
			return *this;
		}
		Iterator& operator ++()
		{
			Debug("\tit::op ++ () [pre]\n");
			if(dir_) { dir_->Iterate(this); }
			return *this;
		}
		const Iterator operator ++(int)
		{
			Debug("\tit::op ++ (int) [post]\n");
			Iterator last = *this;
			++*this;
			return last;
		}
		operator int () const
		{
			Debug("\tit::op int () %d\n", Valid());
			return Valid();
		}
		int Valid() const
		{
			return valid_;
		}
		const char* Path() const
		{
			return path_;
		}
		const char* Group() const
		{
			return group_;
		}
		const char* User() const
		{
			return user_;
		}
		const char* Link() const
		{
			return link_;
		}
		FILE* DebugFile(FILE* debug)
		{
			FILE* last = debug_;
			debug_ = debug;
			return last;
		}
		int ErrorNo() const
		{
			return errno_;
		}
		const char* ErrorString() const
		{
			return strerror(errno_);
		}
		void Debug(const char* format, ...) const
		{
			if(debug_) {
				va_list al;
				va_start(al, format);
				vfprintf(debug_, format, al);
				va_end(al);
			}
		}

	private:
		friend class TarArchive;
		
		Iterator(); // explicitly undefined/uncallable

		Iterator(TarArchive* dir)
		{
			Clear();
			Debug("\tit::it(dir)\n");
			dir_ = dir;
			++(*this);
		}
		void Clear(int valid = 0) // call ONLY on uninitialized memory
		{
			dir_	= 0;
			valid_  = valid;
			offset_	= 0;
			span_	= 0;
			size_	= 0;
			path_	= 0;
			user_	= 0;
			group_	= 0;
			link_	= 0;
			memset(&stat_, 0, sizeof(stat_));
			seek_	= 0;
			errno_	= 0;
			debug_	= 0;
		}
		void Delete()
		{
			Debug("\tit::Delete() valid %d\n", valid_);
			delete [] path_;
			delete [] user_;
			delete [] group_;
			delete [] link_;
		}
		void Copy(const Iterator& it)
		{
			Debug("\tit::Copy()\n");
			dir_    = it.dir_;
			valid_  = it.valid_;
			offset_ = it.offset_;
			span_   = it.span_;
			size_   = it.size_;
			Dup(path_, it.path_);
			Dup(user_, it.user_);
			Dup(group_, it.group_);
			Dup(link_, it.link_);
			stat_	= it.stat_;
			seek_	= it.seek_;
			errno_	= it.errno_;
			debug_	= it.debug_;
		}

		// tar dir data
		TarArchive* dir_;
		int		valid_;
		
		// location of file record/data in archive
		int		offset_; // blocks - offset to header record
		int		span_;   // blocks - blocks for header and data
		int		size_;   // chars - amount of file data
		
		// tar file path name
		char*	path_;
		char*	user_;
		char*	group_;
		char*	link_;
		struct stat stat_;

		int		seek_;	// current read position, relative to start of data

		// error handling
		int		errno_;

		// debugging
		FILE*	debug_;
	};


	static inline void Dup(char*& dst, const char* src)
	{
		if(dst) { delete [] dst; dst = 0; }
		if(src) {
			dst = new char[strlen(src) + 1];
			if(dst) { strcpy(dst, src); }
		}
	}

	typedef Iterator iterator;
	friend class Iterator;

	TarArchive() : fd_(-1), offset_(0), ispipe_(0), errno_(EOK) {}

	int Open(int fd)
	{
		fd_ = dup(fd);
		if(fd_ == -1)
		{
			errno_ = errno;
			return 0;
		}
		// detect if fd is seekable
		if(lseek(fd_, 0, SEEK_CUR) == -1 && errno == ESPIPE) {
			ispipe_ = 1;
		}
		return 1;
	}
	int Open(const char* file)
	{
		if(!file) {
			errno_ = EINVAL;
			return 0;
		}
		
		fd_ = open(file, O_RDONLY);
		if(fd_ == -1)
		{
			errno_ = errno;
			return 0;
		}
		return 1;
	}
	Iterator begin()
	{
		return Iterator(this);
	}

	int ErrorNo() const { return errno_; }

	const char* ErrorString() const { return strerror(errno_); }

private:
	TarArchive(const TarArchive&);

	int		fd_;
	off_t	offset_;
	int		ispipe_;
	int		errno_;

	int Seek(int to)
	{
		if(to == offset_)
			return 0;

		if(!ispipe_)
		{
			if(lseek(fd_, to, SEEK_SET) == -1) {
				errno_ = errno;
				return -1;
			}
			offset_ = to;
		}
		else
		{
			if(offset_ > to)
			{
				errno_ = ESPIPE;
				return -1;
			}
			while(offset_ != to)
			{
				char scratch[BUFSIZ];
				int sz = to - offset_;
				if(sz > sizeof(scratch)) { sz = sizeof(scratch); }
				sz = read(fd_, scratch, sz);
				if(sz == -1)
				{
					errno_ = errno;
					return -1;
				}
				offset_ += sz;
			}
		}
		return 0;
	}

	int Read(off_t offset, void* data, size_t size)
	{
//		it->Debug("\tdir::Read() offset %u size %u\n", offset, size);

		if(Seek(offset) == -1) {
			errno_ = errno;
			return -1;
		}

		int ret = read(fd_, data, size);
		if(ret == -1) {
			errno_ = errno;
			return -1;
		}

		offset_ += ret;

		return ret;
	}

	void Iterate(Iterator* it)
	{
		it->Debug("\tdir::Iterate() valid %d offset %u span %u size %u\n",
			it->valid_, it->offset_, it->span_, it->size_);

		// Seek to the begining-of-file past the iterator's file position.
		assert((it->Valid() && it->span_ > 0) || (!it->Valid() && !it->span_));

		off_t bof = it->offset_ + it->span_;

		if(Seek(bof*BLOCKSZ) == -1) {
				errno_ = errno;
				it->valid_ = 0;
				return;
		}
		// Search for a valid header record.
		TarRecord rec;
		assert(sizeof(rec) == BLOCKSZ);
		do {
			int err = Read(offset_, &rec, sizeof(rec));
			// Check for error and EOF.
			if(err == -1) {
				errno_ = errno;
				it->valid_ = 0;
				return;
			}
			if(err < sizeof(rec)) { it->valid_ = 0; return; }

			bof++; // Read a block

			// Check for valid header.
			if(strncmp(TMAGIC, rec.magic, strlen(TMAGIC)) == 0) {
				it->valid_	= 1;
				it->offset_	= bof - 1;
				it->span_	= 1;
				it->seek_	= 0;
				break;
			};
		} while(1);

		it->Debug("\trec: name %s mode %s uid %s gid %s size %d\n",
			rec.name, rec.mode, rec.uid, rec.gid, strtol(rec.size, 0, 0));

		// Extract info from header record.
		rec.Stat(&it->stat_);

		it->size_ = it->stat_.st_size;

		Dup(it->path_, rec.name);
		Dup(it->user_, rec.uname);
		Dup(it->group_, rec.gname);
		Dup(it->link_, S_ISLNK(it->stat_.st_mode) ? rec.linkname : 0);

		// Calculate span in blocks based on size (rounding size up).
		int size	= it->size_;
		it->span_	= 1 /*header block*/ + size / BLOCKSZ;
		if(size % BLOCKSZ) { it->span_ += 1; }

		it->Debug("\tfound: valid %d offset %u span %u\n",
			it->valid_, it->offset_, it->span_);
	}
};

		inline void TarArchive::Iterator::Dup(char*& dst, const char* src)
		{
			TarArchive::Dup(dst, src);
		}

#endif

