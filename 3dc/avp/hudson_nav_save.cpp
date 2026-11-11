// See hudson_nav_save.h. Written 2026-11-05 by Melanikus for the AvP port.
#include "hudson_nav_save.h"

#include <stdarg.h>
#include <string.h>

HudsonNavSaveFile::HudsonNavSaveFile() : file(0), bytes(0), most(0), why(0)
{
}

HudsonNavSaveFile::~HudsonNavSaveFile()
{
	Close();
}

bool HudsonNavSaveFile::Open(const char *path, long mostBytes)
{
	Close();

	bytes = 0;
	most = mostBytes;
	why = 0;
	file = fopen(path, "w");

	if (!file)
	{
		why = "the disk said no";
	}

	return file != 0;
}

void HudsonNavSaveFile::Line(const char *format, ...)
{
	if (!file)
	{
		return;
	}

	char line[kHudsonNavSaveMostLine + 2];
	va_list arguments;

	va_start(arguments, format);

	int const written = vsnprintf(line, kHudsonNavSaveMostLine, format, arguments);

	va_end(arguments);

	// Nothing that goes in here is that long. If it ever is, what's kept is the start of it.
	int length = written < 0 ? 0 : written;

	if (length >= kHudsonNavSaveMostLine)
	{
		length = kHudsonNavSaveMostLine - 1;
	}

	line[length] = '\n';
	length++;

	if (bytes + length > most)
	{
		// the last thing in the file says it isn't all there
		fputs("# stopped here: the file reached its limit\n", file);
		why = "too big";
		fclose(file);
		file = 0;
		return;
	}

	if (fwrite(line, 1, (size_t)length, file) != (size_t)length)
	{
		why = "the disk said no";
		fclose(file);
		file = 0;
		return;
	}

	bytes += length;
}

void HudsonNavSaveFile::Flush()
{
	if (file && fflush(file) != 0)
	{
		why = "the disk said no";
		fclose(file);
		file = 0;
	}
}

void HudsonNavSaveFile::Close()
{
	if (file)
	{
		if (fclose(file) != 0 && !why)
		{
			why = "the disk said no";
		}

		file = 0;
	}
}
