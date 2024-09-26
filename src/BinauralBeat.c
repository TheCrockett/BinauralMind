/*
   BinauralBeat.c
   Copyright (C) 2008  Bret Logan

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

//Purpose: BB loads an array with 32-bit mixed stereo PCM sound data,
//         specifically mixing any number of "voices" (which currently can
//         be either Pink Noise or a pair of stereo-separated pure sine
//         waves implementing the Binaural Beat principle).

//To use BB:
//  1) Set the number of Voices with BB_InitVoices(#);
//  2) Allot Entry memory for each voice alloted in step 1 by running BB_SetupVoice() on each (BB_LoadDefaultVoice() gives an example)
//  3) Load the entries alotted in step 2 for each voice in any way you see fit (see BB_LoadDefaultVoice() for example);
//  4) Run BB_CalibrateVoice() on each voice to load up the internal variables, set BB_LoopCount (1 for 1 pass, 0 for infinite);
//  5) Init your sound code (I usually use PortAudio), giving it BB_MainLoop() as the fillup callback
//  6) Start your sound routine, ideally running your sound in it's own thread; check BB_InfoFlag occasionally for state info if desired
//  7) When you are done with BB, cleanup all BB resources by running BB_CleanupVoices();
//That's it. In pseudo code form:
//START pseudo code
//    BB_InitVoices(2);
//    for (int i = 0; i < BB_VoiceCount; i++) {
//      BB_SetupVoice(i, BB_DefaultBBSched,
//                   (sizeof(BB_DefaultBBSched)/sizeof(BB_PRECISION_TYPE))/BB_EVENT_NUM_OF_ELEMENTS);
//    }
//    BB_Voice[0].type = BB_VOICETYPE_PINKNOISE;
//Put your sound code here, putting BB_MainLoop() in it's callback to create input. When done making sound:
//    BB_CleanupVoices();
//END pseudo code
//Notes:
// - IMPORTANT: when user reloads all voices in BB, they must be sure to zero BB_TotalDuration
//     before BB_CalibrateVoice is run, because the old BB_TotalDuration will persist
// - IMPORTANT: it is important that User sets up voices so that they are all the same TotalDuration,
//     because BB will set BB_InfoFlag to BB_COMPLETED and/or BB_NEWLOOP whenever *any* voice ends.
//     IOW, it is the responsibility of user to make all voices the same lenght in whatever way they see fit.
// - All volumes are in the range 0.0 to 1.0
// - You can start up sound and not worry that no data has been given
//    yet, since as long as BB_VoiceCount == 0, only 0's will be passed to sound engine.
//TODO:
// - make it so that noise at full volume doesn't break-up
// - figure out a way (union?) to have data members have names that sound relevant to their voices
// - decide whether they're called Entries or Events

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "BinauralBeat.h"

//these are globals defined in SoundEngine.h:
BB_VoiceData *BB_Voice = NULL;
const unsigned int BB_COMPLETED = 1;
const unsigned int BB_NEWLOOP = 2;
const unsigned int BB_NEWENTRY = 4;
const int BB_UPDATEPERIOD_SAMPLES = 16; //larger the number, less frequently I do some computationally expensive stuff. Ex: 441 = every .01 sec. @ 44100khz
const int BB_SIN_SCALER = 0x3FFF; // factors max output of sin() to fit a short (0x3fff)
const BB_PRECISION_TYPE BB_PI = 3.1415926535897932384626433832795;
const BB_PRECISION_TYPE BB_TWO_PI = 3.1415926535897932384626433832795 * 2.0;
const BB_PRECISION_TYPE BB_SAMPLE_FACTOR = ((3.1415926535897932384626433832795 * 2.0) / BB_AUDIOSAMPLERATE); // == 0.000142476 @ 44100.0khz
const BB_PRECISION_TYPE BB_AUDIOSAMPLERATE_HALF = .5 * BB_AUDIOSAMPLERATE;
BB_PRECISION_TYPE BB_TotalDuration = 0; //NOTE: User must be sure to zero BB_TotalDuration if resetting all voices
unsigned int BB_CurrentSampleCount = 0;
unsigned int BB_CurrentSampleCountLooped = 0;
unsigned int BB_InfoFlag = 0; //BB uses this only to send messages to the user, who must reset the messages.
int BB_ManualFreqOffsetControl = 0;
int BB_VoiceCount = 0;
int BB_Loops = 1; //user sets this to wherever BB_LoopCount should be set to in BB_Reset()
int BB_LoopCount = 1; // arbitrary right now; user should set this as desired
unsigned int BB_FileByteCount = 0;
int BB_WriteStopFlag = 0;
BB_PRECISION_TYPE BB_VolumeOverall_left = 1.0; // user can set this between 0.0 and 1.0, or negative to invert;
BB_PRECISION_TYPE BB_VolumeOverall_right = 1.0; // user can set this between 0.0 and 1.0, or negative to invert;
int BB_StereoSwap = 0; //set non-0 to swap left and right stereo channels
int BB_Mono = 0; //set non-0 to mix stereo channels
int BB_InCriticalLoopFlag = FALSE; //a brutish way to not do anything to BB_Voice data while audio thread is accessing data

//IMPORTANT NOTE, new 20070831 -   BB_PauseFlag is mostly used to keep BB_MainLoop()'s thread from entering BB data while main 
//thread is creating it. USER MUST FALSE IT WHEN DONE CREATING THE DATA:
int BB_PauseFlag = TRUE; //USER MUST FALSE THIS WHEN DONE CREATING THEIR DATA. It gets set TRUE whenever BB_InitVoices() is called
int BB_DebugFlag = FALSE;
void (*BB_UserSleep) (int microseconds) = NULL; //user sets this to their own function offering short sleep; used when BB is waiting for locked data
void (*BB_UserFunc) (int) = NULL; //user sets to their own function to cue external stimuli (gets called twice per Beat)
int BB_PeakL = 0; //20101014
int BB_PeakR = 0; //20101014

//start water stuff:
//size of waterdrop array:
#define BB_DROPLEN  8192
#define BB_RAINLEN  44
short *BB_DropMother = NULL; //20110516 this gets alloted 
short *BB_RainMother = NULL; //20110520 this gets alloted 
//end water stuff

// ======================================
//This is totally arbitrary, just to be sure something valid exists at start:
//beatL, beatR, Dur, Base, VolL, VolR:
const BB_PRECISION_TYPE BB_DefaultBBSched[] = {
    0, 1, 0, 0, .5,
    0, 1, 0, .5, 0,
    0, 1, 0, 0, .5,
    0, 1, 0, .5, 0
};

//======================================
//This gets called first, just before BB_SetupVoice:

int BB_InitVoices(int NumberOfVoices) {
    //20070806: Was doing this here... until I realized it gets called every re-loading!
    // BB_SeedRand (3676, 2676862);
    int i = 0;

    //a20070730: fixed critical memory leak -- was setting BB_VoiceCount = 0 BEFORE running BB_CleanupVoices()!
    if (BB_Voice != NULL) {
        while (BB_InCriticalLoopFlag == TRUE) {
            ++i;
            BB_DBGOUT_INT("Waiting to exit datalock until Cleanup:", i);
            if (NULL != BB_UserSleep) {
                BB_UserSleep(100000); // == 1/10th sec.
            }
        }
        BB_PauseFlag = TRUE; //added 20070803
        BB_CleanupVoices();
    }
    //20070803: Next line seems redundant, but it can be important: may not have gotten zero'd
    //in BB_CleanupVoices(), and BB_VoiceCount might be used by audio thread to
    //keep from accessing invalid BB data:
    BB_VoiceCount = 0;
    BB_Voice = (BB_VoiceData *) calloc(NumberOfVoices, sizeof (BB_VoiceData));
    if (BB_Voice == NULL) {
        return FALSE;
    }
    memset(BB_Voice, 0, NumberOfVoices * sizeof (BB_VoiceData));
    for (i = 0; i < NumberOfVoices; i++) {
        BB_Voice[i].Entry = NULL;
        BB_Voice[i].Drop = NULL; //20110516
        BB_Voice[i].id = 0; //looks like this is sort of arbitrary now, for user's use
        BB_Voice[i].mute = FALSE; //TRUE, FALSE
        BB_Voice[i].mono = FALSE; //TRUE, FALSE  [20100614]
        BB_Voice[i].type = BB_VOICETYPE_BINAURALBEAT; //masks: BB_VOICETYPE_BINAURALBEAT, BB_VOICETYPE_PINKNOISE
        BB_Voice[i].EntryCount = 0;
        BB_Voice[i].CurEntry = 0;
        BB_Voice[i].PCM_samples = NULL;
        BB_Voice[i].PCM_samples_size = 0; //this is the raw array size (in shorts) of the PCM_samples array (NOT frame count, which would be half this)
        BB_Voice[i].PCM_samples_currentcount = 0; //this holds current place in the array
        BB_Voice[i].cur_beatfreq = 0.0;
        BB_Voice[i].cur_beatfreq_phaseenvelope = 0.0;
        BB_Voice[i].cur_beatfreq_phaseflag = 0;
        BB_Voice[i].cur_beatfreq_phasesamplecount = 1;
        BB_Voice[i].cur_beatfreq_phasesamplecount_start = 1;
        BB_Voice[i].sinL = 0;
        BB_Voice[i].sinR = 0;
        BB_Voice[i].noiseL = 1;
        BB_Voice[i].noiseR = 1;
    }
    BB_VoiceCount = NumberOfVoices;
    return TRUE;
}

//======================================
//This gets called right after BB_InitVoices:

int BB_SetupVoice(int VoiceID, //array index of voice created by BB_InitVoices
        int VoiceType, //a kind of BB_VOICETYPE_*
        int mute, //TRUE or FALSE
        int mono, //TRUE or FALSE [20100614]
        int NumberOfEvents) {
    int i = VoiceID;

    BB_Voice[i].type = VoiceType;
    BB_Voice[i].mute = mute;
    BB_Voice[i].mono = mono; // [20100614]
    BB_Voice[i].EntryCount = 0;
    if (BB_Voice[i].Drop != NULL) {
        free(BB_Voice[i].Drop);
        BB_Voice[i].Drop = NULL; //just in case user is recycling 20110516
    }
    if (BB_Voice[i].Entry != NULL) {
        free(BB_Voice[i].Entry);
        BB_Voice[i].Entry = NULL; //just in case thread is looking here
    }
    BB_Voice[i].CurEntry = 0;
    BB_Voice[i].Entry =
            (BB_EventData *) calloc(NumberOfEvents, sizeof (BB_EventData));
    if (BB_Voice[i].Entry == NULL) {
        return FALSE;
    }
    memset(BB_Voice[i].Entry, 0, sizeof (BB_EventData) * NumberOfEvents);
    BB_Voice[i].TotalDuration = 0;
    BB_Voice[i].EntryCount = NumberOfEvents;
    return TRUE;
    //Now user must load this voice with data in any way they see fit, then
    //call BB_CalibrateVoice()
}

//======================================
//you must call InitVoices() before calling this...

void BB_LoadDefaultVoice(int VoiceID) {
    int NumberOfEvents =
            (sizeof (BB_DefaultBBSched) / sizeof (BB_PRECISION_TYPE)) /
            BB_EVENT_NUM_OF_ELEMENTS;
    BB_DBGOUT_INT("Sizeof Default Events Array:", (int) sizeof (BB_DefaultBBSched));
    BB_DBGOUT_INT("Default Voice number of events:", NumberOfEvents);
    int i = VoiceID;
    int j;
    int k = 0;

    BB_SetupVoice(VoiceID, BB_VOICETYPE_BINAURALBEAT, FALSE, FALSE, NumberOfEvents); //[modified 20100614]

    //load up starting values:
    for (j = 0; j < NumberOfEvents; j++) {
        BB_Voice[i].Entry[j].beatfreq_start_HALF = BB_DefaultBBSched[k++];
        //k++;                          //needed until I remove corresponding part from default schedule
        BB_Voice[i].Entry[j].duration = BB_DefaultBBSched[k++];
        BB_Voice[i].Entry[j].basefreq_start = BB_DefaultBBSched[k++];
        BB_Voice[i].Entry[j].volL_start = BB_DefaultBBSched[k++];
        BB_Voice[i].Entry[j].volR_start = BB_DefaultBBSched[k++];
    }

    //do preprocessing on dataset:
    BB_CalibrateVoice(i);
}

//======================================
//this simply sets compensates any voices found to be
//shorter than total duration by adding difference in duration
//to last DP in any voices found to be shorter

int BB_FixVoiceDurations() {
    int fixcount = 0; // set to number of voices fixed

    //first get a very solid BB_TotalDuration:
    BB_TotalDuration = 0;
    BB_DetermineTotalDuration();

    //now add duration difference to last point of any voices found to be shorter:
    int i;

    for (i = 0; i < BB_VoiceCount; i++) {
        if (BB_Voice[i].TotalDuration < BB_TotalDuration) {
            int entry = BB_Voice[i].EntryCount - 1;

            if (entry < 0) {
                entry = 0;
            }
            BB_Voice[i].Entry[entry].duration +=
                    (BB_TotalDuration - BB_Voice[i].TotalDuration);
            BB_CalibrateVoice(i);
            ++fixcount;
        }
    }
    return fixcount;
}

//======================================

int BB_DetermineTotalDuration() {
    BB_TotalDuration = 0;
    int i;

    for (i = 0; i < BB_VoiceCount; i++) {
        if (BB_Voice[i].TotalDuration > BB_TotalDuration) {
            BB_TotalDuration = BB_Voice[i].TotalDuration;
        }
    }
    return BB_TotalDuration;
}

//======================================
//InitVoices() needs to have been called once at some point before calling this...

int BB_CalibrateVoice(int VoiceID) {
    int i = VoiceID;
    int j;
    int prevEntry,
            nextEntry;

    //reset total duration:
    BB_Voice[i].TotalDuration = 0;

    for (j = 0; j < BB_Voice[i].EntryCount; j++) {
        //increment voice's total duration:
        BB_Voice[i].TotalDuration += BB_Voice[i].Entry[j].duration;

        //Now save exact total schedule samplecount at end of entry:
        BB_Voice[i].Entry[j].AbsoluteEnd_samples = (unsigned int) (BB_Voice[i].TotalDuration * BB_AUDIOSAMPLERATE); //should I round this?
    }

    //now figure TotalDuration, SampleCounts, Ends, and Spreads:
    for (j = 0; j < BB_Voice[i].EntryCount; j++) {
        if ((nextEntry = j + 1) >= BB_Voice[i].EntryCount) {
            nextEntry = 0;
        }
        BB_Voice[i].Entry[j].beatfreq_end_HALF =
                BB_Voice[i].Entry[nextEntry].beatfreq_start_HALF;
        BB_Voice[i].Entry[j].beatfreq_spread_HALF =
                BB_Voice[i].Entry[j].beatfreq_end_HALF -
                BB_Voice[i].Entry[j].beatfreq_start_HALF;
        BB_Voice[i].Entry[j].basefreq_end =
                BB_Voice[i].Entry[nextEntry].basefreq_start;
        BB_Voice[i].Entry[j].basefreq_spread =
                BB_Voice[i].Entry[j].basefreq_end - BB_Voice[i].Entry[j].basefreq_start;
        BB_Voice[i].Entry[j].volL_end = BB_Voice[i].Entry[nextEntry].volL_start;
        BB_Voice[i].Entry[j].volL_spread =
                BB_Voice[i].Entry[j].volL_end - BB_Voice[i].Entry[j].volL_start;
        BB_Voice[i].Entry[j].volR_end = BB_Voice[i].Entry[nextEntry].volR_start;
        BB_Voice[i].Entry[j].volR_spread =
                BB_Voice[i].Entry[j].volR_end - BB_Voice[i].Entry[j].volR_start;

        if ((prevEntry = j - 1) < 0)
            BB_Voice[i].Entry[j].AbsoluteStart_samples = 0;
        else
            BB_Voice[i].Entry[j].AbsoluteStart_samples =
                BB_Voice[i].Entry[prevEntry].AbsoluteEnd_samples;
    }

    //NOTE: User must be sure to zero BB_TotalDuration if resetting all voices
    if (BB_TotalDuration < BB_Voice[i].TotalDuration) {
        BB_TotalDuration = BB_Voice[i].TotalDuration;
    }
    return TRUE;
}

//======================================

void BB_CleanupVoices() {
    int i;
    for (i = 0; i < BB_VoiceCount; i++) {
        if (BB_Voice[i].Entry != NULL) {
            free(BB_Voice[i].Entry);
            BB_Voice[i].Entry = NULL;
        }

        if (BB_Voice[i].Drop != NULL) {
            free(BB_Voice[i].Drop);
            BB_Voice[i].Drop = NULL;
        }
        //IMPORTANT: User needs to free this elsewhere; it merely gets NULL'd in BB_InitVoices() now
        // BB_Voice[i].PCM_samples = NULL;
        // BB_Voice[i].PCM_samples_size = 0;
        // BB_Voice[i].PCM_samples_currentcount = 0;
    }
    if (BB_Voice != NULL) {
        free(BB_Voice);
    }

    BB_VoiceCount = 0;
    BB_Voice = NULL;
}

//======================================
//////////////////////////////////////////////////
//BB_MainLoop() -- the big one.
//Give this an array (in any unit) pSoundBuffer of length
//(in bytes) bufferLen, and it fills it with 32-bit stereo
//data (alternating left-right, each 16 bits), while internally
//keeping track of where it last stopped in the
//previous call.
//IMPORTANT: you can set pSoundBuffer to any unit, but bufferLen must always be in bytes
//Extra info: BB_UPDATEPERIOD_SAMPLES sets the period of update;
//Fill sound buffer. BB_UPDATEPERIOD_SAMPLES sets the period of update;
//the higher it is, the less often periodic precalculating is done
//(determining entry, changing frequency, etc.).

void BB_MainLoop(void *pSoundBuffer, long bufferLen) {
    static int updateperiod = 1; //critical: initialize as 1 so decrements to zero and resets

    // static int noizval1 = 1;
    // static int noizval2 = 1;
    BB_PRECISION_TYPE sumL = 0,
            sumR = 0;
    BB_PRECISION_TYPE Sample_left,
            Sample_right;
    int k;
    int voice;

    // Cast whatever form pSoundBuffer came in to short ints (16 bits), since
    // each 32 bit frame will consist of two 16bit mono waveforms, alternating
    // left and right stereo data: [see the Java version for a byte-tailored approach]
    signed short *pSample = (signed short int *) pSoundBuffer;

    //Generally speaking, this is what I am doing:
    //long nbSample = bufferLen / (sizeof(int));
    //But since I always know bufferlen in in chars, I just divide by four:
    long nbSample = bufferLen >> 2;

    //-------------------------------------------
    //START Fill sound buffer
    //OUTER LOOP: do everything in this loop for every sample in pSoundBuffer to be filled:
    for (k = 0; k < nbSample; k++) {
        --updateperiod;
        //zero-out the current sample values:
        sumL = sumR = 0;
        if (BB_PauseFlag != TRUE && BB_Voice != NULL) //a20070730 -- critical bug fix
            for (voice = 0; voice < BB_VoiceCount; voice++) {
                Sample_left = Sample_right = 0;
                BB_InCriticalLoopFlag = TRUE; //added 20070803
                //##### START Periodic stuff (the stuff NOT done every cycle)
                if (updateperiod == 0) {
                    //Should not need this, will eventually drop it:
                    while ((BB_Voice[voice].CurEntry >= BB_Voice[voice].EntryCount)) {
                        BB_DBGOUT_INT("Resetting Voices: CurEntry:", BB_Voice[voice].CurEntry);
                        BB_DBGOUT_INT("Resetting Voices: EntryCount:",
                                BB_Voice[voice].EntryCount);
                        BB_DBGOUT_INT("Resetting Voices: Voice:", voice);
                        BB_ResetAllVoices();
                    }

                    //First figure out which Entry we're at for this voice.
                    //20070728: big changes to make CurEntry persistent, to obviate need for 
                    //each voice to start from zero every time (CPU load would progressively 
                    //go sour with lots of entries). Method: don't zero CurEntry, instead only
                    //increment it if totalsamples is now greater than sched entry's endtime/
                    //20080218: The above method turns out not to account for when user sets
                    //BB_CurrentSampleCount manually to place BEFORE CurEntry. Fixed with this:
                    //See if totalsamples is LESS than CurEntry (very rare event):
                    while (BB_CurrentSampleCount <
                            BB_Voice[voice].Entry[BB_Voice[voice].
                            CurEntry].AbsoluteStart_samples) {
                        BB_InfoFlag |= BB_NEWENTRY;
                        --BB_Voice[voice].CurEntry;
                        if (BB_Voice[voice].CurEntry < 0) {
                            BB_Voice[voice].CurEntry = 0;
                        }
                    }

                    //Now see if totalsamples is Greater than CurEntry (common event):
                    while (BB_CurrentSampleCount >
                            BB_Voice[voice].Entry[BB_Voice[voice].
                            CurEntry].AbsoluteEnd_samples) {
                        BB_InfoFlag |= BB_NEWENTRY;
                        ++BB_Voice[voice].CurEntry;
                        if (BB_Voice[voice].CurEntry >= BB_Voice[voice].EntryCount) {
                            //       BB_DBGOUT_INT ("Completed loop", 1 + BB_Loops - BB_LoopCount);
                            BB_CurrentSampleCountLooped += BB_CurrentSampleCount;
                            BB_CurrentSampleCount = 0;
                            //Zero CurEntry for ALL voices:
                            BB_ResetAllVoices();
                            BB_InfoFlag |= BB_NEWLOOP;
                            if (--BB_LoopCount == 0) { //tell user Schedule is totally done
                                BB_InfoFlag |= BB_COMPLETED;
                                BB_DBGOUT("Schedule complete");
                            }
                            break;
                        }
                    }

                    //Now that entry housecleaning is done, start actual signal processing:
                    //NOTE: I wanted to put next line at start of voice loop, but can't
                    //because any voice can end schedule, even if muted -- so all voice
                    //must get checked.
                    if (BB_Voice[voice].mute == FALSE) { //START "Voice NOT muted"
                        //this just to make BB_Voice[voice].CurEntry more handy:
                        int entry = BB_Voice[voice].CurEntry;

                        //First come up with a factor describing exact point in the schedule
                        //by dividing exact point in period by total period time:
                        //[NOTE: critically, duration should never be able to == 0 here because the
                        //entry "for" loop above only gets here if BB_Voice[voice].Entry[entry].AbsoluteEnd_samples
                        //is GREATER than 0]:
                        //if (BB_Voice[voice].Entry[entry].duration == 0) BB_BB_DBGOUT("BUG: duration == 0");
                        BB_PRECISION_TYPE factor;

                        if (0 != BB_Voice[voice].Entry[entry].duration) {
                            factor =
                                    (BB_CurrentSampleCount -
                                    BB_Voice[voice].Entry[entry].AbsoluteStart_samples) /
                                    (BB_Voice[voice].Entry[entry].duration * BB_AUDIOSAMPLERATE);
                        } else {
                            //NOTE: Interestingly, it actually almost never gets here unless it is first-DP, because
                            //because otherwise it is a lotto pick that BB_CurrentSampleCount would land on that DP
                            factor = 0;
                            BB_DBGOUT("Duration == 0!");
                        }
                        //now determine volumes for this slice, since all voices use volume:
                        BB_Voice[voice].CurVolL =
                                (BB_Voice[voice].Entry[entry].volL_spread * factor) +
                                BB_Voice[voice].Entry[entry].volL_start;
                        BB_Voice[voice].CurVolR =
                                (BB_Voice[voice].Entry[entry].volR_spread * factor) +
                                BB_Voice[voice].Entry[entry].volR_start;

                        //Now do stuff that must be figured out at Entry level:
                        //now figure out what sort of voice it is, to do it's particular processing:
                        switch (BB_Voice[voice].type) {
                                //---------A-------------
                            case BB_VOICETYPE_BINAURALBEAT:
                                //first determine base frequency to be used for this slice:
                                BB_Voice[voice].cur_basefreq =
                                        (BB_Voice[voice].Entry[entry].basefreq_spread * factor) +
                                        BB_Voice[voice].Entry[entry].basefreq_start;

                                //20071010 NOTE: heavily changed to optimize calculations and avail instantaneous BeatFreq
                                // It is now assumed that all beatfreqs are symmetric around the basefreq.
                                //now that more difficult one, calculating partially factored beat freqs:
                                //Left Freq is equal to frequency spread from Left Start to Left End (next
                                //schedule's Left Start) multiplied by above factor. Then add FreqBase.
                                BB_Voice[voice].cur_beatfreqL_factor =
                                        BB_Voice[voice].cur_beatfreqR_factor =
                                        (BB_Voice[voice].Entry[entry].beatfreq_spread_HALF * factor);

                                //get current beatfreq in Hz (for user external use):
                                BB_PRECISION_TYPE old_beatfreq = BB_Voice[voice].cur_beatfreq;
                                BB_PRECISION_TYPE new_beatfreq =
                                        BB_Voice[voice].cur_beatfreq =
                                        (BB_Voice[voice].cur_beatfreqL_factor +
                                        BB_Voice[voice].Entry[entry].beatfreq_start_HALF) * 2;

                                //NOTE: BB_SAMPLE_FACTOR == 2*PI/sample_rate
                                BB_Voice[voice].cur_beatfreqL_factor =
                                        (BB_Voice[voice].cur_basefreq +
                                        BB_Voice[voice].Entry[entry].beatfreq_start_HALF +
                                        BB_Voice[voice].cur_beatfreqL_factor) * BB_SAMPLE_FACTOR;

                                BB_Voice[voice].cur_beatfreqR_factor =
                                        (BB_Voice[voice].cur_basefreq -
                                        BB_Voice[voice].Entry[entry].beatfreq_start_HALF -
                                        BB_Voice[voice].cur_beatfreqR_factor) * BB_SAMPLE_FACTOR;

                                //extract phase info for external stimulus cue:
                                //must have a lower limit for beatfreq or else we have divide by zero issues
                                if (new_beatfreq < .0001) {
                                    new_beatfreq = .0001;
                                }
                                if (old_beatfreq != new_beatfreq) {
                                    BB_PRECISION_TYPE phasefactor =
                                            (BB_Voice[voice].cur_beatfreq_phasesamplecount /
                                            (BB_PRECISION_TYPE)
                                            BB_Voice[voice].cur_beatfreq_phasesamplecount_start);
                                    BB_Voice[voice].cur_beatfreq_phasesamplecount_start =
                                            (int) (BB_AUDIOSAMPLERATE_HALF / new_beatfreq);
                                    BB_Voice[voice].cur_beatfreq_phasesamplecount =
                                            BB_Voice[voice].cur_beatfreq_phasesamplecount_start * phasefactor;
                                }
                                break;

                                //---------A-------------
                            case BB_VOICETYPE_PINKNOISE:
                                break;

                                //---------A-------------
                            case BB_VOICETYPE_PCM:
                                if (NULL != BB_Voice[voice].PCM_samples) {
                                    BB_Voice[voice].PCM_samples_currentcount = BB_CurrentSampleCount;
                                    if (BB_Voice[voice].PCM_samples_currentcount >=
                                            BB_Voice[voice].PCM_samples_size) {
                                        BB_Voice[voice].PCM_samples_currentcount =
                                                BB_CurrentSampleCount % (BB_Voice[voice].PCM_samples_size);
                                    }
                                }
                                break;

                                //---------A-------------
                            case BB_VOICETYPE_ISOPULSE:
                            case BB_VOICETYPE_ISOPULSE_ALT:
                            {
                                //in isochronic tones, the beat frequency purely affects base 
                                //frequency pulse on/off duration, not its frequency:
                                //New way starting 20110119:
                                //The approach here is to re-scale however much time was left in 
                                //the old beatfreq's pulse to how much would be left in the new
                                //one's. Otherwise each pulse would have to time-out before a 
                                //new beatfreq could be modulated - unacceptable in many ways.
                                //first get the exact base frequency for this slice:
                                BB_Voice[voice].cur_basefreq =
                                        (BB_Voice[voice].Entry[entry].basefreq_spread * factor) +
                                        BB_Voice[voice].Entry[entry].basefreq_start;

                                //Now get current beatfreq in Hz for this slice:
                                // ("half" works because period alternates silence and tone for this kind of voice)
                                BB_PRECISION_TYPE old_beatfreq = BB_Voice[voice].cur_beatfreq;
                                BB_Voice[voice].cur_beatfreq =
                                        ((BB_Voice[voice].Entry[entry].beatfreq_spread_HALF * factor) +
                                        BB_Voice[voice].Entry[entry].beatfreq_start_HALF) * 2;

                                //must have a lower limit for beatfreq or else we have divide by zero issues
                                if (BB_Voice[voice].cur_beatfreq < .0001) {
                                    BB_Voice[voice].cur_beatfreq = .0001;
                                }

                                //Set both channels to the same base frequency:
                                //NOTE: BB_SAMPLE_FACTOR == 2*PI/sample_rate
                                BB_Voice[voice].cur_beatfreqR_factor =
                                        BB_Voice[voice].cur_beatfreqL_factor =
                                        BB_Voice[voice].cur_basefreq * BB_SAMPLE_FACTOR;

                                //if this is a new beatfreq, adjust phase accordingly
                                if (old_beatfreq != BB_Voice[voice].cur_beatfreq) {
                                    BB_PRECISION_TYPE phasefactor =
                                            (BB_Voice[voice].cur_beatfreq_phasesamplecount /
                                            (BB_PRECISION_TYPE)
                                            BB_Voice[voice].cur_beatfreq_phasesamplecount_start);
                                    BB_Voice[voice].cur_beatfreq_phasesamplecount_start =
                                            (int) (BB_AUDIOSAMPLERATE_HALF / BB_Voice[voice].cur_beatfreq);
                                    BB_Voice[voice].cur_beatfreq_phasesamplecount =
                                            BB_Voice[voice].cur_beatfreq_phasesamplecount_start * phasefactor;
                                }
                            }
                                break;

                                //---------A-------------
                                //Waterdrops: unless user really knows what they're doing,
                                //they should leave both base and beat freq at 0 to
                                //get raindrop defaults. Otherwise, this is the equation:
                                //Drop count(set only at start) == beatfreq_start_HALF * 2 
                                //Random Threshold == basefreq_start
                                //The default values are 2 and 1/ 2834) respectively.
                                //Note: a Drop that's supposed to exist is found to be
                                //NULL that's the signal to set up the whole shebang here:
                            case BB_VOICETYPE_WATERDROPS: //20110516
                            {
                                if (NULL == BB_Voice[voice].Drop) {
                                    if (NULL == BB_DropMother) {
                                        BB_DropMother = BB_WaterInit(BB_DROPLEN, 600);
                                    }
                                    BB_WaterVoiceInit(voice);
                                }

                                //determine base frequency to be used for this slice:
                                BB_Voice[voice].cur_basefreq =
                                        (BB_Voice[voice].Entry[entry].basefreq_spread * factor) +
                                        BB_Voice[voice].Entry[entry].basefreq_start;
                            }
                                break;

                                //---------A-------------
                            case BB_VOICETYPE_RAIN: //20110520
                            {
                                if (NULL == BB_Voice[voice].Drop) {
                                    if (NULL == BB_RainMother) {
                                        BB_RainMother = BB_WaterInit(BB_RAINLEN, 3.4);
                                    }
                                    BB_WaterVoiceInit(voice);
                                }

                                //determine base frequency to be used for this slice:
                                BB_Voice[voice].cur_basefreq =
                                        (BB_Voice[voice].Entry[entry].basefreq_spread * factor) +
                                        BB_Voice[voice].Entry[entry].basefreq_start;
                            }
                                break;

                            default:
                                break;
                        } //END voicetype switch
                    } //END "Voice NOT muted"
                }
                //##### END Periodic stuff (the stuff NOT done every cycle)

                //####START high priority calculations (done for every sample)
                if (BB_Voice[voice].mute == FALSE) { //START "Voice NOT muted"
                    //now figure out what sort of voice it is, to do it's particular processing:
                    switch (BB_Voice[voice].type) {
                            //---------B-------------
                        case BB_VOICETYPE_BINAURALBEAT:
                            //advance to the next sample for each channel:
                            BB_Voice[voice].sinPosL += BB_Voice[voice].cur_beatfreqL_factor;
                            if (BB_Voice[voice].sinPosL >= BB_TWO_PI) {
                                BB_Voice[voice].sinPosL -= BB_TWO_PI;
                            }
                            BB_Voice[voice].sinPosR += BB_Voice[voice].cur_beatfreqR_factor;
                            if (BB_Voice[voice].sinPosR >= BB_TWO_PI) {
                                BB_Voice[voice].sinPosR -= BB_TWO_PI;
                            }

                            //there are probably shortcuts to avoid doing a sine for every voice, but I don't know them:
                            BB_Voice[voice].sinL = sin(BB_Voice[voice].sinPosL);
                            Sample_left = BB_Voice[voice].sinL * BB_SIN_SCALER;
                            BB_Voice[voice].sinR = sin(BB_Voice[voice].sinPosR);
                            Sample_right = BB_Voice[voice].sinR * BB_SIN_SCALER;

                            //extract external stimuli cue:
                            if (1 > --BB_Voice[voice].cur_beatfreq_phasesamplecount) {
                                BB_Voice[voice].cur_beatfreq_phasesamplecount =
                                        BB_Voice[voice].cur_beatfreq_phasesamplecount_start;
                                if (0 != BB_Voice[voice].cur_beatfreq_phaseflag) {
                                    BB_Voice[voice].cur_beatfreq_phaseflag = 0;
                                } else {
                                    BB_Voice[voice].cur_beatfreq_phaseflag = 1;
                                }
                                if (NULL != BB_UserFunc) {
                                    BB_UserFunc(voice);
                                }
                                BB_Voice[voice].cur_beatfreq_phaseenvelope = 0;
                            }
                            break;

                            //---------B-------------
                        case BB_VOICETYPE_PINKNOISE:
                            /*
                               //i put the following in an equation to get ready to make
                               //other colors of noise:
                               //NOTE: the following odd equation is a fast way
                               //of simulating "pink noise" from white:
                               BB_Voice[voice].noiseL =
                               ((BB_Voice[voice].noiseL * 31) + (BB_Rand () >> 15)) >> 5;
                               Sample_left = BB_Voice[voice].noiseL;
                               BB_Voice[voice].noiseR =
                               ((BB_Voice[voice].noiseR * 31) + (BB_Rand () >> 15)) >> 5;
                               Sample_right = BB_Voice[voice].noiseR;
                             */
                            Sample_left = BB_LoPass(&BB_Voice[voice].noiseL, BB_Rand() >> 15);
                            Sample_right = BB_LoPass(&BB_Voice[voice].noiseR, BB_Rand() >> 15);
                            //Sample_right = BB_PowerLaw (BB_Rand (), 0x7fffffff, 0x7fff);
                            break;

                        case BB_VOICETYPE_PCM:
                        {
                            if (NULL != BB_Voice[voice].PCM_samples) {
                                if (BB_Voice[voice].PCM_samples_currentcount >=
                                        BB_Voice[voice].PCM_samples_size) {
                                    BB_Voice[voice].PCM_samples_currentcount =
                                            BB_CurrentSampleCount % (BB_Voice[voice].PCM_samples_size);
                                }
                                Sample_left =
                                        ((short)
                                        ((BB_Voice[voice].PCM_samples
                                        [BB_Voice[voice].PCM_samples_currentcount]) >> 16));
                                Sample_right =
                                        ((short)
                                        (BB_Voice[voice].PCM_samples
                                        [BB_Voice[voice].PCM_samples_currentcount]));
                                ++BB_Voice[voice].PCM_samples_currentcount;
                            }
                        }
                            break;

                            //---------B-------------
                        case BB_VOICETYPE_ISOPULSE:
                        case BB_VOICETYPE_ISOPULSE_ALT:
                        {
                            int iso_alternating = 0;
                            if (BB_VOICETYPE_ISOPULSE_ALT == BB_Voice[voice].type) {
                                iso_alternating = 1;
                            }
                            //New way starting 20110119:
                            //advance to the next sample for each channel:
                            BB_Voice[voice].sinPosL += BB_Voice[voice].cur_beatfreqL_factor;
                            if (BB_Voice[voice].sinPosL >= BB_TWO_PI) {
                                BB_Voice[voice].sinPosL -= BB_TWO_PI;
                            }
                            BB_Voice[voice].sinPosR = BB_Voice[voice].sinPosL;

                            //now determine whether tone or silence:
                            //Set toggle-time for current beat polarity (for user external use)
                            if (1 > --BB_Voice[voice].cur_beatfreq_phasesamplecount) {
                                BB_Voice[voice].cur_beatfreq_phasesamplecount =
                                        BB_Voice[voice].cur_beatfreq_phasesamplecount_start;
                                if (0 != BB_Voice[voice].cur_beatfreq_phaseflag) {
                                    BB_Voice[voice].cur_beatfreq_phaseflag = 0;
                                } else {
                                    BB_Voice[voice].cur_beatfreq_phaseflag = 1;
                                }

                                if (NULL != BB_UserFunc) {
                                    BB_UserFunc(voice);
                                }
                                BB_Voice[voice].cur_beatfreq_phaseenvelope = 0;
                            }

                            //there are probably shortcuts to avoid doing a sine for every sample, but I don't know them:
                            BB_Voice[voice].sinR =
                                    BB_Voice[voice].sinL = sin(BB_Voice[voice].sinPosL);

                            if (0 != BB_Voice[voice].cur_beatfreq_phaseflag) {
                                Sample_left =
                                        BB_Voice[voice].sinL * BB_SIN_SCALER *
                                        (1.0 - BB_Voice[voice].cur_beatfreq_phaseenvelope);

                                //Decide if it is alternating or not:
                                if (0 == iso_alternating) {
                                    Sample_right = Sample_left;
                                } else {
                                    Sample_right = BB_Voice[voice].sinL * BB_SIN_SCALER *
                                            BB_Voice[voice].cur_beatfreq_phaseenvelope;
                                }
                            } else {
                                Sample_left =
                                        BB_Voice[voice].sinL * BB_SIN_SCALER *
                                        BB_Voice[voice].cur_beatfreq_phaseenvelope;
                                if (0 == iso_alternating) {
                                    Sample_right = Sample_left;
                                } else {
                                    Sample_right = BB_Voice[voice].sinL * BB_SIN_SCALER *
                                            (1.0 - BB_Voice[voice].cur_beatfreq_phaseenvelope);
                                }
                            }

                            //cur_beatfreq_data is used arbitrarily as a Modulator here (to reduce harmonics in transition between on and off pulses)
                            if ((BB_Voice[voice].cur_beatfreq_phaseenvelope += .01) > 1)
                                BB_Voice[voice].cur_beatfreq_phaseenvelope = 1;
                        }
                            break;

                            //---------B-------------
                        case BB_VOICETYPE_WATERDROPS:
                        {
                            BB_Water(voice, BB_DropMother, BB_DROPLEN, 8.f);

                            Sample_left = BB_Voice[voice].cur_beatfreq_phasesamplecount;
                            Sample_right = BB_Voice[voice].cur_beatfreq_phasesamplecount_start;
                        }
                            break;

                            //---------B-------------
                            //see waterdrops for explanation
                        case BB_VOICETYPE_RAIN:
                        {
                            BB_Water(voice, BB_RainMother, BB_RAINLEN, .15f);

                            Sample_left = BB_Voice[voice].cur_beatfreq_phasesamplecount;
                            Sample_right = BB_Voice[voice].cur_beatfreq_phasesamplecount_start;
                        }
                            break;

                        default:
                            break;
                    }
                    //handle stereo/mono mixing:
                    if (TRUE != BB_Voice[voice].mono) {
                        sumL += (Sample_left * BB_Voice[voice].CurVolL);
                        sumR += (Sample_right * BB_Voice[voice].CurVolR);
                    } else {
                        Sample_left = (Sample_left + Sample_right) * 0.5;
                        sumL += (Sample_left * BB_Voice[voice].CurVolL);
                        sumR += (Sample_left * BB_Voice[voice].CurVolR);
                    }
                } //END "Voice NOT muted"
                //####END high priority calculations (done for every sample)   
            } //END Voices loop

        BB_InCriticalLoopFlag = FALSE;

        //Finally, load the array with the mixed audio:
        //In C, this is the approach:
        if (TRUE == BB_Mono) {
            sumL = .5 * (sumL + sumR);
            sumR = sumL;
        }

        int peakL,
                peakR;
        if (FALSE == BB_StereoSwap) {
            *pSample++ = peakL = (signed short) (sumL * BB_VolumeOverall_left);
            *pSample++ = peakR = (signed short) (sumR * BB_VolumeOverall_right);
        } else {
            *pSample++ = peakL = (signed short) (sumR * BB_VolumeOverall_right);
            *pSample++ = peakR = (signed short) (sumL * BB_VolumeOverall_left);
        }

        //20101014: attempt at external volume monitoring:
        if (abs(peakL) > BB_PeakL) {
            BB_PeakL = abs(peakL);
            //BB_DBGOUT_INT ("BB_PeakL:", BB_PeakL);
        }
        if (abs(peakR) > BB_PeakR) {
            BB_PeakR = abs(peakR);
            //BB_DBGOUT_INT ("BB_PeakR:", BB_PeakR);
        }

        //In Java, this is the equivalent:
        //pSample[pSample_index++] = (byte) (((short)sumL) & 0xFF);
        //pSample[pSample_index++] = (byte) (((short)sumL) >> 8);
        //pSample[pSample_index++] = (byte) (((short)sumR) & 0xFF);
        //pSample[pSample_index++] = (byte) (((short)sumR) >> 8);

        if (updateperiod == 0) {
            updateperiod = BB_UPDATEPERIOD_SAMPLES;
            BB_CurrentSampleCount += BB_UPDATEPERIOD_SAMPLES; // NOTE: I could also just do BB_CurrentSampleCount++ for each sample...
        }
    } //END samplecount
    // END Fill sound buffer
}

