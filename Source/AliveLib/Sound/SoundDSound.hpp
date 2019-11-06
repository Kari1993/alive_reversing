#pragma once

#include "FunctionFwd.hpp"
#include "Sound/Sound.hpp"

#if !USE_SDL2_SOUND
signed int CC SND_CreateDS_DSound(unsigned int sampleRate, int bitsPerSample, int isStereo);
EXPORT int CC SND_Reload_DSound(SoundEntry* pSoundEntry, unsigned int sampleOffset, unsigned int size);
EXPORT void CC SND_Init_WaveFormatEx_4EEA00(WAVEFORMATEX *pWaveFormat, int sampleRate, unsigned __int8 bitsPerSample, int isStereo);
EXPORT signed int CC SND_Renew_4EEDD0(SoundEntry *pSnd);
EXPORT signed int CC SND_Reload_4EF1C0(const SoundEntry* pSnd, DWORD sampleOffset, unsigned char* pSoundBuffer, unsigned int sampleCount);
EXPORT signed int CC SND_New_4EEFF0(SoundEntry *pSnd, int sampleLength, int sampleRate, int bitsPerSample, int isStereo);
EXPORT char * CC SND_HR_Err_To_String_4EEC70(HRESULT hr);
#endif
