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
//   the player remembers that a music file could not be decoded, so it never tries to read that file again
// Modified 2026-11-08 by Melanikus for the AvP port:
//   a track ends at the end of its file, once all of it has been heard, so a level's music goes on to its next track (#94,
//   TRACKS-1). The fix follows Bobblen's avp-forever (f15abb6, e02bf47), rewritten. What the music thread and the game both
//   read is kept so that both can read it safely at once

#ifndef _VorbisPlayer_h_
#define _VorbisPlayer_h_

#include "os_header.h"
#include <atomic>

#ifdef _XBOX
	#define _fseeki64 fseek // ensure libvorbis uses fseek and not _fseeki64 for xbox
#endif

#include <vorbis/vorbisfile.h>
#include "audioStreaming.h"
#include <string>

#include <pthread.h>

class VorbisPlayback
{
	public:
		FILE          *_vorbisFile;
		vorbis_info	  *_vorbisInfo;
		OggVorbis_File _oggFile;

		AudioStream *_audioStream;
		uint32_t  _bufferSize;
		uint32_t  _halfBufferSize;
		uint8_t	  *_audioData;

		// The game turns this off to stop the track, and the music thread reads it, both at once, so it's an atomic: a value two
		// threads can read and write at the same time without either seeing half of a change
		std::atomic<bool> _isPlaying;

		bool	  _readFailed;	// the file could not be decoded: it must not be read again
		bool	  _reachedEnd;	// the end of the file has been read: the track isn't played again from the start
		bool	  _gotAnyAudio;	// some audio has come out of this file, so its end is the end of a track, not an empty file

		// Set by the music thread once the track is over and all of it has been heard, or once it can't be read any more and what
		// was already sent has been heard. IsVorbisPlaying then says it isn't playing, so the game picks the next track
		std::atomic<bool> _finished;

		VorbisPlayback() :
			_vorbisFile(0),
			_vorbisInfo(0),
			_audioStream(0),
			_bufferSize(0),
			_halfBufferSize(0),
			_audioData(0),
			_isPlaying(false),
			_readFailed(false),
			_reachedEnd(false),
			_gotAnyAudio(false),
			_finished(false),
			_decodeThreadInited(false)
		{
			memset(&_oggFile, 0, sizeof(OggVorbis_File));
		}
		~VorbisPlayback();

		bool Open(const std::string &fileName);
		void Stop();
		uint32_t GetVorbisData(uint32_t sizeToRead);

	private:
		pthread_t _playbackThread;
		bool	  _decodeThreadInited;
};

void Vorbis_CloseSystem();
extern void LoadVorbisTrack(size_t track);
extern bool LoadVorbisTrackList();
bool IsVorbisPlaying();
size_t CheckNumberOfVorbisTracks();
int SetStreamingMusicVolume(int volume);
int GetStreamingMusicVolume();

#endif
