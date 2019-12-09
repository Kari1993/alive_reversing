#include "stdafx.h"
#include "SoundSDL.hpp"

#if USE_SDL2_SOUND

#include "Function.hpp"
#include "Error.hpp"
#include "Sound/Midi.hpp"
#include <mutex>
#include <list>
#include "Reverb.hpp"
#include "Sys.hpp"

#include <cinder/audio/dsp/RingBuffer.h>

bool gReverbEnabled = false;
bool gAudioStereo = true;

std::list<SDLSoundBuffer*> gBuffers;

SDLSoundBuffer::SDLSoundBuffer(const DSBUFFERDESC& bufferDesc, int soundSysFreq)
    : mSoundSysFreq(soundSysFreq)
{
    mState.iVolume = 0;
    mState.iVolumeTarget = 127;
    mState.bVolDirty = false;
    mState.iPan = 0;
    mState.fFrequency = 1.0f;
    mState.bIsReleased = false;
    mState.bLoop = false;
    mState.iChannels = 1;
    mState.fPlaybackPosition = 0;
    mState.eStatus = AE_SDL_Voice_Status::Stopped;


    mState.iSampleCount = bufferDesc.dwBufferBytes / 2;
    mBuffer = std::make_unique<std::vector<BYTE>>(bufferDesc.dwBufferBytes);
    mState.iBlockAlign = bufferDesc.lpwfxFormat->nBlockAlign;
    mState.iChannels = bufferDesc.lpwfxFormat->nChannels;

    gBuffers.push_back(this);
}


SDLSoundBuffer::SDLSoundBuffer(const SDLSoundBuffer& rhs)
{
    // Use assignment operator
    *this = rhs;
}


SDLSoundBuffer::~SDLSoundBuffer()
{
    gBuffers.remove(this);
}

SDLSoundBuffer& SDLSoundBuffer::operator=(const SDLSoundBuffer& rhs)
{
    if (this != &rhs)
    {
        mState = rhs.mState;
        mBuffer = rhs.mBuffer;
        mSoundSysFreq = rhs.mSoundSysFreq;
    }
    return *this;
}

HRESULT SDLSoundBuffer::SetVolume(int volume)
{
    mState.iVolumeTarget = volume;

    if (!mState.bVolDirty)
    {
        mState.iVolume = mState.iVolumeTarget;
    }

    mState.bVolDirty = true;

    return S_OK;
}

HRESULT SDLSoundBuffer::Play(int, int, int flags)
{
    mState.fPlaybackPosition = 0;
    mState.eStatus = AE_SDL_Voice_Status::Playing;

    if (flags & DSBPLAY_LOOPING)
    {
        mState.bLoop = true;
    }

    return S_OK;
}

HRESULT SDLSoundBuffer::Stop()
{
    mState.eStatus = AE_SDL_Voice_Status::Stopped;

    return S_OK;
}

HRESULT SDLSoundBuffer::SetFrequency(int frequency)
{
    mState.fFrequency = frequency / static_cast<float>(mSoundSysFreq);
    return S_OK;
}

HRESULT SDLSoundBuffer::SetCurrentPosition(int position) // This offset is apparently in bytes
{
    mState.fPlaybackPosition = static_cast<float>(position / mState.iBlockAlign);
    return S_OK;
}

HRESULT SDLSoundBuffer::GetCurrentPosition(DWORD * readPos, DWORD * writePos)
{
    *readPos = static_cast<DWORD>(mState.fPlaybackPosition * mState.iBlockAlign);
    *writePos = 0;

    return S_OK;
}

HRESULT SDLSoundBuffer::GetFrequency(DWORD * freq)
{
    *freq = static_cast<DWORD>(mState.fFrequency * mSoundSysFreq);
    return S_OK;
}

HRESULT SDLSoundBuffer::SetPan(signed int pan)
{
    mState.iPan = pan;
    return S_OK;
}

void SDLSoundBuffer::Release()
{
    mState.bIsReleased = true;
}

