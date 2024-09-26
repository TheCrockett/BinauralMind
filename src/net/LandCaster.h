
/***************************************************************************
 *            LandCaster_c.h
 *
 *  Fri Jan 20 17:43:43 2006
 *  Copyright  2006  Bret Logan
 *  Email gnaural@sourceforge.net
 ****************************************************************************/
/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _LANDCASTER_C_H
#define _LANDCASTER_C_H

#ifdef __cplusplus
extern "C"
{
#endif
 //Defines:
#define LC_SHIFT          10    /*10 is normal - but don't change this without changing LC_DOUBLESHIFT! */
#define LC_DOUBLESHIFT    20    /*always must be double LC_SHIFT */
#define LC_YSHIFT         19    /*always one less than LC_DOUBLESHIFT */
#define LC_SPEEDSHIFT     4     /*4 works with LC_SHIFT of 10 */
#define LC_VISIBLE  1
#define LC_TARGET   1
#define LC_SKINNY    1
#define LC_SIDEWAYS   0
#define LC_FACING     2
#define LC_CHANCE     0
#define LC_LINEDUP    1
#define LC_MISSILE  0
#define LC_OBJECTHIT  2
#define LC_GROUND     3
#define LC_STARTING_RAYLENGTH 1024

 //======================================
 // Global Variables:
 //   User MUST set these (Do it through LC_LandCaster_Initialize():
 typedef struct FLYINGOBJECT_TYPE
 {
  int bExternalBitmap;          //this gets set true only if user gives bitmap
  int Type;
  int Speed;
  int Turn;
  int Visible;
  int Alt;
  int Direction;
  int Lift;
  int SizeX;                    //width of the texture bitmap
  int SizeY;                    //height of the texture bitmap
  int PosX;                     //scaled by <<10
  int PosY;                     //scaled by <<10
  int *pErase;                  //usually needs to be allotted to SizeY; unless a "3D" object
  int *pTextureBitmap;          //address to first byte of texture bitmap
 } FLYINGOBJECT;
 extern FLYINGOBJECT *LC_FlyingObject;

 typedef struct AUTOPILOTDATA_TYPE
 {
  int Turn;
  int Lift;
  int EyeLevel;
  int running;                  //if non-zero, it's running
 } AUTOPILOTDATA;
 extern AUTOPILOTDATA LC_AP;

 typedef struct LC_BITMAP_TYPE
 {
  //  unsigned char * pixel;
  int *pixel;                   //address of first byte of color data bitmap
  int datasize;                 //size of pixel, either 3 chars  for rgb or 4 chars for rgbp
  int w;                        //width
  int h;                        //height
 } LC_BITMAP;

 //the bitmaps: 
 extern LC_BITMAP LC_BmpDest;
 extern LC_BITMAP LC_BmpColors;
 extern LC_BITMAP LC_BmpHeights;
 extern LC_BITMAP LC_BmpSky;

 //general globals:
 extern long LC_BmpColorsHeight_scaled; //internal use
 extern long LC_BmpColorsWidth_scaled;  //internal use
 /*User doesn't have to set any of these: */
 extern int LC_SkyFlag;         /*if present, and there is a sky bitmap, will draw sky */
 extern int LC_MaxFrameWidth;
 extern int *LC_Sin;
 extern int *LC_Cos;
 extern int *LC_Correction;
 extern int *LC_DistanceScaling;
 extern int *LC_TextureScaling;
 extern int LC_PositionY;       /*X,Y Po extern int on Map scaled by <<LC_SHIFT */
 extern int LC_PositionX;       /*X,Y Po extern int on Map scaled by <<LC_SHIFT */
 extern int LC_Roll;            /*never set less than zero or more than circlerez; use LC_SetRoll(); */
 extern int LC_Tilt;            /*sets tilt of view */
 extern int LC_Altitude;        /*sets altitude of view */
 extern int LC_Direction;       /*set view direction based on full circle of LC_CircleRez */
 extern int LC_CircleRez;       /*sets the number of steps in everything circular */
 extern int LC_Raylength;       /*determines how far to raycast */
 extern int LC_InitFlag;        /*ensures I have Initted stuff before running Landscape() */
 extern int *LC_TextureStepX;   /*array for LC_FlyingObject funcs */
 extern int *LC_TextureStepCountX;      /*array for LC_FlyingObject funcs */
 extern int *LC_TextureStepCountY;      /*array for LC_FlyingObject funcs */
 extern int *LC_StartDraw;      /*array for LC_FlyingObject funcs */
 extern int *LC_StopDraw;       /*array for LC_FlyingObject funcs */
 extern int **LC_TextureBitmapIndex;    /*array for LC_FlyingObject funcs */
 extern unsigned short int *LC_BmpFlyingObjects;        /* extern internal bitmap showing location of all objects MAX_NUMBER_OF_OBJECTS == 255 */
 extern int LC_MaxRayDistance;
 extern int LC_WaterHeight;     /*determines height below which renderer blends sky pixels with ground */
 extern int LC_ResolutionInterval;      /*the distance at which resolution first gets reduced */
 extern int LC_FlyingObjectCount;       /* max number of objects in landscape */
 extern int LC_TargetOrientation;
 extern int LC_MissileControl;
 extern int LC_Turn;            /*sets rate of turn and direction of turn (+/-). 0 means no change */
 extern int LC_Lift;            /*sets climb or sink */
 extern int LC_Speed;           /*0 means no movement. can be negative */
 extern int LC_UserData1;       /*used to make certain data available when messages are sent */
 extern int LC_UserData2;       /*used to make certain data available when messages are sent */
 extern void LC_SetCallbackFast (void (*CallbackFunction) (int message));
 extern void (*LC_pUserFunctionFast) (int message);     /*fast, for sending messages in rendering loops */
 extern int LC_UserSky;
 extern int LC_Seed;
 extern int LC_BmpDestWidthCenter;
 extern int LC_BmpDestHeightCenter;
 extern int LC_DestLowerLeftStartPoint;
 extern int LC_MaxLowerLeftStartPoint;
 extern float LC_RadianToCircleRezFactor;
 extern int LC_Scale;
 extern int LC_Resolution;
 extern int LC_TargetSpeed;
 extern int LC_MissileSpeed;
 extern int LC_TargetStartPos;
 extern int LC_TargetSize;      /*between 1 and 12. Default is 5  */
 extern int LC_FacedTargetFlag; /*non-zero add directional mark on target */
 extern const double LC_SHIFTFACTOR;

/*====================================== */
 /*Functions: */
 extern int LC_DoMovement (int groundcollisionflag);
 extern void LC_SetRoll (int roll);
 extern int LC_InitTables (void);
 extern int LC_InitCorrectionTable (void);
 extern int LC_CreateSomeFlyingObjects (void);
 extern void LC_DoFlyingObjectsMovement (void);
 extern void LC_LandscapeRollSkyFlyingObjects (void);
 extern void LC_UserFunctionFast (int message);
 extern void LC_LandCaster_Cleanup (void);
 extern int LC_DeleteFlyingObjectsResources (void);
 extern void LC_InitVars (void);
 extern void LC_MakeSky (int Size);
 extern void LC_InitVars (void);
 extern void LC_Blur (int *OrigBitmap, int width, int height,
                      int blur_factor);
 extern void LC_PlasmaSky (void);
 extern void LC_Plasma (int *bitmap, int Size, int RandFactor, int floor,
                        int ceiling);
 extern void LC_ConvertIntBitmapToPixels (int *bitmap, int w, int h);
 extern int LC_SetNumberOfFlyingObjects (int number_of_objects);
 extern int LC_SetLC_Raylength (int length);
 extern int LC_CreateFlyingObjectsResources (void);
 extern int LC_SetSkyFlag (int state);
 extern int LC_rand_hi (int hi_limit);
 extern int LC_InitLandscape (void);    /*must be called before using anything */
 extern void LC_ZeroContrast (int *OrigBitmap, int width, int height,
                              int zero_thresh);
 extern void LC_CopyBitmap (int *pTempBitmapSource, int *pTempBitmapDest,
                            int i, int j, int x, int y, int Width,
                            int Height);
 extern void LC_pixelput (int *pTempBitmap, int x_temp, int y_temp, int val,
                          int width);
 extern int LC_pixelget (int *pTempBitmap, int x_temp, int y_temp, int width);
 extern void LC_LandCaster_Initialize (int *pLC_BmpDestTemp,
                                       int LC_BmpDestWidthTemp,
                                       int LC_BmpDestHeightTemp,
                                       int *pLC_BmpHeightsTemp,
                                       int LC_BmpHeightsWidthTemp,
                                       int LC_BmpHeightsHeightTemp,
                                       int *pLC_BmpColorsTemp,
                                       int LC_BmpColorsWidthTemp,
                                       int LC_BmpColorsHeightTemp,
                                       int _FlyingObjectCount,
                                       int _SkySize, int seed);
 extern int LC_SetupFlyingObject (int id,       /*the only required parameter */
                                  int type, int x, int y, int altitude, int speed, int direction, int turn, int visible, unsigned char red, unsigned char green, unsigned char blue, int *pFlyingObjectBitmapTemp,      /*optional - can leave empty */
                                  int FlyingObjectBitmapWidthTemp,      /* optional - can leave empty */
                                  int FlyingObjectBitmapHeightTemp);    /* optional - can leave empty */
 extern int LC_MakeMissile (int id, unsigned char Red, unsigned char Green,
                            unsigned char Blue);
 extern int LC_MakeTarget (int id, unsigned char Red, unsigned char Green,
                           unsigned char Blue);
 extern void LC_DumbCircle (int *Bitmap, int BitmapWidth, int Radius,
                            int color, int X, int Y);
 extern void LC_FlyingObjectConstructor (FLYINGOBJECT * tempFlyingObject);
 extern void LC_FlyingObjectDestructor (FLYINGOBJECT * tempFlyingObject);
 extern void LC_PixelFly ();
 extern void LC_AutoPilot (int Seek,    //Seek=1 seeks high ground; -1 = low
                           int ScanWidth);      //distance from first sample to third (last) sample taken
 extern void LC_AutoAlt ();
 extern void LC_TargetFollowTheLeader ();       //aka BlindFollowBlind
 extern void LC_ShowColorsBitmap ();
#ifdef __cplusplus
}
#endif

#endif                          /* _LANDCASTER_C_H */
