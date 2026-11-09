// The game's OpenAL sound streams: sound that comes in pieces while it plays, such as a level's music or a film's sound.
// AvP port by Barry Duncan.
// Moved here from openal_audio.cpp, as it was, on 2026-11-08 by Melanikus for the AvP port, so that tests/audio_stream_test.cpp can
// build it with a stand-in for OpenAL and two real threads.
//
// Modified 2026-11-08 by Melanikus for the AvP port:
//   the list of free buffers is only ever looked at with its lock held. Before, its length was read without the lock in five places
//   while the stream's own thread changed it, and C++ makes no promise about what reading a list while another thread changes it does.
//   Whether the stream is playing or paused, and how much it has played and written, are atomics, because both threads use them;
//   a buffer OpenAL couldn't hand back isn't counted as free; how much has been played is 0, not a division by 0, for a stream that
//   didn't start; SetVolume keeps the volume, so GetVolume says what it is
//   GetNumSamplesWritten and GetWritableBufferSize, which the game never calls, are gone, and so is the count of samples written,
//   which only the first of them read

#ifdef USE_OPENAL

#include "AudioStreaming.h"
#include "openal_shared.h"
#include "console.h"
#include <new>
#include <string.h>
#include <assert.h>
#include <windows.h>

// How long the stream's thread waits before it looks again: a sixtieth of a second
static const DWORD kQuantum = 1000 / 60;

AudioStream::AudioStream()
{
	_source = NULL;
	_bufferSize    = 0;
	_bufferCount   = 0;
	_currentBuffer = 0;
	_nChannels   = 0;
	_rate = 0;
	_bytesPerSample      = 0;
	_totalBytesPlayed    = 0;
	_isPaused  = false;
	_isPlaying = false;
	_buffers  = NULL;
	_dataBuffers = NULL;
	_format = 0;
	_volume = 0;
	_threadStarted = false;
	_mutexMade = false;
}

bool AudioStream::Init(uint32_t nChannels, uint32_t rate, uint32_t bitsPerSample, uint32_t bufferSize, uint32_t nBuffers)
{
	if (!OpenAL_SoundIsOn()) {
		return false;
	}

	_format = GetALFormat(bitsPerSample, nChannels);

	// allocate space for n AL buffers
	_buffers = new(std::nothrow) ALuint[nBuffers];
	if (NULL == _buffers) {
		Con_PrintError("Out of memory trying to create streaming audio AL buffer(s)");
		return false;
	}

	// allocate space where actual audio data goes
	_dataBuffers = new(std::nothrow) uint8_t[bufferSize * nBuffers];
	if (NULL == _dataBuffers) {
		Con_PrintError("Out of memory trying to create streaming audio data buffer");
		return false;
	}

	// clear OpenAL errors
	alGetError();

	alGenBuffers(nBuffers, _buffers);
	AlCheckError();
	_bufferCount = nBuffers;

	_source = OpenAL_FindFreeSource();
	if (!_source) {
		Con_PrintError("No free sources available for streaming audio");
		return false;
	}

	_bytesPerSample = bitsPerSample / 8;
	_nChannels = nChannels;
	_rate = rate;
	_bufferSize  = bufferSize;
	_bufferCount = nBuffers;

	// set this to true for threads to run
	_isPlaying = true;

	// The lock comes first, because even here every buffer goes into the list of free ones through it
	if (pthread_mutex_init(&_bufferMutex, NULL) != 0) {
		return false;
	}
	_mutexMade = true;

	for (uint32_t i = 0; i < nBuffers; i++) {
		GiveBackBuffer(_buffers[i]);
	}

	if (pthread_create(&_playbackThread, NULL, CheckProcessedBuffers, static_cast<void*>(this)) != 0) {
		return false;
	}
	_threadStarted = true;

	return true;
}

// ---- The list of free buffers. The only three things that look at it, each with the lock held for as long as it looks.

bool AudioStream::TakeFreeBuffer(ALuint *buffer)
{
	if (!_mutexMade) {
		return false;
	}

	pthread_mutex_lock(&_bufferMutex);

	bool const there = !_freeBufferQueue.empty();

	if (there) {
		*buffer = _freeBufferQueue.front();
		_freeBufferQueue.pop_front();
	}

	pthread_mutex_unlock(&_bufferMutex);

	return there;
}

void AudioStream::GiveBackBuffer(ALuint buffer)
{
	if (!_mutexMade) {
		return;
	}

	pthread_mutex_lock(&_bufferMutex);
	_freeBufferQueue.push_back(buffer);
	pthread_mutex_unlock(&_bufferMutex);
}

uint32_t AudioStream::CountFreeBuffers()
{
	if (!_mutexMade) {
		return 0;
	}

	pthread_mutex_lock(&_bufferMutex);
	uint32_t const count = static_cast<uint32_t>(_freeBufferQueue.size());
	pthread_mutex_unlock(&_bufferMutex);

	return count;
}