HRESULT SDLSoundBuffer::GetStatus(DWORD * r)
{
    if (mState.eStatus == AE_SDL_Voice_Status::Playing)
    {
        *r |= DSBSTATUS_PLAYING;
    }
    if (mState.bLoop)
    {
        *r |= DSBSTATUS_LOOPING;
    }
    if (mState.bIsReleased)
    {
        *r |= DSBSTATUS_TERMINATED;
    }
    return S_OK;
}

void SDLSoundBuffer::Destroy()
{
    // remove self from global list and
    // decrement shared mem ptr to audio buffer
    delete this;
}

std::shared_ptr<std::vector<BYTE>>  SDLSoundBuffer::GetBuffer()
{
    return mBuffer;
}

int SDLSoundBuffer::Duplicate(SDLSoundBuffer** dupePtr)
{
    *dupePtr = new SDLSoundBuffer(*this);
    return 0;
}

// Exoddus Functions:

int CC SND_Clear_SDL(SoundEntry* pSoundEntry, unsigned int sampleOffset, unsigned int size)
{
    const DWORD alignedOffset = sampleOffset * pSoundEntry->field_1D_blockAlign;
    const DWORD alignedSize = size * pSoundEntry->field_1D_blockAlign;

    // TODO: Should only clear from offset to size ??
    memset(pSoundEntry->field_4_pDSoundBuffer->GetBuffer()->data(), 0, pSoundEntry->field_14_buffer_size_bytes);

    return 0;
}

signed int CC SND_LoadSamples_SDL(const SoundEntry* pSnd, DWORD sampleOffset, unsigned char* pSoundBuffer, unsigned int sampleCount)
{
    const int offsetBytes = sampleOffset * pSnd->field_1D_blockAlign;
    const unsigned int bufferSizeBytes = sampleCount * pSnd->field_1D_blockAlign;
    memcpy(reinterpret_cast<Uint8*>(pSnd->field_4_pDSoundBuffer->GetBuffer()->data()) + offsetBytes, pSoundBuffer, bufferSizeBytes);
    return 0;
}

signed int CC SND_CreateDS_SDL(unsigned int sampleRate, int bitsPerSample, int isStereo)
{
    sDSound_BBC344 = new SDLSoundSystem();
    sDSound_BBC344->Init(sampleRate, bitsPerSample, isStereo);
    return 0;
}


// TODO: Consolidate this
EXPORT char * CC SND_HR_Err_To_String_4EEC70(long)
{
    return "unknown error";
}

#endif

void SDLSoundSystem::Init(unsigned int /*sampleRate*/, int /*bitsPerSample*/, int /*isStereo*/)
{
    if (SDL_Init(SDL_INIT_AUDIO) != 0)
    {
        LOG_ERROR("SDL_Init(SDL_INIT_AUDIO) failed " << SDL_GetError());
        return;
    }

    for (int i = 0; i < SDL_GetNumAudioDrivers(); i++)
    {
        LOG_INFO("SDL Audio Driver " << i << " " << SDL_GetAudioDriver(i));
    }

    mAudioDeviceSpec.callback = SDLSoundSystem::AudioCallBackStatic;
    mAudioDeviceSpec.format = AUDIO_S16;
    mAudioDeviceSpec.channels = 2;
    mAudioDeviceSpec.freq = 44100;
    mAudioDeviceSpec.samples = 256;
    mAudioDeviceSpec.userdata = this;

    if (SDL_OpenAudio(&mAudioDeviceSpec, NULL) < 0)
    {
        LOG_ERROR("Couldn't open SDL audio: " << SDL_GetError());
        return;
    }

    LOG_INFO("-----------------------------");
    LOG_INFO("Audio Device opened, got specs:");
    LOG_INFO(
        "Channels: " << mAudioDeviceSpec.channels << " " <<
        "nFormat: " << mAudioDeviceSpec.format << " " <<
        "nFreq: " << mAudioDeviceSpec.freq << " " <<
        "nPadding: " << mAudioDeviceSpec.padding << " " <<
        "nSamples: " << mAudioDeviceSpec.samples << " " <<
        "nSize: " << mAudioDeviceSpec.size << " " <<
        "nSilence: " << mAudioDeviceSpec.silence);
    LOG_INFO("-----------------------------");

    Reverb_Init(mAudioDeviceSpec.freq);

    SDL_PauseAudio(0);

    SND_InitVolumeTable_4EEF60();

    if (sLoadedSoundsCount_BBC394)
    {
        for (int i = 0; i < 256; i++)
        {
            if (sSoundSamples_BBBF38[i])
            {
                SND_Renew_4EEDD0(sSoundSamples_BBBF38[i]);
                SND_LoadSamples_4EF1C0(sSoundSamples_BBBF38[i], 0, sSoundSamples_BBBF38[i]->field_8_pSoundBuffer, sSoundSamples_BBBF38[i]->field_C_buffer_size_bytes / (unsigned __int8)sSoundSamples_BBBF38[i]->field_1D_blockAlign);
                if ((i + 1) == sLoadedSoundsCount_BBC394)
                    break;
            }
        }
    }

    sLastNotePlayTime_BBC33C = SYS_GetTicks();
}

