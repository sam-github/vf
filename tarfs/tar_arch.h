/*
 * cctar.h: 
 *
 * $Id$
 * $Log$
 * Revision 1.1  1999/04/11 01:56:54  sam
 * Initial revision
 *
 */

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <tar.h>

/*
 * Header block on tape.
 *
 * I'm going to use traditional DP naming conventions here.
 * A "block" is a big chunk of stuff that we do I/O on.
 * A "record" is a piece of info that we care about.
 * Typically many "record"s fit into a "block".
 */
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
#define TMAGIC		"ustar"		/* QNX doesn't have the spaces... */

/* The linkflag defines the type of file */
#define	LF_OLDNORMAL	'\0'		/* Normal disk file, Unix compat */
#define	LF_NORMAL	'0'		/* Normal disk file */
#define	LF_LINK		'1'		/* Link to previously dumped file */
#define	LF_SYMLINK	'2'		/* Symbolic link */
#define	LF_CHR		'3'		/* Character special file */
#define	LF_BLK		'4'		/* Block special file */
#define	LF_DIR		'5'		/* Directory */
#define	LF_FIFO		'6'		/* FIFO special file */
#define	LF_CONTIG	'7'		/* Contiguous file */
/* Further link types may be defined later. */


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

	int Int(const char* str) { return strtol(str, 0, 0); }

	int Stat(struct stat* stat)
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
		stat->st_mode	= (mode_t) Int(mode);
		stat->st_nlink	= 0;
		stat->st_status	= 0;

		switch(linkflag) {
			case REGTYPE:	// new and old name for regular files
			case AREGTYPE:	stat->st_mode &= S_IFREG;	break;
			case LNKTYPE:	stat->st_mode &= S_IFREG;	break; // hard link...
			case SYMTYPE:	stat->st_mode &= S_IFLNK;	break;
			case CHRTYPE:	stat->st_mode &= S_IFCHR;	break;
			case BLKTYPE:	stat->st_mode &= S_IFBLK;	break;
			case DIRTYPE:	stat->st_mode &= S_IFDIR;	break;
			case FIFOTYPE:	stat->st_mode &= S_IFIFO;	break;
		}
		return 1;
	}
};

		char* StringDup(const char* in)
		{
			if(!in) { return 0; }
			char* out = new char[strlen(in) + 1];
			if(out) { strcpy(out, in); }
			return out;
		}

class TarArchive
{
public:
	class Iterator
	{
	public:
		int Seek(off_t seek)
		{
			if(seek < 0) {
				errno_ = EINVAL;
				return -1;
			}
			seek_ = seek;
			return seek_;
		}
		int Read(void* buf, size_t size)
		{
			buf = buf, size = size;
			return 0;
		}
		int Stat(struct stat* stat)
		{
			TarRecord record;
			if(Record(&record) == -1) {
				return 0;
			}
			return record.Stat(stat);
		}
		int Record(TarRecord* r)
		{
			if(!r) { return 0; }
			if(dir_->Read(BLOCKSZ*offset_, r, sizeof(*r)) != sizeof(*r)) {
				return 0;
			}
			return 1;
		}
/*
		Iterator()
		{
			Debug("\tit::it()\n");
			Clear();
		}
*/

		Iterator(const Iterator& it) : debug_(it.debug_)
		{
			Debug("\tit::it(it&)\n");
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

		FILE* DebugFile(FILE* debug)
		{
			FILE* last = debug_;
			debug_ = debug;
			return last;
		}

	private:
		friend class TarArchive;
		
		Iterator(TarArchive* dir)
		{
			Clear();
			Debug("\tit::it(dir)\n");
			dir_ = dir;
			++(*this);
		}
		void Clear()
		{
			dir_	= 0;
			valid_  = 0;
			offset_	= 0;
			span_	= 0;
			size_	= 0;
			path_	= 0;
			seek_	= 0;
			errno_	= 0;
			debug_	= 0;
		}
		void Delete()
		{
			Debug("\tit::Delete() valid %d\n", valid_);
			if(valid_)
			{
				delete [] path_;
			}
		}
		void Copy(const Iterator& it)
		{
			Debug("\tit::Copy()\n");
			dir_    = it.dir_;
			valid_  = it.valid_;
			offset_ = it.offset_;
			span_   = it.span_;
			size_   = it.size_;
			path_   = StringDup(it.path_);
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
		int		seek_;	// current read position, relative to start of data

		// error handling
		int		errno_;

		int ErrorNo() const { return errno_; }
		const char* ErrorString() const { return strerror(errno_); }

		// debugging
		FILE*	debug_;

		void Debug(const char* format, ...) const
		{
			if(debug_) {
				va_list al;
				va_start(al, format);
				vfprintf(debug_, format, al);
				va_end(al);
			}
		}
	};

	typedef Iterator iterator;
	friend class Iterator;

	TarArchive() : file_(0), fd_(-1), errno_(EOK) {}
	
	int Open(const char* file)
	{
		file_ = file;
		
		fd_ = open(file_, O_RDONLY);
		if(fd_ == -1)
		{
			file_ = 0;
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

	enum { BLOCKSZ = 512 };

	const char* file_;
	int fd_;
	int errno_;

	int Read(off_t offset, void* data, size_t size)
	{
		if(lseek(fd_, offset, SEEK_SET) == -1) {
			errno_ = errno; return -1;
		}

		int ret = read(fd_, data, size);
		if(ret == -1) { errno_ = errno; }
		return ret;
	}

	void Iterate(Iterator* it)
	{
		it->Debug("\tdir::Iterate() valid %d offset %u span %u size %u\n",
			it->valid_, it->span_, it->size_);

		// Seek to the begining-of-file past the it's file position.
		assert((it->Valid() && it->span_ > 0) || (!it->Valid() && !it->span_));
		off_t bof = it->offset_ + it->span_;

		if(lseek(fd_, bof*BLOCKSZ, SEEK_SET) == -1) {
			errno_ = errno; it->valid_ = 0; return;
		}

		// Search for a valid header record.
		TarRecord rec;
		assert(sizeof(rec) == BLOCKSZ);
		do {
			int err = read(fd_, &rec, sizeof(rec));
			// Check for error and EOF.
			if(err == -1) { errno_ = errno; it->valid_ = 0; return; }
			if(err < sizeof(rec)) { it->valid_ = 0; return; }

			bof++; // Read a block

			// Check for valid header.
			if(strcmp(TMAGIC, rec.magic) == 0) {
				it->valid_	= 1;
				it->offset_	= bof - 1;
				it->span_	= 1;
				break;
			};
		} while(1);

		it->Debug("\trec: name %s mode %s uid %s gid %s size %d\n",
			rec.name,
			rec.mode,
			rec.uid,
			rec.gid,
			strtol(rec.size, 0, 0)
			);

		// Extract info from header record.
		it->size_	= strtol(rec.size, 0, 0);
		it->path_	= StringDup(rec.name);

		// Calculate span in blocks based on size (rounding size up).
		int size	= it->size_;
		it->span_	= 1 /*header block*/ + size / BLOCKSZ;
		if(size % BLOCKSZ) { it->span_ += 1; }

		it->Debug("\tfound: valid %d offset %u span %u\n",
			it->valid_, it->offset_, it->span_);
	}
};