//======================================

void BB_Reset() {
    BB_CurrentSampleCount = BB_CurrentSampleCountLooped = 0;
    BB_InfoFlag &= ~BB_COMPLETED;
    BB_LoopCount = BB_Loops;
    BB_ResetAllVoices();
}

//======================================

inline void BB_ResetAllVoices() {
    if (BB_Voice != NULL) {
        int i;

        for (i = 0; i < BB_VoiceCount; i++) {
            BB_Voice[i].CurEntry = 0;
        }
    }
}

//======================================
// I need to add two size parameters when I am done- in the simplified case of 44100
// 2 channel, I can basically sum the size of WAVheader in to total data size, then
// subtract 8 (god knows why - RIFF is insane)

int BB_WriteWAVFile(char *szFilename) {
    FILE *stream;

    if ((stream = fopen(szFilename, "wb")) == NULL) {
        return 0;
    }

    BB_WriteWAVToStream(stream);
    //now that data has been written, write those two damn header values correctly this time:
    rewind(stream);
    BB_WriteWAVHeaderToStream(stream);
    fclose(stream);
    return 1;
}

//======================================
//writes a basic 16bit 44100hz stereo WAV file to whatever stream you give it (stdout, file, etc.)
//it is up to the user to ensure that the stream is writable and in binary mode.

