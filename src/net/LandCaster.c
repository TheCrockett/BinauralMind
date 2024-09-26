// LandCaster.c - Barebones raycasting renderer in C by Bret Logan
//Copyright (C) 2010 Bret Byron Logan (gnaural@sourceforge.net)
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//      
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//      
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

   //To use, just load-up LC_LandCaster_Initialize(), then call LC_LandscapeRollSkyFlyingObjects() and LC_DoMovement()
   //until you are finished using it. Then call LC_LandCaster_Cleanup() when you are done. Do
   //LC_SetRoll(LC_CircleRez>>1);
   //at some pre-drawing point if image is inverted -- in which case, you should probably
   //reverse "gravity" too:       
   //LC_Lift=-LC_Lift;

#include <stdlib.h>
#include <math.h>
#include "LandCaster.h"

//======================================
//Defines:
#define LC_SHIFT          10    //10 is normal - but don't change this without changing LC_DOUBLESHIFT!
#define LC_DOUBLESHIFT    20    //always must be double LC_SHIFT
#define LC_YSHIFT         19    //always one less than LC_DOUBLESHIFT
#define LC_SPEEDSHIFT     4     //4 works with LC_SHIFT of 10
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
//Global Variables:
//User MUST set these:
LC_BITMAP LC_BmpDest;
LC_BITMAP LC_BmpColors;         //address of first byte of color data bitmap
LC_BITMAP LC_BmpHeights;
LC_BITMAP LC_BmpSky;
 //User doesn't have to set these:
int LC_SkyFlag;                 //if present, and there is a sky bitmap, will draw sky
int LC_MaxFrameWidth;
int *LC_Sin = NULL;
int *LC_Cos = NULL;
int *LC_Correction = NULL;
int *LC_DistanceScaling = NULL;
int *LC_TextureScaling = NULL;
typedef struct ROLLDATA_TYPE
{
 int StartX;
 int StartY;
 unsigned short endpoints;
} ROLLDATA;
ROLLDATA *LC_RollData = NULL;
AUTOPILOTDATA LC_AP;
int LC_PositionY;               //X,Y Point on Map scaled by <<LC_SHIFT
int LC_PositionX;               //X,Y Point on Map scaled by <<LC_SHIFT
int LC_Roll = 0;                //never set less than zero or more than circlerez; use LC_SetRoll();
int LC_Tilt = 0;                //sets tilt of view
int LC_Altitude = 0;            //sets altitude of view
int LC_Direction = 0;           //set view direction based on full circle of LC_CircleRez
int LC_Turn;                    //sets rate of turn and direction of turn (+/-). 0 means no change
int LC_Lift;                    //sets climb or sink
int LC_Speed;                   //0 means no movement. can be negative
int LC_Eyelevel;                //height above ground at collision
int LC_CircleRez;               //sets the number of steps in everything circular
int LC_Raylength;               //determines how far to raycast 
long LC_BmpColorsHeight_scaled;
long LC_BmpColorsWidth_scaled;
int LC_InitFlag;                //ensures I have Initted stuff before running Landscape()
int *LC_TextureStepX;           //array for LC_FlyingObject funcs
int *LC_TextureStepCountX;      //array for LC_FlyingObject funcs
int *LC_TextureStepCountY;      //array for LC_FlyingObject funcs
int *LC_StartDraw;              //array for LC_FlyingObject funcs
int *LC_StopDraw;               //array for LC_FlyingObject funcs
int **LC_TextureBitmapIndex;    //array for LC_FlyingObject funcs
unsigned short int *LC_BmpFlyingObjects;        //internal bitmap showing location of all objects MAX_NUMBER_OF_OBJECTS == 255
FLYINGOBJECT *LC_FlyingObject = NULL;
int LC_MaxRayDistance = 0;
int LC_WaterHeight = 0;         //determines height below which renderer blends sky pixels with ground
int LC_ResolutionInterval = 384;        //the distance at which resolution first gets reduced
int LC_FlyingObjectCount;       // max number of objects in landscape
int LC_TargetOrientation;
int LC_MissileControl;
int LC_UserData1;               //used to make certain data available when messages are sent
int LC_UserData2;               //used to make certain data available when messages are sent
void (*LC_pUserFunctionFast) (int message);     //fast, for sending messages in rendering loops
int LC_UserSky;
int LC_Seed;
int LC_BmpDestWidthCenter;
int LC_BmpDestHeightCenter;
int LC_DestLowerLeftStartPoint;
int LC_MaxLowerLeftStartPoint;
float LC_RadianToCircleRezFactor;
int LC_Scale;
int LC_Resolution;
int LC_TargetSpeed;
int LC_MissileSpeed;
int LC_TargetStartPos;
int LC_TargetSize;              //between 1 and 12. Default is 5
int LC_FacedTargetFlag;         //non-zero add directional mark on target
const double LC_SHIFTFACTOR = (1 << LC_SHIFT) * 1.0;

//=========================================================================

//START WITH THIS FUNCTION
/////////////////////////////////////////////////////////////////////
//void LC_LandCaster_Initialize(int *pLC_BmpDestTemp,
/////////////////////////////////////////////////////////////////////
void LC_LandCaster_Initialize (int *pLC_BmpDestTemp,
                               int LC_BmpDestWidthTemp,
                               int LC_BmpDestHeightTemp,
                               int *pLC_BmpHeightsTemp,
                               int LC_BmpHeightsWidthTemp,
                               int LC_BmpHeightsHeightTemp,
                               int *pLC_BmpColorsTemp,
                               int LC_BmpColorsWidthTemp,
                               int LC_BmpColorsHeightTemp,
                               int _FlyingObjectCount, int _SkySize, int seed)
{
 LC_InitVars ();
 LC_BmpDest.pixel = pLC_BmpDestTemp;
 LC_BmpDest.w = LC_BmpDestWidthTemp;
 LC_BmpDest.h = LC_BmpDestHeightTemp;
 LC_BmpHeights.pixel = pLC_BmpHeightsTemp;
 LC_BmpHeights.w = LC_BmpHeightsWidthTemp;
 LC_BmpHeights.h = LC_BmpHeightsHeightTemp;
 LC_BmpColors.pixel = pLC_BmpColorsTemp;
 LC_BmpColors.w = LC_BmpColorsWidthTemp;
 LC_BmpColors.h = LC_BmpColorsHeightTemp;
 LC_Seed = seed;
 LC_MakeSky (_SkySize);
 LC_SetNumberOfFlyingObjects (_FlyingObjectCount);

 LC_InitLandscape ();
}

/////////////////////////////////////////////////////////////////////
//int LC_InitLandscape()//must be called before using anything
/////////////////////////////////////////////////////////////////////
int LC_InitLandscape ()         //must be called before using anything
{
 LC_InitFlag = 0;       //just in case LC_InitLandscape() exits early, set this low
 //all of these must be set to do anything later:
 if (LC_BmpColors.w < 1 || LC_BmpColors.h < 1 || LC_BmpHeights.w < 1
     || LC_BmpHeights.h < 1 || LC_BmpDest.w < 1 || LC_BmpDest.h < 1)
 {
  return 0;     //user didn't initialize enough
 }
 LC_MaxFrameWidth =
  (int) (sqrt
         (LC_BmpDest.w * LC_BmpDest.w + LC_BmpDest.h * LC_BmpDest.h) + 3);
 //if (!(LC_MaxFrameWidth%2)) ++LC_MaxFrameWidth;
 //next line solves out of range subscript problems
 if (LC_CircleRez < (LC_MaxFrameWidth << 1))
  LC_CircleRez = (LC_MaxFrameWidth << 1);
 if (LC_Direction >= LC_CircleRez)
  LC_Direction = LC_CircleRez - (LC_CircleRez / 8);
 LC_BmpColorsWidth_scaled = LC_BmpColors.w << LC_SHIFT;
 LC_BmpColorsHeight_scaled = LC_BmpColors.h << LC_SHIFT;
 LC_BmpDestWidthCenter = LC_BmpDest.w >> 1;
 LC_BmpDestHeightCenter = LC_BmpDest.h >> 1;
 LC_DestLowerLeftStartPoint = (LC_BmpDest.w * (LC_BmpDest.h - 1));
 LC_MaxLowerLeftStartPoint = (LC_MaxFrameWidth * (LC_MaxFrameWidth - 1));
 LC_RadianToCircleRezFactor =
  (float) ((double) LC_CircleRez / 6.28318530717958647692528676655901);

 //now do allocations:
 //since everything should equal NULL or something 
 //valid, no problem deleting... right?
 if (LC_Sin != NULL)
  free (LC_Sin);
 if (LC_Cos != NULL)
  free (LC_Cos);
 if (LC_Correction != NULL)
  free (LC_Correction);
 if (LC_DistanceScaling != NULL)
  free (LC_DistanceScaling);
 if (LC_TextureScaling != NULL)
  free (LC_TextureScaling);
 if (LC_RollData != NULL)
  free (LC_RollData);

 LC_Sin = (int *) calloc (LC_CircleRez, sizeof (int));
 LC_Cos = (int *) calloc (LC_CircleRez, sizeof (int));
 LC_Correction = (int *) calloc (LC_MaxFrameWidth, sizeof (int));
 LC_DistanceScaling = (int *) calloc (LC_Raylength, sizeof (int));
 LC_TextureScaling = (int *) calloc (LC_Raylength, sizeof (int));
 LC_RollData = (ROLLDATA *) calloc (LC_CircleRez, sizeof (ROLLDATA));

 if (!LC_Sin || !LC_Cos || !LC_Correction || !LC_DistanceScaling
     || !LC_TextureScaling || !LC_RollData)
  return 0;

 if (!LC_InitTables ())
  return 0;
 LC_InitFlag = 1;       //did inits; set flag

 return 1;
}

/////////////////////////////////////////////////////////////////////
// tables() - set up sin, cos, perspective, and correction tables
/////////////////////////////////////////////////////////////////////
int LC_InitTables ()
{
 int i,
  j,
  limit = LC_CircleRez + LC_Raylength + LC_MaxFrameWidth;
 double temp_angle;

 if (LC_CircleRez >= LC_Raylength && LC_CircleRez >= LC_MaxFrameWidth)
  limit = LC_CircleRez;
 if (LC_Raylength >= LC_CircleRez && LC_Raylength >= LC_MaxFrameWidth)
  limit = LC_Raylength;
 if (LC_MaxFrameWidth >= LC_Raylength && LC_MaxFrameWidth >= LC_CircleRez)
  limit = LC_MaxFrameWidth;

 for (i = 0; i < limit; i++)
 {
  if (i < LC_CircleRez)
  {     //start all less that LC_CircleRez
   temp_angle = (2.0 * M_PI * (LC_CircleRez - i)) / LC_CircleRez;
   //above makes LC_CircleRez equate with radians
   LC_Cos[i] = (int) (LC_SHIFTFACTOR * cos (temp_angle));
   LC_Sin[i] = (int) (LC_SHIFTFACTOR * sin (temp_angle));

   //Now I load the LC_Roll tables
   //Cos is conventionally associated with X, Sin with Y. if LC_Roll = 0, then I want to do 
   //a straight (literally) x=0, y=1 copy of pMaxBitmap to LC_BmpDest. But Sin(0)=0, and
   //Cos(0)=1, so trying to copy like this would have me moving across X on pMaxBitmap 
   //while making Y columns on LC_BmpDest. To correct this, I need to rotate the LC_Roll variable 
   //in my setup of the tables 1/4 turn so that when LC_Roll=0, Sin gives me 0 and Cos gives 1.
   //also keep in mind that it is arbitrary that I use LC_CircleRez for LC_Roll.
   LC_RollData[i].StartX =
    (int) (((LC_BmpDest.w << (LC_SHIFT - 1)) -
            (sin (temp_angle) * (LC_SHIFTFACTOR / 2.0) * LC_MaxFrameWidth)) +
           (cos (temp_angle) * (LC_SHIFTFACTOR / 2.0) * LC_MaxFrameWidth));
   LC_RollData[i].StartY =
    (int) (((LC_BmpDest.h << (LC_SHIFT - 1)) -
            (cos (temp_angle) * (LC_SHIFTFACTOR / 2.0) * LC_MaxFrameWidth)) -
           (sin (temp_angle) * (LC_SHIFTFACTOR / 2.0) * LC_MaxFrameWidth));
   //the following is simply an algebraic simplification of above. Not sure why it is right, incidentally
   //int StartPointX=(int)((LC_BmpDest.h<<(LC_SHIFT-1))+(cos(temp_angle)-sin(temp_angle))*(LC_SHIFTFACTOR/2.0)*FrameSize*0.7);
   //int StartPointY=(int)((LC_BmpDest.w <<(LC_SHIFT-1))-(sin(temp_angle)+cos(temp_angle))*(LC_SHIFTFACTOR/2.0)*FrameSize*0.7);

   //The following section gives a specific X start point for rendering
   //according to how much LC_Roll  is present. Not required, just helps. Its sole reason for
   //existing: to cut down on wasted (not used) rendering to the sides of MaxBitmap-
   //stuff that would never get seen when transfered to LC_BmpDest.
   //It works by comparing all four corners of the cookie-cut of the screen
   //at the given LC_Roll i, and uses the one closest to the left for
   //start point reference for that given LC_Roll.
   j =
    ((LC_MaxFrameWidth << (LC_SHIFT - 1)) -
     (-LC_Sin[i] * LC_BmpDestHeightCenter)) -
    (LC_Cos[i] * LC_BmpDestWidthCenter);

   if (j >
       ((LC_MaxFrameWidth << (LC_SHIFT - 1)) -
        (-LC_Sin[i] * LC_BmpDestHeightCenter)) +
       (LC_Cos[i] * LC_BmpDestWidthCenter))
    j =
     ((LC_MaxFrameWidth << (LC_SHIFT - 1)) -
      (-LC_Sin[i] * LC_BmpDestHeightCenter)) +
     (LC_Cos[i] * LC_BmpDestWidthCenter);

   if (j >
       ((LC_MaxFrameWidth << (LC_SHIFT - 1)) +
        (-LC_Sin[i] * LC_BmpDestHeightCenter)) +
       (LC_Cos[i] * LC_BmpDestWidthCenter))
    j =
     ((LC_MaxFrameWidth << (LC_SHIFT - 1)) +
      (-LC_Sin[i] * LC_BmpDestHeightCenter)) +
     (LC_Cos[i] * LC_BmpDestWidthCenter);

   if (j >
       ((LC_MaxFrameWidth << (LC_SHIFT - 1)) +
        (-LC_Sin[i] * LC_BmpDestHeightCenter)) -
       (LC_Cos[i] * LC_BmpDestWidthCenter))
    j =
     ((LC_MaxFrameWidth << (LC_SHIFT - 1)) +
      (-LC_Sin[i] * LC_BmpDestHeightCenter)) -
     (LC_Cos[i] * LC_BmpDestWidthCenter);

   //   LC_RollData[i].endpoints=((j+511)>>LC_SHIFT);//added 511 to "round" it up. 011219
   LC_RollData[i].endpoints = ((j + (int) ((LC_SHIFTFACTOR / 2.0) - 1)) >> LC_SHIFT);   //added 511 to "round" it up. 011219
   //LC_RollData[i].endpoints=0;//for debugging

  }     //End all less that LC_CircleRez

  if (i < LC_Raylength)
  {     //start all less than LC_Raylength
   //   LC_DistanceScaling[i]=(int)((LC_SHIFTFACTOR/2.0)+(((1<<LC_SCALESHIFT)<<LC_SHIFT)/(i+1.0)));//64 used to be LC_Scale. Don't change.
   //   int i_inv=LC_Raylength-i-1;
   LC_DistanceScaling[i] = (int) (LC_Scale / (i + 1.0));
   //   LC_DistanceScaling[i]=(int)(LC_Scale/(i_inv+1.0));
   if (0 == LC_DistanceScaling[i])
    LC_DistanceScaling[i] = 1;
   LC_TextureScaling[i] = (1 << LC_DOUBLESHIFT) / LC_DistanceScaling[i];
  }     //end all less than LC_Raylength
 }      //end major loop

 //now do correction table:
 LC_InitCorrectionTable ();
 return 1;
}

