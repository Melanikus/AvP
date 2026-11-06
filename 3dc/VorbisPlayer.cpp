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
//   a file that cannot be decoded, or that holds no audio, is refused instead of being read again

// Modified 2026-11-05 by Melanikus for the AvP port:
//   a music thread that can't start is noticed, and not waited for later
#include <stdio.h>
#include "VorbisPlayer.h"
#include <vector>
#include <fstream>
#include <assert.h>
#include "utilities.h" // avp_open()
#include "logstring.h"
#include "console.h"
#include <new>

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
	if (!_audioStream->Init(_vorbisInfo->channels, _vorbisInfo->rate, 16, kBufferSize, kBufferCount)) {
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

uint32_t VorbisPlayback::GetVorbisData(uint32_t sizeToRead)
{
	uint32_t bytesReadTotal = 0;
	int32_t bytesReadPerLoop = 0;
	bool gotAudioThisPass = true;	// whether audio has come out since we last went back to the start of the file

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
			LogErrorString("OV_HOLE", __LINE__, __FILE__);
		}
		// any other negative number is an error, not a count of bytes, so it never goes into the total. The file can't
		// be decoded, and asking again is not safe: the decoder can then carry on with half-built tables, which crashes
		// or hangs the game. So we remember it, and nothing reads this file again.
		else if (bytesReadPerLoop < 0)
		{
			LogErrorString("ov_read encountered an error", __LINE__, __FILE__);
			_readFailed = true;
			break;
		}
		// if we reach the end of the file, go back to start. Reaching the end again with no audio in between means
		// the file holds none, and going round again would never finish.
		else if (bytesReadPerLoop == 0)
		{
			if (!gotAudioThisPass)
			{
				LogErrorString("ogg vorbis file holds no audio", __LINE__, __FILE__);
				_readFailed = true;
				break;
			}
			gotAudioThisPass = false;
			ov_raw_seek(&_oggFile, 0);
		}
		else
		{
			gotAudioThisPass = true;
			bytesReadTotal += bytesReadPerLoop;
		}
	}

	return bytesReadTotal;
}

void *VorbisUpdateThread(void *argument)
{
	VorbisPlayback *vorbis = static_cast<VorbisPlayback*>(argument);

#ifdef USE_XAUDIO2
	::CoInitializeEx (NULL, COINIT_MULTITHREADED);
#endif

	while (vorbis->_isPlaying)
	{
		uint32_t numBuffersFree = vorbis->_audioStream->GetNumFreeBuffers();

		// once a read has failed, the file is left alone: what is already queued plays out, then there is silence
		if (numBuffersFree && !vorbis->_readFailed)
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

	// TODO? rather than return, pick a random track or just play last?
	if (track > TrackList.size()) {
		return;
	}

	// if user enters 1, decrement to 0 to align to array (enters 2, decrement to 1 etc)
	if (track != 0) {
		track--;
	}

	inGameMusic = new VorbisPlayback;
	if (!inGameMusic->Open(TrackList[track]))
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

bool IsVorbisPlaying()
{
	if (inGameMusic) {
		return inGameMusic->_isPlaying;
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