void BB_WriteWAVHeaderToStream(FILE * stream) {

    struct WAVheader {
        char ckID[4]; // chunk id 'RIFF'
        unsigned int ckSize; // chunk size [it is total filesize-8]
        char wave_ckID[4]; // wave chunk id 'WAVE'
        char fmt_ckID[4]; // format chunk id 'fmt '
        unsigned int fmt_ckSize; // format chunk size
        unsigned short formatTag; // format tag currently pcm
        unsigned short nChannels; // number of channels
        unsigned int nSamplesPerSec; // sample rate in hz
        unsigned int nAvgBytesPerSec; // average bytes per second
        unsigned short nBlockAlign; // number of bytes per sample
        unsigned short nBitsPerSample; // number of bits in a sample
        char data_ckID[4]; // data chunk id 'data'
        unsigned int data_ckSize; // length of data chunk [byte count of actual data (not descriptors)]
    }
    wavh;

    wavh.ckID[0] = 'R';
    wavh.ckID[1] = 'I';
    wavh.ckID[2] = 'F';
    wavh.ckID[3] = 'F';
    wavh.ckSize = BB_FileByteCount + sizeof (wavh) - 8; //ultimately needs to be accurate
    wavh.wave_ckID[0] = 'W';
    wavh.wave_ckID[1] = 'A';
    wavh.wave_ckID[2] = 'V';
    wavh.wave_ckID[3] = 'E';
    wavh.fmt_ckID[0] = 'f';
    wavh.fmt_ckID[1] = 'm';
    wavh.fmt_ckID[2] = 't';
    wavh.fmt_ckID[3] = ' ';
    wavh.fmt_ckSize = 16;
    wavh.formatTag = 1;
    wavh.nChannels = 2;
    wavh.nSamplesPerSec = 44100;
    wavh.nAvgBytesPerSec = 176400; //nAvgBytesPerSec= nSamplesPerSec* nBitsPerSample/8* nChannels
    wavh.nBlockAlign = 4;
    wavh.nBitsPerSample = 16;
    wavh.data_ckID[0] = 'd';
    wavh.data_ckID[1] = 'a';
    wavh.data_ckID[2] = 't';
    wavh.data_ckID[3] = 'a';
    wavh.data_ckSize = BB_FileByteCount; //ultimately needs to be accurate

    //create the header:
    fwrite(&wavh, sizeof (wavh), 1, stream);
}

