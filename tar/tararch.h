//
// tararch.h: 
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

#ifndef TARARCH_H
#define TARARCH_H

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdiobuf.h>
#include <stdlib.h>
#include <string.h>
#include <strstream.h>
#include <tar.h>
#include <unistd.h>

#include <String.h>

#include <sys/stat.h>

struct Tar
{
	/**
	* A strdup() that uses new[].
	*/
	static inline char* StrDup(const char* s)
	{
		if(!s) { return 0; }

		char* r = new char[strlen(s) + 1];

		if(!r) { return 0; }

		return strcpy(r, s);
	}

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

	// The checksum field is filled with this while the checksum is computed.
	#define	CHKBLANKS	"        "	// 8 blanks, no null

	/* The magic field is filled with this if uname and gname are valid. */
	//#define TMAGIC		"ustar  "	// 7 chars and a null
	#define TMAGIC		"ustar"	
		// QNX doesn't have the spaces... but GNU does

	union Record {
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
		} header;

		int Int(const char* str) const {	// assume octal, hope this is right!
			return strtol(str, 0, 8);
		} 
		int Stat(struct stat* stat) const
		{
			if(!stat) { return 0; }

			memset(stat, 0, sizeof(*stat));
			stat->st_ino	= 0;
			stat->st_dev	= 0;
			stat->st_size	= Int(header.size);
			stat->st_rdev	= 0;
			stat->st_uid	= Int(header.uid);
			stat->st_gid	= Int(header.gid);
			stat->st_ftime	= 0;
			stat->st_mtime	= Int(header.mtime);
			stat->st_atime	= Int(header.atime);
			stat->st_ctime	= Int(header.ctime);
			stat->st_mode	= 0777 & (mode_t) Int(header.mode);
			stat->st_nlink	= 0;
			stat->st_status	= 0;

			switch(header.linkflag) {
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

	class Member;

	class Archive
	{
	private:
		streambuf* sb_;

		Tar::Record	rec_;
		struct stat	stat_;

		int		ispipe_;
		int     valid_;
		off_t	seek_;

		// location of file record/data in archive
		int     offset_;// blocks - offset to header record
		int     span_;	// blocks - blocks for header and data
		int     size_;	// chars - amount of file data

		int		errno_;
		const char*	errinfo_;

		int Seek(int to)
		{
			if(to == seek_) { return 1; }

			if(!ispipe_) {
				if(sb_->seekoff(to, ios::beg, ios::in) == EOF) {
					Err("seekoff", errno);
					return 0;
				}
				seek_ = to;
			} else {
				if(seek_ > to)
				{
					Err("Seek", ESPIPE);
					return 0;
				}
				while(seek_ != to)
				{
					errno = EOK;
					if(sb_->sbumpc() == EOF)
					{
						Err("sbumpc", errno);
						return 0;
					}
					seek_ ++;
				}
			}
			return 1;
		}

	public:
		Archive() :
			sb_			(0),
			ispipe_		(0),
			valid_		(0),
			seek_		(0),
			offset_		(0),
			span_		(0),
			size_		(0),
			errno_		(EOK),
			errinfo_	(0)
		{
		}
		const streambuf* Stream() const
		{
			return sb_;
		}
		int Offset()
		{
			return offset_;
		}

		int Open(streambuf* sb)
		{
			sb_ = sb;

			// detect if fd is seekable
			if(sb_->seekoff(0, ios::cur, ios::in) == EOF) {
				ispipe_ = 1;
			}
			return 1;
		}
		int Close()
		{
			return 1;
		}
		int Read(off_t seek, void* data, size_t size)
		{
			if(Seek(seek) == 0) {
				return 0;
			}
			int ret = sb_->sgetn((char*)data, size);

			// doesn't seem to be any way to do error detection

			seek_ += ret;

			return ret;
		}

		int Next()
		{
			// Seek to the begining-of-file past the iterator's file position.
			off_t bof = offset_ + span_;

			if(Seek(bof*BLOCKSZ) == 0) {
					return 0;
			}
			// Search for a valid header record.
			assert(sizeof(rec_) == BLOCKSZ);
			do {
				int err = Read(seek_, &rec_, sizeof(rec_));
				// Check for error and EOF.
				if(err < sizeof(rec_)) { valid_ = 0; return 0; }

				bof++; // Read a block

				// Check for valid header.
				if(strncmp(TMAGIC, rec_.header.magic, strlen(TMAGIC)) == 0) {
					valid_	= 1;
					offset_	= bof - 1;
					span_	= 1;
					break;
				};
			} while(1);

			// Extract info from header record.
			rec_.Stat(&stat_);

			// Calculate span in blocks based on size (rounding size up).
			int size	= stat_.st_size;
			span_	= 1 /*header block*/ + size / BLOCKSZ;
			if(size % BLOCKSZ) { span_ += 1; }

			return 1;
		}
		const Tar::Record* Record() const
		{
			return &rec_;
		}
		const char* Path()
		{
			if(!valid_) {
				Err("Path", EINVAL);
				return 0;
			}
			return rec_.header.name;
		}
		const char* LinkTo()
		{
			if(!valid_) {
				Err("Path", EINVAL);
				return 0;
			}
			if(!S_ISLNK(stat_.st_mode)) {
				Err("Path", EINVAL);
				return 0;
			}
			return rec_.header.linkname;
		}
		/**
		* Returns a handle to the current member, the caller is
		* responsible for deallocating the handle when complete.
		*/
		Tar::Member* Member(Tar::Member* from = 0)
		{
			from = from;

			if(!valid_) {
				Err("not currently positioned", EINVAL);
				return 0;
			}
			if(from) {
				*from = *this;
			} else {
				from = new Tar::Member(this);
				if(!from) {
					Err("new", errno);
					return 0;
				}
			}
			return from;
		}
		int	ErrorNo() const
		{
			return errno_;
		}
		const char*	ErrorStr() const
		{
			return strerror(errno_);
		}
		const char*	ErrorInfo() const
		{
			return errinfo_;
		}
	protected:
		streambuf* Stream()
		{
			return sb_;
		}
		void Err(const char* info, int no) const
		{
			Archive* a = (Archive*) this;

			a->errinfo_	= info;
			a->errno_	= no;
		}
	};

	class Member
	{
	private:
		Tar::Archive*	tar_;
		Tar::Record		rec_;
		int				offset_;

		off_t			seek_;
		off_t			size_;

		int				err_no;
		const char*		err_info;

		void Err()
		{
			err_no		= tar_->ErrorNo();
			err_info	= tar_->ErrorInfo();
		}
		void Err(const char* info, int no)
		{
			err_no		= no;
			err_info	= info;
		}
		Member(Tar::Archive* tar) :
				tar_	(0),
				offset_	(0),
				seek_	(0),
				size_	(0),
				err_no	(EOK),
				err_info("")
		{
			Ctor(tar);
		}
		Member& operator=(Tar::Archive& tar)
		{
			Ctor(&tar);

			return *this;
		}
		void Ctor(Tar::Archive* tar)
		{
			tar_	= tar;
			rec_	= *tar->Record();
			offset_	= tar->Offset();

			struct stat stat;
			rec_.Stat(&stat);
			size_ = stat.st_size;
		}
		friend class /*Tar::*/ Archive;

	public:
		int	ErrorNo() const
		{
			return err_no;
		}
		const char*	ErrorStr() const
		{
			return strerror(err_no);
		}
		const char*	ErrorInfo() const
		{
			return err_info;
		}
		int Seek(off_t seek);

		int Read(void* buf, size_t nbytes)
		{
			if(seek_ > size_) {
				nbytes = 0;
			} else if(nbytes + seek_ > size_) {
				nbytes = size_ - seek_;
			}
			off_t off = (offset_ + 1 /*header*/) * BLOCKSZ + seek_;

			int ret = tar_->Read(off, buf, nbytes);
			if(ret == -1) {
				return 0;
			} else {
				seek_ += ret;
			}
			return ret;
		}
//		const struct stat* Stat() const;

		int Stat(struct stat* stat) const;

		int Record(Tar::Record* record) const;

		// No dynamic memory, so use the compiler generated defaults.
		// op =(const Member&);
		// Member(const Member&);
		// ~Member();
	};

	class Reader : public Tar::Archive
	{
	public:
		enum Method
		{
			GUESS	= 'g',	// use magic bytes to guess method
			CAT		= 'c',	// use cat as a filter
			FREEZE	= 'f',	// use fcat as a filter
			GZIP	= 'z',	// use zcat as a filter
			FILTER	= 'F',	// use a custom filter cmd
			TAR		= 't',	// don't filter, treat as a tar file
			UNKNOWN	= '?'	// Only used as an error return for GuessType()
		};

	private:
		char*	archive_;
		char*	file_;
		Method	method_;
		char*	filter_;

		// Used if archive opened through a pipe.
		FILE*		pipe_;
		stdiobuf*	stdiobuf_;

		// Used if archive opened as a file.
		filebuf*	filebuf_;

		streambuf* PipeSb(const char* filter)
		{
			ostrstream ss;

			ss << filter << " " << archive_ << '\0';

			char* cmd = ss.str();

			if(cmd && (pipe_ = popen(cmd, "r"))) {
				stdiobuf_ = new stdiobuf(pipe_);
				if(!stdiobuf_) {
					Err("new stdiobuf", errno);
				}
			} else {
				Err("popen", errno);
			}
			delete[] cmd;

			return stdiobuf_;
		}
		streambuf* FileSb()
		{
			if((filebuf_ = new filebuf)) {
				if(!filebuf_->open(archive_, ios::in|ios::nocreate)){
					Err("filebuf::open", errno);
					delete filebuf_;
					filebuf_ = 0;
				}
			} else {
				Err("new filebuf", errno);
			}
			return filebuf_;
		}

	public:
		/**
		* @param filter Optional and unused unless 'method' is FILTER,
		*			in which case it specifies the filter command.
		*/
		Reader(
				const char* archive,
				Method method = GUESS,
				const char* filter = 0)
		{
			// redirect to full ctor
			new(this) Reader(archive, 0, method, filter);
		}
		Reader(
				const char* archive,
				const char* file,
				Method method = GUESS,
				const char* filter = 0) :
			archive_	(Tar::StrDup(archive)),
			file_		(Tar::StrDup(file)),
			method_		(method),
			filter_		(Tar::StrDup(filter)),
			pipe_		(0),
			stdiobuf_	(0),
			filebuf_	(0)
		{
		}
		~Reader()
		{
			Close();

			delete[] archive_;
			delete[] file_;
			delete[] filter_;
		}
		Method GuessType(const char* file)
		{
			int fd = open(file, O_RDONLY);
			if(fd == -1) {
				Err("open", errno);
				return UNKNOWN;
			}
			char magic[2] = { 0, 0 };
			errno = EINVAL;
			if(read(fd, magic, sizeof(magic)) != 2) {
				Err("read()", errno);
				return UNKNOWN;
			}
			close(fd);
			char gzip[]		= "\037\213";
			char freeze2[]	= "\037\237";
			char freeze1[]	= "\037\236";

			if(memcmp(magic, gzip, sizeof(magic)) == 0) {
				return GZIP;
			} else if(memcmp(magic, freeze2, sizeof(magic)) == 0) {
				return FREEZE;
			} else if(memcmp(magic, freeze1, sizeof(magic)) == 0) {
				return FREEZE;
			}
			return UNKNOWN;
		}
		int Open(const char* file = 0)
		{
			if(Stream()) {
				Err("already open", EINVAL);
				return 0;
			}
			if(method_ == GUESS)
			{
				method_ = GuessType(archive_);
			}
			streambuf* sb = 0;

			switch(method_)
			{
				case FREEZE:	sb = PipeSb("fcat"); break;
				case GZIP:		sb = PipeSb("zcat"); break;
				case FILTER:	sb = PipeSb(filter_); break;
				case CAT:		sb = PipeSb("cat"); break;
				case UNKNOWN:
				case TAR:		sb = FileSb(); break;
				default:
					Err("GuessType", EINVAL);
			}
			if(!sb) { return 0; }

			if(!Archive::Open(sb)) { return 0; }

			// need to advance to our member, unless member is null
			while(Next()) {
				if(!file || Path() == file) { return 1; }
			}
			return 0;
		}
		int Close()
		{
			if(stdiobuf_) {
				while(getc(pipe_) != EOF)
					/* clear any remaining input */;

				if(pclose(pipe_) == -1) {
					Err("pclose", errno);
					return 0;
				}
				pipe_ = 0;
				delete stdiobuf_;
				stdiobuf_ = 0;
			}
			if(filebuf_) {
				delete filebuf_;
				filebuf_ = 0;
			}
			return 1;
		}
		int Next()
		{
			if(!file_) {
				return Archive::Next();
			}
			Err("Next", EINVAL);
			return 0;
		}
	};
};

#endif