/////////////////////////////////////////////////////////////////////
// LC_InitCorrectionTable() - does 1/cos correction for LC_Roll scenario
/////////////////////////////////////////////////////////////////////
int LC_InitCorrectionTable ()
{
 int i;
 for (i = 0; i < LC_MaxFrameWidth; i++)
 {      //start fisheye-correction
  //below:figure angles from half-screen_width left and right of my POV
  LC_Correction[i] =
   (int) (0.5 +
          ((LC_Resolution << LC_SHIFT) /
           cos (((2.0 * M_PI * i) / LC_CircleRez) -
                ((2.0 * M_PI * 0.5 * LC_MaxFrameWidth) / LC_CircleRez))));
 }      //end fisheye correction
 //next equation figures longest possible *distance* to throw ray
 //with the available LC_Raylength number of *steps*. The shortest
 //distance is what I want to know, so I don't go over the subscript
 //range for it. Hence, I multiply the middle ray's step length by 
 //the number of steps my subscrript allows for (LC_Raylength-1). 
 LC_MaxRayDistance =
  (int) ((LC_Raylength - 1) * LC_Correction[LC_MaxFrameWidth >> 1]);
 LC_Tilt = LC_MaxFrameWidth >> 1;
 return 1;
}

/////////////////////////////////////////////////////////////////////
// LC_LandscapeRollSkyFlyingObjects() - 021024 - THE MAIN RENDERING ROUTINE
/////////////////////////////////////////////////////////////////////
void LC_LandscapeRollSkyFlyingObjects ()
{
 LC_DoFlyingObjectsMovement ();

 //debug lines:
 //static int Bilbo = 0;
 //LC_SetRoll (Bilbo += 100);
 //if (Bilbo > 2559)
 // Bilbo = 0;

 //FrameSize is the Width and Height to be false-rendered around LC_BmpDest
 //if you make it LC_MaxFrameWidth, you can be sure it will correctly
 //render every roll. it could be dynamic, changing each frame - but a major
 //problem arises from a different effective "tilt" and sky placement for
 //each setting.
 //First prepare all the vars:
 int HStepX = -LC_Cos[LC_Roll]; //0 degrees is now to the right
 int HStepY = -LC_Sin[LC_Roll]; //0 degrees is now to the right
 int VStepX = LC_Sin[LC_Roll];
 int VStepY = -LC_Cos[LC_Roll];
 int OldDrawPointY = 0;
 int OldDrawPointX = 0;
 //if (LC_Altitude<LC_WaterHeight) LC_Altitude=LC_WaterHeight;

 ////////////////
 //now set up Dynamic X
 int FrameSize = LC_MaxFrameWidth;
 int xend = LC_RollData[LC_Roll].endpoints;     //variable startpoint for x, depending on roll
 int x = FrameSize - xend;
 //end Dynamic X

 //this next is used instead of a LC_Roll lookup entry:020119
 //double temp_angle=(2.0*LC_PI*(LC_CircleRez-LC_Roll))/LC_CircleRez;
 //int StartPointX=(int)(((LC_BmpDest.w <<(LC_SHIFT-1))-(sin(temp_angle)*(LC_SHIFTFACTOR/2.0)*FrameSize))+(cos(temp_angle)*(LC_SHIFTFACTOR/2.0)*FrameSize));
 //int StartPointY=(int)(((LC_BmpDest.h<<(LC_SHIFT-1))-(cos(temp_angle)*(LC_SHIFTFACTOR/2.0)*FrameSize))-(sin(temp_angle)*(LC_SHIFTFACTOR/2.0)*FrameSize));
 //the following is simply an algebraic simplification of above. Don't know why it is right, incidentally
 // int StartPointX=(int)((LC_BmpDest.h<<(LC_SHIFT-1))+(cos(temp_angle)-sin(temp_angle))*(LC_SHIFTFACTOR/2.0)*FrameSize*0.7);
 // int StartPointY=(int)((LC_BmpDest.w <<(LC_SHIFT-1))-(sin(temp_angle)+cos(temp_angle))*(LC_SHIFTFACTOR/2.0)*FrameSize*0.7);

 //this is simply an all-int version, for speed:
 //int StartPointX=(int)((LC_BmpDest.w <<(LC_SHIFT-1))-(LC_Sin[LC_Roll]*FrameSize>>1)+(LC_Cos[LC_Roll]*FrameSize>>1));
 //int StartPointY=(int)((LC_BmpDest.h<<(LC_SHIFT-1))-(LC_Cos[LC_Roll]*FrameSize>>1)-(LC_Sin[LC_Roll]*FrameSize>>1));
 //the following is simply an algebraic simplification of above. Don't know why it is right, incidentally
 //int StartPointY=(LC_BmpDest.w <<(LC_SHIFT-1))-(((LC_Sin[LC_Roll]+LC_Cos[LC_Roll])*FrameSize)>>1);
 //int StartPointX=(LC_BmpDest.h<<(LC_SHIFT-1))+(((LC_Cos[LC_Roll]-LC_Sin[LC_Roll])*FrameSize)>>1);
 //next one includes dynamic width:
 //int StartPointY=((LC_BmpDest.w <<(LC_SHIFT-1))-(((LC_Sin[LC_Roll]+LC_Cos[LC_Roll])*FrameSize)>>1))-HStepY*xend;
 //int StartPointX=((LC_BmpDest.h<<(LC_SHIFT-1))+(((LC_Cos[LC_Roll]-LC_Sin[LC_Roll])*FrameSize)>>1))+HStepX*xend;

 int StartPointX = LC_RollData[LC_Roll].StartX + HStepX * xend;
 int StartPointY = LC_RollData[LC_Roll].StartY - HStepY * xend;

 //now determine draw slope:
 int SLOPEY,
  SLOPEX;
 if (StartPointY < (StartPointY - VStepY))
  SLOPEY = LC_BmpDest.w;
 else
  SLOPEY = -LC_BmpDest.w;
 if (StartPointX < (StartPointX + VStepX))
  SLOPEX = 1;
 else
  SLOPEX = -1;

 //initialize sky variables:
 int *pSkyEnd = LC_BmpSky.pixel + LC_BmpSky.w * LC_BmpSky.h;
 int SkyX = LC_Direction - xend;
 //SkyX is now actually on Y axis of LC_BmpSky, for speed - 020119
 if (SkyX >= LC_BmpSky.h)
  do
  {
   SkyX -= LC_BmpSky.h;
  }
  while (SkyX >= LC_BmpSky.h);
 int SkyTilt = LC_Tilt;
 if (SkyTilt >= LC_BmpSky.w)
  do
   SkyTilt -= LC_BmpSky.w;
  while (SkyTilt >= LC_BmpSky.w);
 else if (SkyTilt < 0)
  do
   SkyTilt += LC_BmpSky.w;
  while (SkyTilt < 0);
 //end sky initialization

 int height;
 int color;
 int holemark;
 int draw_length;
 //object vars: 
 int tempcount;
 int id;
 //end object vars
 int x_offset;
 //******Start X******
 do     // x is the number of rays I send
 {
  int y = 0;                    //needed for array indexing
  int yinc = 1;
  int y_distance = 0;
  int y_distance_step = LC_Correction[x];
  int y_origdiststep = y_distance_step;

  int top_height = FrameSize;
  int DrawPointX = StartPointX; //go to bottom of screen to start line
  int DrawPointY = StartPointY; //go to bottom of screen to start line
  int *pScreenIndex = 0;
  int DrawFlag = 0;             //every object pixel that gets hit advances this arrays-subscript
  int DrawCount = 0;
  int OldFlyingObjectPixel = -1;        //make sure I treat first pass freshly
  //next line starts my first column on the left side of screen
  if ((x_offset = LC_Direction - (FrameSize >> 1) + x) >= LC_CircleRez)
   x_offset -= LC_CircleRez;
  else if (x_offset < 0)
   x_offset += LC_CircleRez;
  int x_step = (LC_Cos[x_offset] * LC_Correction[x]) >> LC_DOUBLESHIFT;
  int x_origstep = x_step;
  int y_step = (LC_Sin[x_offset] * LC_Correction[x]) >> LC_DOUBLESHIFT;
  int y_origstep = y_step;
  int x_int = LC_PositionX;
  int y_int = LC_PositionY;

  int *pSkyStart = LC_BmpSky.w * SkyX + LC_BmpSky.pixel + SkyTilt;
  if (pSkyStart >= pSkyEnd)
   do
    pSkyStart -= LC_BmpSky.h;
   while (pSkyStart >= pSkyEnd);
  else if (pSkyStart < LC_BmpSky.pixel)
   do
    pSkyStart += LC_BmpSky.h;
   while (pSkyStart < LC_BmpSky.pixel);
  //////////////////////////////////////
  //******Start Y********
  do
  {
   if ((y_int += y_step) < 0)
   {
    y_int += LC_BmpColorsHeight_scaled;
    OldFlyingObjectPixel = -1;
   }
   else if (y_int >= LC_BmpColorsHeight_scaled)
   {
    y_int -= LC_BmpColorsHeight_scaled;
    OldFlyingObjectPixel = -1;  //necessary for doing multiple instances properly
   }
   if ((x_int += x_step) < 0)
   {
    x_int += LC_BmpColorsWidth_scaled;
    OldFlyingObjectPixel = -1;
   }
   else if (x_int >= LC_BmpColorsWidth_scaled)
   {
    x_int -= LC_BmpColorsWidth_scaled;
    OldFlyingObjectPixel = -1;
   }

   //#########start PRE DRAW ########## Predraw loads drawing arrays.
   //the point of this: Drawing objects is still about drawing landscape, but
   //with the slight difference of checking the Start and Stop arrays 
   //to see if the point at which I am drawing on the screen should
   //draw an object instead of land
   //Note: color was just handy to use temporarily here to hold raw bitmap subscript:
   //get bitmap position:
   int data_index =
    (y_int >> LC_SHIFT) * LC_BmpHeights.w + (x_int >> LC_SHIFT);
   //make sure I'm not just hitting the same object pixel again and if not,
   //get the column data (if any) from LC_BmpFlyingObjects and reference it to the texture:
   if (data_index != OldFlyingObjectPixel &&
       (LC_TextureStepCountY[DrawFlag] = LC_BmpFlyingObjects[data_index]))
   {    //it wasn't the same pixel again, and there was data on that pixel, so:
    //remember this subscript for next pass:
    OldFlyingObjectPixel = data_index;
    //    id=LC_TextureStepCountY[DrawFlag]>>6;//for int LC_BmpFlyingObjects (3 objects max)
    id = LC_TextureStepCountY[DrawFlag] >> 8;   //for unsigned short int LC_BmpFlyingObjects
    //get the column of the object's texture bmp:
    //note- I subtract 1 from next because column data is coded +1 so column
    //zero is discernable from background zeros.
    //i.e., so I don't lose a column (column 1 is 0)
    //    LC_TextureStepCountY[DrawFlag]=(LC_TextureStepCountY[DrawFlag]&63)-1;//for uchar bitmap
    LC_TextureStepCountY[DrawFlag] = (LC_TextureStepCountY[DrawFlag] & 255) - 1;        //for short bitmap

    //get scaled step size for walking over the texture bmp:
    LC_TextureStepX[DrawFlag] = LC_TextureScaling[y_distance >> LC_YSHIFT];

    //get the absolute screen height of the bottom of the object:
    LC_StartDraw[DrawFlag] = FrameSize - LC_Tilt - (((LC_Altitude - LC_FlyingObject[id].Alt) * LC_DistanceScaling[y_distance >> LC_YSHIFT]) >> LC_SHIFT);       //020202
    //now get the end screen height for the object column by adding scaled object sizeX:
    LC_StopDraw[DrawFlag] = LC_StartDraw[DrawFlag] + ((LC_FlyingObject[id].SizeX * LC_DistanceScaling[y_distance >> LC_YSHIFT]) >> LC_SHIFT);   //020202

    //reset texture step to zero to get ready to begin walking the texture
    LC_TextureStepCountX[DrawFlag] = 0;
    //set my handy index pointer to the exact place in the texture bitmap
    //that the object bitmap starts drawing from:
    LC_TextureBitmapIndex[DrawFlag] = LC_FlyingObject[id].pTextureBitmap + (LC_TextureStepCountY[DrawFlag] * LC_FlyingObject[id].SizeX);        //don't forget this
    ++DrawFlag;
    //increment drawflag, to keep total count of object parts I need to draw
    //in next part, and to get it ready to be subscript for next vars:
    //     ++DrawFlag;
   }
   //#########end PRE DRAW ##########
   //Theory of rendering is this: I don't scale anything at my altitude, but
   //I do scale everything higher or lower than me by distince - that is, just
   //by multiplying it by 1/distance. LC_Tilt is really screen height.
   //In practice, the viewed height of a pixel is simply 
   //(LC_Altitude-PixelHeight)/RayStepCount, or in this program's
   //language, height=(LC_Altitude-PixelHeight)/y
   //here's a functional example, needing no lookup tables:
   //height=LC_Tilt+((LC_Altitude-LC_BmpHeights.pixel[y_index*LC_BmpHeights.w+x_index])<<LC_SCALESHIFT)/++y;
   //Now here's what I use with Lookup tables:
   //height=LC_Tilt+((LC_Altitude-LC_BmpHeights.pixel[data_index])<<LC_SCALESHIFT)/(++y);
   if ((height =
        LC_Tilt +
        (((LC_Altitude -
           LC_BmpHeights.pixel[data_index]) *
          LC_DistanceScaling[y_distance >> LC_YSHIFT]) >> LC_SHIFT)) < 0)
   {
    y_distance = LC_MaxRayDistance;
    height = 0;
   }

   //////////////////////////////////////
   //start draw landscape
   //***here's my blazing-fast drawing algorithm:
   if ((draw_length = top_height - height) > 0)
   {
    //first see if this land is wet:
    if (LC_BmpHeights.pixel[data_index] <= LC_WaterHeight)
    {
     //NOTE: user must do a LC_BmpHeights.pixel[data_index]=LC_WaterHeight, or else water looks weird.
     int *pWaterIndex = pSkyStart - top_height;
     if (pWaterIndex < LC_BmpSky.pixel)
      do
       pWaterIndex += LC_BmpSky.h;
      while (pWaterIndex < LC_BmpSky.pixel);
     //FAST Transparency (low precision) follow:
     //50%-50% transparency:
     color =
      ((LC_BmpColors.pixel[data_index] & 0xfefefe) >> 1) +
      ((*pWaterIndex & 0xfefefe) >> 1);
     //50%+25% transparency:
     //if (LC_BmpHeights.pixel[data_index]<=LC_WaterHeight) color=((LC_BmpColors[data_index]&0xfefefe)>>1)+((*pWaterIndex&0xfcfcfc)>>2);
     //if (LC_BmpHeights.pixel[data_index]<=LC_WaterHeight) color=((LC_BmpColors[data_index]&0xfcfcfc)>>2)+((*pWaterIndex&0xfefefe)>>1);
     //NO Transparency:
     //if (LC_BmpHeights.pixel[data_index]<=LC_WaterHeight) color=*(pWaterIndex++);
    }
    else
    {
     color = LC_BmpColors.pixel[data_index];
    }
    top_height = height;        //not sure if this should be put here or above 
    do
    {
     if (DrawPointX > 0 && DrawPointY > 0 &&
         (DrawPointX >> LC_SHIFT) < LC_BmpDest.w &&
         (DrawPointY >> LC_SHIFT) < LC_BmpDest.h)
     {
      holemark = 2;     //holemark here for consistency with sky, where there had been a bug 020212
      //see if first time entering drawing plane:
      if (!pScreenIndex)
      {
       OldDrawPointY = (DrawPointY >> LC_SHIFT);
       OldDrawPointX = (DrawPointX >> LC_SHIFT);
       pScreenIndex =
        LC_BmpDest.pixel + OldDrawPointY * LC_BmpDest.w + OldDrawPointX;
      }
      else
      { //I am within drawing zone
       //first of all, get pScreenIndex to the right position:
       if (OldDrawPointY != (DrawPointY >> LC_SHIFT))
       {
        --holemark;
        pScreenIndex += SLOPEY;
        OldDrawPointY = (DrawPointY >> LC_SHIFT);
       }
       if (OldDrawPointX != (DrawPointX >> LC_SHIFT))
       {
        --holemark;
        OldDrawPointX = (DrawPointX >> LC_SHIFT);
        pScreenIndex += SLOPEX;
       }
       //pScreenIndex is now ready to be written to
       //Now start test-for-object rendering section:
       if (DrawFlag)
       {        //Start Major If
        tempcount = 0;
        do
        {
         //Case 1:if within the window for an object, draw a pixel of the object:
         if (DrawCount < LC_StopDraw[tempcount] &&
             DrawCount >= LC_StartDraw[tempcount])
         {      //it was within the window:
          if (DrawCount > LC_StartDraw[tempcount])
          {     //increment texture by how much gets skipped:
           LC_TextureStepCountX[tempcount] += LC_TextureStepX[tempcount] * (((DrawCount - LC_StartDraw[tempcount])) - 1);       //011217
           //increment LC_StartDraw one step beyond current screenindex:
           LC_StartDraw[tempcount] = DrawCount + 1;     //011217
          }     //StartDraw right at the bottom of window, so just move it up one
          else
           ++LC_StartDraw[tempcount];   //011217

          //now, assign color to destbmp and test transparency:
          if ((*pScreenIndex =
               *(LC_TextureBitmapIndex[tempcount] +
                 (LC_TextureStepCountX[tempcount] >> LC_SHIFT))))
          {     //not transparent, so advance to next texture pixel:
           LC_TextureStepCountX[tempcount] += LC_TextureStepX[tempcount];
           if (!holemark)
            *(pScreenIndex - SLOPEX) = *pScreenIndex;
           //this break is all-important, for many reasons. Most important:
           //it makes sure this closer object doesn't get written on
           //later by a more distant object on this ray.
           break;       //!!!!!!!
          }
          else
          {     //it is transparent, so re-assign pixel value:
           //Next tempcount may draw here, in case another
           //object is visible inside the closer (wow).
           *pScreenIndex = color;       //too bad I must do this repeatedly...
           if (!holemark)
            *(pScreenIndex - SLOPEX) = *pScreenIndex;
           LC_TextureStepCountX[tempcount] += LC_TextureStepX[tempcount];
          }
         }
         else
         {      //case 2: object here, but no pixels within screen start/stop, so just draw stuff 
          //below or above object, sort of like any other Landscape()
          *pScreenIndex = color;
          if (!holemark)
           *(pScreenIndex - SLOPEX) = *pScreenIndex;
         }      //got to check all objects every time! "break" above saves many cycles
        }
        while (++tempcount < DrawFlag);
       }        //case 3: no object around, draw at will
       //end test for object rendering section
       else
        //start normal landscape rendering section:
       {
        *pScreenIndex = color;
        if (!holemark)
         *(pScreenIndex - SLOPEX) = color;      //0xffff;
       }
       //end normal landscape rendering section
      }
     }
     //next else works on principle that after entering window, exiting
     //at any point means done with ray:
     else if (pScreenIndex)
     {
      draw_length = 0;
      top_height = 0;
      y_distance = LC_MaxRayDistance;
     }
     DrawPointX += VStepX;
     DrawPointY -= VStepY;
     ++DrawCount;
    }
    while (--draw_length > 0);
   }    //end draw_length if
   //***end of new, fast drawing algorithm!
   //end draw landscape
   if ((y += yinc) > LC_ResolutionInterval)
   {
    y = 0;
    yinc++;
    x_step += x_origstep;
    y_step += y_origstep;
    y_distance_step += y_origdiststep;
   }

   //////////////////////////////////////
   //Decrement ray yardstick:
  }
  while ((y_distance += y_distance_step) < LC_MaxRayDistance);  //length I cast the ray
  //********End Y**********
  //////////////////////////////////////

  /////////////////////
  //start sky
  //   pSkyStart=LC_BmpSky.w*SkyX+LC_BmpSky.pixel+SkyTilt-top_height;
  pSkyStart -= top_height;
  //   if (pSkyStart>=pSkyEnd) do pSkyStart-=LC_BmpSky.h; while (pSkyStart>=pSkyEnd); else 
  //    if (pSkyStart<LC_BmpSky.pixel) do pSkyStart+=LC_BmpSky.h; while (pSkyStart<LC_BmpSky.pixel);
  if (pSkyStart < LC_BmpSky.pixel)
   do
    pSkyStart += LC_BmpSky.h;
   while (pSkyStart < LC_BmpSky.pixel);

  while (--top_height > 0)
  {
   if (DrawPointX > 0 && DrawPointY > 0 &&
       (DrawPointX >> LC_SHIFT) < LC_BmpDest.w &&
       (DrawPointY >> LC_SHIFT) < LC_BmpDest.h)
   {
    holemark = 2;       //putting holemark here solved bug - 020212
    //see if first time entering drawing plane:
    if (!pScreenIndex)
    {
     OldDrawPointY = (DrawPointY >> LC_SHIFT);
     OldDrawPointX = (DrawPointX >> LC_SHIFT);
     pScreenIndex =
      LC_BmpDest.pixel + OldDrawPointY * LC_BmpDest.w + OldDrawPointX;
    }
    else
    {
     if (OldDrawPointY != (DrawPointY >> LC_SHIFT))
     {
      OldDrawPointY = (DrawPointY >> LC_SHIFT);
      --holemark;
      pScreenIndex += SLOPEY;
     }
     if (OldDrawPointX != (DrawPointX >> LC_SHIFT))
     {
      --holemark;
      OldDrawPointX = (DrawPointX >> LC_SHIFT);
      pScreenIndex += SLOPEX;
     }
    }
    //Now start test-for-object rendering section:
    if (DrawFlag)
    {   //Start Major If
     tempcount = 0;
     do
     {
      //Case 1:if within the window for an object, draw a pixel of the object:
      if (DrawCount < LC_StopDraw[tempcount] &&
          DrawCount >= LC_StartDraw[tempcount])
      { //it was within the window:
       if (DrawCount > LC_StartDraw[tempcount])
       {        //increment texture by how much gets skipped:
        LC_TextureStepCountX[tempcount] += LC_TextureStepX[tempcount] * (((DrawCount - LC_StartDraw[tempcount])) - 1);  //011217
        //increment LC_StartDraw one step beyond current screenindex:
        LC_StartDraw[tempcount] = DrawCount + 1;        //011217
       }        //StartDraw right at the bottom of window, so just move it up one
       else
        ++LC_StartDraw[tempcount];      //011217

       //now, assign color to destbmp and test transparency:
       if ((*pScreenIndex =
            *(LC_TextureBitmapIndex[tempcount] +
              (LC_TextureStepCountX[tempcount] >> LC_SHIFT))))
       {        //not transparent, so advance to next texture pixel:
        LC_TextureStepCountX[tempcount] += LC_TextureStepX[tempcount];
        if (!holemark)
         *(pScreenIndex - SLOPEX) = *pScreenIndex;
        //this break is all-important, for many reasons. Most important:
        //it makes sure this closer object doesn't get written on
        //later by a more distant object on this ray.
        break;
       }
       else
       {        //it is transparent, so re-assign pixel value:
        //Next tempcount may draw here, in case another
        //object is visible inside the closer (wow).
        *pScreenIndex = *pSkyStart;     //too bad I must do this repeatedly...
        if (!holemark)
         *(pScreenIndex - SLOPEX) = *pScreenIndex;
        LC_TextureStepCountX[tempcount] += LC_TextureStepX[tempcount];
       }
      }
      else
      { //case 2: object here, but no pixels within screen start/stop, so just draw stuff 
       //below or above object, sort of like any other Landscape()
       *pScreenIndex = *pSkyStart;
       if (!holemark)
        *(pScreenIndex - SLOPEX) = *pScreenIndex;
      } //got to check all objects every time! "break" above saves many cycles
     }
     while (++tempcount < DrawFlag);
    }   //case 3: no object around, draw at will
    //end test for object rendering section
    else
    {
     *pScreenIndex = *pSkyStart;
     if (!holemark)
      *(pScreenIndex - SLOPEX) = *pSkyStart;
    }
   }
   //following based on principle that after entering, exiting means done:
   else if (pScreenIndex)
    break;
   DrawPointX += VStepX;
   DrawPointY -= VStepY;
   ++DrawCount;
   if (++pSkyStart >= pSkyEnd)
    pSkyStart -= LC_BmpSky.w;
  }
  if (--SkyX < 0)
   SkyX += LC_BmpSky.h;
  //end sky
  ////////////////////////

  StartPointX += HStepX;
  StartPointY -= HStepY;
  // }while (--x>-1); // end x
 }
 while (--x > xend);    // end x
 //*************end X**************
}       // end LC_LandscapeRollSkyFlyingObjects() - 021024

