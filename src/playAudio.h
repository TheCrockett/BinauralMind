/*
   playAudio.h 
   PortAudio v19 header file for playAudio.c
   Depends on:
   portaudio.h

   Copyright (C) 2008  Bret Logan

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

//IMPORTANT: PortAudio was used for multi-platform compatibility. PortAudio v18 
//still uses OSS, which is a problem because most linux systems now are ALSA, 
//and therefore only emulate OSS. And the drawback: ALSA doesn't mix OSS apps 
//well or at all. This means that a cheap soundcard with no onboard mixers 
//(most, actually), can only run one OSS a time (unlike real OSS, which did 
//software mixing).
//PortAudio v19 has ALSA in it; this file represents the last stable v18 
//before Gnaural switches to v19, which is sometimes called "PortAudio2"

#ifndef _PLAYAUDIO_H_
#define _PLAYAUDIO_H_

#include <portaudio.h>

#define GNAURAL_SAMPLE_RATE (BB_AUDIOSAMPLERATE)
//20081128: buffer reduced from 4096 to 1024 because MacOS didn't like it:
//20090107: turns out Win32 doesn't like 1024! Adding #ifdef for MacOSX and 
//          upping buffer for others to 8192 so Vista might stop stuttering:
#ifdef GNAURAL_MACOSX
#define GNAURAL_FRAMES_PER_BUFFER (1024)
#else
#define GNAURAL_FRAMES_PER_BUFFER (8192)
#endif
#define GNAURAL_OUTPUT_DEVICE Pa_GetDefaultOutputDeviceID()

//START Globals for Port Audio handling:
extern PaStream *playAudio_SoundStream; //this will equal NULL if the sound system wasn't detected, so use to check
extern PaDeviceIndex playAudio_SoundDevice;

extern int playAudio_SoundInit (char *result_str);      //pass it a string >= 1024
extern void playAudio_SoundStart ();
extern void playAudio_SoundCleanup ();

//NOTE: in main.c also has static int playAudio_MyCallback ();

#endif
