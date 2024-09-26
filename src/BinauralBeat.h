/*
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

#ifndef _BINAURALBEAT_H_
#define _BINAURALBEAT_H_

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

//////VOICE TYPES////////////////////////////
//You can add new to the end, but NEVER change old ones; everything 
//depends on their consistency:
#define BB_VOICETYPE_BINAURALBEAT   0
#define BB_VOICETYPE_PINKNOISE      1
#define BB_VOICETYPE_PCM            2   //this will always refer internally to a short int array holding interleaved stereo 44.1khz data
#define BB_VOICETYPE_ISOPULSE       3
#define BB_VOICETYPE_ISOPULSE_ALT   4
#define BB_VOICETYPE_WATERDROPS     5   //20110516 takes the first encountered value of beatfreq_start_HALF to set drop number
#define BB_VOICETYPE_RAIN           6   //20110520 variation on WATERDROPS
////////////////////////////////////////////

#define BB_PRECISION_TYPE double
#define BB_AUDIOSAMPLERATE          44100       //not sure what happens if you need a samplerate other than 44100
#define BB_EVENT_NUM_OF_ELEMENTS    5   //this MUST exactly match number of "columns" in a "row" of event data
#define GNAURAL_USEDEFAULTSOUNDDEVICE -255

extern const unsigned int BB_COMPLETED;
extern const unsigned int BB_NEWLOOP;
extern const unsigned int BB_NEWENTRY;
extern const int BB_UPDATEPERIOD_SAMPLES; //larger the number, less frequently I do some computationally expensive stuff. Ex: 441 = every .01 sec. @ 44100khz
extern const BB_PRECISION_TYPE BB_TWO_PI;
extern const BB_PRECISION_TYPE BB_SAMPLE_FACTOR;
extern const int BB_SIN_SCALER; // factors max output of sin() to fit a short (0x3fff)

//Main Structures:

typedef struct {
    BB_PRECISION_TYPE duration;
    unsigned int AbsoluteStart_samples; //total sample count at start of entry. about 27 hours possible with uint
    unsigned int AbsoluteEnd_samples; //total sample count at end of entry. about 27 hours possible with uint
    BB_PRECISION_TYPE volL_start;
    BB_PRECISION_TYPE volL_end;
    BB_PRECISION_TYPE volL_spread;
    BB_PRECISION_TYPE volR_start;
    BB_PRECISION_TYPE volR_end;
    BB_PRECISION_TYPE volR_spread;
    //== Following names reflect BB_VOICETYPE_BINAURALBEAT, but get used for arbitrarily for other voice types:
    BB_PRECISION_TYPE basefreq_start;
    BB_PRECISION_TYPE basefreq_end;
    BB_PRECISION_TYPE basefreq_spread;
    BB_PRECISION_TYPE beatfreq_start_HALF; //for a Beat Freq of 1.0, you'd make this 0.5. Done for calculation speed
    BB_PRECISION_TYPE beatfreq_end_HALF; //.5 the Beat Freq of the next Event (or first event if there is no next).
    BB_PRECISION_TYPE beatfreq_spread_HALF; //this is the difference between start_HALF and end_HALF
    BB_PRECISION_TYPE LastPositionL;
    BB_PRECISION_TYPE LastPositionR;
} BB_EventData;

typedef struct {
    float count;
    float decrement;
    float stereoMix;
} BB_Waterdrop;

typedef struct {
    int id; // 0,1,2,...
    int type; //masks: BB_VOICETYPE_BINAURALBEAT, BB_VOICETYPE_PINKNOISE, BB_VOICETYPE_PCM
    int mute; //TRUE or FALSE
    int mono; //TRUE or FALSE [added 20100614]
    BB_PRECISION_TYPE TotalDuration; //NOTE: this is strictly the duration of this voice, which may or may not be the same as BB_TotalDuration
    int EntryCount;
    int CurEntry; //this will always hold the current entry being processed in voice
    BB_EventData *Entry;
    BB_PRECISION_TYPE CurVolL;
    BB_PRECISION_TYPE CurVolR;
    //the rest are all Voice-type specific data, to be used in any way appropriate for their kind of voice:
    BB_PRECISION_TYPE cur_basefreq; //BB_VOICETYPE_BINAURALBEAT: cur_basefreq;
    BB_PRECISION_TYPE cur_beatfreq; //BB_VOICETYPE_BINAURALBEAT: a freq snapshot in Hz of actual beat being generated
    BB_PRECISION_TYPE cur_beatfreqL_factor; //BB_VOICETYPE_BINAURALBEAT: cur_beatfreqL_factor;
    BB_PRECISION_TYPE cur_beatfreqR_factor; //BB_VOICETYPE_BINAURALBEAT: cur_beatfreqR_factor;
    int cur_beatfreq_phasesamplecount; //decremented to determine where in phase we are for binaural and isochronic (also calls user func if !=NULL)
    int cur_beatfreq_phasesamplecount_start; //updated each time a beatfreq changes to load cur_beatfreq_phasesamplecount
    int cur_beatfreq_phaseflag; //toggles between TRUE/FALSE every BB cycle (useful for triggering external stimuli)
    BB_PRECISION_TYPE cur_beatfreq_phaseenvelope; //BB_VOICETYPE_BINAURALBEAT: snapshot between 0 and 2 of phase data of BB frequency
    BB_PRECISION_TYPE sinPosL; //BB_VOICETYPE_BINAURALBEAT: sinPosL; phase info for left channel
    BB_PRECISION_TYPE sinPosR; //BB_VOICETYPE_BINAURALBEAT: sinPosR; phase info for right channel
    BB_PRECISION_TYPE sinL; //BB_VOICETYPE_BINAURALBEAT: sinL; instantaneous sin being used for the sample's left channel
    BB_PRECISION_TYPE sinR; //BB_VOICETYPE_BINAURALBEAT: sinR; instantaneous sin being used for the sample's right channel
    int noiseL; //BB_VOICETYPE_PINKNOISE: instantaneous noise value left sample
    int noiseR; //BB_VOICETYPE_PINKNOISE: instantaneous noise value left sample
    //== Following is only used for BB_VOICETYPE_PCM:
    int *PCM_samples; //this is an int array holding stereo 44.1khz data, created by user (and MUST BE free'd by user too). Set to NULL if it holds no data.
    unsigned int PCM_samples_size; //this is the number of elements in PCM_samples (in ints), also used for Waterdrops
    unsigned int PCM_samples_currentcount; //this holds current place in the array
    BB_Waterdrop *Drop; //20110516
} BB_VoiceData;

//END Main Structures

//===============The important variables=====================
extern BB_VoiceData *BB_Voice; // the biggie, used to load data
extern BB_PRECISION_TYPE BB_TotalDuration; //total runtime in seconds of longest voice - USER MUST BE SURE TO ZERO BB_TotalDuration if resetting all voices
extern unsigned int BB_CurrentSampleCount; //index of current sample count; can be set (to FF or RW through schedule)
extern unsigned int BB_CurrentSampleCountLooped; //This is not used internally; just a courtesy to code using BB, keeping big
extern int BB_VoiceCount; //can be read, but not set manually (use BB_InitVoices()).
extern unsigned int BB_InfoFlag;
extern int BB_LoopCount; //This IS used -- set to 1 to do one pass before BB sets BB_InfoFlag to BB_COMPLETED
extern int BB_Loops; //This is used whenever BB_Reset() is called, and sets BB_LoopCount (like when writing a WAV file, for instance);
extern const BB_PRECISION_TYPE BB_DefaultBBSched[]; //This is totally arbitrary, just to be sure something valid exists at start
extern BB_PRECISION_TYPE BB_VolumeOverall_left;
extern BB_PRECISION_TYPE BB_VolumeOverall_right;
extern int BB_StereoSwap; //set true to swap left and right stereo channels
extern int BB_Mono; //set true to mix and halve left and right stereo channels
extern unsigned int BB_FileByteCount; //ultimately keeps count of total count of bytes currently written
extern int BB_WriteStopFlag; // set to non-zero to stop a WAV file write
extern int BB_InCriticalLoopFlag; //a brutish way to not do anything to BB_Voice data while audio thread is accessing data

//IMPORTANT NOTE, new 20070831 -   BB_PauseFlag is mostly used to keep BB_MainLoop()'s thread from entering BB data while main 
//thread is creating it. USER MUST FALSE IT WHEN DONE CREATING THE DATA:
extern int BB_PauseFlag; //USER MUST FALSE THIS WHEN DONE CREATING THEIR DATA. It gets set TRUE whenever BB_InitVoices() is called
extern int BB_DebugFlag; //set to TRUE to dump debug info
extern void (*BB_UserSleep) (int ms); //user sets this to their own function offering short sleep; used when BB is waiting for locked data
extern int BB_PeakL; //20101014
extern int BB_PeakR; //20101014

//===============Function Declarations=====================
void BB_MainLoop(void *pSoundBuffer, long bufferLen);
int BB_InitVoices(int NumberOfVoices);
void BB_CleanupVoices();
int BB_CalibrateVoice(int VoiceID);
int BB_FixVoiceDurations(); //added 20070129 to deal with BB_TotalDuration weirdness if user illegally has different lengthed voices
int BB_DetermineTotalDuration(); //added 20070129 to deal with BB_TotalDuration weirdness, but BB_FixVoiceDurations() solved it
void BB_LoadDefaultVoice(int VoiceID);
int BB_WriteWAVFile(char *szFilename); //call this to write a WAV file from start of current schedule
int BB_WriteWAVToStream(FILE * stream); //user can call this if they want to send a WAV to any specific stream
void BB_WriteWAVHeaderToStream(FILE * stream); //internal use only
void BB_SeedRand(unsigned int i1, unsigned int i2);
int BB_Rand();
double BB_Rand_pm();
int BB_LoPass(int *staticvar, int randomnumber); //20110506, weird lo-pass filter essentially
int BB_PowerLaw(int randomnumber, int rand_max, int max_out);
void BB_NullAllPCMData(); //this is for the user; never called internally. when PCM_samples == NULL, it is silenced
void BB_Reset();
void BB_ResetAllVoices(); // zeros all CurEntry's
int BB_SetupVoice(int VoiceID, // Array index for a BB_Voice created by BB_InitVoices()
        int VoiceType, // A BB_VOICETYPE defined above
        int mute, //TRUE or FALSE
        int mono, //TRUE or FALSE [added 20100614]
        int NumberOfEvents); //how many events in your array
extern void (*BB_UserFunc) (int voice);
void BB_Water(int voice, short *array, int arraylen, float Lowcut);
short *BB_WaterInit(int arraylen, float pitch);
void BB_WaterVoiceInit(int voice);

#define BB_DBGFLAG if (TRUE == BB_DebugFlag)
#define BB_DBGOUT_STR(a,b)            BB_DBGFLAG  fprintf(stderr,"%s %d: %s %s\n",__FILE__,__LINE__,a, b);
#define BB_DBGOUT_INT(a,b)            BB_DBGFLAG  fprintf(stderr,"%s %d: %s %d\n",__FILE__,__LINE__,a, b);
#define BB_DBGOUT_DBL(a,b)            BB_DBGFLAG  fprintf(stderr,"%s %d: %s %g\n",__FILE__,__LINE__,a, b);
#define BB_DBGOUT_PNT(a,b)            BB_DBGFLAG  fprintf(stderr,"%s %d: %s %p\n",__FILE__,__LINE__,a,b);
#define BB_DBGOUT(a)                  BB_DBGFLAG  fprintf(stderr,"%s %d: %s\n",__FILE__,__LINE__,a);
#define BB_DBG()                      BB_DBGFLAG  fprintf(stderr,"%s %d\n",__FILE__,__LINE__);
#define BB_ERROUT(a)                  BB_DBGFLAG  fprintf(stderr,"%s %d: #Error# %s\n",__FILE__,__LINE__,a);

#endif