void SDLSoundSystem::AudioCallBack(Uint8 *stream, int len)
{
    memset(stream, 0, len);
    RenderAudio(reinterpret_cast<StereoSample_S16 *>(stream), len / sizeof(StereoSample_S16));
}

void SDLSoundSystem::RenderAudio(StereoSample_S16* pSampleBuffer, int sampleBufferCount)
{
    // Check if our buffer size changes, and if its buffer, then resize the array
    if (sampleBufferCount > mCurrentSoundBufferSize)
    {
        if (mTempSoundBuffer != nullptr)
        {
            delete[] mTempSoundBuffer;
            mTempSoundBuffer = nullptr;
        }
        if (mNoReverbBuffer != nullptr)
        {
            delete[] mNoReverbBuffer;
          
            mNoReverbBuffer = nullptr;
        }
        mCurrentSoundBufferSize = sampleBufferCount;
    }

    if (mTempSoundBuffer == nullptr)
    {
        mTempSoundBuffer = new StereoSample_S16[sampleBufferCount];
    }
    if (mNoReverbBuffer == nullptr)
    {
        mNoReverbBuffer = new StereoSample_S16[sampleBufferCount];
    }

    memset(mNoReverbBuffer, 0, sampleBufferCount * sizeof(StereoSample_S16));

    auto tmpList = gBuffers;
    //for (int i = 0; i < 256; i++)
    for (auto entry : tmpList)
    {
        if (entry)
        {
            RenderSoundBuffer(*entry, pSampleBuffer, sampleBufferCount);
        }
    }

    // Do Reverb Pass

    if (gReverbEnabled)
    {
        Reverb_Mix(pSampleBuffer, AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), kMixVolume);
    }

    // Mix our no reverb buffer
    SDL_MixAudioFormat(reinterpret_cast<Uint8 *>(pSampleBuffer), reinterpret_cast<Uint8 *>(mNoReverbBuffer), AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), kMixVolume);

    //delete[] tempBuffer;
    //delete[] noReverbBuffer;
}


