/*
	MODPlay
	Copyright (C) 2024  Michal Procházka

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MUSICPLAY_H_INCLUDED
#define MUSICPLAY_H_INCLUDED

#include <SDL2/SDL.h>
#include <stdint.h>
#include "xm.h"

#define SAMPLERATE 44100
#define AUDIO_BUFFERSIZE 8192 // Puffer reicht für ((8192 / 44100) * 1000) / 2 = ca. 90 ms, durch 2, da Puffer für beide Kanäle

#define MODULE_TYPE_UNKNOWN 0 // unbekannter Modultyp
#define MODULE_TYPE_MOD 1	  // MOD
#define MODULE_TYPE_XM 2	  // XM (Extended Module)
#define MODULE_TYPE_SID 3	  // SID (C64)

typedef struct
{
	int nAvailableSongs; // Anzahl der verfügbaren Songs (mod/xm)
	SDL_AudioDeviceID audio_device;
	SDL_AudioSpec sdl_audio;
	short audiobuffer[AUDIO_BUFFERSIZE];	// Puffer für beide Kanäle
	float xm_audiobuffer[AUDIO_BUFFERSIZE]; // Zwischenpuffer für Extended Module
	uint8_t *pMusicStart;
	int nMusicSize;
	int nMusicIndex;
	int nNextMusicIndex;		 // wird nur in main() zum Umschalten der Musik verwendet
	uint8_t *pMusicAll;			 // Alle Songs in einem Speicherblock
	uint8_t *pTheMusic;			 // Zeiger auf Kopie eines Songs, da durch das Abspielen die Daten verändert werden
	xm_context_t *pCtxXm;		 // Context für XM-Player
	int nModulType;				 // Modul-Typ (MOD, XM) oder unbekannt
	uint8_t uMusicVolumePercent; // Musiklautstärke in Prozent
} AUDIOPLAYER;

void InitMusicPointer(void);
int InitAudioplayerStruct(void);
int SetMusic(int nMusicIndex);
int PlayMusic(void);
void CheckMusicSwitch(const Uint8 *pKeyboardArray);
void SetMusicVolume(uint8_t uVolumePercent);

// Ab hier MODPlay

#ifndef CHANNELS
#define CHANNELS 32
#endif

typedef struct
{
	const int8_t *sample;
	uint32_t age;
	uint32_t currentptr;
	uint32_t length;
	uint32_t looplength;
	uint32_t period;
	int32_t volume;
	int32_t currentsubptr; // only lower 16 bits are used in generation
	int8_t muted;
} PaulaChannel_t;

typedef struct
{
	const int8_t *data;
	uint16_t actuallength;
	uint16_t looplength;
} Sample_t;

typedef struct
{
	int32_t val;
	uint8_t waveform;
	uint8_t phase;
	uint8_t speed;
	uint8_t depth;
} Oscillator_t;

typedef struct
{
	uint32_t note;
	uint8_t sample, eff, effval;

	uint8_t slideamount, sampleoffset;
	short volume;
	int32_t slidenote;

	int32_t period;

	Oscillator_t vibrato, tremolo;
	PaulaChannel_t samplegen;
} TrackerChannel_t;

typedef struct /*__attribute__((packed))*/
{
	char name[22];
	uint8_t lengthhi;
	uint8_t lengthlo;
	uint8_t finetune;
	uint8_t volume;
	uint8_t looppointhi;
	uint8_t looppointlo;
	uint8_t looplengthhi;
	uint8_t looplengthlo;
} SampleHeader_t;

typedef struct
{
	int channels, orders, maxpattern, order, row, tick, maxtick, speed,
		skiporderrequest, skiporderdestrow,
		patlooprow, patloopcycle;

	uint32_t samplerate, paularate, audiospeed, audiotick, random;

	TrackerChannel_t ch[CHANNELS];

	const uint8_t *patterndata, *ordertable;
	const SampleHeader_t *sampleheaders;
	Sample_t samples[31];
	uint8_t uVolumePercent; // MIK
} ModPlayerStatus_t;

ModPlayerStatus_t *InitMOD(const uint8_t *mod, uint32_t samplerate);
ModPlayerStatus_t *RenderMOD(short *buf, int len);
ModPlayerStatus_t *ProcessMOD(void);

#endif // MUSICPLAY_H_INCLUDED
