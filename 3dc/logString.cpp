// Copyright (C) 2010 Barry Duncan. All Rights Reserved.
// The original author of this code can be contacted at: bduncan22@hotmail.com

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Modified 2026-11-03 by Melanikus for the AvP port:
//   adds DxErrorToString, which turns a DirectX error code into readable text (it replaces an old DirectX
//   SDK library), and LogErrorStringC, which writes a log line from code that does not use C++ strings
// Modified 2026-11-04 by Melanikus for the AvP port:
//   lines are written by a thread of their own, so a slow disk or a virus scanner can't hold up the game
// Modified 2026-11-05 by Melanikus for the AvP port:
//   if the save folder couldn't be found or made, the log says why as soon as it can write
// Modified 2026-11-07 by Melanikus for the AvP port:
//   the last logs are kept as avp_log.1.txt to avp_log.5.txt, and only a plain file is ever written to (log_files.h); a crash writes what
//   it was and the places on the stack, as file names and distances, straight to the file without waiting for a lock
//   every line, and the crash lines, leave out the player's Windows user name, computer name, folders and network addresses

#include "logString.h"
#include "renderer.h"
#include "utilities.h"
#include "frame_timing.h"
#include "log_files.h"

#ifdef _XBOX
	#include "D3dx8core.h"
	std::string logFilename = "d:\\avp_log.txt";
#else
	std::string logFilename = "avp_log.txt";
#endif

static std::string lastError;

#ifndef _XBOX
// Replacement for the DirectX SDK's dxerr library (DXGetErrorString / DXGetErrorDescription), which is not part
// of the Windows 10 SDK: the symbolic name of the common Direct3D 9, D3DX9 and COM results (or the hex value),
// followed by the system's message for the code when Windows has one.
std::string DxErrorToString(HRESULT hr)
{
	const char *name = 0;
	switch (hr)
	{
#define DXERR_CASE(x) case x: name = #x; break;
		DXERR_CASE(D3DERR_WRONGTEXTUREFORMAT)
		DXERR_CASE(D3DERR_UNSUPPORTEDCOLOROPERATION)
		DXERR_CASE(D3DERR_UNSUPPORTEDCOLORARG)
		DXERR_CASE(D3DERR_UNSUPPORTEDALPHAOPERATION)
		DXERR_CASE(D3DERR_UNSUPPORTEDALPHAARG)
		DXERR_CASE(D3DERR_TOOMANYOPERATIONS)
		DXERR_CASE(D3DERR_CONFLICTINGTEXTUREFILTER)
		DXERR_CASE(D3DERR_UNSUPPORTEDFACTORVALUE)
		DXERR_CASE(D3DERR_CONFLICTINGRENDERSTATE)
		DXERR_CASE(D3DERR_UNSUPPORTEDTEXTUREFILTER)
		DXERR_CASE(D3DERR_CONFLICTINGTEXTUREPALETTE)
		DXERR_CASE(D3DERR_DRIVERINTERNALERROR)
		DXERR_CASE(D3DERR_NOTFOUND)
		DXERR_CASE(D3DERR_MOREDATA)
		DXERR_CASE(D3DERR_DEVICELOST)
		DXERR_CASE(D3DERR_DEVICENOTRESET)
		DXERR_CASE(D3DERR_NOTAVAILABLE)
		DXERR_CASE(D3DERR_OUTOFVIDEOMEMORY)
		DXERR_CASE(D3DERR_INVALIDDEVICE)
		DXERR_CASE(D3DERR_INVALIDCALL)
		DXERR_CASE(D3DERR_DRIVERINVALIDCALL)
		DXERR_CASE(D3DERR_WASSTILLDRAWING)
		DXERR_CASE(D3DXERR_CANNOTMODIFYINDEXBUFFER)
		DXERR_CASE(D3DXERR_INVALIDMESH)
		DXERR_CASE(D3DXERR_CANNOTATTRSORT)
		DXERR_CASE(D3DXERR_SKINNINGNOTSUPPORTED)
		DXERR_CASE(D3DXERR_TOOMANYINFLUENCES)
		DXERR_CASE(D3DXERR_INVALIDDATA)
		DXERR_CASE(D3DXERR_LOADEDMESHASNODATA)
		DXERR_CASE(D3DXERR_DUPLICATENAMEDFRAGMENT)
		DXERR_CASE(D3DXERR_CANNOTREMOVELASTITEM)
		DXERR_CASE(E_OUTOFMEMORY)
		DXERR_CASE(E_INVALIDARG)
		DXERR_CASE(E_NOTIMPL)
		DXERR_CASE(E_FAIL)
#undef DXERR_CASE
		default:
			break;
	}

	std::ostringstream out;
	if (name) {
		out << name;
	}
	else {
		out << "HRESULT 0x" << std::hex << std::uppercase << (unsigned long)hr;
	}

	char *message = 0;
	DWORD length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	                              NULL, (DWORD)hr, 0, (LPSTR)&message, 0, NULL);
	if (length && message)
	{
		std::string text(message, length);
		while (!text.empty() && (text[text.size() - 1] == '\r' || text[text.size() - 1] == '\n' || text[text.size() - 1] == ' ')) {
			text.erase(text.size() - 1);
		}
		out << " - " << text;
	}
	if (message) {
		LocalFree(message);
	}

	return out.str();
}
#endif