void SDLSoundSystem::RenderSoundBuffer(SDLSoundBuffer& entry, StereoSample_S16* pSampleBuffer, int sampleBufferCount)
{
    bool reverbPass = false;

    SDLSoundBuffer* pVoice = &entry;

    if (pVoice == nullptr || !pVoice->mBuffer || pVoice->mBuffer->empty())
    {
        return;
    }

    if (pVoice->mState.bIsReleased)
    {
        pVoice->Destroy(); // TODO: Still correct ??
        return;
    }

    // Clear Temp Sample Buffer
    memset(mTempSoundBuffer, 0, sampleBufferCount * sizeof(StereoSample_S16));

    Sint16* pVoiceBufferPtr = reinterpret_cast<Sint16*>(pVoice->GetBuffer()->data());

    bool loopActive = true;
    for (int i = 0; i < sampleBufferCount && pVoice->mBuffer && loopActive; i++)
    {
        if (pVoice->mBuffer->empty() || pVoice->mState.eStatus != AE_SDL_Voice_Status::Playing || pVoice->mState.iSampleCount == 0)
        {
            loopActive = false;
            continue;
        }

        if (pVoice->mState.iVolume < pVoice->mState.iVolumeTarget)
        {
            pVoice->mState.iVolume++;
        }
        else if (pVoice->mState.iVolume > pVoice->mState.iVolumeTarget)
        {
            pVoice->mState.iVolume--;
        }

        if (pVoice->mState.iChannels == 2)
        {
            reverbPass = false; // Todo: determine this with flags in the sound object itself.
                                // For Stereo buffers. The only time this is played is for FMV's.
                                // Right now, unless the playback device is at 44100 hz, it sounds awful.
                                // TODO: Resampling for stereo

            StereoSample_S16 pSample = reinterpret_cast<StereoSample_S16*>(pVoiceBufferPtr)[static_cast<int>(pVoice->mState.fPlaybackPosition)];
            mTempSoundBuffer[i].left = static_cast<signed short>((pSample.left * pVoice->mState.iVolume) / 127);
            mTempSoundBuffer[i].right = static_cast<signed short>((pSample.right * pVoice->mState.iVolume) / 127);

            pVoice->mState.fPlaybackPosition += pVoice->mState.fFrequency;
        }
        else
        {
            reverbPass = true;

            int s = 0;

            switch (mAudioFilterMode)
            {
            case AudioFilterMode::NoFilter:
                s = pVoiceBufferPtr[static_cast<int>(pVoice->mState.fPlaybackPosition)];
                break;
            case AudioFilterMode::Linear:
                const signed short s1 = pVoiceBufferPtr[static_cast<int>(pVoice->mState.fPlaybackPosition)];
                const signed short s2 = pVoiceBufferPtr[(static_cast<int>(pVoice->mState.fPlaybackPosition) + 1) % pVoice->mState.iSampleCount];

                s = static_cast<int>((s1 + ((s2 - s1) * (pVoice->mState.fPlaybackPosition - floorf(pVoice->mState.fPlaybackPosition)))));
                break;
            }

            signed int leftPan = 10000;
            signed int rightPan = 10000;

            if (gAudioStereo)
            {
                if (pVoice->mState.iPan < 0)
                {
                    rightPan = 10000 - abs(pVoice->mState.iPan);
                }
                else if (pVoice->mState.iPan > 0)
                {
                    leftPan = 10000 - abs(pVoice->mState.iPan);
                }
            }
            else
            {
                signed int r = (leftPan + rightPan) / 2;
                leftPan = r;
                rightPan = r;
            }

            mTempSoundBuffer[i].left = static_cast<signed short>((((s * leftPan) / 10000) * pVoice->mState.iVolume) / 127);
            mTempSoundBuffer[i].right = static_cast<signed short>((((s * rightPan) / 10000) * pVoice->mState.iVolume) / 127);

            pVoice->mState.fPlaybackPosition += pVoice->mState.fFrequency;
        }

        if (pVoice->mState.fPlaybackPosition >= pVoice->mState.iSampleCount / pVoice->mState.iChannels)
        {
            if (pVoice->mState.bLoop)
            {
                // Restart playback for loop.
                pVoice->mState.fPlaybackPosition = 0;
            }
            else
            {
                pVoice->mState.fPlaybackPosition = 0;
                pVoice->mState.eStatus = AE_SDL_Voice_Status::Stopped;
            }
        }
    }

    if (reverbPass)
    {
        SDL_MixAudioFormat(reinterpret_cast<Uint8 *>(pSampleBuffer), reinterpret_cast<Uint8 *>(mTempSoundBuffer), AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), 45);
    }
    else
    {
        SDL_MixAudioFormat(reinterpret_cast<Uint8 *>(mNoReverbBuffer), reinterpret_cast<Uint8 *>(mTempSoundBuffer), AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), 45);
    }
}

void SDLSoundSystem::AudioCallBackStatic(void* userdata, Uint8 *stream, int len)
{
    static_cast<SDLSoundSystem*>(userdata)->AudioCallBack(stream, len);
}