///////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
// int LC_SetLC_Raylength(int length)
/////////////////////////////////////////////////////////////////////
int LC_SetLC_Raylength (int length)
{
 static int old_length = LC_STARTING_RAYLENGTH;

 //next line is to deal with a discovery that
 //bigger raylengths fail for some reason. need to figure out why.
 if (LC_FlyingObjectCount && length > 4150)
  length = 4140;

 //I have no data to suggest next line actually
 //has a reference; but 40 does work.
 if (length < 40)
  length = 40;

 if (LC_Raylength == length)
  return 1;     //keep this

 LC_Raylength = length;
 if (!LC_InitLandscape ())
 {
  LC_Raylength = old_length;
  LC_InitLandscape ();
  return 0;
 }
 old_length = LC_Raylength;
 return 1;
}

/////////////////////////////////////////////////////////////////////
// int LC_SetNumberOfFlyingObjects(int number_of_objects)
/////////////////////////////////////////////////////////////////////
int LC_SetNumberOfFlyingObjects (int number_of_objects)
{
 LC_DeleteFlyingObjectsResources ();    //redundant, but must do before I change LC_FlyingObjectCount()

 LC_FlyingObjectCount = number_of_objects;
 if (LC_FlyingObjectCount < 1)
  LC_FlyingObjectCount = 1;
 if (LC_FlyingObjectCount > 254)
  LC_FlyingObjectCount = 254;

 LC_SetLC_Raylength (LC_Raylength);     //to make sure it isn't too long or FlyingObjects
 //I've rewritten so that I don't need to call initlandscape() with objects
 // if (!LC_InitLandscape()) return 0;//LC_InitLandscape() calls LC_CreateFlyingObjectsResources(),
 if (LC_FlyingObjectCount)
 {
  if (!LC_SkyFlag)
   if (!LC_SetSkyFlag (1))
    return 0;
  if (!LC_CreateFlyingObjectsResources ())
   return 0;
 }
 return 1;
}

