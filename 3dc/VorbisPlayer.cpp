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
//   a music file that turns out not to be valid Ogg Vorbis is now closed, instead of being left open;
//   a file that cannot be decoded, or that holds no audio, is refused instead of being read again; no memory for
//   the audio stream counts as a stream that cannot start
// Modified 2026-11-05 by Melanikus for the AvP port:
//   took out what was only for the XAudio2 sound backend, which nothing has built since OpenAL replaced it

// Modified 2026-11-05 by Melanikus for the AvP port:
//   a music thread that can't start is noticed, and not waited for later
// Modified 2026-11-07 by Melanikus for the AvP port:
//   error messages name this file by hand, from the top of the repository, instead of with __FILE__, which put the whole folder
//   it was built in, with the name of whoever built it, into the game
// Modified 2026-11-08 by Melanikus for the AvP port:
//   a track ends at the end of its file: it isn't played again from the start, and once all of it has been heard the game goes
//   on to the level's next track (#94, TRACKS-1). The fix follows Bobblen's avp-forever (f15abb6, e02bf47), rewritten.
//   Asking for a track when no tracks were found no longer reads outside the empty list
#include <stdio.h>
#include "VorbisPlayer.h"
#include <vector>
#include <fstream>
#include <assert.h>
#include "utilities.h" // avp_open()
#include "logstring.h"
#include "console.h"
#include <new>

// This file's name in the error log, written by hand. __FILE__ would give the whole folder it was built in (C:\Users\<name>\...),
// and that would go into the game for anyone to read. tools/check-package-imports.ps1 refuses a package that names any such folder.
static const char kThisFile[] = "3dc/VorbisPlayer.cpp";

#ifdef _XBOX
	#define _fseeki64 fseek // ensure libvorbis uses fseek and not _fseeki64 for xbox
#endif

static const int kBufferSize  = 32768;
static const int kBufferCount = 3;
static DWORD kQuantum = 1000 / 60;

void *VorbisUpdateThread(void *args);

extern int SetStreamingMusicVolume(int volume);
extern int musicVolume; // volume control from menus

std::vector<std::string> TrackList;

#ifdef _WIN32
	const std::string tracklistFilename = "Music/ogg_tracks.txt";
	const std::string musicFolderName = "Music/";
#endif
#ifdef _XBOX
	const std::string tracklistFilename = "d:\\Music\\ogg_tracks.txt";
	const std::string musicFolderName = "d:\\Music\\";
#endif

static VorbisPlayback *inGameMusic = NULL;

bool VorbisPlayback::Open(const std::string &fileName)
{
	_vorbisFile = fopen(fileName.c_str(), "rb");
	if (!_vorbisFile) {
		Con_PrintError("Can't find OGG Vorbis file " + fileName);
		return false;
	}

	if (ov_open_callbacks(_vorbisFile, &_oggFile, NULL, 0, OV_CALLBACKS_DEFAULT) < 0) {
		Con_PrintError("File " + fileName + "is not a valid OGG Vorbis file");
		// a failed open leaves the file to us: libvorbis clears its datasource before ov_clear
		fclose(_vorbisFile);
		_vorbisFile = NULL;
		return false;
	}

	// get audio info
	_vorbisInfo = ov_info(&_oggFile, -1);

	// create the streaming audio buffer
	_audioStream = new(std::nothrow) AudioStream;
	if (!_audioStream || !_audioStream->Init(_vorbisInfo->channels, _vorbisInfo->rate, 16, kBufferSize, kBufferCount)) {
		Con_PrintError("Couldn't initialise audio stream for Vorbis playback");
		return false;
	}

	// init some temp audio data storage
	_audioData = new(std::nothrow) uint8_t[kBufferSize];
	if (_audioData == NULL) {
		Con_PrintError("Couldn't allocate memory for Vorbis temp buffer");
		return false;
	}

	GetVorbisData(kBufferSize);

	if (_readFailed) {
		Con_PrintError("File " + fileName + " could not be played");
		return false;
	}

	// fill the first buffer
	_audioStream->WriteData(_audioData, kBufferSize);

	// start playing
	if (_audioStream->Play() == AUDIOSTREAM_OK) {
		_isPlaying = true;
		_audioStream->SetVolume(musicVolume);

		// Only a thread that really started can be waited for later. Without one, the music just doesn't play.
		if (pthread_create(&_playbackThread, NULL, VorbisUpdateThread, static_cast<void*>(this)) == 0) {
			_decodeThreadInited = true;
		}
		else {
			LogErrorString("couldn't start the thread for ogg vorbis playback\n");
			_audioStream->Stop();
			_isPlaying = false;
		}
	}
	else {
		LogErrorString("couldn't play ogg vorbis buffer\n");
	}

	return true;
}

VorbisPlayback::~VorbisPlayback()
{
	Stop();

	delete _audioStream;

	// clear out the OggVorbis_File struct
	ov_clear(&_oggFile);

	delete[] _audioData;
}