const std::string& GetLastErrorMessage()
{
	return lastError;
}

#ifdef _XBOX

void ClearLog()
{
	std::string filePath(GetSaveFolderPath());
	filePath += logFilename;

	std::ofstream file(filePath.c_str(), std::ios::out);
}

void WriteToLog(const std::string &logLine)
{
	std::string filePath(GetSaveFolderPath());
	filePath += logFilename;

	std::ofstream file(filePath.c_str(), std::ios::out | std::ios::app);

	file << logLine;
#if _DEBUG
	OutputDebugString(logLine.c_str());
#endif
	file.close();
}

void WriteErrorToLog(const std::string &logLine)
{
	WriteToLog(logLine);
}

void LogTakeWriteTimes(int *writes, float *slowestMilliseconds, int *linesLost)
{
	*writes = 0;
	*slowestMilliseconds = 0.0f;
	*linesLost = 0;
}

#else

// THE LOG'S WRITER
//
// The log used to open its file, write one line and close it, every line, on the game's own thread. A report is about 25 lines, and a virus scanner
// can look at a file each time it's closed, so writing the log could stop the game. Now a line only goes into a waiting list in memory, which takes
// next to no time, and a thread of its own writes the list out, with the file left open.
//
// A line in memory is lost if the game dies, and those can be the lines that matter most. So errors are written at once, on the spot, along with
// everything waiting ahead of them. And if the game crashes, or ends, the list is written then. What's written goes to Windows, which keeps it even
// if the game's gone. If the disk stops altogether, the list stops growing at kMostWaiting, and the log says how much it lost.
class LogWriter
{
public:
	LogWriter() : file(INVALID_HANDLE_VALUE), wake(0), thread(0), writes(0), slowest(0.0), linesLost(0)
	{
		InitializeCriticalSection(&listLock);
		InitializeCriticalSection(&fileLock);
		QueryPerformanceFrequency(&ticksPerSecond);

		// no folder means the current one: the log is only worth so much, and it mustn't be why the game won't start
		filePath = GetSaveFolderPath();
		filePath += logFilename;

		// What every line leaves out about the player: their folders, names and addresses (log_files.h). Found before any line is written.
		LogFiles_PrivateGather(privateThings, GetSaveFolderPath());

		wake = CreateEvent(NULL, FALSE, FALSE, NULL);

		if (wake)
		{
			thread = CreateThread(NULL, 0, WriterThread, this, 0, NULL);
		}
	}