/////////////////////////////////////////////////////////////////////
// int LC_SetSkyFlag(int state)
/////////////////////////////////////////////////////////////////////
int LC_SetSkyFlag (int state)
{
 LC_SkyFlag = state;
 if (LC_SkyFlag && !LC_BmpSky.pixel)
 {
  LC_MakeSky (8);
  return 0;
 }
 return 1;
}

/////////////////////////////////////////////////////////////////////
// int LC_CreateFlyingObjectsResources(void)
/////////////////////////////////////////////////////////////////////
//This is not called directly- LC_SetNumberOfFlyingObjects() must be called,
//which then calls LC_InitLandscape();
int LC_CreateFlyingObjectsResources (void)
{
 int i;

 LC_DeleteFlyingObjectsResources ();    //in case there were any 
 i = 1024;
 //if (i<LC_Raylength) i=LC_Raylength;//you don't want to run under, but it almost never gets
 //over 600.

 //next allot arrays:
 //this is the landscape FlyingObjects trot along on:
 LC_BmpFlyingObjects =
  (unsigned short int *) calloc (LC_BmpHeights.w * LC_BmpHeights.h,
                                 sizeof (unsigned short int));
 //following 6 arrays are all part of CLandcaster's internal 
 //drawing machinery:
 LC_TextureStepX = (int *) calloc (i, sizeof (int));
 LC_TextureStepCountX = (int *) calloc (i, sizeof (int));
 LC_TextureStepCountY = (int *) calloc (i, sizeof (int));
 LC_StartDraw = (int *) calloc (i, sizeof (int));
 LC_StopDraw = (int *) calloc (i, sizeof (int));
 LC_TextureBitmapIndex = (int **) calloc (i, sizeof (int *));
 //Biggie array follows - OBJECT it calls the constructors 
 //individually for all objects created:
 LC_FlyingObject =
  (FLYINGOBJECT *) calloc (LC_FlyingObjectCount, sizeof (FLYINGOBJECT));

 if (!LC_BmpFlyingObjects || !LC_TextureStepX || !LC_TextureStepCountX ||
     !LC_TextureStepCountY || !LC_StartDraw || !LC_StopDraw
     || !LC_TextureBitmapIndex || !LC_FlyingObject)
 {
  LC_DeleteFlyingObjectsResources ();
  return 0;
 }

 //Set FlyingObjects to some default values:
 for (i = 0; i < LC_FlyingObjectCount; i++)
  LC_FlyingObjectConstructor (&LC_FlyingObject[i]);

 //clear FlyingObjectBitmap array:
 for (i = 0; i < (LC_BmpHeights.w * LC_BmpHeights.h); i++)
 {
  LC_BmpFlyingObjects[i] = 0;
 }

 //Next line creates arbitrary objects, just so there is something in
 //the arrays if User fails to provide their own object creation
 if (LC_FlyingObjectCount)
  LC_CreateSomeFlyingObjects ();
 return 1;
}

/////////////////////////////////////////////////////////////////////
//MUST CALL LC_SetNumberOfFlyingObjects FIRST!!
//makes default objects- one harpoon and LC_FlyingObjectCount-1 targets
//user can use LC_SetupFlyingObject() to modify default objects 
//note- in default, user's harpoon is always at object[0]
/////////////////////////////////////////////////////////////////////
int LC_CreateSomeFlyingObjects ()
{
 int posx = 0,
  posy = 0,
  dir = 0,
  i;

 for (i = 0; i < LC_FlyingObjectCount; i++)
 {
  if (i > 0)
  {
   switch (LC_TargetStartPos)
   {
   case LC_CHANCE:
    posx = rand () % LC_BmpColors.w;
    posy = rand () % LC_BmpColors.h;
    dir = rand () % LC_CircleRez;
    if (posx < 0)
     posx += LC_BmpColors.w;
    else if (posx >= LC_BmpColors.w)
     posx -= LC_BmpColors.w;
    if (posy < 0)
     posy += LC_BmpColors.h;
    else if (posy >= LC_BmpColors.h)
     posy -= LC_BmpColors.h;
    if (dir < 0)
     dir += LC_CircleRez;
    else if (dir >= LC_CircleRez)
     dir -= LC_CircleRez;
    break;

   case LC_LINEDUP:
    switch (LC_TargetOrientation)
    {
    case LC_SKINNY:
     posx = posy = 0;
     //     dir=LC_CircleRez+((LC_FlyingObjectCount>>1)-i);
     dir = i;
     if (dir < 0)
      dir += LC_CircleRez;
     else if (dir >= LC_CircleRez)
      dir -= LC_CircleRez;
     break;

    case LC_SIDEWAYS:
     if ((posx -= 7) < 0)
      posx += LC_BmpColors.w;
     dir = 10;
     break;

    case LC_FACING:
     if ((posx -= 17) < 0)
      posx += LC_BmpColors.w;
     dir = 20;
     break;

    default:
     if ((posx -= 17) < 0)
      posx += LC_BmpColors.w;
     dir = 20;
     break;
    }
    break;

   default:
    posx = rand () % LC_BmpColors.w;
    posy = rand () % LC_BmpColors.h;
    dir = rand () % LC_CircleRez;
    if (posx < 0)
     posx += LC_BmpColors.w;
    else if (posx >= LC_BmpColors.w)
     posx -= LC_BmpColors.w;
    if (posy < 0)
     posy += LC_BmpColors.h;
    else if (posy >= LC_BmpColors.h)
     posy -= LC_BmpColors.h;
    if (dir < 0)
     dir += LC_CircleRez;
    else if (dir >= LC_CircleRez)
     dir -= LC_CircleRez;
    break;
   }
   LC_SetupFlyingObject (i, LC_TARGET, posx, posy, LC_TargetSpeed, 300, dir,
                         0, LC_VISIBLE, 0, 0, 0, NULL, 0, 0);
  }
  else
  {
   LC_SetupFlyingObject (i, LC_MISSILE, 0, 0, LC_MissileSpeed, 300, 0, 0,
                         LC_VISIBLE, 0, 0, 0, NULL, 0, 0);
  }
 }
 return 1;
}

/////////////////////////////////////////////////////////////////////
// NOTE: LC_SetNumberOfFlyingObjects(int number_of_objects) 
//  must be called before anything!
/////////////////////////////////////////////////////////////////////
int LC_SetupFlyingObject (int id,       //the only required parameter
                          int type, int x, int y, int altitude, int speed, int direction, int turn, int visible, unsigned char red, unsigned char green, unsigned char blue, int *pFlyingObjectBitmapTemp,      //optional - can leave empty
                          int FlyingObjectBitmapWidthTemp,      // optional - can leave empty
                          int FlyingObjectBitmapHeightTemp)     // optional - can leave empty
{
 int erasex;                    //041025 added to fix SizeY-erasure bug
 if (!(red + green + blue))
 {
  do
  {
   red = rand () % 256;
   green = rand () % 256;
   blue = rand () % 256;
  }
  while ((red + green + blue) < 200);
 }
 if (x == -1)
  LC_FlyingObject[id].PosX = ((rand () % LC_BmpHeights.w) << LC_SHIFT);
 else
  LC_FlyingObject[id].PosX = x << LC_SHIFT;
 if (y == -1)
  LC_FlyingObject[id].PosY = ((rand () % LC_BmpHeights.h) << LC_SHIFT);
 else
  LC_FlyingObject[id].PosY = y << LC_SHIFT;
 LC_FlyingObject[id].Alt = altitude;
 LC_FlyingObject[id].Speed = speed;
 LC_FlyingObject[id].Direction = direction;
 LC_FlyingObject[id].Type = type;
 LC_FlyingObject[id].Visible = visible;
 LC_FlyingObject[id].Turn = turn;

 if (pFlyingObjectBitmapTemp == NULL)
 {
  switch (type)
  {
  case LC_MISSILE:
   LC_MakeMissile (id, red, green, blue);
   break;

  case LC_TARGET:
   LC_MakeTarget (id, red, green, blue);
   break;

  default:
   break;
  }
  //041025 added to fix SizeY-erasure bug:
  erasex = LC_FlyingObject[id].SizeY;   //remember, texmaps are done sideways for speed
 }
 else   // It must be a User bitmap here, so do that:
 {
  //041025 added to fix SizeY-erasure bug:
  erasex = LC_FlyingObject[id].SizeY;   //remember, texmaps are done sideways for speed
  //if object was an *internal* bitmap before, erase it:
  if (LC_FlyingObject[id].bExternalBitmap == 0)
   free (LC_FlyingObject[id].pTextureBitmap);   // delete whatever had previously been there
  //definitely a user bitmap, so inform not to erase it:
  LC_FlyingObject[id].bExternalBitmap = 1;      //user gave me a bitmap  
  LC_FlyingObject[id].Lift = -1;
  LC_FlyingObject[id].pTextureBitmap = pFlyingObjectBitmapTemp;
  LC_FlyingObject[id].SizeX = FlyingObjectBitmapWidthTemp;
  LC_FlyingObject[id].SizeY = FlyingObjectBitmapHeightTemp;
 }
 //done with determining type of object
 ////////////////////////////////////////////////////////

 //Now set up object's erasure arrays, regardless of User of Internal bmap:
 //First erase whatever previous object had written (if anything)
 if (LC_FlyingObject[id].pErase)
 {
  do
  {
   LC_BmpFlyingObjects[LC_FlyingObject[id].pErase[--erasex]] = 0;       //erase old positions
  }
  while (erasex);       //041025 bugfix 
  //now that it is erased, I can delete it:
  free (LC_FlyingObject[id].pErase);    //in case one was already there
 }

 //now I allocate pErase according to SizeY:
 LC_FlyingObject[id].pErase = (int *) calloc (LC_FlyingObject[id].SizeY, sizeof (int)); //erasure array has been assigned!
 //now I zero the array:
 for (erasex = 0; erasex < LC_FlyingObject[id].SizeY; erasex++)
  LC_FlyingObject[id].pErase[erasex] = 0;

 return 1;
}

/////////////////////////////////////////////////////////////////////
// int rand(int hi_limit) 
/////////////////////////////////////////////////////////////////////
int LC_rand_hi (int hi_limit)
{
 return rand () % (hi_limit + 1);
}

/////////////////////////////////////////////////////////////////////
// MakeFacedTarget()
/////////////////////////////////////////////////////////////////////
int LC_MakeTarget (int id, unsigned char Red, unsigned char Green,
                   unsigned char Blue)
{
 int x,
  y,
  Size = LC_TargetSize;

 int *Bitmap;
 if (Size > 12)
  Size = 12;    //keep factor 12 or less (for God's sake!)
 if (Size < 0)
  Size = LC_rand_hi (-Size) + 4;
 Size = (1 << (Size));
 //create bitmap, assign to globals:
 Bitmap = (int *) calloc (Size * Size, sizeof (int));
 int BitmapHeight = Size,
  BitmapWidth = Size;
 //start by making everything transparent (pixel 0)
 for (x = 0; x < BitmapWidth; x++)
 {
  // if (!UserFunction(x+1,BitmapWidth)) break;
  for (y = 0; y < BitmapHeight; y++)
  {
   Bitmap[y * BitmapWidth + x] = 0;
   // Bitmap[y*BitmapWidth+x]=ColorValue[id];
  }     //end y
 }      //end x

 int ColorValue = 0xff000000 | Red << 16 | Green << 8 | Blue;
 //for a small donut:
 LC_DumbCircle (Bitmap, BitmapWidth, BitmapWidth / 2, ColorValue,
                BitmapWidth / 2 - 1, BitmapWidth / 2 - 1);
 LC_DumbCircle (Bitmap, BitmapWidth, BitmapWidth / 3, 0, BitmapWidth / 2 - 1,
                BitmapWidth / 2 - 1);
 LC_DumbCircle (Bitmap, BitmapWidth, BitmapWidth / 8, ColorValue,
                BitmapWidth / 2 - 1, BitmapWidth / 2 - 1);
 //to add a face:
 if (LC_FacedTargetFlag)
 {
  if (Red + Green + Blue < 640)
  {
   ColorValue = 0xffffffff;
  }
  LC_DumbCircle (Bitmap, BitmapWidth, BitmapWidth / 7, ColorValue,
                 BitmapWidth / 2 - 1, BitmapWidth - BitmapWidth / 8 - 1);
 }      //end face
 LC_Blur (Bitmap, BitmapWidth, BitmapHeight, 0);
 LC_ZeroContrast (Bitmap, BitmapWidth, BitmapHeight, 100);

 //done making missile, so assign bitmap and its parameters to object:
 LC_FlyingObject[id].pTextureBitmap = Bitmap;
 LC_FlyingObject[id].SizeX = Size;
 LC_FlyingObject[id].SizeY = Size;
 LC_FlyingObject[id].Type = LC_TARGET;
 LC_FlyingObject[id].Speed = LC_TargetSpeed;
 LC_FlyingObject[id].Lift = -1;
 return 1;      //bogus for now
}

/////////////////////////////////////////////////////////////////////
// LC_ZeroContrast- just zero's any pixel summing under zero_thresh
/////////////////////////////////////////////////////////////////////
void LC_ZeroContrast (int *OrigBitmap, int width, int height, int zero_thresh)
{
 int color,
  y,
  x;
 for (x = 0; x < width; x++)
 {      //start x
  for (y = 0; y < height; y++)
  {     //start y
   color = LC_pixelget (OrigBitmap, x, y, width);
   color = ((color >> 16) & 0xff) + ((color >> 8) & 0xff) + (color & 0xff);
   if (color < zero_thresh)
    LC_pixelput (OrigBitmap, x, y, 0x00000000, width);
  }     //y end
 }      //x end
}       //end LC_ZeroContrast()