//======================================
//writes a basic 16bit 44100hz stereo WAV file to whatever stream you give it (stdout, file, etc.)
//it is up to the user to ensure that the stream is writable and in binary mode.

int BB_WriteWAVToStream(FILE * stream) {
#define WRITESIZE 4
    signed short buff[WRITESIZE];

    //20051129 - I need to estimate final size of file for stream/piping use,
    //and so something is in there in case user cancels mid-write:
    BB_FileByteCount = (unsigned int) (BB_Loops * BB_TotalDuration * 176400);

    BB_WriteWAVHeaderToStream(stream);

    BB_FileByteCount = 0; //now reset FileByteCount of estimate in order to have it count real samples
    BB_WriteStopFlag = 0;

    BB_Reset(); //make sure we start at absolute beginning

    // BB_WriteStopFlag = 0;
    while (!(BB_InfoFlag & BB_COMPLETED) && FALSE == BB_WriteStopFlag) {
        BB_MainLoop(buff, WRITESIZE);
        fwrite(buff, WRITESIZE, 1, stream);
        BB_FileByteCount += WRITESIZE;
    }

    return 1;
}

//======================================
//this function is to assist user in cleaning up any PCM data
//they opened to use here; when voice PCM_samples == NULL, it gets
//ignored by audio engine. 