	// mustGoIn is for errors, which go on the list even if it's full. Every line that reaches the file comes through here, and the player is
	// left out of it first, before it waits anywhere.
	void Add(const std::string &line, bool mustGoIn)
	{
		std::string const written = LogFiles_LeaveOutThePlayer(line, privateThings);

		EnterCriticalSection(&listLock);

		if (!mustGoIn && waiting.size() + written.size() > kMostWaiting)
		{
			linesLost++;
		}
		else
		{
			waiting += written;
		}

		LeaveCriticalSection(&listLock);

		// with no thread to do it, this is the old way: write it here and now
		if (thread)
		{
			SetEvent(wake);
		}
		else
		{
			WriteWaiting();
		}
	}

	void WriteWaiting()
	{
		EnterCriticalSection(&fileLock);
		WriteListToFile();
		LeaveCriticalSection(&fileLock);
	}

	// For a crash. Whatever crashed may be holding a lock, and waiting for it for ever would hang the crash, so this only tries, for a moment, in case
	// the writer thread is halfway through a write. The list's lock is kept once it's been won: if it were let go, another thread could take it and
	// stop on the same broken memory that caused the crash. It's fine to take it again below, because a thread can hold a lock twice over.
	void WriteWaitingIfFree()
	{
		for (int tries = 0; tries < kCrashTries; tries++)
		{
			if (TryEnterCriticalSection(&fileLock))
			{
				if (TryEnterCriticalSection(&listLock))
				{
					WriteListToFile();
					LeaveCriticalSection(&listLock);
					LeaveCriticalSection(&fileLock);
					return;
				}

				LeaveCriticalSection(&fileLock);
			}

			Sleep(1);
		}
	}

	void Empty()
	{
		EnterCriticalSection(&fileLock);

		EnterCriticalSection(&listLock);
		waiting.clear();
		LeaveCriticalSection(&listLock);

		if (file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(file);
			file = INVALID_HANDLE_VALUE;
		}

		// The last run's log, and the ones before it, are kept: they move along by one, and this run starts a fresh one (log_files.h)
		LogFiles_MoveAlong(filePath);
		LogFiles_Empty(filePath);

		LeaveCriticalSection(&fileLock);
	}

	// For a crash: these words go straight to the file, with a handle of their own, so no lock is needed. Whatever crashed may be holding one.
	// They're only file names and numbers, but they go through the same leaving out as every other line, in a buffer kept for it, so that
	// nothing is asked of memory while the game is crashing.
	void WriteCrashWords(const char *words)
	{
		LogFiles_LeaveOutThePlayer(words, privateThings, crashWritten, sizeof(crashWritten));

		HANDLE const crashFile = LogFiles_OpenToAdd(filePath);

		if (crashFile != INVALID_HANDLE_VALUE)
		{
			DWORD written = 0;
			WriteFile(crashFile, crashWritten, (DWORD)strlen(crashWritten), &written, NULL);
			CloseHandle(crashFile);
		}
	}

	void TakeWriteTimes(int *writesDone, float *slowestMilliseconds, int *lost)
	{
		EnterCriticalSection(&listLock);
		*writesDone = writes;
		*slowestMilliseconds = (float)slowest;
		*lost = linesLost;
		writes = 0;
		slowest = 0.0;
		linesLost = 0;
		LeaveCriticalSection(&listLock);
	}

private:
	static const size_t kMostWaiting = 4 * 1024 * 1024;
	static const int kCrashTries = 200;   // a millisecond each

	static DWORD WINAPI WriterThread(void *self)
	{
		LogWriter *writer = (LogWriter *)self;

		for (;;)
		{
			WaitForSingleObject(writer->wake, INFINITE);
			writer->WriteWaiting();
		}
	}