/////////////////////////////////////////////////////////////////////
// void LC_DumbCircle()
/////////////////////////////////////////////////////////////////////
//pretty dumb circle drawer: 
void LC_DumbCircle (int *Bitmap, int BitmapWidth, int Radius, int color,
                    int X, int Y)
{
 int Left,
  Bottom,
  PolarX,
  PolarY,
  x,
  y,
  ScaledRadius;

 while ((Y - Radius) < 0)
  Radius--;
 while ((Y + Radius) > BitmapWidth)
  Radius--;
 while ((X - Radius) < 0)
  Radius--;
 while ((X + Radius) > BitmapWidth)
  Radius--;

 if (!(Radius % 2))
  Radius--;

 Left = Bottom = -Radius;
 // ScaledRadius=(Radius-1)*(Radius-1);
 ScaledRadius = Radius * Radius;
 PolarX = Left;
 for (x = X - Radius; x < 1 + X + Radius; x++)
 {
  PolarY = Bottom;
  for (y = Y - Radius; y < 1 + Y + Radius; y++)
  {
   if (((PolarX * PolarX) + (PolarY * PolarY)) <= ScaledRadius)
    Bitmap[y * BitmapWidth + x] = color;
   PolarY++;
  }
  PolarX++;
 }
}

/////////////////////////////////////////////////////////////////////
// skunk missile
/////////////////////////////////////////////////////////////////////
int LC_MakeMissile (int id, unsigned char Red, unsigned char Green,
                    unsigned char Blue)
{
 int x,
  y;
 int BitmapWidth = 3;           //remember, LANDSCAPE will render it sideways
 int BitmapHeight = 32;
 //create bitmap, assign to globals:
 int *Bitmap = (int *) calloc (BitmapHeight * BitmapWidth, sizeof (int));

 for (y = 0; y < BitmapHeight; y++)
 {
  for (x = 0; x < BitmapWidth; x++)
  {
   if (x == 1)
    Bitmap[y * BitmapWidth + x] = 0xffffffff;
   else
    Bitmap[y * BitmapWidth + x] = 0xff000000;
  }
 }

 //now I have the bitmap, so assign it and all it's parameters to object:
 LC_FlyingObject[id].pTextureBitmap = Bitmap;
 LC_FlyingObject[id].Type = LC_MISSILE;
 LC_FlyingObject[id].SizeX = BitmapWidth;
 LC_FlyingObject[id].SizeY = BitmapHeight;
 LC_FlyingObject[id].Visible = 0;       //start not visible
 LC_FlyingObject[id].Speed = LC_MissileSpeed;
 LC_FlyingObject[id].Lift = 0;  //will be set at launch
 return 1;      //bogus
}       //end makemissile()

/////////////////////////////////////////////////////////////////////
// void LC_MakeSky(int Size)
/////////////////////////////////////////////////////////////////////
void LC_MakeSky (int Size)
{
 int x,
  y;
 int *Bitmap;
 long old_seed = LC_Seed;
 if (Size > 12)
  Size = 12;    //keep factor 12 or less (for God's sake!)
 if (Size < 0)
  Size = 0;
 Size = (1 << (Size));

 //create bitmap, assign to globals:
 Bitmap = (int *) calloc (Size * Size, sizeof (int));
 //for (x=(Size*Size)-1;x>-1;x--){  Bitmap[x]=0xff000000; }

 //start find point and background colors:
 //start former globals in old code:
 int BitmapHeight = Size;
 int BitmapWidth = Size;
 //end of former globals in old code
 //first make a dark background:
 for (x = 0; x < BitmapWidth; x++)
 {      //put darks down
  // if (!UserFunction(x+1,BitmapWidth)) break;
  for (y = 0; y < BitmapHeight; y++)
  {
   Bitmap[y * BitmapWidth + x] =
    // 0xff000000|(rand()%20)<<16|(rand()%16)<<8|rand()%64;
    0xff000000 | (rand () % 16) << 16 | (rand () % 16) << 8 | rand () % 34;
  }     //end y
 }      //end x
 //LC_Blur(Bitmap,BitmapWidth,BitmapHeight,1);
 for (x = 0; x < BitmapWidth; x++)
 {      //put lights down
  for (y = 0; y < BitmapHeight; y++)
  {
   // if ((rand()%1024)<32)
   if ((rand () % 1024) < 12)
   {
    Bitmap[y * BitmapWidth + x] =
     0xff000000 | (230 + (rand () % 26)) << 16 | (230 +
                                                  (rand () %
                                                   26)) << 8 | (230 +
                                                                rand () % 26);
   }
  }     //end y
 }      //end x
 LC_Blur (Bitmap, BitmapWidth, BitmapHeight, 2);
 int color;                     //highlight the whites:
 for (x = 0; x < BitmapWidth; x++)
 {      //start x
  for (y = 0; y < BitmapHeight; y++)
  {     //start y
   color = Bitmap[y * BitmapWidth + x];
   color = ((color >> 16) & 0xff) + ((color >> 8) & 0xff) + (color & 0xff);
   if (color > 375)
    Bitmap[y * BitmapWidth + x] = 0xFFFFFFFF;
  }     //y end
 }      //x end
 LC_Blur (Bitmap, BitmapWidth, BitmapHeight, 12);
 LC_Seed = old_seed;
 //don't delete Bitmap! it is LC_BmpSky
 if (LC_UserSky == 0)
  free (LC_BmpSky.pixel);
 LC_BmpSky.pixel = Bitmap;
 LC_BmpSky.w = Size;
 LC_BmpSky.h = Size;
 LC_UserSky = 0;
 LC_PlasmaSky ();
}

////////////////////////////////////////////////////////////////////
//int * PlasmaGenerator(int Size,int RandFactor,int Ceiling,int Floor) -
//Size must represent a square bitmap with a width and height a clean power of 2
//RandFactor is the highest random number than can be added to the plasma at the
//start- complex relationship, so consider it carefully. For instance, making
//it equal size means the last random additions to the landscape will be 0!
//Double the Size is a good start. Ceiling and Floor
//do a "hard limit" on the final bitmap values.
////////////////////////////////////////////////////////////////////
void LC_Plasma (int *bitmap, int Size, int RandFactor, int floor, int ceiling)
{
 int i,
  x,
  x_offset,
  y,
  y_offset,
  Step = Size;
 int ul,
  ur,
  lr,
  ll,
  MidT,
  MidB,
  MidR,
  MidL,
  Midpoint;
 //start mapmaking iterations:
 for (i = (Size >> 0); i > 1; i /= 2)
 {
  for (x = 0; x < Size; x += Step)
  {
   if ((x_offset = x + Step) >= Size)
    x_offset -= Size;
   for (y = 0; y < Size; y += Step)
   {
    if ((y_offset = y + Step) >= Size)
     y_offset -= Size;
    ul = bitmap[y * Size + x];
    ur = bitmap[y * Size + x_offset];
    ll = bitmap[y_offset * Size + x];
    lr = bitmap[y_offset * Size + x_offset];
    MidT =
     ((ul + ur) >> 1) + ((rand () % (RandFactor + 1)) - (RandFactor >> 1));
    MidB =
     ((ll + lr) >> 1) + ((rand () % (RandFactor + 1)) - (RandFactor >> 1));
    MidR =
     ((ur + lr) >> 1) + ((rand () % (RandFactor + 1)) - (RandFactor >> 1));
    MidL =
     ((ul + ll) >> 1) + ((rand () % (RandFactor + 1)) - (RandFactor >> 1));
    if (MidT < floor)
     MidT = floor;
    else if (MidT > ceiling)
     MidT = ceiling;
    if (MidB < floor)
     MidB = floor;
    else if (MidB > ceiling)
     MidB = ceiling;
    if (MidR < floor)
     MidR = floor;
    else if (MidR > ceiling)
     MidR = ceiling;
    if (MidL < floor)
     MidL = floor;
    else if (MidL > ceiling)
     MidL = ceiling;
    Midpoint =
     ((MidT + MidB + MidL + MidR) >> 2) + ((rand () % (RandFactor + 1)) -
                                           (RandFactor >> 1));
    if (Midpoint < floor)
     Midpoint = floor;
    else if (Midpoint > ceiling)
     Midpoint = ceiling;
    bitmap[y * Size + (x + (Step >> 1))] = MidT;
    bitmap[y_offset * Size + (x + (Step >> 1))] = MidB;
    bitmap[(y + (Step >> 1)) * Size + x] = MidL;
    bitmap[(y + (Step >> 1)) * Size + x_offset] = MidR;
    bitmap[(y + (Step >> 1)) * Size + (x + (Step >> 1))] = Midpoint;
   }    //end y
  }     //end x
  Step /= 2;
  if ((RandFactor /= 2) < 2)
   RandFactor = 2;
 }      //end i
}       //end of LC_Plasma

/////////////////////////////////////////////////////////////////////
//void LC_ConvertIntBitmapToPixels() -- I think this was a Java required func
/////////////////////////////////////////////////////////////////////
void LC_ConvertIntBitmapToPixels (int *bitmap, int w, int h)
{
 int y,
  x;
 for (x = 0; x < w; x++)
  for (y = 0; y < h; y++)
   bitmap[y * w + x] |= 0xff000000;
}

/////////////////////////////////////////////////////////////////////
// void LC_PlasmaSky() just throws some plasma over an existant sky, can only be called after LC_MakeSky(x) has been called
/////////////////////////////////////////////////////////////////////
void LC_PlasmaSky ()
{
 int w,
  h,
  j,
  i;
 long oldseed = LC_Seed;
 w = h = LC_BmpSky.w;
 int *bmp = (int *) calloc (w * h, sizeof (int));
 for (j = (w * h) - 1; j > -1; j--)
 {
  bmp[j] = 0;
 }
 LC_Plasma (bmp, LC_BmpSky.w, w, 0, 0xff);
 //this next weirdness simply adds, grays, and limits the
 //plasma with the original starmap, in the least
 //efficient way possible.
 //        for (int i=0;i<24;i+=8) {
 //         for (int y,x=0;x<w;x++) for (y=0;y<h;y++) {
 //          j=y*w+x;
 //          if ( (((LC_BmpSky[j]>>i)&0xff)+bmp[j])<256 )
 //           LC_BmpSky[j]+=(bmp[j]<<i);
 //          else LC_BmpSky[j]|=(0xff<<i);
 //         }
 //        }

 //I've written it a little more efficiently here, but
 //kept the above in case I do want to do a matrix-type
 //of operation (which often use the funny for config)
 for (j = (w * h) - 1; j > -1; j--)
 {
  for (i = 0; i < 24; i += 8)
  {
   if ((((LC_BmpSky.pixel[j] >> i) & 0xff) + bmp[j]) < 256)
    LC_BmpSky.pixel[j] += (bmp[j] << i);
   else
    LC_BmpSky.pixel[j] |= (0xff << i);
  }
 }

 LC_ConvertIntBitmapToPixels (LC_BmpSky.pixel, LC_BmpSky.w, LC_BmpSky.h);
 LC_Seed = oldseed;
 free (bmp);
}       //end LC_PlasmaSky();

/////////////////////////////////////////////////////////////////////
//LC_SetRoll(int roll) - safely sets roll angle in LC_CircleRez units
/////////////////////////////////////////////////////////////////////
void LC_SetRoll (int roll)
{
 LC_Roll = roll;
 if (LC_Roll < 0)
  do
   LC_Roll += LC_CircleRez;
  while (LC_Roll < 0);
 else if (LC_Roll >= LC_CircleRez)
  do
   LC_Roll -= LC_CircleRez;
  while (LC_Roll >= LC_CircleRez);
}

/////////////////////////////////////////////////////////////////////
// LC_DoMovement() - Increments position and direction on map
/////////////////////////////////////////////////////////////////////
int LC_DoMovement (int groundcollisionflag)
{

 if (!LC_InitFlag)
 {
  if (!LC_InitLandscape ())
   return 0;
 }
 //deal with turn:
 LC_Direction += LC_Turn;
 if (LC_Direction < 0)
  do
   LC_Direction += LC_CircleRez;
  while (LC_Direction < 0);
 else if (LC_Direction >= LC_CircleRez)
  do
   LC_Direction -= LC_CircleRez;
  while (LC_Direction >= LC_CircleRez);

 ///////////////
 //start motion
 LC_PositionX += ((LC_Cos[LC_Direction] * LC_Speed) >> LC_SPEEDSHIFT);  //the >>8 is only needed for g_framerate
 LC_PositionY += ((LC_Sin[LC_Direction] * LC_Speed) >> LC_SPEEDSHIFT);  //the >>8 is only needed for g_framerate

 //test bitmap limits- fast way:
 if (LC_PositionX < 0)
  LC_PositionX += LC_BmpColorsWidth_scaled;
 else if (LC_PositionX >= LC_BmpColorsWidth_scaled)
  LC_PositionX -= LC_BmpColorsWidth_scaled;
 if (LC_PositionY < 0)
  LC_PositionY += LC_BmpColorsHeight_scaled;
 else if (LC_PositionY >= LC_BmpColorsHeight_scaled)
  LC_PositionY -= LC_BmpColorsHeight_scaled;

 //test bitmap limits- careful way (for unlimited high LC_Speed values):
 //  if (LC_PositionX<0) do LC_PositionX+=LC_BmpColorsWidth_scaled; while (LC_PositionX<0);
 //  else if (LC_PositionX>LC_BmpColorsWidth_scaled) do LC_PositionX-=LC_BmpColorsWidth_scaled; while (LC_PositionX>LC_BmpColorsWidth_scaled);
 //  if (LC_PositionY<0) do LC_PositionY+=LC_BmpColorsHeight_scaled; while (LC_PositionY<0);
 // else if (LC_PositionY>LC_BmpColorsHeight_scaled) do LC_PositionY-=LC_BmpColorsHeight_scaled; while (LC_PositionY>LC_BmpColorsHeight_scaled);

 //things go haywire after 0x3FFF
 if ((LC_Altitude += LC_Lift) > 5362)
 {
  LC_Altitude = 5362;
 }
 else if (0 > LC_Altitude)
 {
  LC_Altitude = 0;
 }

 //////////////////////////////////
 //start ground collision detection
 //Do not call before LC_BmpHeights.pixel is available!
 //20110404: out of bounds in the array below, so made PosY\X check above >= instead of > 
 if (0 != groundcollisionflag)
 {
  int ground =
   LC_Eyelevel +
   LC_BmpHeights.pixel[(LC_PositionY >> LC_SHIFT) * LC_BmpHeights.w +
                       (LC_PositionX >> LC_SHIFT)];
  //the scale-down factor (last number above) must relate to the calc in 
  //tables() to scale pixels appropriately  
  if (LC_Altitude < ground)
   LC_Altitude = ground;
 }
 //end ground collision detection
 //////////////////////////////////
 return 1;
}       //end LC_DoMovement()

