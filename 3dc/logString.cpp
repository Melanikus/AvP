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

#include "logString.h"
#include "renderer.h"
#include "utilities.h"

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
	WriteToLog("\t" + temp);

	lastError = temp;
}

// logs a string to file, stating line number of error, and source file it occured in
void LogErrorString(const std::string &errorString, int LINE, const char* FILE)
{
	std::string temp = "Error: " + errorString + " Line: " + Util::IntToString(LINE) + " File: " + FILE + "\n";
	WriteToLog("\t" + temp);

	lastError = temp;
}

// more basic version of above function. just log a string.
void LogErrorString(const std::string &errorString)
{
	std::string temp = "Error: " + errorString + "\n";
	WriteToLog("\t" + temp);

	lastError = temp;
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