	// The file lock has to be held. Swapping the two lists takes no time, so the game never waits for the disk through the list's lock.
	void WriteListToFile()
	{
		EnterCriticalSection(&listLock);
		beingWritten.swap(waiting);
		LeaveCriticalSection(&listLock);

		if (beingWritten.empty())
		{
			return;
		}

		LARGE_INTEGER started, finished;
		QueryPerformanceCounter(&started);

		// Only a plain file is written to: a folder or a link in the log's place gets nothing
		if (file == INVALID_HANDLE_VALUE)
		{
			file = LogFiles_OpenToAdd(filePath);
		}

		if (file != INVALID_HANDLE_VALUE)
		{
			DWORD written = 0;
			WriteFile(file, beingWritten.data(), (DWORD)beingWritten.size(), &written, NULL);
		}

#if _DEBUG
		OutputDebugString(beingWritten.c_str());
#endif
		beingWritten.clear();

		QueryPerformanceCounter(&finished);

		double const milliseconds = (double)(finished.QuadPart - started.QuadPart) * 1000.0 / (double)ticksPerSecond.QuadPart;

		EnterCriticalSection(&listLock);
		writes++;
		slowest = (milliseconds > slowest) ? milliseconds : slowest;
		LeaveCriticalSection(&listLock);
	}

	CRITICAL_SECTION listLock;   // guards the waiting list and the counts
	CRITICAL_SECTION fileLock;   // guards the file and the list being written
	std::string waiting;
	std::string beingWritten;
	std::string filePath;
	LogFiles_Private privateThings;   // found once, before any line, and only read after that, so no lock is needed for it
	char crashWritten[8192];          // the crash words with the player left out: they can come out longer than they went in
	HANDLE file;
	HANDLE wake;
	HANDLE thread;
	LARGE_INTEGER ticksPerSecond;
	int writes;
	double slowest;
	int linesLost;
};

static LogWriter *logWriter = 0;
static LPTOP_LEVEL_EXCEPTION_FILTER filterBeforeOurs = 0;

// Kept here rather than on the stack, because a crash can be the stack running out
static void *crashPlaces[kLogFilesMostCrashPlaces];
static char crashWords[4096];

static LONG WINAPI WriteLogBeforeCrash(EXCEPTION_POINTERS *crash)
{
	// Where it happened, as distances from the start of the game's own code, so that the linker's map can name the function. The stack holds the way back: any number on it that points into the game is probably a place that called something.
	char const *const base = (char const *)GetModuleHandle(NULL);
	IMAGE_NT_HEADERS const *const headers = (IMAGE_NT_HEADERS const *)(base + ((IMAGE_DOS_HEADER const *)base)->e_lfanew);
	size_t const size = headers->OptionalHeader.SizeOfImage;
	EXCEPTION_RECORD const *const record = crash->ExceptionRecord;
	char line[200];

	_snprintf(line, sizeof(line) - 1, "crash: code %08lX at %p, %IX into the game, on the address %p", record->ExceptionCode, record->ExceptionAddress, (size_t)((char const *)record->ExceptionAddress - base), record->NumberParameters > 1 ? (void *)record->ExceptionInformation[1] : NULL);
	line[sizeof(line) - 1] = 0;
	LogString(line);

	size_t const *stack = (size_t const *)crash->ContextRecord->Esp;
	std::string found = "crash: on the stack";
	int kept = 0;

	for (int i = 0; i < 400 && kept < 24 && !IsBadReadPtr(stack + i, sizeof(size_t)); i++)
	{
		size_t const distance = stack[i] - (size_t)base;

		if (distance > headers->OptionalHeader.BaseOfCode && distance < size)
		{
			char one[16];

			_snprintf(one, sizeof(one) - 1, " %IX", distance);
			one[sizeof(one) - 1] = 0;
			found += one;
			kept++;
		}
	}

	LogString(found);

	if (logWriter)
	{
		// What was waiting first, so the lines stay in order, then what crashed and where
		logWriter->WriteWaitingIfFree();

		// Windows gives the places on this thread's stack, starting here, so they go back through the code that crashed. Each is written
		// as a file name and a distance into it, to be looked up in the .map kept beside the build.
		USHORT const count = RtlCaptureStackBackTrace(0, kLogFilesMostCrashPlaces, crashPlaces, NULL);
		LogFiles_DescribeCrash(crash->ExceptionRecord->ExceptionCode, crash->ExceptionRecord->ExceptionAddress, crashPlaces, count, crashWords, sizeof(crashWords));
		logWriter->WriteCrashWords(crashWords);
	}

	return filterBeforeOurs ? filterBeforeOurs(crash) : EXCEPTION_CONTINUE_SEARCH;
}