/////////////////////////////////////////////////////////////////////
// LC_DoFlyingObjectsMovement() - Increments all live views/objects
/////////////////////////////////////////////////////////////////////
void LC_DoFlyingObjectsMovement ()
{
 int tempint,
  id;

 id = LC_FlyingObjectCount - 1;
 do
 {      //start major do
  if (LC_FlyingObject[id].Visible)
   switch (LC_FlyingObject[id].Type)
   {    //start type switch   

   case LC_TARGET:
    {
     //  if (LC_FlyingObject[id].Direction<0) LC_FlyingObject[id].Direction+=LC_CircleRez;
     //  else if (LC_FlyingObject[id].Direction>=LC_CircleRez) LC_FlyingObject[id].Direction-=LC_CircleRez;

     if ((LC_FlyingObject[id].Direction += LC_FlyingObject[id].Turn) < 0)
      LC_FlyingObject[id].Direction += LC_CircleRez;
     else if (LC_FlyingObject[id].Direction >= LC_CircleRez)
      LC_FlyingObject[id].Direction -= LC_CircleRez;

     LC_FlyingObject[id].PosX +=
      ((LC_Cos[LC_FlyingObject[id].Direction] *
        LC_FlyingObject[id].Speed) >> LC_SPEEDSHIFT);
     LC_FlyingObject[id].PosY +=
      ((LC_Sin[LC_FlyingObject[id].Direction] *
        LC_FlyingObject[id].Speed) >> LC_SPEEDSHIFT);

     //that mistake was below too: marked with //#
     //test bitmap limits- fast way:
     if (LC_FlyingObject[id].PosX < 0)
      LC_FlyingObject[id].PosX += LC_BmpColorsWidth_scaled;
     else if (LC_FlyingObject[id].PosX >= LC_BmpColorsWidth_scaled)
      LC_FlyingObject[id].PosX -= LC_BmpColorsWidth_scaled;     //#
     if (LC_FlyingObject[id].PosY < 0)
      LC_FlyingObject[id].PosY += LC_BmpColorsHeight_scaled;
     else if (LC_FlyingObject[id].PosY >= LC_BmpColorsHeight_scaled)
      LC_FlyingObject[id].PosY -= LC_BmpColorsHeight_scaled;    //#

     //now draw the object on the object map:
     int x_count = LC_FlyingObject[id].PosX,
      y_count = LC_FlyingObject[id].PosY,
      x_step,
      y_step,
      pos,
      id_code;

     switch (LC_TargetOrientation)
     {
      //decide how to face the targets:     
     case LC_SKINNY:
      pos = LC_FlyingObject[id].Direction;
      break;

     case LC_SIDEWAYS:
      pos = LC_FlyingObject[id].Direction - (LC_CircleRez >> 2);
      if (pos < 0)
       pos += LC_CircleRez;
      else if (pos >= LC_CircleRez)
       pos -= LC_CircleRez;
      break;

     case LC_FACING:
      pos = LC_Direction - (LC_CircleRez >> 2);
      if (pos < 0)
       pos += LC_CircleRez;
      else if (pos >= LC_CircleRez)
       pos -= LC_CircleRez;
      break;

     default:
      pos = LC_FlyingObject[id].Direction - (LC_CircleRez >> 2);
      if (pos < 0)
       pos += LC_CircleRez;
      else if (pos >= LC_CircleRez)
       pos -= LC_CircleRez;
      break;
     }

     x_step = (LC_Cos[pos] >> 1);
     y_step = (LC_Sin[pos] >> 1);

     // id_code=((id<<6)+1);//for int LC_BmpFlyingObjects (3 objects max)
     id_code = ((id << 8) + 1); // for unsigned short int LC_BmpFlyingObjects (255 obj)

     //I have to do this first, unfortunately, and not in the main
     //placement loop because I discovered that the erasure loop
     //was erasing some NEW pixels if the object was going backward
     //(and other un-considered movements).
     //NOTE: crashes in following usually mean Erase hasn't been zeroed:
     pos = LC_FlyingObject[id].SizeY;   //remember, texmaps are done sideways for speed
     do
      LC_BmpFlyingObjects[LC_FlyingObject[id].pErase[--pos]] = 0;       //erase old positions
     while (pos);

     //Now do main placement loop:
     pos = LC_FlyingObject[id].SizeY;   //remember, texmaps are done sideways for speed

     //draw the lines:
     do
     {
      LC_FlyingObject[id].pErase[--pos] =
       (y_count >> LC_SHIFT) * LC_BmpColors.w + (x_count >> LC_SHIFT);
      LC_BmpFlyingObjects[LC_FlyingObject[id].pErase[pos]] = pos | id_code;
      if ((x_count -= x_step) < 0)
       x_count += LC_BmpColorsWidth_scaled;
      else if (x_count >= LC_BmpColorsWidth_scaled)
       x_count -= LC_BmpColorsWidth_scaled;
      if ((y_count -= y_step) < 0)
       y_count += LC_BmpColorsHeight_scaled;
      else if (y_count >= LC_BmpColorsHeight_scaled)
       y_count -= LC_BmpColorsHeight_scaled;
     }
     while (pos);

     /*    
        //experimental
        unsigned short * pBmpIndex=LC_BmpFlyingObjects+(y_count>>LC_SHIFT)*LC_BmpColors.w+(x_count>>LC_SHIFT);
        //now determine draw slope:
        int SLOPEY,SLOPEX,holemark;
        int DrawPointX=x_count;
        int OldDrawPointX=DrawPointX>>10;
        int DrawPointY=y_count;
        int OldDrawPointY=DrawPointY>>10;
        if (DrawPointY<(DrawPointY-y_step)) SLOPEY=LC_BmpColors.w;
        else SLOPEY=-LC_BmpColors.w;
        if (DrawPointX<(DrawPointX-x_step)) SLOPEX=1;
        else SLOPEX=-1;
        do {
        *pBmpIndex=pos|id_code;
        LC_FlyingObject[id].pErase[pos]=pBmpIndex-LC_BmpFlyingObjects;
        DrawPointX-=x_step;
        DrawPointY-=y_step;
        if (DrawPointX<0) DrawPointX+=LC_BmpColorsWidth_scaled;
        else if (DrawPointX>=LC_BmpColorsWidth_scaled) DrawPointX-=LC_BmpColorsWidth_scaled;
        if (DrawPointY<0) DrawPointY+=LC_BmpColorsHeight_scaled; 
        else if (DrawPointY>=LC_BmpColorsHeight_scaled) DrawPointY-=LC_BmpColorsHeight_scaled;

        holemark=2;
        if (OldDrawPointY!=(DrawPointY>>LC_SHIFT)) {
        --holemark;
        pBmpIndex+=SLOPEY;
        OldDrawPointY=(DrawPointY>>LC_SHIFT);
        }
        if (OldDrawPointX!=(DrawPointX>>LC_SHIFT)) {
        --holemark;
        OldDrawPointX=(DrawPointX>>LC_SHIFT);
        pBmpIndex+=SLOPEX;
        }

        //if (!holemark) *(pBmpIndex-SLOPEX)=2|id_code;//0xffff;
        } while (pos);
        //end experimental
      */

     LC_FlyingObject[id].Alt += LC_FlyingObject[id].Lift;
     if ((pos =
          10 +
          LC_BmpHeights.pixel[LC_FlyingObject[id].pErase
                              [LC_FlyingObject[id].SizeY >> 1]]) >
         LC_FlyingObject[id].Alt)
      //  if ((pos=10+LC_BmpHeights.pixel[LC_FlyingObject[id].pErase[LC_FlyingObject[id].SizeY>>1]])>--LC_FlyingObject[id].Alt)
     {
      LC_FlyingObject[id].Alt = pos;
     }
    }
    break;      //type LC_TARGET

   case LC_MISSILE:
    {   //start IsVisible
     int pos = LC_FlyingObject[id].SizeY;       //remember, texmaps are done sideways for speed
     //see if it is time to die:
     if (!(--LC_FlyingObject[id].Visible))      //missile too old; erase it
     {
      do
       LC_BmpFlyingObjects[LC_FlyingObject[id].pErase[--pos]] = 0;      //erase old positions
      while (pos);
      break;
     }
     if (LC_MissileControl)     //fly-by-wire: only works for solo user
     {
      LC_FlyingObject[id].Direction = LC_Direction + (LC_Turn << 2);    //direction==mine
      if (LC_FlyingObject[id].Direction < 0)
       LC_FlyingObject[id].Direction += LC_CircleRez;
      else if (LC_FlyingObject[id].Direction >= LC_CircleRez)
       LC_FlyingObject[id].Direction -= LC_CircleRez;
      LC_FlyingObject[id].Lift = LC_Lift;       //takes my sink or rise
     }

     LC_FlyingObject[id].PosX +=
      ((LC_Cos[LC_FlyingObject[id].Direction] *
        LC_FlyingObject[id].Speed) >> LC_SPEEDSHIFT);
     LC_FlyingObject[id].PosY +=
      ((LC_Sin[LC_FlyingObject[id].Direction] *
        LC_FlyingObject[id].Speed) >> LC_SPEEDSHIFT);

     //found my '>' insead of '>=" bug again - I will always mark with //#
     //test bitmap limits- fast way:
     if (LC_FlyingObject[id].PosX < 0)
      LC_FlyingObject[id].PosX += LC_BmpColorsWidth_scaled;
     else if (LC_FlyingObject[id].PosX >= LC_BmpColorsWidth_scaled)
      LC_FlyingObject[id].PosX -= LC_BmpColorsWidth_scaled;     //#
     if (LC_FlyingObject[id].PosY < 0)
      LC_FlyingObject[id].PosY += LC_BmpColorsHeight_scaled;
     else if (LC_FlyingObject[id].PosY >= LC_BmpColorsHeight_scaled)
      LC_FlyingObject[id].PosY -= LC_BmpColorsHeight_scaled;    //#

     //now draw the object on the object map:
     int x_step = (LC_Cos[LC_FlyingObject[id].Direction] >> 1),
      y_step = (LC_Sin[LC_FlyingObject[id].Direction] >> 1),
      x_count = LC_FlyingObject[id].PosX,
      y_count = LC_FlyingObject[id].PosY,
      id_code = ((id << 8) + 1);        // for unsigned short int LC_BmpFlyingObjects (255 obj)
     //try 1:
     do
     {  //first, test to see if missile has hit something
      if (LC_BmpFlyingObjects
          [tempint =
           (y_count >> LC_SHIFT) * LC_BmpColors.w + (x_count >> LC_SHIFT)])
      { //something is in that spot- now see if I really hit it
       //note- LC_UserData1 is intentional- used to give info to user ID of hit
       LC_UserData1 = LC_BmpFlyingObjects[tempint] >> 8;        //the ID of what got hit
       if (LC_UserData1 != id &&        //it is not itself
           LC_FlyingObject[id].Alt > LC_FlyingObject[LC_UserData1].Alt &&       //it's higher than its bottom
           LC_FlyingObject[id].Alt < (LC_FlyingObject[LC_UserData1].Alt + LC_FlyingObject[LC_UserData1].SizeX)) //lower than its top
       {        //I really hit it
        LC_UserData2 = id;      //also make ID of missile available
        LC_UserFunctionFast (LC_OBJECTHIT);     //tell user she hit something, get ID from LC_UserData1
        //     do LC_BmpFlyingObjects[LC_FlyingObject[id].pErase[--pos]]=0;  while (pos>0);
        break;  //keeps me from counting too many hits
       }
      }
      LC_BmpFlyingObjects[LC_FlyingObject[id].pErase[--pos]] = 0;       //erase old position
      LC_FlyingObject[id].pErase[pos] = tempint;
      LC_BmpFlyingObjects[LC_FlyingObject[id].pErase[pos]] = pos | id_code;
      x_count -= x_step;
      y_count -= y_step;
      if (x_count < 0)
       x_count += LC_BmpColorsWidth_scaled;
      else if (x_count >= LC_BmpColorsWidth_scaled)
       x_count -= LC_BmpColorsWidth_scaled;
      if (y_count < 0)
       y_count += LC_BmpColorsHeight_scaled;
      else if (y_count >= LC_BmpColorsHeight_scaled)
       y_count -= LC_BmpColorsHeight_scaled;
     }
     while (pos);

     LC_FlyingObject[id].Alt += LC_FlyingObject[id].Lift;       //do lift
     //now do ground testing:
     //    if (LC_BmpHeights.pixel[LC_FlyingObject[id].pErase[LC_FlyingObject[id].SizeY-1]]>LC_FlyingObject[id].Alt)
     if (LC_BmpHeights.pixel[tempint] > LC_FlyingObject[id].Alt)
     {
      LC_UserFunctionFast (LC_GROUND);  //tell user she hit something
      LC_FlyingObject[id].Visible = 0;  //stick it in a mountain (no erase)
      break;
     }
    }   //end IsVisible
    break;      //end LC_MISSILE

   default:
    break;      //default
   }    //end type switch
 }
 while (id--);  //end major do
}