void BB_NullAllPCMData() {
    int i;

    for (i = 0; i < BB_VoiceCount; i++) {
        BB_Voice[i].PCM_samples = NULL;
        BB_Voice[i].PCM_samples_size = 1; //this is set to 1 simply so that there is never a divide by zero situation.
        BB_Voice[i].PCM_samples_currentcount = 0;
    }
}

//################################################
//START PSEUDORANDOM NUMBER GENERATOR SECTION
//Make a choice.
//=============================================
//~ The McGill Super-Duper Random Number Generator
//~ G. Marsaglia, K. Ananthanarayana, N. Paul
//~ Incorporating the Ziggurat method of sampling from decreasing
//~ or symmetric unimodal density functions.
//~ G. Marsaglia, W.W. Tsang
//~ Rewritten into C by E. Schneider
//~ [Date last modified: 05-Jul-1997]
//=============================================
//These being longs are the key to the long between-repeat period:
unsigned long BB_mcgn = 3677;
unsigned long BB_srgn = 127;

#define ML_MULT 69069L
//IMPORTANT OBSERVATION: when using a fixed i2, for some
//reason Seed pairs for i1 like this:
// even
// even+1
//produce idential sequences when r2 returned (r1 >> 12).
//Practically, this means that 2 and 3 produce one landscape;
//likewise 6 and 7, 100 and 101, etc.
//This is why i do the dopey "add 1" to i2
//ALSO, JUST DON'T USE 0 FOR i1 or i2.

