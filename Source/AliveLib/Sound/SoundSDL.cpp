#include "stdafx.h"
#include "SoundSDL.hpp"

#if USE_SDL2_SOUND

#include "Function.hpp"
#include "Error.hpp"
#include "Sound/Midi.hpp"
#include <mutex>
#include "Reverb.hpp"
#include "Sys.hpp"

#include <cinder/audio/dsp/RingBuffer.h>

#define MAX_VOICE_COUNT 1024

bool gReverbEnabled = false;
bool gAudioStereo = true;

static int gSoundBufferSamples = 256;
static int gCurrentSoundBufferSize = 0;
const int gMixVolume = 127;

SDLSoundBuffer* sAE_ActiveVoices[MAX_VOICE_COUNT] = {};

static SDL_AudioSpec gAudioDeviceSpec = {};
static AudioFilterMode gAudioFilterMode = AudioFilterMode::Linear;
static StereoSample_S16 * pTempSoundBuffer;
static StereoSample_S16 * pNoReverbBuffer;

void AddVoiceToActiveList(SDLSoundBuffer * pVoice)
{
    for (int i = 0; i < MAX_VOICE_COUNT; i++)
    {
        if (sAE_ActiveVoices[i] == nullptr)
        {
            sAE_ActiveVoices[i] = pVoice;
            return;
        }
    }

    LOG_WARNING("WARNING !!: Failed to allocate voice! No space left!");
}

void RemoveVoiceFromActiveList(SDLSoundBuffer * pVoice)
{
    for (int i = 0; i < MAX_VOICE_COUNT; i++)
    {
        if (sAE_ActiveVoices[i] == pVoice)
        {
            sAE_ActiveVoices[i] = nullptr;
            return;
        }
    }

    LOG_WARNING("WARNING !!: Could not find voice to remove!?!");
}

void AE_SDL_Audio_Generate(StereoSample_S16 * pSampleBuffer, int sampleBufferCount)
{
    // Check if our buffer size changes, and if its buffer, then resize the array
    if (sampleBufferCount > gCurrentSoundBufferSize)
    {
        if (pTempSoundBuffer != nullptr)
        {
            delete[] pTempSoundBuffer;
            pTempSoundBuffer = nullptr;
        }
        if (pNoReverbBuffer != nullptr)
        {
            delete[] pNoReverbBuffer;
            pNoReverbBuffer = nullptr;
        }
    }

    if (pTempSoundBuffer == nullptr)
    {
        pTempSoundBuffer = new StereoSample_S16[sampleBufferCount];
    }
    if (pNoReverbBuffer == nullptr)
    {
        pNoReverbBuffer = new StereoSample_S16[sampleBufferCount];
    }

    memset(pNoReverbBuffer, 0, sampleBufferCount * sizeof(StereoSample_S16));

    bool reverbPass = false;

    for (int vi = 0; vi < MAX_VOICE_COUNT; vi++)
    {
        SDLSoundBuffer * pVoice = sAE_ActiveVoices[vi];

        if (pVoice == nullptr || !pVoice->pBuffer)
        {
            continue;
        }

        if (pVoice->mState.bIsReleased)
        {
            pVoice->Destroy();
            continue;
        }

        // Clear Temp Sample Buffer
        memset(pTempSoundBuffer, 0, sampleBufferCount * sizeof(StereoSample_S16));

        Sint16* pVoiceBufferPtr = reinterpret_cast<Sint16*>(pVoice->GetBuffer()->data());

        bool loopActive = true;
        for (int i = 0; i < sampleBufferCount && pVoice->pBuffer && loopActive; i++)
        {
            if (!pVoice->pBuffer || pVoice->mState.eStatus != AE_SDL_Voice_Status::Playing || pVoice->mState.iSampleCount == 0)
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
                pTempSoundBuffer[i].left = static_cast<signed short>((pSample.left * pVoice->mState.iVolume) / 127);
                pTempSoundBuffer[i].right = static_cast<signed short>((pSample.right * pVoice->mState.iVolume) / 127);

                pVoice->mState.fPlaybackPosition += pVoice->mState.fFrequency;
            }
            else
            {
                reverbPass = true;

                int s = 0;

                switch (gAudioFilterMode)
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

                pTempSoundBuffer[i].left = static_cast<signed short>((((s * leftPan) / 10000) * pVoice->mState.iVolume) / 127);
                pTempSoundBuffer[i].right = static_cast<signed short>((((s * rightPan) / 10000) * pVoice->mState.iVolume) / 127);

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
            SDL_MixAudioFormat(reinterpret_cast<Uint8 *>(pSampleBuffer), reinterpret_cast<Uint8 *>(pTempSoundBuffer), AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), 45);
        }
        else
        {
            SDL_MixAudioFormat(reinterpret_cast<Uint8 *>(pNoReverbBuffer), reinterpret_cast<Uint8 *>(pTempSoundBuffer), AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), 45);
        }
    }

    // Do Reverb Pass

    if (gReverbEnabled)
    {
        Reverb_Mix(pSampleBuffer, AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), gMixVolume);
    }

    // Mix our no reverb buffer
    SDL_MixAudioFormat(reinterpret_cast<Uint8 *>(pSampleBuffer), reinterpret_cast<Uint8 *>(pNoReverbBuffer), AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), gMixVolume);

    //delete[] tempBuffer;
    //delete[] noReverbBuffer;
}

