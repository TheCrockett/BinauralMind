#ifndef _EXPORTPCM_H_
#define _EXPORTPCM_H_

//the Majortypes can be lots of stuff, but SubTypes can only 
//be SF_FORMAT_PCM_16 or SF_FORMAT_VORBIS; see sndfile.h
int exportAudioToFile (char *filename, int format);

#endif