void BB_SeedRand(unsigned int i1, unsigned int i2) {
    if (i2 == (unsigned int) - 1) {
        i2 = i1 + 1; //yech
    }
    BB_mcgn = (unsigned long) ((i1 == 0L) ? 0L : i1 | 1L);
    BB_srgn = (unsigned long) ((i2 == 0L) ? 0L : (i2 & 0x7FFL) | 1L);
    BB_DBGOUT_INT("BB_mcgn:", (int) BB_mcgn);
    BB_DBGOUT_INT("BB_srgn:", (int) BB_srgn);
}

/*
   //these lines are useful for debugging Rand():
   int test = r1;
   if (test > 0 && test < 0xffff) BB_DBGOUT_INT("Got a positive: ", test);
   if (test < 0 && test > -0xffff) BB_DBGOUT_INT("Got a negative: ", test);
 */

//======================================
//This returns an int from -2^31 to +2^31;

int BB_Rand() {
    unsigned long r0,
            r1;

    r0 = (BB_srgn >> 15);
    r1 = BB_srgn ^ r0;
    r0 = (r1 << 17);
    BB_srgn = r0 ^ r1;
    BB_mcgn = ML_MULT * BB_mcgn;
    r1 = BB_mcgn ^ BB_srgn;
    return r1;
}