static void WriteLogAtExit()
{
	logWriter->WriteWaiting();
}

// It's never given back: it has to outlive everything that might still write to the log as the game ends. It can give back nothing, while it's being
// made, so that a line written from inside that doesn't come straight back here. Such a line would be lost, so finding the save folder keeps what went wrong, and ClearLog says it.
static LogWriter *TheLogWriter()
{
	static bool beingMade = false;

	if (!logWriter && !beingMade)
	{
		beingMade = true;
		logWriter = new LogWriter;
		filterBeforeOurs = SetUnhandledExceptionFilter(WriteLogBeforeCrash);
		atexit(WriteLogAtExit);
	}

	return logWriter;
}

void ClearLog()
{
	if (LogWriter *writer = TheLogWriter())
	{
		writer->Empty();
	}

	// Making the log asked for the save folder, and if that went wrong, there was no log yet to say so in. It's said here and not when the log is made, because this empties whatever came before it,
	// and it's the first thing the game does (WinMain in winmain.cpp).
	if (char const *const problem = GetSaveFolderProblem())
	{
		LogErrorString(problem);
	}
}

void WriteToLog(const std::string &logLine)
{
	ProcessorStageClock clock(kProcessorLog);

	if (LogWriter *writer = TheLogWriter())
	{
		writer->Add(logLine, false);
	}
}

void WriteErrorToLog(const std::string &logLine)
{
	ProcessorStageClock clock(kProcessorLog);

	if (LogWriter *writer = TheLogWriter())
	{
		writer->Add(logLine, true);
		writer->WriteWaiting();
	}
}

void LogTakeWriteTimes(int *writes, float *slowestMilliseconds, int *linesLost)
{
	*writes = 0;
	*slowestMilliseconds = 0.0f;
	*linesLost = 0;

	if (LogWriter *writer = TheLogWriter())
	{
		writer->TakeWriteTimes(writes, slowestMilliseconds, linesLost);
	}
}

#endif

void LogDxError(HRESULT hr, int LINE, const char* FILE)
{
	std::string temp = "DirectX Error! : ";

#ifdef _XBOX
	char buffer[60];
	D3DXGetErrorString(hr, LPSTR(&buffer), 60);
	temp.append(buffer);
#else
	temp.append(DxErrorToString(hr));
#endif
	temp.append(" Line: ");
	temp.append(Util::IntToString(LINE));
	temp.append(" File: ");
	temp.append(FILE);
	temp.append("\n");
	WriteErrorToLog("\t" + temp);

	lastError = temp;
}

// logs a string to file, stating line number of error, and source file it occured in
void LogErrorString(const std::string &errorString, int LINE, const char* FILE)
{
	std::string temp = "Error: " + errorString + " Line: " + Util::IntToString(LINE) + " File: " + FILE + "\n";
	WriteErrorToLog("\t" + temp);

	lastError = temp;
}

// more basic version of above function. just log a string.
void LogErrorString(const std::string &errorString)
{
	std::string temp = "Error: " + errorString + "\n";
	WriteErrorToLog("\t" + temp);

	lastError = temp;
}

// the same for a C string, for callers that do not include <string>
void LogErrorStringC(const char *errorString)
{
	LogErrorString(std::string(errorString ? errorString : ""));
}

void LogDebugString(const std::string &debugString, int LINE, const char* FILE)
{
	std::string temp = "Debug message: " + debugString + " Line: " + Util::IntToString(LINE) + " File: " + FILE + "\n";
	WriteToLog("\t" + temp);
}

void LogDebugString(const std::string &debugString)
{
	// TODO - can specify a flag on cmd args to print these or not?
	std::string temp = debugString + "\n";
	WriteToLog(temp);
}

void LogString(const std::string &logString)
{
	std::string temp = logString + "\n";
	WriteToLog(temp);
}