////////////////////////////////////////////////////////////////////////
// void LC_UserFunctionFast(int message) - for fast uses (in rendering loop, etc)
////////////////////////////////////////////////////////////////////////
void LC_UserFunctionFast (int message)
{
 if (!LC_pUserFunctionFast)
  return;
 LC_pUserFunctionFast (message);
 return;
}

/////////////////////////////////////////////////////////////////////
// SetCallback - set what function I use to communicate with user's program
/////////////////////////////////////////////////////////////////////
void LC_SetCallbackFast (void (*CallbackFunction) (int message))
{
 LC_pUserFunctionFast = CallbackFunction;
}

/////////////////////////////////////////////////////////////////////
// this is LandCaster's (not OBJECT's) way of deleting 
// what it allocated to objects
/////////////////////////////////////////////////////////////////////
int LC_DeleteFlyingObjectsResources (void)
{
 if (LC_BmpFlyingObjects != NULL)
 {
  free (LC_BmpFlyingObjects);
  LC_BmpFlyingObjects = NULL;
 }

 if (LC_TextureStepX != NULL)
 {
  free (LC_TextureStepX);
  LC_TextureStepX = NULL;
 }

 if (LC_TextureStepCountX != NULL)
 {
  free (LC_TextureStepCountX);
  LC_TextureStepCountX = NULL;
 }

 if (LC_TextureStepCountY != NULL)
 {
  free (LC_TextureStepCountY);
  LC_TextureStepCountY = NULL;
 }

 if (LC_StartDraw != NULL)
 {
  free (LC_StartDraw);
  LC_StartDraw = NULL;
 }

 if (LC_StopDraw != NULL)
 {
  free (LC_StopDraw);
  LC_StopDraw = NULL;
 }

 if (LC_TextureBitmapIndex != NULL)
 {
  free (LC_TextureBitmapIndex);
  LC_TextureBitmapIndex = NULL;
 }

 if (LC_FlyingObject != NULL)
 {
  int i;
  for (i = 0; i < LC_FlyingObjectCount; i++)
   LC_FlyingObjectDestructor (&LC_FlyingObject[i]);
  free (LC_FlyingObject);
  LC_FlyingObject = NULL;
 }
 LC_BmpFlyingObjects = NULL;
 LC_TextureStepX = NULL;
 LC_TextureStepCountX = NULL;
 LC_TextureStepCountY = NULL;
 LC_StartDraw = NULL;
 LC_StopDraw = NULL;
 LC_TextureBitmapIndex = NULL;
 LC_FlyingObject = NULL;
 return 1;
}

void LC_InitVars ()
{
 LC_AP.EyeLevel = 0;
 LC_AP.Lift = 0;
 LC_AP.Turn = 0;
 LC_AP.running = 1;
 LC_Eyelevel = 5;
 LC_WaterHeight = 0;
 LC_ResolutionInterval = 384;   //distance at which renderer starts taking longer ray steps
 LC_RadianToCircleRezFactor = 0;
 LC_UserSky = 0;        //not using a user's skybitmap is default
 LC_FacedTargetFlag = 1;
 LC_TargetOrientation = LC_SKINNY;
 LC_MissileControl = 0; //start with missile control off
 LC_MissileSpeed = 250;
 LC_TargetStartPos = LC_CHANCE;
 LC_TargetSpeed = 26;
 LC_TargetSize = 5;

 //pointers:
 LC_BmpDest.pixel = 0;
 LC_BmpColors.pixel = 0;
 LC_BmpHeights.pixel = 0;
 LC_BmpSky.pixel = 0;
 LC_Sin = 0;
 LC_Cos = 0;
 LC_Correction = 0;
 LC_DistanceScaling = 0;
 LC_TextureScaling = 0;
 LC_RollData = 0;
 LC_TextureStepX = 0;
 LC_TextureStepCountX = 0;
 LC_TextureStepCountY = 0;
 LC_StartDraw = 0;
 LC_StopDraw = 0;
 LC_TextureBitmapIndex = 0;
 LC_BmpFlyingObjects = 0;
 LC_StopDraw = 0;
 LC_TextureBitmapIndex = 0;
 LC_pUserFunctionFast = 0;
 LC_FlyingObject = 0;
 //endpointers

 //20101129: default datasize is 4 (int): 
 LC_BmpDest.datasize = 4;
 LC_BmpColors.datasize = 4;
 LC_BmpHeights.datasize = 4;
 LC_BmpSky.datasize = 4;

 LC_FlyingObjectCount = 0;      //user turns this on using LC_SetNumberOfFlyingObjects()
 LC_Turn = 0;
 LC_Direction = 0;
 LC_Lift = 9;
 LC_InitFlag = 0;       //haven't run LC_InitLandscape yet.
 LC_Speed = 50;
 LC_CircleRez = 2560;
 LC_Resolution = 500;
 LC_Raylength = 1024;
 LC_Scale = 400000;     //400000;//change to suit hill scaling
 LC_Tilt = 200; //Screen altitude, really - should be about 1/2 MaxFramewidth
 LC_SkyFlag = 0;        //0 means don't draw fancy sky in Landscape
 LC_Altitude = 173;
 LC_PositionX = 0;
 LC_PositionY = 0;
 LC_Roll = 0;
 LC_MaxFrameWidth = 0;  //in case user replaces it.
 //set all bitmap info to 0, to be sure user sets them later
 LC_BmpColors.w = 0;
 LC_BmpColors.h = 0;
 LC_BmpHeights.w = 0;
 LC_BmpHeights.h = 0;
 LC_BmpDest.w = 0;
 LC_BmpDest.h = 0;
 LC_BmpSky.w = 0;
 LC_BmpSky.h = 0;
}

/////////////////////////////////////////////////////////////////////
// void LC_pixelput() -- pretty dumb
/////////////////////////////////////////////////////////////////////
void LC_pixelput (int *pTempBitmap, int x_temp, int y_temp, int val,
                  int width)
{
 pTempBitmap[y_temp * width + x_temp] = val;
}

/////////////////////////////////////////////////////////////////////
// void LC_pixelget() -- similarly dumb
/////////////////////////////////////////////////////////////////////
int LC_pixelget (int *pTempBitmap, int x_temp, int y_temp, int width)
{
 return (pTempBitmap[y_temp * width + x_temp]);
}

/////////////////////////////////////////////////////////////////////
// LC_Blur() - Higher blur_factor means less blur
/////////////////////////////////////////////////////////////////////
void LC_Blur (int *OrigBitmap, int width, int height, int blur_factor)
{
 double divisor = 1.0 / (blur_factor + 9);
 int *TempBitmap;
 TempBitmap = (int *) calloc (width * height, sizeof (int));
 int x,
  y,
  x_step,
  y_step,
  x_temp,
  y_temp,
  sum_red,
  sum_green,
  sum_blue,
  image_width,
  image_height,
  color;

 image_height = height - 1;
 image_width = width - 1;

 LC_CopyBitmap (OrigBitmap, TempBitmap, 0, 0, 0, 0, width, height);
 for (x = 0; x < width; x++)
 {      //start x
  for (y = 0; y < height; y++)
  {     //start y
   sum_red = sum_green = sum_blue = 0;  //reset sums for new average
   //start with upper left pixel of the 3x3
   for (x_step = -1; x_step < 2; x_step++)
   {
    x_temp = x + x_step;
    if (x_temp < 0)
     x_temp += width;
    else if (x_temp > image_width)
     x_temp -= width;
    for (y_step = -1; y_step < 2; y_step++)
    {
     y_temp = y + y_step;
     if (y_temp < 0)
      y_temp += height;
     else if (y_temp > image_height)
      y_temp -= height;
     //now I take the rgb info from the designated pixel:
     color = LC_pixelget (TempBitmap, x_temp, y_temp, width);
     sum_red += ((color >> 16) & 0xff);
     sum_green += ((color >> 8) & 0xff);
     sum_blue += (color & 0xff);
     //now I emphasize the original pixel:
     if (x_step == 0 && y_step == 0 && blur_factor != 0)
     {
      sum_red += (((color >> 16) & 0xff) * blur_factor);
      sum_green += (((color >> 8) & 0xff) * blur_factor);
      sum_blue += ((color & 0xff) * blur_factor);
     }
    }   //y_temp end
   }    // x_temp end
   //now I get the average of all the sums- 9 + 1 pixels, so divide by 10:
   sum_red = (int) ((sum_red * divisor) + 0.5);
   sum_green = (int) ((sum_green * divisor) + 0.5);
   sum_blue = (int) ((sum_blue * divisor) + 0.5);
   if (sum_red > 255)
    sum_red = 255;
   else if (sum_red < 0)
    sum_red = 0;
   if (sum_green > 255)
    sum_green = 255;
   else if (sum_green < 0)
    sum_green = 0;
   if (sum_blue > 255)
    sum_blue = 255;
   else if (sum_blue < 0)
    sum_blue = 0;
   LC_pixelput (OrigBitmap, x, y,
                ((0xff000000 | sum_red << 16 | sum_green << 8 | sum_blue)),
                width);
  }     //y end
 }      //x end
 free (TempBitmap);
}       //end LC_Blur

////////////////////////////////////////////////////////////////////////
// void LC_CopyBitmap() - slow, unoptimized
////////////////////////////////////////////////////////////////////////
void LC_CopyBitmap (int *pTempBitmapSource, int *pTempBitmapDest, int i,
                    int j, int x, int y, int Width, int Height)
{
 for (x = 0; x < Width; x++)
 {
  for (y = 0; y < Height; y++)
  {
   pTempBitmapDest[y * Width + x] = pTempBitmapSource[y * Width + x];
  }     //end y
 }      //end x
}

/////////////////////////////////////////////////////////////////////
// void LC_FlyingObjectDestructor() -- taken from the C++ ver
/////////////////////////////////////////////////////////////////////
void LC_FlyingObjectDestructor (FLYINGOBJECT * tempFlyingObject)
{
 //if user isn't using an external bitmap, delete the bitmap:
 if (!tempFlyingObject->bExternalBitmap
     && NULL != tempFlyingObject->pTextureBitmap)
 {
  free (tempFlyingObject->pTextureBitmap);
  tempFlyingObject->pTextureBitmap = NULL;
 }
 //now delete erasure array:
 if (NULL != tempFlyingObject->pErase)
 {
  free (tempFlyingObject->pErase);      //in case one was already there
  tempFlyingObject->pErase = NULL;
 }
}

/////////////////////////////////////////////////////////////////////
// void LC_FlyingObjectConstructor() -- taken from the C++ ver
/////////////////////////////////////////////////////////////////////
void LC_FlyingObjectConstructor (FLYINGOBJECT * tempFlyingObject)
{
 tempFlyingObject->bExternalBitmap = 0; //FLYINGOBJECT always creates NULL, or maybe a 1x1 pixel, bitmap
 tempFlyingObject->Type = LC_TARGET;    //arbitrary
 tempFlyingObject->Speed = 0;
 tempFlyingObject->Turn = 0;
 tempFlyingObject->Visible = LC_VISIBLE;
 tempFlyingObject->Alt = 100;
 tempFlyingObject->Direction = 0;
 tempFlyingObject->Lift = 0;
 tempFlyingObject->SizeX = 0;   //width of the texture bitmap
 tempFlyingObject->SizeY = 0;   //height of the texture bitmap
 tempFlyingObject->PosX = 0;    //scaled by <<LC_SHIFT
 tempFlyingObject->PosY = 0;    //scaled by <<LC_SHIFT
 tempFlyingObject->pErase = NULL;       //usually needs to be allotted to SizeY, unless a "3D" object
 tempFlyingObject->pTextureBitmap = NULL;       //address to first byte of texture bitmap
}

////////////////////////////////////////////////////////////////////////
// void LC_LandCaster_Cleanup()
////////////////////////////////////////////////////////////////////////
void LC_LandCaster_Cleanup ()
{
 if (LC_Sin != NULL)
 {
  free (LC_Sin);
  LC_Sin = NULL;
 }

 if (LC_Cos != NULL)
 {
  free (LC_Cos);
  LC_Cos = NULL;
 }

 if (LC_Correction != NULL)
 {
  free (LC_Correction);
  LC_Correction = NULL;
 }

 if (LC_DistanceScaling != NULL)
 {
  free (LC_DistanceScaling);
  LC_DistanceScaling = NULL;
 }

 if (LC_TextureScaling != NULL)
 {
  free (LC_TextureScaling);
  LC_TextureScaling = NULL;
 }

 if (LC_RollData != NULL)
 {
  free (LC_RollData);
  LC_RollData = NULL;
 }

 if (!LC_UserSky && NULL != LC_BmpSky.pixel)
 {
  free (LC_BmpSky.pixel);       //already deleted if user gave sky
  LC_BmpSky.pixel = NULL;
 }

 LC_DeleteFlyingObjectsResources ();
}