///////////////////////////////////////////
//returns a random double between -1 and 1

double BB_Rand_pm() {
    // return (BB_Rand () / (double) (1 << 31));
    //20100604: discovered bug, was sizeof(long), needed to be sizeof(int):
    return (BB_Rand() / (double) (1 << ((sizeof (int) * 8) - 1)));
}

//=========================================
//unconscionably shorthand way to lo-pass
//noiz "pink": given a persistent variable 
//you maintain, and an int of audio data
//this func smoothes it a LOT:

inline int BB_LoPass(int *staticvar, int value) {
    return ((*staticvar) = (((*staticvar) * 31) + value) >> 5);
    //NOTE: i benchmarked it against this and above was still faster:
    //((((*staticvar) << 5)-(*staticvar)) + value) >> 5);
}

//=========================================

inline int BB_PowerLaw(int randomnumber, int rand_max, int max_out) {
    float upper = .1;
    float lower = .01; //varying this between .999 and .01 is interesting 
    float r = randomnumber / (float) rand_max;
    //return the rare stuff:
    if (upper > r && lower < r) {
        return (int) (lower * max_out / r);
    }
    //discard the common and more-rare stuff:
    return 0;
}

//END PSEUDORANDOM NUMBER GENERATOR SECTION
//################################################

//--------------------------------
//20100522
//called second, before BB_Water()
//if droparray == null, it gets allotted and returned.
//droparray is the single drop array that all drops use
//if BB_Voice[voice].Drop == null it gets allotted;
//

