#pragma once

#include "FunctionFwd.hpp"
#include "Sound/Sound.hpp"

#if !USE_SDL2_SOUND
signed int CC SND_CreateDS_DSound(unsigned int sampleRate, int bitsPerSample, int isStereo);
EXPORT int CC SND_Clear_DSound(SoundEntry* pSoundEntry, unsigned int sampleOffset, unsigned int size);
EXPORT signed int CC SND_LoadSamples_4EF1C0(const SoundEntry* pSnd, DWORD sampleOffset, unsigned char* pSoundBuffer, unsigned int sampleCount);
EXPORT char * CC SND_HR_Err_To_String_4EEC70(HRESULT hr);
#endif