/////////////////////////////////////////////////////////////////////
// LC_PixelFly() - has sky and tilt, but no roll
// SPECIAL NOTE: This function is Java-compatible (no pointers)
/////////////////////////////////////////////////////////////////////
void LC_PixelFly ()
{
 int SkySize = LC_BmpSky.w * LC_BmpSky.h;
 int SkyX = LC_Direction;
 if (SkyX >= LC_BmpSky.w)
  do
   SkyX -= LC_BmpSky.w;
  while (SkyX >= LC_BmpSky.w);
 int tiltFactor;
 int eyeLevel = (LC_BmpDest.h >> 1) - LC_Tilt;  //eyelevel set here
 int x = LC_BmpDest.w;
 int x_offset;
 int CenterWidth = LC_BmpDest.w >> 1;
 //******Start X******
 do     // x is the number of rays I send
 {
  //next line starts my first column on the left side of screen
  if ((x_offset = LC_Direction - CenterWidth + x) >= LC_CircleRez)
   x_offset -= LC_CircleRez;
  else if (x_offset < 0)
   x_offset += LC_CircleRez;

  //in the regular world (not DIB sections), window lower-left is this:      
  int pScreenIndex = LC_DestLowerLeftStartPoint + --x;

  int x_step = (LC_Cos[x_offset] * LC_Correction[x]) >> 20;
  int y_step = (LC_Sin[x_offset] * LC_Correction[x]) >> 20;
  int y = LC_BmpDest.h;
  //////////////////////////////////////
  //******Start Y********
  do
  {
   //PFly2: the philosphy of rendering:
   //(TrueAltitude*EyeToScreenDistance)/RowsFromEyeLevelToCurentScreenRow
   //or the speed equivlent: (1/Drop)*(Alt<<eyedistance)
   if ((tiltFactor = y + eyeLevel) < 1)
    break;      //this does LC_Tilt
   int x_int = (x_step * LC_Altitude << 10) / tiltFactor + LC_PositionX;
   int y_int = (y_step * LC_Altitude << 10) / tiltFactor + LC_PositionY;

   //I must use the following cumbersome method of bounds checking because
   //when altitude increases in PFly, single steps can be even larger than
   //either image dimensions (010224)   
   //if (y_int>=LC_BmpColorsHeight_scaled) do y_int-=LC_BmpColorsHeight_scaled; while (y_int>=LC_BmpColorsHeight_scaled);
   //else if(y_int<0) do y_int+=LC_BmpColorsHeight_scaled; while (y_int<0);
   //if (x_int>=LC_BmpColorsWidth_scaled) do x_int-=LC_BmpColorsWidth_scaled; while (x_int>=LC_BmpColorsWidth_scaled);
   //else if (x_int<0) do x_int+=LC_BmpColorsWidth_scaled; while (x_int<0);
   //the following actually is faster, since most of the time the 
   //indexes are out of bounds:
   if (y_int < 0 || y_int >= LC_BmpColorsHeight_scaled)
   {
    y_int %= LC_BmpColorsHeight_scaled;
    if (y_int < 0)
     y_int += LC_BmpColorsHeight_scaled;
   }
   if (x_int < 0 || x_int >= LC_BmpColorsWidth_scaled)
   {
    x_int %= LC_BmpColorsWidth_scaled;
    if (x_int < 0)
     x_int += LC_BmpColorsWidth_scaled;
   }
   //////////////////////////////////////
   //start draw landscape
   LC_BmpDest.pixel[pScreenIndex] =
    LC_BmpColors.pixel[(y_int >> 10) * LC_BmpColors.w + (x_int >> 10)];
   pScreenIndex -= LC_BmpDest.w;
  }
  while (--y > 0);      //stop_ray); //length I cast the ray
  //********End Y**********
  //////////////////////////////////////
  //put the sky bitmap in the background:
  if (pScreenIndex > 0)
  {
   if (SkyX < 0)
    SkyX += LC_BmpSky.w;
   int SkyY = (pScreenIndex / LC_BmpDest.w) - LC_Tilt;
   if (SkyY >= LC_BmpSky.h)
    do
     SkyY -= LC_BmpSky.h;
    while (SkyY >= LC_BmpSky.h);
   else if (SkyY < 0)
    do
     SkyY += LC_BmpSky.h;
    while (SkyY < 0);
   SkyY = SkyY * LC_BmpSky.w + SkyX;
   //*** start sky draw ***
   do
   {
    LC_BmpDest.pixel[pScreenIndex] = LC_BmpSky.pixel[SkyY];
    if ((SkyY -= LC_BmpSky.w) < 0)
     SkyY += SkySize;
   }
   while ((pScreenIndex -= LC_BmpDest.w) > 0);
  }
  //*** end sky draw ***
  //##############END COLUMN OF SKY DRAWING#############
  --SkyX;
 }
 while (x > 0); // end x
 //*************end X**************
}       //end PFly()

/////////////////////////////////////////////////////////////////////
// LC_AutoPilot() - 030223 
/////////////////////////////////////////////////////////////////////
void LC_AutoAlt ()
{
 static int ground_sum = 32 << 4;
 static int altitude_sum = 32 << 3;
 static const int distanceAlt = 20;     //distance feeler goes to determine altitude

 //alert: out of bounds error occurs in next line (fixed 11-30-00?)
 int x_start = LC_PositionX + LC_Cos[LC_Direction] * distanceAlt;
 int y_start = LC_PositionY + LC_Sin[LC_Direction] * distanceAlt;
 if (x_start < 0)
  do
   x_start += LC_BmpColorsWidth_scaled;
  while (x_start < 0);
 else if (x_start >= LC_BmpColorsWidth_scaled)
  do
   x_start -= LC_BmpColorsWidth_scaled;
  while (x_start > LC_BmpColorsWidth_scaled);
 if (y_start < 0)
  do
   y_start += LC_BmpColorsHeight_scaled;
  while (y_start < 0);
 else if (y_start >= LC_BmpColorsHeight_scaled)
  do
   y_start -= LC_BmpColorsHeight_scaled;
  while (y_start >= LC_BmpColorsHeight_scaled);
 altitude_sum += LC_Altitude;
 ground_sum +=
  LC_BmpHeights.pixel[(y_start >> LC_SHIFT) * LC_BmpHeights.w +
                      (x_start >> LC_SHIFT)] + LC_AP.EyeLevel;
 LC_AP.Lift = (ground_sum >> 4) - (altitude_sum >> 3);
 altitude_sum -= (altitude_sum >> 3);   //drain the running sum. Crucial
 ground_sum -= (ground_sum >> 4);       //drain the running sum. Crucial
}

/////////////////////////////////////////////////////////////////////
// LC_AutoPilot() - 030223 
/////////////////////////////////////////////////////////////////////
void LC_AutoPilot (int Seek,    //Seek=1 seeks high ground; -1 = low
                   int ScanWidth)       //distance from first sample to third (last) sample taken
{
 int HeightCount1,
  HeightCount2 = 0;
 static int AutoTurn = 0;
 int y,
  x_int,
  y_int,
  x_offset,
  x_step,
  y_step;
 int x = -ScanWidth;

 // ******Start X ******
 do     // x is the number of rays I send
 {
  y = 10;       //length of ray
  HeightCount1 = 0;
  //next line starts my first column on the left side of screen
  //CAUTION: theoretically x could push it beyond one LC_CircleRez
  if ((x_offset = LC_Direction + x) >= LC_CircleRez)
   x_offset -= LC_CircleRez;
  else if (x_offset < 0)
   x_offset += LC_CircleRez;
  x_step = LC_Cos[x_offset] << 2;
  y_step = LC_Sin[x_offset] << 2;
  x_int = LC_PositionX;
  y_int = LC_PositionY;
  //////////////////////////////////////
  // ******Start Y ********
  do
  {
   x_int += x_step;
   y_int += y_step;
   if (y_int >= LC_BmpColorsHeight_scaled)
    do
     y_int -= LC_BmpColorsHeight_scaled;
    while (y_int >= LC_BmpColorsHeight_scaled);
   else if (y_int < 0)
    do
     y_int += LC_BmpColorsHeight_scaled;
    while (y_int < 0);
   if (x_int >= LC_BmpColorsWidth_scaled)
    do
     x_int -= LC_BmpColorsWidth_scaled;
    while (x_int >= LC_BmpColorsWidth_scaled);
   else if (x_int < 0)
    do
     x_int += LC_BmpColorsWidth_scaled;
    while (x_int < 0);

   //the important part:
   HeightCount1 +=
    LC_BmpHeights.pixel[(y_int >> LC_SHIFT) * LC_BmpHeights.w +
                        (x_int >> LC_SHIFT)];
  }
  while (--y);  //this is tied to refheight, so don't casually change it
  // ********End Y **********
  //////////////////////////////////////
  //autopilot stuff:
  if (x < 0)
   HeightCount2 = HeightCount1;
 }
 while ((x += (ScanWidth << 1)) <= ScanWidth);  // end x
 // *************end X **************

 AutoTurn += Seek * (HeightCount1 - HeightCount2);      //make positive to seek high
 LC_AP.Turn = (AutoTurn -= (AutoTurn >> 3)) >> 2;
 if (LC_AP.Turn >= LC_CircleRez)
  LC_AP.Turn = LC_CircleRez - 1;
 else if (LC_AP.Turn <= -LC_CircleRez)
  LC_AP.Turn = -(LC_CircleRez - 1);

 //now I find out which way around circle is shortest to meet the new direction:
 if (abs (LC_AP.Turn) > (LC_CircleRez >> 1))
 {
  if (LC_AP.Turn < 0)
   LC_AP.Turn = LC_CircleRez + LC_AP.Turn;
  else
   LC_AP.Turn = -(LC_CircleRez - LC_AP.Turn);
 }

 LC_AutoAlt ();
}       //end LC_AutoPilot()

/////////////////////////////////////////////////////////////////////
//uses a dot-product to turn object towards another object
int LC_FlyingObjectSeekFlyingObject (int followerId, int targetID, int circle)
{
 //subtract targeter's position from target's position
 int ax = (LC_FlyingObject[targetID].PosX - LC_FlyingObject[followerId].PosX);
 int ay = (LC_FlyingObject[targetID].PosY - LC_FlyingObject[followerId].PosY);
 int bx = LC_Cos[LC_FlyingObject[followerId].Direction];
 int by = LC_Sin[LC_FlyingObject[followerId].Direction];

 //regular dotproduct:
 //(this makes it circle object)
 //return ax*bx+ay*by;//>>LC_DOUBLESHIFT;

 //dotproduct turned 90 degrees: ya * xb - xa * yb
 //(this makes it follow object)
 if (!circle)
  return -(ay * bx - ax * by);
 return -(ax * bx + ay * by);
}

/////////////////////////////////////////////////////////////////////
//uses a dot-product to turn object towards user position
int LC_FlyingObjectSeekUser (int followerId, int circle)
{
 //subtract targeter's position from target's position
 int ax = (LC_PositionX - LC_FlyingObject[followerId].PosX);
 int ay = (LC_PositionY - LC_FlyingObject[followerId].PosY);
 int bx = LC_Cos[LC_FlyingObject[followerId].Direction];
 int by = LC_Sin[LC_FlyingObject[followerId].Direction];

 //regular dotproduct:
 //(this makes it circle object)
 //return ax*bx+ay*by;//>>LC_DOUBLESHIFT;

 //dotproduct turned 90 degrees: ya * xb - xa * yb
 //(this makes it follow object)
 if (!circle)
  return -(ay * bx - ax * by);
 return -(ax * bx + ay * by);
}

/////////////////////////////////////////////////////////////////////
//uses a dot-product to turn object towards a position
int LC_FlyingObjectSeekPosition (int ObjID, int PosX, int PosY, int circle)
{
 //subtract seeker's position from sought position
 int ax = (PosX - LC_FlyingObject[ObjID].PosX);
 int ay = (PosY - LC_FlyingObject[ObjID].PosY);
 int bx = LC_Cos[LC_FlyingObject[ObjID].Direction];
 int by = LC_Sin[LC_FlyingObject[ObjID].Direction];

 //regular dotproduct:
 //(this makes it circle object)
 //return ax*bx+ay*by;//>>LC_DOUBLESHIFT;

 //dotproduct turned 90 degrees: ya * xb - xa * yb
 //(this makes it follow object)
 if (!circle)
  return -(ay * bx - ax * by);
 return -(ax * bx + ay * by);
}

/////////////////////////////////////////////////////////////////////
// LC_TargetFollowTheLeader() - 030223 
void LC_TargetFollowTheLeader ()        //aka BlindFollowBlind
{       //this searches for targets that are still live, and links them
 //in to a "followtheleader" endless loop. Interesting "chaos attractor"
 //results, seems
 int i = LC_FlyingObjectCount;
 if (i < 3)
  return;       //less that two targets existent
 //if (LiveTargetCount<2) return;//less than two targets left
 int j;
 int firstID = -1;
 int foundone;
 while (--i > 0)
 {      //since 0 is always a missile...
  if (LC_FlyingObject[i].Type == LC_TARGET && LC_FlyingObject[i].Visible)
  {
   if (firstID < 0)
    firstID = i;        //this will be what the last live target follows
   foundone = 0;
   j = i;
   while (--j > 0)
   {    //look for a live target to follow:
    if (LC_FlyingObject[j].Type == LC_TARGET && LC_FlyingObject[j].Visible)
    {
     foundone = 1;
     LC_FlyingObject[i].Turn = LC_FlyingObjectSeekFlyingObject (i, j, 0) >> LC_DOUBLESHIFT;     //may want to change shift depending on landsize
     break;
    }
   }    //end j
   if (!foundone)
   {    //nothing to follow beneath it, so follow first
    LC_FlyingObject[i].Turn = LC_FlyingObjectSeekFlyingObject (i, firstID, 0) >> LC_DOUBLESHIFT;        //shift can be changed
    return;     //if all is good (plenty of live targets), this is function exit
   }
  }     //end was target and visible
 }      //end i;
 //should never get here...
}

///////////////////////////////////////////////////////////////////////
// ShowColorsMap() - this was more complicated than I expected
/////////////////////////////////////////////////////////////////////
void LC_ShowColorsBitmap ()
{
 int x,
  y;
 double xf,
  yf,
  Step;

 //clear DestBitmap
 for (x = 0; x < LC_BmpDest.w; x++)
 {
  for (y = 0; y < LC_BmpDest.h; y++)
   LC_BmpDest.pixel[y * LC_BmpDest.w + x] = 0;
 }      //end x

 //use whichever is the largest step for Step:
 if (((double) LC_BmpColors.w / LC_BmpDest.w) >
     ((double) LC_BmpColors.h / LC_BmpDest.h))
 {
  Step = ((double) LC_BmpColors.w / LC_BmpDest.w);
 }
 else
 {
  Step = ((double) LC_BmpColors.h / LC_BmpDest.h);
 }

 //start the ultra-safe transfer:
 for (xf = 0.0, x = 0; xf < LC_BmpColors.w && x < LC_BmpDest.w;
      xf += Step, x++)
 {
  for (yf = 0.0, y = 0; yf < LC_BmpColors.h && y < LC_BmpDest.h;
       yf += Step, y++)
  {
   LC_BmpDest.pixel[y * LC_BmpDest.w + x] =
    LC_BmpColors.pixel[((int) yf) * LC_BmpColors.w + ((int) xf)];
  }     //end y
 }      //end x
}