// Fills the buffer with the next part of the track. At the end of the file it stops, and what's left of the buffer is silence.
uint32_t VorbisPlayback::GetVorbisData(uint32_t sizeToRead)
{
	uint32_t bytesReadTotal = 0;
	int32_t bytesReadPerLoop = 0;

	while (bytesReadTotal < sizeToRead)
	{
		bytesReadPerLoop = ov_read(
			&_oggFile,									// what file to read from
			reinterpret_cast<char*>(_audioData + bytesReadTotal),
			sizeToRead - bytesReadTotal,				// how much data to read
			0,											// 0 specifies little endian decoding mode
			2,											// 2 specifies 16-bit samples
			1,											// 1 specifies signed data
			0
		);

		// a gap in the data: the decoder carries on after it, so this is not the end of the file
		if (bytesReadPerLoop == OV_HOLE)
		{
			LogErrorString("OV_HOLE", __LINE__, kThisFile);
		}
		// any other negative number is an error, not a count of bytes, so it never goes into the total. The file can't
		// be decoded, and asking again is not safe: the decoder can then carry on with half-built tables, which crashes
		// or hangs the game. So we remember it, and nothing reads this file again.
		else if (bytesReadPerLoop < 0)
		{
			LogErrorString("ov_read encountered an error", __LINE__, kThisFile);
			_readFailed = true;
			break;
		}
		// The end of the file. Before, it went back to the start here, so a track never ended, and a level's music played its first
		// track for ever (#94). Now the track is over: nothing more is read, and once all of it has been heard the game picks the
		// level's next track (VorbisUpdateThread and IsVorbisPlaying, below). A file that ends before any audio has come out of
		// it holds none, and is refused.
		else if (bytesReadPerLoop == 0)
		{
			if (!_gotAnyAudio)
			{
				LogErrorString("ogg vorbis file holds no audio", __LINE__, kThisFile);
				_readFailed = true;
				break;
			}
			_reachedEnd = true;
			break;
		}
		else
		{
			_gotAnyAudio = true;
			bytesReadTotal += bytesReadPerLoop;
		}
	}

	// After the end, the rest of the buffer is silence, so the track isn't followed by whatever the buffer held before
	if (_reachedEnd && bytesReadTotal < sizeToRead)
	{
		memset(_audioData + bytesReadTotal, 0, sizeToRead - bytesReadTotal);
	}

	return bytesReadTotal;
}

void *VorbisUpdateThread(void *argument)
{
	VorbisPlayback *vorbis = static_cast<VorbisPlayback*>(argument);


	while (vorbis->_isPlaying)
	{
		uint32_t numBuffersFree = vorbis->_audioStream->GetNumFreeBuffers();

		// Once the end of the file has been read, or a read has failed, nothing more is read: what is already queued plays out.
		// When every buffer has come back, all of it has been heard, and the track has finished. The game then picks the next.
		if (vorbis->_reachedEnd || vorbis->_readFailed)
		{
			if (numBuffersFree >= static_cast<uint32_t>(kBufferCount))
			{
				vorbis->_finished = true;
				break;
			}
		}
		else if (numBuffersFree)
		{
			vorbis->GetVorbisData(kBufferSize);
			if (!vorbis->_readFailed)
			{
				vorbis->_audioStream->WriteData(vorbis->_audioData, kBufferSize);
			}
		}

		::Sleep(kQuantum);
	}

	return 0;
}

void VorbisPlayback::Stop()
{
	if (_isPlaying)
	{
		_audioStream->Stop();
		_isPlaying = false;
	}

	// wait until audio processing thread has finished running before continuing
	if (_decodeThreadInited) {
		pthread_join(_playbackThread, NULL);
	}
}

void LoadVorbisTrack(size_t track)
{
	// if we're already playing a track, stop it
	if (inGameMusic)
	{
		delete inGameMusic;
		inGameMusic = NULL;
	}

	// Tracks count from 1, as CD Tracks.txt numbers them: track 1 is the first name in ogg_tracks.txt. 0 is taken as 1, as it always
	// was. A number with no track is refused, and so is any number when no tracks were found: TrackList[0] of an empty list would read
	// outside it.
	size_t const index = (track == 0) ? 0 : track - 1;

	if (index >= TrackList.size()) {
		return;
	}

	inGameMusic = new VorbisPlayback;
	if (!inGameMusic->Open(TrackList[index]))
	{
		delete inGameMusic;
		inGameMusic = NULL;
	}
}

bool LoadVorbisTrackList()
{
	std::ifstream file(tracklistFilename.c_str());

	if (!file.is_open())
	{
		LogErrorString("no music tracklist found - not using ogg vorbis music");
		return false;
	}
	else {
		LogString("music tracklist found - music playback is available");
	}

	std::string trackName;

	while (std::getline(file, trackName))
	{
		TrackList.push_back(musicFolderName + trackName);
	}

	file.close();
	return true;
}

size_t CheckNumberOfVorbisTracks()
{
	return TrackList.size();
}

// A track that has finished isn't playing, so Music_Check (MusicPlayer.cpp) picks the level's next one
bool IsVorbisPlaying()
{
	if (inGameMusic) {
		return inGameMusic->_isPlaying && !inGameMusic->_finished;
	}

	return false;
}

int GetStreamingMusicVolume()
{
	if (inGameMusic) // hack to stop this call before the audio stream is initialised
	{
		return inGameMusic->_audioStream->GetVolume();
	}

	return 0;
}

int SetStreamingMusicVolume(int volume)
{
	if (inGameMusic) // hack to stop this call before the audio stream is initialised
	{
		return inGameMusic->_audioStream->SetVolume(volume);
	}

	return 0;
}

void Vorbis_CloseSystem()
{
	delete inGameMusic;
	inGameMusic = NULL;
}
