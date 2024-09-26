/*
   playAudio.c 
   PortAudio v19 code for all actually outputting the audio 
   Depends on:
   playAudio.h
   BinauralBeat.h

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "BinauralBeat.h"
#include "playAudio.h"

//START Globals for Port Audio handling:
PaStream *playAudio_SoundStream = NULL; //this will equal NULL if the sound system wasn't detected, so use to check
PaDeviceIndex playAudio_SoundDevice = GNAURAL_USEDEFAULTSOUNDDEVICE;
PaStreamParameters outputStreamParams;

//################################################
//START PortAudio v19 code:

// ======================================
// PortAudio will call this local function:
static int playAudio_MyCallback (const void *inputBuffer,
                                 void *outputBuffer,
                                 unsigned long framesPerBuffer,
                                 const PaStreamCallbackTimeInfo * timeInfo,
                                 PaStreamCallbackFlags statusFlags,
                                 void *userData)
{
 if (playAudio_SoundStream != NULL)
 {
  BB_MainLoop (outputBuffer, (framesPerBuffer << 2));
 }
 // BB_UserSoundProc(outputBuffer, (framesPerBuffer<<2));//IMPORTANT: no matter what units the sndbuffer uses, CBinauralBeat wants sndbufsize in bytes
 return 0;      //return 1 to stop sound server
}

//======================================
void playAudio_SoundStart ()
{
 Pa_StartStream (playAudio_SoundStream);
}

// ======================================
void playAudio_GetAPI (int i)
{
 char tmpstr[64];
 const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo (i);

 if (NULL == deviceInfo)
 {
  BB_ERROUT ("Pa_GetDeviceInfo returned NULL"); //20100628: fixed a segfault when "Cannot connect to server socket err = No such file or directory"
  return;
 }

 const PaHostApiInfo *apiInfo = Pa_GetHostApiInfo (deviceInfo->hostApi);

 if (NULL == apiInfo)
 {
  BB_ERROUT ("CPa_GetHostApiInfo returned NULL");
  return;
 }

 strcpy (tmpstr, "Sound API: ");
 switch (apiInfo->type)
 {
 case paALSA:
  strcat (tmpstr, "ALSA");
  break;

 case paOSS:
  strcat (tmpstr, "OSS");
  break;

 case paJACK:
  strcat (tmpstr, "JACK");
  break;

 case paASIO:
  strcat (tmpstr, "ASIO");
  break;

 case paMME:
  strcat (tmpstr, "MME");
  break;

 case paDirectSound:
  strcat (tmpstr, "DirectSound");
  break;

 case paBeOS:
  strcat (tmpstr, "BeOS");
  break;

 case paAL:
  strcat (tmpstr, "AL");
  break;

 case paCoreAudio:
  strcat (tmpstr, "CoreAudio");
  break;

 case paSoundManager:
  strcat (tmpstr, "SoundManager");
  break;

 case paAudioScienceHPI:
  strcat (tmpstr, "AudioScienceHPI");
  break;

 case paWASAPI:
  strcat (tmpstr, "WASAPI");
  break;

 case paWDMKS:
  strcat (tmpstr, "WDMKS");
  break;

 case paInDevelopment:
  strcat (tmpstr, "In Development");
  break;

 default:
  strcat (tmpstr, "Unknown");
  break;
 }

 BB_DBGOUT (tmpstr);
}

//======================================
int playAudio_SoundInit (char *local_tmpstr)
{
 PaError err;
 int data;                      //Not using this currently

 BB_DBGOUT_STR ("Starting sound:", Pa_GetVersionText ());

 err = Pa_Initialize ();
 if (err != paNoError)
 {
  //let user know status of audio system:
  sprintf (local_tmpstr, "Audio System Init: %s", Pa_GetErrorText (err));
  return EXIT_FAILURE;
 }

 if (playAudio_SoundDevice == GNAURAL_USEDEFAULTSOUNDDEVICE)
 {
  playAudio_SoundDevice = Pa_GetDefaultOutputDevice ();
 }

 if (TRUE == BB_DebugFlag)
  playAudio_GetAPI (playAudio_SoundDevice);

 outputStreamParams.device = playAudio_SoundDevice;
 outputStreamParams.channelCount = 2;
 outputStreamParams.sampleFormat = paInt16;     // short -- use for inteleaved 32-bit stereo

 err = Pa_OpenStream (&playAudio_SoundStream, NULL, &outputStreamParams, GNAURAL_SAMPLE_RATE, GNAURAL_FRAMES_PER_BUFFER, paClipOff,     // we won't output out of range samples so don't bother clipping them
                      playAudio_MyCallback, &data);

 //let user know status of audio system:
 sprintf (local_tmpstr, "Init Audio System: %s [device %d]",
          Pa_GetErrorText (err), playAudio_SoundDevice);

 if (err != paNoError)
 {
  BB_ERROUT (local_tmpstr);
  return EXIT_FAILURE;
 }

 return EXIT_SUCCESS;
}       //end playAudio_SoundInit

// ======================================
void playAudio_SoundCleanup ()
{
 BB_DBGOUT ("Stopping PortAudio sound");
 if (playAudio_SoundStream != NULL)
 {
  Pa_StopStream (playAudio_SoundStream);
  Pa_CloseStream (playAudio_SoundStream);
 }
 Pa_Terminate ();
 playAudio_SoundStream = NULL;
 playAudio_SoundDevice = GNAURAL_USEDEFAULTSOUNDDEVICE;
 //all done with PortAudio
}

//END PortAudio v19 code
//################################################
