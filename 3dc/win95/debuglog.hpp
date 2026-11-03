// Modified 2026-11-03 by Melanikus for the AvP port:
//   the lines not yet written are a std::vector<std::string> (was a List<char *> of new[] copies, which a
//   failed final reopen leaked and the copy constructor / operator= shared); the fallback formatting into
//   buf[4096] is bounded (_vsnprintf, not vsprintf)

#ifndef _included_debuglog_hpp_
#define _included_debuglog_hpp_

#include <string>
#include <vector>
//#include <stdio.h>
#include "utilities.h"
#include "debuglog.h"
#include "list_tem.hpp"

/*	Changed 27/1/98 by DHM:
	-----------------------
	
	Made LogFile derived from R_DumpContext rather than being a base class.

	This is in order to give a clean interface for debug dumps to e.g. the
	screen, with the same interface as to a log file.  This base class will
	perform an analagous role to the class CDumpContext in the Microsoft
	Foundation Class library.

	The virtual functions dputs(), dprintf() and vdprintf() will eventually replace
	lputs(), lprintf() and vlprintf().

	For the moment I've copied and pasted the implementations of both.  I would prefer
	in the short term to make one call the other, but this isn't easy with variable
	arguments.  In the long term I want to eliminate the l* functions (lputs() etc)
	but can't because of heritage code (and heritage libraries).
*/

	#ifndef _dump_context_hpp
		#include "dump_context.hpp"
	#endif

struct LogFile : public R_DumpContext
{
private:
	char * fname;
	std::vector<std::string> unwritten;
	int ever_written;
	void FlushOut(FILE * fp);

public:
	LogFile(char const * const _fname);
	virtual ~LogFile();
	LogFile & operator = (LogFile const & l);
	LogFile(LogFile const & l);

	// {{{ Virtual dump implementations:
	inline int dputs(char const * const buf)
	{
		if (!fname) return EOF;
		FILE * fp = avp_open_userfile(fname, "a");
		if (!fp)
		{
			if (!ever_written) return EOF;
			unwritten.push_back(buf);
			return 0;
		}
		if (unwritten.size()) FlushOut(fp);
		ever_written = 1;
		int rv = fputs(buf,fp);
		fclose(fp);
		return rv;
	}

	inline int dprintf(char const * format, ... )
	{
		if (!fname) return -1;
		FILE * fp = avp_open_userfile(fname, "a");
		if (!fp && !ever_written) return -1;
		va_list ap;
		va_start(ap, format);
		int rv;
		if (fp)
		{
			if (unwritten.size()) FlushOut(fp);
			rv = vfprintf(fp,format,ap);
			ever_written = 1;
		}
		else
		{
			char buf[4096];
			rv = _vsnprintf(buf,sizeof(buf)-1,format,ap);
			buf[sizeof(buf)-1] = '\0';
			unwritten.push_back(buf);
		}
		va_end(ap);
		if (fp) fclose(fp);
		return rv;
	}

	inline int vdprintf(char const * format, va_list ap)
	{
		if (!fname) return -1;
		FILE * fp = avp_open_userfile(fname, "a");
		if (!fp && !ever_written) return -1;

		int rv;
		if (fp)
		{
			if (unwritten.size()) FlushOut(fp);
			rv = vfprintf(fp,format,ap);
			ever_written = 1;
			fclose(fp);
		}
		else
		{
			char buf[4096];
			rv = _vsnprintf(buf,sizeof(buf)-1,format,ap);
			buf[sizeof(buf)-1] = '\0';
			unwritten.push_back(buf);
		}
		return rv;
	}
	// }}}

	// {{{ Deprecated logging functions:
	inline int lputs(char const * const buf)
	{
		return dputs(buf);
	}

	inline int lprintf(char const * format, ... )
	{
		if (!fname) return -1;
		FILE * fp = avp_open_userfile(fname, "a");
		if (!fp && !ever_written) return -1;
		va_list ap;
		va_start(ap, format);
		int rv;
		if (fp)
		{
			if (unwritten.size()) FlushOut(fp);
			rv = vfprintf(fp,format,ap);
			ever_written = 1;
		}
		else
		{
			char buf[4096];
			rv = _vsnprintf(buf,sizeof(buf)-1,format,ap);
			buf[sizeof(buf)-1] = '\0';
			unwritten.push_back(buf);
		}
		va_end(ap);
		if (fp) fclose(fp);
		return rv;
	}

	inline int vlprintf(char const * format, va_list ap)
	{
		if (!fname) return -1;
		FILE * fp = avp_open_userfile(fname, "a");
		if (!fp && !ever_written) return -1;

		int rv;
		if (fp)
		{
			if (unwritten.size()) FlushOut(fp);
			rv = vfprintf(fp,format,ap);
			ever_written = 1;
			fclose(fp);
		}
		else
		{
			char buf[4096];
			rv = _vsnprintf(buf,sizeof(buf)-1,format,ap);
			buf[sizeof(buf)-1] = '\0';
			unwritten.push_back(buf);
		}
		return rv;
	}
	// }}}

};

#endif // ! _included_debuglog_hpp_
