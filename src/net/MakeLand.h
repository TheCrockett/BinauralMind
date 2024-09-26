// MakeLand.h: interface for the CMakeLand class.
// Author: Bret Logan 011208
// Typical usage and recipe for good landscape:
// MakeLand_Init_InternalBitmaps(8);
// MakeLand_Clear(HeightsBitmap,0);
// MakeLand_HeightsCreate();
// MakeLand_TranslateHeightsToColors(256);
// MakeLand_Erosion(1,-1);
// MakeLand_HeightsSmooth(2);
// MakeLand_HeightsBottomReference();
// MakeLand_HeightsScaleMax(2048);
// MakeLand_HeightsSmooth(1);
// MakeLand_HeightsBottomReference();
// MakeLand_HeightsScaleMax(256);
// land->Water(8,0,79,119); // 0x00004F77, my usual color
// land->ColorsShade(-15,'u',1);
// land->ColorsBlur(3,true);
// land->ColorsBlur(16);
// land->HeightsScaleMax(128);
//////////////////////////////////////////////////////////////////////

#include <time.h>

#define ML_RANDMASK 0xFFFFF
 //0xFFFFF returns number between 0 and 1048575
 //0x7FFF gives makes rand() and gaussian()
 //return a number between 0 and range of 32767, which matches
 //the standard C lib rand()'s output. But this function has been tested 
 //much higher, and maintains excellent distribution without missing ints
 //NOTE: gaussian() never achieves a number of points on either extreme,
 //regardless of mask. 021102

#define ML_SHIFT 10
extern int *MakeLand_bmpHeights;
extern int *MakeLand_bmpColors;
extern int MakeLand_Width;
extern int MakeLand_Height;
extern int MakeLand_InternallyGeneratedBitapsFlag;
typedef int (*MAKELAND_USER_CALLBACK) (int message);    //declaration usage: USER_CALLBACK pUserCallback;
extern MAKELAND_USER_CALLBACK MakeLand_UserCallback;

 /////////////////////////////
 //creates the color and heights bitmap:
extern void MakeLand_Init_InternalBitmaps (int Size_LShift);    //size is given as power of 2, since bitmap must be
extern void MakeLand_Cleanup ();
extern void MakeLand_Clear (int *bitmap, int value);
extern int MakeLand_rand (void);
extern int MakeLand_gaussian ();
extern int (*MakeLand_Rand) (); //gets set to whatever type of rand function I want to use
extern void MakeLand_SetGaussWidth (int width);
extern void MakeLand_MakeMPDLand ();
extern void MakeLand_MakeFFTLand ();
extern void MakeLand_SeedRand (unsigned int i1, unsigned int i2);
typedef struct complex          //for FFT image processing
{
 double real;                   //Magnitude information
 double imag;                   //Phase information
} COMPLEX;
extern int MakeLand_FFT2D (COMPLEX * cmplx, int nx, int ny, int dir);
extern int MakeLand_FFT (int dir, int m, double *x, double *y);
extern int MakeLand_HeightsBottomReference (int newbottom);
extern int MakeLand_GetBottomHeight ();
extern int MakeLand_Powerof2 (int number, int *power);
extern void MakeLand_TranslateHeightsToColors (int palsize);
extern int MakeLand_GetBottomHeight ();
extern int MakeLand_GetTopHeight ();
extern unsigned int MakeLand_DefaultPalette[];
extern void MakeLand_HeightsSmooth (int source_weight, int iterations);
extern int MakeLand_RampPixel_rgb (int rgb, int value);
extern void MakeLand_RampPixel_bmp (int *bitmap, int index, int value);
extern void MakeLand_blit (int *pTempBitmapSource,
                           int *TempBitmapDest,
                           int i, int j, int x, int y, int Width, int Height);
extern void MakeLand_HeightsScaleMax (int top);;
extern void MakeLand_Clear (int *_bitmap, int value);
extern void MakeLand_HeightsCreate ();
extern void MakeLand_HeightsSmooth (int source_weight, int iterations);
extern void MakeLand_Erosion (int cut,
                              int shade,
                              int intensity, int raylength, int crispness);
extern void MakeLand_ColorsBlur (int source_weight,
                                 int iterations, int PalFlag);
extern void MakeLand_ColorsShade (int intensity,
                                  char direction,
                                  int BlurIterations,
                                  int BlurSourcePixelWeight, int Max);
extern void MakeLand_ErosionFine (int cut,
                                  int shade,
                                  int intensity,
                                  int raylength, int crispness);
extern void MakeLand_Level (int height);
extern void MakeLand_ColorsRGBtoBGR ();
extern void MakeLand_TerrainCraters (int NumCraters, int MaxRadius,
                                     int MinRadius,
                                     double DepthToRadiusRatio,
                                     double RimHeightToRadiusRatio,
                                     double RimWidthToRadiusRatio,
                                     double RimDecayFactor,
                                     double TailZeroFactor,
                                     int UnderlyingDetailRetention,
                                     int PowerLaw, int StopSize);
