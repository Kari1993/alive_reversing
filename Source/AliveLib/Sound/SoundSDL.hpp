#pragma once

#include "Sound.hpp"
#include "Sound/Midi.hpp"

#if USE_SDL2_SOUND
#include "FunctionFwd.hpp"
#include "stdlib.hpp"
#include "SDL.h"
#include <atomic>

#define DSBSTATUS_PLAYING           0x00000001
#define DSBSTATUS_BUFFERLOST        0x00000002
#define DSBSTATUS_LOOPING           0x00000004
#define DSBSTATUS_TERMINATED        0x00000020

#define DSBPAN_LEFT                 -10000
#define DSBPAN_CENTER               0
#define DSBPAN_RIGHT                10000

#define DSBPLAY_LOOPING             0x00000001

#define DSBFREQUENCY_MIN            100
#define DSBFREQUENCY_MAX            200000

#define DSERR_BUFFERLOST            0x88780096


struct MIDI_Struct1;
struct SoundEntry;

template <typename T>
struct StereoSample
{
    T left;
    T right;
};

typedef StereoSample<signed short> StereoSample_S16;
typedef StereoSample<signed int> StereoSample_S32;
typedef StereoSample<float> StereoSample_F32;

enum AE_SDL_Voice_Status
{
    Stopped = 0,
    Paused = 1,
    Playing = 2,
};

enum AudioFilterMode
{
    NoFilter = 0,
    Linear = 1,
};

// An SDL implement of used IDirectSoundBuffer API's.
class AE_SDL_Voice
{

public:
    AE_SDL_Voice();

    int SetVolume(int volume);
    int Play(int reserved, int priority, int flags);
    int Stop();

    int SetFrequency(int frequency);
    int SetCurrentPosition(int position);
    int GetCurrentPosition(DWORD* readPos, DWORD* writePos);
    int GetFrequency(DWORD* freq);
    int SetPan(signed int pan);
    void Release();
    int GetStatus(DWORD * r);

    // Not part of the API
    void Destroy();
    
    std::vector<BYTE>* GetBuffer();
    int Duplicate(AE_SDL_Voice ** dupePtr);

public:
    struct AE_SDL_Voice_State
    {
        int iVolume;
        bool bVolDirty;
        int iVolumeTarget;
        float fFrequency;
        signed int iPan;

        AE_SDL_Voice_Status eStatus;
        bool bLoop;
        std::atomic<bool> bIsReleased;
        float fPlaybackPosition;

        int iSampleCount;
        int iChannels;
        int iBlockAlign;
    };

    AE_SDL_Voice_State mState;
    std::shared_ptr<std::vector<BYTE>> pBuffer;
    
};

// An SDL implementation of used IDirectSound API's
class SDLSoundSystem
{
public:
    int DuplicateSoundBuffer(AE_BUFFERTYPE* pDSBufferOriginal, AE_BUFFERTYPE** ppDSBufferDuplicate)
    {
        pDSBufferOriginal->Duplicate(ppDSBufferDuplicate);
        return 0;
    }

    int CreateSoundBuffer(LPCDSBUFFERDESC /*pcDSBufferDesc*/, AE_BUFFERTYPE** /*ppDSBuffer*/, void* /*pUnkOuter*/)
    {
        return 0;
    }

    int Release()
    {
        return 0;
    }
};

signed int CC SND_CreateDS_SDL(unsigned int /*sampleRate*/, int /*bitsPerSample*/, int /*isStereo*/);
int CC SND_Clear_SDL(SoundEntry* pSoundEntry, unsigned int sampleOffset, unsigned int size);
signed int CC SND_LoadSamples_SDL(const SoundEntry* pSnd, DWORD sampleOffset, unsigned char* pSoundBuffer, unsigned int sampleCount);
EXPORT char * CC SND_HR_Err_To_String_4EEC70(long hr);


#endif