void AE_SDL_Audio_Callback(void * /*userdata*/, Uint8 *stream, int len)
{
    memset(stream, 0, len);
    AE_SDL_Audio_Generate(reinterpret_cast<StereoSample_S16 *>(stream), len / sizeof(StereoSample_S16));

    // Frag code    
    /*const int lenStereo = len / sizeof(StereoSample_S16);
    const int fragSize = 128;
    const int fragments = lenStereo / fragSize;

    for (int i = 0; i < fragments; i++)
    {
        AE_SDL_Audio_Generate(reinterpret_cast<StereoSample_S16 *>(stream + (i * fragSize * sizeof(StereoSample_S16))), fragSize);
    }*/
}

void AE_SDL_AudioShutdown()
{
    // Todo: clean up audio + reverb buffers
}

SDLSoundBuffer::SDLSoundBuffer()
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

    AddVoiceToActiveList(this);
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
    mState.fFrequency = frequency / static_cast<float>(gAudioDeviceSpec.freq);
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
    *freq = static_cast<DWORD>(mState.fFrequency * gAudioDeviceSpec.freq);
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

    RemoveVoiceFromActiveList(this);
    delete this;
}

std::vector<BYTE>* SDLSoundBuffer::GetBuffer()
{
    return pBuffer.get();
}

int SDLSoundBuffer::Duplicate(SDLSoundBuffer ** dupePtr)
{
    SDLSoundBuffer * dupe = new SDLSoundBuffer();
    memcpy(&dupe->mState, &this->mState, sizeof(AE_SDL_Voice_State));
    dupe->pBuffer = this->pBuffer;
    *dupePtr = dupe;
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

signed int CC SND_CreateDS_SDL(unsigned int /*sampleRate*/, int /*bitsPerSample*/, int /*isStereo*/)
{
    sDSound_BBC344 = new SDLSoundSystem();

    if (!SDL_Init(SDL_INIT_AUDIO))
    {
        for (int i = 0; i < SDL_GetNumAudioDrivers(); i++)
        {
            LOG_INFO("SDL Audio Driver " << i << " " << SDL_GetAudioDriver(i));
        }

        gAudioDeviceSpec.callback = AE_SDL_Audio_Callback;
        gAudioDeviceSpec.format = AUDIO_S16;
        gAudioDeviceSpec.channels = 2;
        gAudioDeviceSpec.freq = 44100;
        gAudioDeviceSpec.samples = static_cast<Uint16>(gSoundBufferSamples);
        gAudioDeviceSpec.userdata = NULL;

        if (SDL_OpenAudio(&gAudioDeviceSpec, NULL) < 0) 
        {
            LOG_ERROR("Couldn't open SDL audio: " << SDL_GetError());
        }
        else
        {
            LOG_INFO("-----------------------------");
            LOG_INFO("Audio Device opened, got specs:");
            LOG_INFO(
                "Channels: " << gAudioDeviceSpec.channels << " " <<
                "nFormat: " << gAudioDeviceSpec.format << " " <<
                "nFreq: " << gAudioDeviceSpec.freq << " " <<
                "nPadding: " << gAudioDeviceSpec.padding << " " <<
                "nSamples: " << gAudioDeviceSpec.samples << " " <<
                "nSize: " << gAudioDeviceSpec.size << " " <<
                "nSilence: " << gAudioDeviceSpec.silence);
            LOG_INFO("-----------------------------");

            gSoundBufferSamples = gAudioDeviceSpec.samples;

            Reverb_Init(gAudioDeviceSpec.freq);

            memset(&sAE_ActiveVoices, 0, sizeof(sAE_ActiveVoices));

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

            return 0;
        }
    }

    return 0;
}


// TODO: Consolidate this
EXPORT char * CC SND_HR_Err_To_String_4EEC70(long)
{
    return "unknown error";
}

#endif
