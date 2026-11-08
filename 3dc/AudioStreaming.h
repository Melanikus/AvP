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

// Modified 2026-11-04 by Melanikus for the AvP port:
//   a stream remembers what it got as far as making, so that one that failed to start can be let go of
// Modified 2026-11-05 by Melanikus for the AvP port:
//   took out what was only for the XAudio2 sound backend, which nothing has built since OpenAL replaced it
// Modified 2026-11-08 by Melanikus for the AvP port:
//   an OpenAL stream's list of free buffers is only ever looked at with its lock held, and what its own thread and its user both
//   read is kept so that both can read it at once safely. Its code is in 3dc/avp/win95/openal_stream.cpp. GetBytesWritten, which
//   was never written or used, is gone

#ifndef _AudioStreaming_h_
#define _AudioStreaming_h_

#include <stdint.h>

enum
{
	AUDIOSTREAM_OK,
	AUDIOSTREAM_ERROR,
	AUDIOSTREAM_OUTOFMEMORY
};

#ifdef USE_OPENAL

#include <AL/al.h>
#include <pthread.h>
#include <deque>
#include <atomic>

struct sndSource;

void *CheckProcessedBuffers(void *argument);

// A stream: sound that comes in pieces while it plays, such as the level's music or a film's sound. Two threads use one at once. Its
// user, such as the music thread, writes each piece into a free buffer and gives it to OpenAL to play. The stream's own thread
// (CheckProcessedBuffers) takes back each buffer OpenAL has played, so it's free again.
class AudioStream
{
	private:
		uint32_t	_currentBuffer;
		uint32_t	_nChannels;
		uint32_t	_rate;
		uint32_t	_bytesPerSample;
		std::atomic<uint64_t> _totalSamplesWritten;
		ALuint		*_buffers;
		ALenum		_format;
		uint8_t		*_dataBuffers;
		uint32_t	_volume;

		// The buffers that are free to be written into. Both threads change this list, so nothing looks at it, not even at how long
		// it is, without _bufferMutex held. These three are the only way to it: each takes the lock, does its one thing, and lets go.
		std::deque<ALuint> _freeBufferQueue;

	public:
		AudioStream();
		~AudioStream();
		bool Init(uint32_t nChannels, uint32_t rate, uint32_t bitsPerSample, uint32_t bufferSize, uint32_t nBuffers);
		int32_t  Stop();
		int32_t  Play();
		int32_t  SetVolume(uint32_t volume);
		uint32_t GetVolume();
		int32_t  SetPan(uint32_t pan);
		uint32_t WriteData(uint8_t *audioData, uint32_t size);
		uint32_t GetNumFreeBuffers();
		uint64_t GetNumSamplesPlayed();
		uint64_t GetNumSamplesWritten();
		uint32_t GetWritableBufferSize();
		uint32_t GetBufferSize();
		void WaitForFreeBuffer();

		// The free buffers, with the lock held (see _freeBufferQueue above). With no lock made, because Init stopped early, there are
		// no free buffers, and nothing is taken or given back.
		bool TakeFreeBuffer(ALuint *buffer);   // takes the oldest free buffer, or says there's none
		void GiveBackBuffer(ALuint buffer);    // a buffer OpenAL has played is free again
		uint32_t CountFreeBuffers();

		// what Init got as far as making, so that letting go of a stream only lets go of what there is
		bool _threadStarted;
		bool _mutexMade;

		uint32_t	_bufferCount;
		sndSource	*_source;
		pthread_t	_playbackThread;
		std::atomic<uint64_t> _totalBytesPlayed;   // added to by the stream's thread, read by the stream's user
		uint32_t	_bufferSize;
		pthread_mutex_t _bufferMutex;

		// Set by the stream's user and read by the stream's own thread, at the same time, so each is an atomic: a value two threads can
		// read and write at once without either seeing half of a change
		std::atomic<bool> _isPaused;
		std::atomic<bool> _isPlaying;
};

#endif

#ifdef _XBOX

#include <xtl.h>
#include <vector>

struct StreamingVoiceContext
{
	HANDLE hBufferEndEvent;

	void TriggerEvent()
	{
		SetEvent(hBufferEndEvent);
	}

	StreamingVoiceContext() : hBufferEndEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	{
	}
	virtual ~StreamingVoiceContext()
	{
		CloseHandle(hBufferEndEvent);
	}
};

class AudioStream
{
	private:
		uint32_t	_bufferSize;
		uint32_t	_bufferCount;
		uint32_t	_currentBuffer;
		uint32_t	_nChannels;
		uint32_t	_rate;
		uint32_t	_bytesPerSample;
		uint64_t	_totalSamplesWritten;
		bool		_isPaused;
		uint32_t    _volume;
		uint8_t	   *_buffers;
		std::vector<DWORD> _packetStatus;
		LPDIRECTSOUNDSTREAM _dsStreamBuffer;

	public:
		AudioStream();
		bool Init(uint32_t channels, uint32_t rate, uint32_t bitsPerSample, uint32_t bufferSize, uint32_t nBuffers);
		int32_t  Stop();
		int32_t  Play();
		int32_t  SetVolume(uint32_t volume);
		uint32_t GetVolume();
		int32_t  SetPan(uint32_t pan);
		uint32_t WriteData(uint8_t *audioData, uint32_t size);
		uint32_t GetNumFreeBuffers();
		uint64_t GetNumSamplesPlayed();
		uint64_t GetNumSamplesWritten();
		uint32_t GetWritableBufferSize();
		uint32_t GetBufferSize();
		void WaitForFreeBuffer();

		StreamingVoiceContext *_voiceContext;
		uint64_t _totalBytesPlayed;

		~AudioStream();
};

#endif

#endif