// ---- Writing and playing

// The stream's user calls this with the next piece of sound. It goes into a free buffer, if there is one, and to OpenAL to play.
// The buffer is taken from the list with the lock held, and then it belongs to this call alone, so the OpenAL calls after it need no
// lock of ours: OpenAL guards its own, and the stream's thread was already calling it at the same time as this before.
uint32_t AudioStream::WriteData(uint8_t *audioData, uint32_t size)
{
	assert (audioData);
	assert (size == _bufferSize);

	// clear OpenAL errors
	alGetError();

	ALuint buffer = 0;

	if (TakeFreeBuffer(&buffer)) {

		memcpy(&_dataBuffers[_currentBuffer * _bufferSize], audioData, size);

		alBufferData(buffer, _format, &_dataBuffers[_currentBuffer * _bufferSize], size, _rate);
		AlCheckError();

		// queue it
		alSourceQueueBuffers(_source->_alSource, 1, &buffer);
		AlCheckError();
	}

	_currentBuffer++;
	_currentBuffer %= _bufferCount;

	return size;
}

// Waits until there's room for more sound, or until the stream is stopped. There may never be room after that.
void AudioStream::WaitForFreeBuffer()
{
	while (CountFreeBuffers() == 0 && _isPlaying) {
		Sleep(20);
	}
}

// The stream's own thread: it takes back each buffer OpenAL has played, so it's free to be written into again
void *CheckProcessedBuffers(void *args)
{
	AudioStream *stream = static_cast<AudioStream*>(args);

	while (stream->_isPlaying)
	{
		if (!stream->_isPaused)
		{
			// clear OpenAL errors
			alGetError();

			ALint nProcessed = 0;
			alGetSourcei(stream->_source->_alSource, AL_BUFFERS_PROCESSED, &nProcessed);
			AlCheckError();

			if (nProcessed > 0 && stream->CountFreeBuffers() < stream->_bufferCount)
			{
				ALuint buffer = 0;
				alSourceUnqueueBuffers(stream->_source->_alSource, 1, &buffer);

				// Only a buffer OpenAL really handed back is free: on an error, buffer doesn't name one
				if (AlCheckError() == AL_NO_ERROR)
				{
					stream->GiveBackBuffer(buffer);
					stream->_totalBytesPlayed += stream->_bufferSize;
				}
			}
			else {
				// wait and try again
				::Sleep(kQuantum);
			}
		}
	}

	return 0;
}

uint32_t AudioStream::GetBufferSize()
{
	return _bufferSize;
}

uint32_t AudioStream::GetNumFreeBuffers()
{
	return CountFreeBuffers();
}

uint64_t AudioStream::GetNumSamplesPlayed()
{
	// A stream whose Init stopped early has no sample size yet, and has played nothing
	if (_bytesPerSample == 0 || _nChannels == 0) {
		return 0;
	}

	return ((_totalBytesPlayed / _bytesPerSample) / _nChannels);
}

int32_t AudioStream::SetVolume(uint32_t volume)
{
	// kept, so GetVolume says what it is. Before, it was never kept, and GetVolume always said 0
	_volume = volume;

	if (!_source)
	{
		return AUDIOSTREAM_OK;
	}

	alSourcef(_source->_alSource, AL_GAIN, OpenAL_StreamGain(volume));
	alGetError();

	return 1;
}

uint32_t AudioStream::GetVolume()
{
	return _volume;
}

int32_t AudioStream::SetPan(uint32_t pan)
{
	// todo: implement this function
	return AUDIOSTREAM_OK;
}

int32_t AudioStream::Stop()
{
	_isPlaying = false;

	// a stream that couldn't start has nothing to stop
	if (_source)
	{
		alSourceStop(_source->_alSource);
		alGetError();
	}

	return AUDIOSTREAM_OK;
}

int32_t AudioStream::Play()
{
	if (!_source)
	{
		return AUDIOSTREAM_OK;
	}

	alSourcePlay(_source->_alSource);
	AlCheckError();
	_isPaused = false;

	return AUDIOSTREAM_OK;
}

AudioStream::~AudioStream()
{
	Stop();

	// Init can stop at any step, so this only lets go of what it got as far as making
	if (_threadStarted)
	{
		pthread_join(_playbackThread, NULL);
	}

	if (_mutexMade)
	{
		pthread_mutex_destroy(&_bufferMutex);
	}

	if (_source)
	{
		alSourcei(_source->_alSource, AL_BUFFER, NULL);

		_source->_isFree = true;
		_source = NULL;
	}

	if (_buffers && _bufferCount)
	{
		alDeleteBuffers(_bufferCount, _buffers);
	}

	// clear the new-ed memory
	delete[] _buffers;
	delete[] _dataBuffers;
}

#endif // #ifdef USE_OPENAL