void BB_WaterVoiceInit(int voice) {
    if (NULL == BB_Voice[voice].Drop) {
        BB_Voice[voice].PCM_samples_size =
                (int) (BB_Voice[voice].Entry[0].beatfreq_start_HALF * 2);
        if (1 > BB_Voice[voice].PCM_samples_size)
            BB_Voice[voice].PCM_samples_size = 2; //default drops is 2
        if (100 < BB_Voice[voice].PCM_samples_size)
            BB_Voice[voice].PCM_samples_size = 100;
        BB_Voice[voice].Drop =
                (BB_Waterdrop *) calloc(BB_Voice[voice].PCM_samples_size,
                sizeof (BB_Waterdrop));
        int i;
        for (i = 0; i < BB_Voice[voice].PCM_samples_size; i++) {
            BB_Voice[voice].Drop[i].count = 0;
            BB_Voice[voice].Drop[i].decrement = 1;
            BB_Voice[voice].Drop[i].stereoMix = 0.5f;
        }
    }
}

// =========================================
// 20110616
//called first, before BB_WaterVoiceInit()
//creates an array with a single drop of sound
//and returns it

short *BB_WaterInit(int arraylen, float pitch) {
    short *array;
    array = (short *) calloc(arraylen, sizeof (short));

    double p = 0;
    double q = 1.0 / pitch;
    double r = 0;
    double s = 0x7fff / (double) arraylen;
    int i;
    for (i = 0; i < arraylen; i++) {
        array[i] = (short) (r * sin(p * M_PI * 2.));
        p += q;
        r += s;
    }
    return array;
}

//===============================
//the big one of the water family. 
//Waterdrops: two parameters are adjustable:
//1) Drop count, set only once, taken from very first
//beatfreq_start_HALF multiplied by 2 
//2)Random Threshold, continuously varying as from 
//basefreq_start
//Sort of ugly, but i had to 
//make cur_beatfreq_phasesamplecount == left channel result and
//cur_beatfreq_phasesamplecount_start == right channel result.

void BB_Water(int voice, short *array, int arraylen, float Lowcut) {
    if (NULL == BB_Voice[voice].Drop)
        return;

    const int Window = 126;
    float dropthresh = BB_Voice[voice].cur_basefreq;

    // make water:
    int p,
            mixL = 0,
            mixR = 0;
    for (p = 0; p < BB_Voice[voice].PCM_samples_size; p++) {
        if (0 <= BB_Voice[voice].Drop[p].count) {
            if (TRUE == BB_Voice[voice].mono) {
                mixL += array[(int) BB_Voice[voice].Drop[p].count];
                mixR += array[(int) BB_Voice[voice].Drop[p].count];
            } else {
                mixL +=
                        (int) (array[(int) BB_Voice[voice].Drop[p].count] *
                        BB_Voice[voice].Drop[p].stereoMix);
                mixR +=
                        (int) (array[(int) BB_Voice[voice].Drop[p].count] *
                        (1.0f - BB_Voice[voice].Drop[p].stereoMix));
            }
            BB_Voice[voice].Drop[p].count -= BB_Voice[voice].Drop[p].decrement;
            //} else if (0 == (int) ((Math.random() * Rarity))) {
        }            //        else if (0 == (rand () >> 20)) //shortcut to default
        else if (dropthresh > (rand() / (float) RAND_MAX)) {
            // load up a drop:
            BB_Voice[voice].Drop[p].count = arraylen - 1;
            // give it a random pitch:
            BB_Voice[voice].Drop[p].decrement =
                    (float) (((rand() / (double) RAND_MAX) * Window) + Lowcut);
            //place it somewhere in the stereo mix:
            BB_Voice[voice].Drop[p].stereoMix = rand() / (float) RAND_MAX;
        }
    }

    // do just a touch of lo-pass filtering on it and return in java friendly way:
    BB_Voice[voice].cur_beatfreq_phasesamplecount =
            BB_LoPass(&(BB_Voice[voice].noiseL), mixL);
    BB_Voice[voice].cur_beatfreq_phasesamplecount_start =
            BB_LoPass(&(BB_Voice[voice].noiseR), mixR);
}
