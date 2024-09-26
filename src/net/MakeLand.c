//////////////////////////////////////////////////////////////////////
// MakeLand.c (c) Bret Byron Logan 2010
//////////////////////////////////////////////////////////////////////

#include "MakeLand.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0

int *MakeLand_bmpHeights = NULL;
int *MakeLand_bmpColors = NULL;
int MakeLand_Width = 0;
int MakeLand_Height = 0;
int MakeLand_InternallyGeneratedBitapsFlag = TRUE;
int MakeLand_FFT_Flag = TRUE;
int MakeLand_GaussWidth = 3;    //set this for MakeLand_Width of gaussian_skinny
int MakeLand_Seed = 12345;
int (*MakeLand_Rand) () = MakeLand_rand;        //gets set to whatever type of rand I want to use
double MakeLand_LandSmoothness = 2.2;   //2.2; ////usually between 1.8 and 2.5
MAKELAND_USER_CALLBACK MakeLand_UserCallback = NULL;

 /////////////////////////////
 //creates the color and heights bitmap:

void MakeLand_Init_InternalBitmaps (int Size_LShift)    //size is given as power of 2, since bitmap must be
{
 if (Size_LShift > 12)
 {
  Size_LShift = 12;     //keep factor 12 or less (for God's sake!)
 }
 if (Size_LShift < 0)
 {
  Size_LShift = 0;
 }
 Size_LShift = (1 << (Size_LShift));
 MakeLand_Width = MakeLand_Height = Size_LShift;
 MakeLand_bmpHeights =
  (int *) calloc (MakeLand_Width * MakeLand_Height, sizeof (int));
 MakeLand_bmpColors =
  (int *) calloc (MakeLand_Width * MakeLand_Height, sizeof (int));
 MakeLand_InternallyGeneratedBitapsFlag = TRUE;
 int i;
 for (i = MakeLand_Width * MakeLand_Height - 1; i > -1; --i)
 {
  MakeLand_bmpHeights[i] =
   0x7fff -
   ((MakeLand_bmpColors[i] & 0xFF) * ((MakeLand_bmpColors[i] >> 8) & 0xFF) *
    ((MakeLand_bmpColors[i] >> 16) & 0xFF));
  if (MakeLand_bmpHeights[i] < 0)
  {
   MakeLand_bmpHeights[i] = 0;
  }
 }
}

/////////////////////////////
void MakeLand_Cleanup ()
{
 if (TRUE == MakeLand_InternallyGeneratedBitapsFlag)
 {
  if (NULL != MakeLand_bmpHeights)
  {
   free (MakeLand_bmpHeights);
   MakeLand_bmpHeights = NULL;
  }
  if (NULL != MakeLand_bmpColors)
  {
   free (MakeLand_bmpColors);
   MakeLand_bmpColors = NULL;
  }
 }
}

/////////////////////////////
void MakeLand_Clear (int *_bitmap, int value)
{
 //make flat floor for MakeLand_bmpHeights-
 //the value you choose to set MakeLand_bmpHeights to
 //determines affects the final ratio of peaks to valleys
 //for (int i=(MakeLand_Width*MakeLand_Height)-1;i>0;i--){//amazing what this does
 int i;
 char progcount = 0;            //for user
 int End = MakeLand_Width * MakeLand_Height / 256;      //for user
 for (i = (MakeLand_Width * MakeLand_Height) - 1; i > -1; i--)
 {
  if (NULL != MakeLand_UserCallback && !progcount++)
  {
   if (MakeLand_UserCallback (End))
   {
    break;
   }
  }
  _bitmap[i] = value;
 }
}       //end of ClearBitmap()

/////////////////////////////
void MakeLand_HeightsCreate ()
{
 MakeLand_SetGaussWidth (MakeLand_GaussWidth);  //just in case someone set it manually
 if (0 != MakeLand_FFT_Flag)
 {
  MakeLand_MakeFFTLand ();
 }
 else
 {
  MakeLand_MakeMPDLand ();
 }
}

/////////////////////////////
void MakeLand_MakeMPDLand ()
{
 int Size =
  (MakeLand_Width > MakeLand_Height) ? MakeLand_Height : MakeLand_Width;
 int i,
  x,
  x_offset,
  y,
  y_offset,
  r,
  Step;
 int ul,
  ur,
  lr,
  ll,
  MidT,
  MidB,
  MidR,
  MidL;
 int Midpoint;                  //(0xffffffff>>2);
 MakeLand_SeedRand (MakeLand_Seed, (unsigned int) -1);
 //give user an end point for his progress monitoring:[BROKEN]
 int UserEnd = Size;
 i = Size;
 while ((i /= 2) > 1)
  ++UserEnd;
 //set up crucial variables:
 Step = Size;
 r = ML_RANDMASK;
 //Initialize Map: these represent the 4 pixels that get comprise the entire first pass
 //Note that the starting, [0], never would get touched even on first pass
 //It is, in this style of mapmaking, the pixel that ultimately affects everything.
 // MakeLand_bmpHeights[0]=((MakeLand_Rand()%r)-(r>>1));
 // MakeLand_bmpHeights[(Size>>1)]=((MakeLand_Rand()%r)-(r>>1));
 // MakeLand_bmpHeights[(Size>>1)*Size]=((MakeLand_Rand()%r)-(r>>1));
 // MakeLand_bmpHeights[(Size>>1)*Size+(Size>>1)]=((MakeLand_Rand()%r)-(r>>1));
 //start mapmaking iterations:
 for (i = Size; i > 1; i /= 2)
 {
  if (MakeLand_UserCallback)
   if (MakeLand_UserCallback (UserEnd))
   {
    break;
   }
  for (x = 0; x < Size; x += Step)
  {
   if ((x_offset = x + Step) >= Size)
    x_offset -= Size;   //x_offset=0;
   for (y = 0; y < Size; y += Step)
   {
    if ((y_offset = y + Step) >= Size)
     y_offset -= Size;  //y_offset=0;
    //Get 4 corners of square:
    ul = MakeLand_bmpHeights[y * Size + x];
    ur = MakeLand_bmpHeights[y * Size + x_offset];
    ll = MakeLand_bmpHeights[y_offset * Size + x];
    lr = MakeLand_bmpHeights[y_offset * Size + x_offset];
    //Determine MakeLand_random displacements for diamnond points and mid point:
    MidT = ((ul + ur) >> 1) + ((MakeLand_Rand () % r) - (r >> 1));
    MidB = ((ll + lr) >> 1) + ((MakeLand_Rand () % r) - (r >> 1));
    MidR = ((ur + lr) >> 1) + ((MakeLand_Rand () % r) - (r >> 1));
    MidL = ((ul + ll) >> 1) + ((MakeLand_Rand () % r) - (r >> 1));
    Midpoint =
     ((MidT + MidB + MidL + MidR) >> 2) + ((MakeLand_Rand () % r) - (r >> 1));
    //Apply MakeLand_random displacements to diamond points and mid point:
    MakeLand_bmpHeights[y * Size + (x + (Step >> 1))] = MidT;
    MakeLand_bmpHeights[y_offset * Size + (x + (Step >> 1))] = MidB;
    MakeLand_bmpHeights[(y + (Step >> 1)) * Size + x_offset] = MidR;
    MakeLand_bmpHeights[(y + (Step >> 1)) * Size + x] = MidL;
    MakeLand_bmpHeights[(y + (Step >> 1)) * Size + (x + (Step >> 1))] =
     Midpoint;
    //this is a variation that sums with whatever already existed on bitmap:
    // MidT=MakeLand_bmpHeights[y*Size+(x+(Step>>1))]+=((ul+ur)>>1)+((MakeLand_Rand()%r)-(r>>1));
    // MidB=MakeLand_bmpHeights[y_offset*Size+(x+(Step>>1))]+=((ll+lr)>>1)+((MakeLand_Rand()%r)-(r>>1));
    // MidR=MakeLand_bmpHeights[(y+(Step>>1))*Size+x_offset]+=((ur+lr)>>1)+((MakeLand_Rand()%r)-(r>>1));
    // MidL=MakeLand_bmpHeights[(y+(Step>>1))*Size+x]+=((ul+ll)>>1)+((MakeLand_Rand()%r)-(r>>1));
    // MakeLand_bmpHeights[(y+(Step>>1))*Size+(x+(Step>>1))]+=((MidT+MidB+MidL+MidR)>>2)+((MakeLand_Rand()%r)-(r>>1));
   }    //end y
  }     //end x
  Step /= 2;
  r = (int) ((r / MakeLand_LandSmoothness) + 0.5);
  if (!(r & 1))
   ++r; //if it is even, make it odd
 }      //end i
}       //end of Create

/////////////////////////////
void MakeLand_SetGaussWidth (int MakeLand_Width)
{
 MakeLand_GaussWidth = MakeLand_Width;
 if (MakeLand_GaussWidth < 0)
 {
  MakeLand_GaussWidth = 0;
 }
 MakeLand_Rand = MakeLand_rand;
 if (MakeLand_GaussWidth > 0)
 {
  MakeLand_Rand = MakeLand_gaussian;
 }
}

/////////////////////////////
//MakeLand_gaussian approximation 
//center should equal ML_RANDMASK>>1
int MakeLand_gaussian ()
{
 static int oldg = 0;
 if (MakeLand_GaussWidth == 1)
 {
  return oldg = (MakeLand_rand () + oldg) >> 1;
 }
 //weird MakeLand_gaussian approximation using standard MakeLand_rand implementation in C libraries:
 //center is basically floating, depending on widthshift
 oldg += MakeLand_rand ();
 return (oldg -= (oldg >> MakeLand_GaussWidth)) >> MakeLand_GaussWidth;
}

/////////////////////////////
//################################################
//START PSEUDORANDOM NUMBER GENERATOR SECTION

/********************************************************************
 The McGill Super-Duper Random Number Generator
 G. Marsaglia, K. Ananthanarayana, N. Paul
 Incorporating the Ziggurat method of sampling from decreasing
 or symmetric unimodal density functions.
 G. Marsaglia, W.W. Tsang
 Rewritten into C by E. Schneider 
 [Date last modified: 05-Jul-1997]
 *********************************************************************/
#define ML_MULT 69069L
#define ML_RANDSHIFT 12 //12 gives range 0 to 1048576
unsigned long MakeLand_mcgn = 3677;
unsigned long MakeLand_srgn = 127;

/////////////////////////////
//IMPORTANT NOTE:when using a fixed i2, for some reason seed pairs 
//for i1 like this: 
// even 
// even+1 
//produce idential sequences when r2 returned (r1 >> 12).
//Practically, this means that 2 and 3 
//produce one landscape; likewise 6 and 7, 100 and 101, etc.
//This is why i do the dopey "add 1" to i2
void MakeLand_SeedRand (unsigned int i1, unsigned int i2)
//void rstart (long i1, long i2)
{
 if (i2 == (unsigned int) -1)
  i2 = i1 + 1;  //yech
 MakeLand_Seed = i1;
 MakeLand_mcgn = (unsigned long) ((i1 == 0L) ? 0L : i1 | 1L);
 MakeLand_srgn = (unsigned long) ((i2 == 0L) ? 0L : (i2 & 0x7FFL) | 1L);
}

/////////////////////////////
//returns positive int between 0 and 2^20 (1048576)
int MakeLand_rand (void)
{
 unsigned long r0,
  r1;

 r0 = (MakeLand_srgn >> 15);
 r1 = MakeLand_srgn ^ r0;
 r0 = (r1 << 17);
 MakeLand_srgn = r0 ^ r1;
 MakeLand_mcgn = ML_MULT * MakeLand_mcgn;
 r1 = MakeLand_mcgn ^ MakeLand_srgn;
 //Use ML_RANDSHIFT 1 for full 2^31 (0 to 2147483648),
 //use ML_RANDSHIFT 12 for 2^20 (0 to 1048576) like old MakeLand_rand();
 return (r1 >> 1);
}

/////////////////////////////
//returns int from -2^31 to +2^31; "PM" means "PlusMinus"
int MakeLand_randPM ()
{
 unsigned long r0,
  r1;

 r0 = (MakeLand_srgn >> 15);
 r1 = MakeLand_srgn ^ r0;
 r0 = (r1 << 17);
 MakeLand_srgn = r0 ^ r1;
 MakeLand_mcgn = ML_MULT * MakeLand_mcgn;
 r1 = MakeLand_mcgn ^ MakeLand_srgn;
 return r1;
}

/////////////////////////////
void MakeLand_MakeFFTLand ()
{
 int totalsize = MakeLand_Width * MakeLand_Height;
 if (MakeLand_UserCallback)
  if (MakeLand_UserCallback (MakeLand_Width << 2))
   return;
 COMPLEX *cmplx;
 cmplx = (COMPLEX *) calloc (totalsize, sizeof (COMPLEX));

 //always do before creating new land:
 MakeLand_SeedRand (MakeLand_Seed, (unsigned int) -1);
 //fill array with random numbers:
 int i = 0;
 for (; i < totalsize; i++)
 {      //set magnitude to pixel values, phase to zero:
  cmplx[i].real = (double) (MakeLand_Rand ());
  cmplx[i].imag = 0.0;  //not even necessary
 }

 //do a 2d MakeLand_FFT to translate random field to freq domain:
 MakeLand_FFT2D (cmplx, MakeLand_Width, MakeLand_Height, 1);    //forward MakeLand_FFT

 /* 
    //TYPE ONE:
    //NOTE: Following algo 1/f^power scaling, using corners as 0Hz 
    //It needs no translation after processing; ready for inverse MakeLand_FFT
    //1/f^power scaling, needs no translation from original, uses corners as 0
    //start of corner-BMP scaling algo
    for (int x=0;x<MakeLand_Width;x++) for (int y=0;y<MakeLand_Height;y++){
    int x1=x,y1=y;
    if (x1>(MakeLand_Width >>1)) x1=(MakeLand_Width )-x;
    if (y1>(MakeLand_Height>>1)) y1=(MakeLand_Height)-y;
    if (x1==0 && y1==0) y1++;//hack to avoid divide-by-zero
    //now do the 1/f^power computation:
    cmplx[y*MakeLand_Width+x].real/=(pow(sqrt((x1*x1)+(y1*y1)),MakeLand_LandSmoothness));
    cmplx[y*MakeLand_Width+x].imag/=(pow(sqrt((x1*x1)+(y1*y1)),MakeLand_LandSmoothness));
    // MakeLand_bmpHeights[y*MakeLand_Width+x]/=(sqrt((x1*x1)+(y1*y1)));
    } 
    //end of corner-BMP scaling algo
    //END TYPE ONE
  */

 //TYPE TWO:
 //Following algo does 1/f^MakeLand_LandSmoothness scaling using center as 0Hz
 //then quadrant swaps it to make it ready for inverse MakeLand_FFT.
 //(i,e., filter is centered around middle of BMP)
 //This tends to be more convienient than the simpler
 //corner-oriented scaling above, even though this one requires 
 //subsequent quandrant swapping, because it is simply more 
 //intuitive to process a mound in the center of the bitmap.
 //For example, it is very easy to do the offset operation
 //with this one.
 //start of middle-BMP scaling algo
 int Xoffset = 0,
  Yoffset = 0;
 int x,
  y;
 for (x = 0; x < MakeLand_Width; x++)
  for (y = 0; y < MakeLand_Height; y++)
  {
   int x1 = Xoffset + x - (MakeLand_Width >> 1),
    y1 = Yoffset + y - (MakeLand_Height >> 1);
   if (x1 == 0 && y1 == 0)
    y1++;       //hack to avoid divide-by-zero
   //now do the 1/f^MakeLand_LandSmoothness computation:
   cmplx[y * MakeLand_Width +
         x].real /= (pow (sqrt ((x1 * x1) + (y1 * y1)),
                          MakeLand_LandSmoothness));
   cmplx[y * MakeLand_Width +
         x].imag /= (pow (sqrt ((x1 * x1) + (y1 * y1)),
                          MakeLand_LandSmoothness));

   //solid low-pass filter example:
   //double Radius=3000.0;
   //int q=((MakeLand_Width>>1)-x)*((MakeLand_Width>>1)-x)+((MakeLand_Height>>1)-y)*((MakeLand_Height>>1)-y);
   //if (q>Radius) {
   // double d1=Radius/q;
   // cmplx[y*MakeLand_Width+x].real*=d1;
   // cmplx[y*MakeLand_Width+x].imag*=d1;
   //}
  }
 //end of middle-BMP scaling algo

 //Now start of quadrant swapping (required for middle-BMP scaling algo):
 //Swap ul with lr and ur with ll quadrants - this is required with
 //the middle-BMP style of power filtering (above) in order to put
 //the low-frequency portion of the bitmap (i.e., the corners) so that 
 //the inverse 2dFFT algo will process correctly.
 int halfsize;
 int origpos,
  newpos;
 double tmpy;

 // swap UL with LR
 halfsize = (totalsize >> 1) + (MakeLand_Width >> 1);
 for (x = 0; x < (MakeLand_Width >> 1); x++)
 {
  for (y = 0; y < (MakeLand_Height >> 1); y++)
  {
   origpos = y * MakeLand_Width + x;
   newpos = origpos + halfsize;
   tmpy = cmplx[origpos].real;
   cmplx[origpos].real = cmplx[newpos].real;
   cmplx[newpos].real = tmpy;
   tmpy = cmplx[origpos].imag;
   cmplx[origpos].imag = cmplx[newpos].imag;
   cmplx[newpos].imag = tmpy;
  }
 }

 // swap UR with LL
 halfsize = (totalsize >> 1) - (MakeLand_Width >> 1);
 for (x = (MakeLand_Width >> 1); x < MakeLand_Width; x++)
 {
  for (y = 0; y < (MakeLand_Height >> 1); y++)
  {
   origpos = y * MakeLand_Width + x;
   newpos = origpos + halfsize;
   tmpy = cmplx[origpos].real;
   cmplx[origpos].real = cmplx[newpos].real;
   cmplx[newpos].real = tmpy;
   tmpy = cmplx[origpos].imag;
   cmplx[origpos].imag = cmplx[newpos].imag;
   cmplx[newpos].imag = tmpy;
  }
 }
 //End of quadrant swapping:
 //END TYPE TWO

 /*
    //start slider
    {
    int x,y,rpos,lpos;
    double tmpy;
    int xmov=40,ymov=40;
    for (int j=0;j<xmov;j++) {
    x=MakeLand_Width-1;
    do {
    y=MakeLand_Height-1;
    do {
    rpos=y*MakeLand_Width+x;
    lpos=rpos-1;
    tmpy=cmplx[rpos].real;
    cmplx[rpos].real=cmplx[lpos].real;
    cmplx[lpos].real=tmpy;
    tmpy=cmplx[rpos].imag;
    cmplx[rpos].imag=cmplx[lpos].imag;
    cmplx[lpos].imag=tmpy;
    } while (--y>-1);
    }while (--x>0);//end x
    }//end j
    for (j=0;j<ymov;j++) {
    x=MakeLand_Width-1;
    do {
    y=MakeLand_Height-1;
    do {
    rpos=y*MakeLand_Width+x;
    lpos=rpos-MakeLand_Width;
    tmpy=cmplx[rpos].real;
    cmplx[rpos].real=cmplx[lpos].real;
    cmplx[lpos].real=tmpy;
    tmpy=cmplx[rpos].imag;
    cmplx[rpos].imag=cmplx[lpos].imag;
    cmplx[lpos].imag=tmpy;
    } while (--y>0);
    }while (--x>-1);//end x
    }//end j
    }
    //end slider
  */

 //do inverse 2d MakeLand_FFT to translate processed field to spatial domain:
 MakeLand_FFT2D (cmplx, MakeLand_Width, MakeLand_Height, -1);
 //transfer it to MakeLand_bmpHeights:
 for (i = 0; i < totalsize; i++)
  MakeLand_bmpHeights[i] = (int) (cmplx[i].real + 0.5);
 // MakeLand_bmpHeights[i]=(int)(log(1+cmplx[i].real)*5000);
 MakeLand_HeightsBottomReference (0);
 free (cmplx);
}

int MakeLand_FFT2D (COMPLEX * cmplx, int nx, int ny, int dir)
{
 int i,
  j;
 int m;
 double *real,
 *imag;

 if (MakeLand_Powerof2 (nx, &m))
  return (FALSE);

 // Transform the rows 
 real = (double *) calloc (nx, sizeof (double));
 if (real == NULL)
  return (FALSE);
 imag = (double *) calloc (nx, sizeof (double));
 if (imag == NULL)
 {
  free (real);
  return (FALSE);
 }
 for (j = 0; j < ny; j++)
 {
  if (MakeLand_UserCallback)
   if (MakeLand_UserCallback (MakeLand_Width << 2))
    return FALSE;
  for (i = 0; i < nx; i++)
  {
   real[i] = cmplx[i * MakeLand_Width + j].real;
   imag[i] = cmplx[i * MakeLand_Width + j].imag;
  }
  MakeLand_FFT (dir, m, real, imag);
  for (i = 0; i < nx; i++)
  {
   cmplx[i * MakeLand_Width + j].real = real[i];
   cmplx[i * MakeLand_Width + j].imag = imag[i];
  }
 }
 free (real);
 free (imag);

 if (MakeLand_Powerof2 (ny, &m))
  return (FALSE);
 // Transform the columns 
 real = (double *) calloc (nx, sizeof (double));
 if (real == NULL)
  return (FALSE);
 imag = (double *) calloc (nx, sizeof (double));
 if (imag == NULL)
 {
  free (real);
  return (FALSE);
 }

 for (i = 0; i < nx; i++)
 {
  if (MakeLand_UserCallback)
   if (MakeLand_UserCallback (MakeLand_Width << 2))
    return FALSE;
  for (j = 0; j < ny; j++)
  {
   real[j] = cmplx[i * MakeLand_Width + j].real;
   imag[j] = cmplx[i * MakeLand_Width + j].imag;
  }
  MakeLand_FFT (dir, m, real, imag);
  for (j = 0; j < ny; j++)
  {
   cmplx[i * MakeLand_Width + j].real = real[j];
   cmplx[i * MakeLand_Width + j].imag = imag[j];
  }
 }
 free (real);
 free (imag);
 return TRUE;
}

/////////////////////////////////
//MakeLand_FFT stuff follows:
int MakeLand_FFT (int dir, int m, double *x, double *y)
{
 long nn,
  i,
  i1,
  j,
  k,
  i2,
  l,
  l1,
  l2;
 double c1,
  c2,
  tx,
  ty,
  t1,
  t2,
  u1,
  u2,
  z;

 // Calculate the number of points
 nn = 1;
 for (i = 0; i < m; i++)
  nn *= 2;

 // Do the bit reversal 
 i2 = nn >> 1;
 j = 0;
 for (i = 0; i < nn - 1; i++)
 {
  if (i < j)
  {
   tx = x[i];
   ty = y[i];
   x[i] = x[j];
   y[i] = y[j];
   x[j] = tx;
   y[j] = ty;
  }
  k = i2;
  while (k <= j)
  {
   j -= k;
   k >>= 1;
  }
  j += k;
 }

 // Compute the MakeLand_FFT
 c1 = -1.0;
 c2 = 0.0;
 l2 = 1;
 for (l = 0; l < m; l++)
 {
  l1 = l2;
  l2 <<= 1;
  u1 = 1.0;
  u2 = 0.0;
  for (j = 0; j < l1; j++)
  {
   for (i = j; i < nn; i += l2)
   {
    i1 = i + l1;
    t1 = u1 * x[i1] - u2 * y[i1];
    t2 = u1 * y[i1] + u2 * x[i1];
    x[i1] = x[i] - t1;
    y[i1] = y[i] - t2;
    x[i] += t1;
    y[i] += t2;
   }
   z = u1 * c1 - u2 * c2;
   u2 = u1 * c2 + u2 * c1;
   u1 = z;
  }
  c2 = sqrt ((1.0 - c1) / 2.0);
  if (dir == 1)
   c2 = -c2;
  c1 = sqrt ((1.0 + c1) / 2.0);
 }

 // Scaling for forward transform 
 if (dir == 1)
 {
  for (i = 0; i < nn; i++)
  {
   x[i] /= (double) nn;
   y[i] /= (double) nn;
  }
 }
 return TRUE;
}

/////////////////////////////////
//returns 0 if number is power of 2
int MakeLand_Powerof2 (int number, int *power)
{
 *power = 0;
 int orignum = number;
 while ((number >>= 1) > 0)
  ++ * power;
 return (orignum - (1 << *power));
}

/////////////////////////////////
int MakeLand_HeightsBottomReference (int newbottom)
{
 if (MakeLand_UserCallback)
  if (MakeLand_UserCallback (MakeLand_Width))
   return 0;
 int i,
  totalsize = MakeLand_Width * MakeLand_Height - 1;
 //find lowest pixel on image
 newbottom -= MakeLand_GetBottomHeight ();      //offset
 for (i = totalsize; i > -1; i--)
 {
  MakeLand_bmpHeights[i] += newbottom;
 }
 return newbottom;
}       //end raw_bottom_reference()

/////////////////////////////////
int MakeLand_GetBottomHeight ()
{
 int j,
  bot_value = 2147483647;
 //find highest value on bitmap:
 for (j = MakeLand_Width * MakeLand_Height - 1; j > -1; j--)
 {
  if (MakeLand_bmpHeights[j] < bot_value)
   bot_value = MakeLand_bmpHeights[j];
 }
 return bot_value;
}

/////////////////////////////////
//assigns colors on MakeLand_bmpColors from a palette to 
//altitudes from MakeLand_bmpHeights
void MakeLand_TranslateHeightsToColors (int palsize)
{
 int UpperLimit = MakeLand_GetTopHeight ();
 int LowerLimit = MakeLand_GetBottomHeight ();
 double scalar;
 int i,
  offset,
  index;
 scalar = ((double) palsize) / (UpperLimit - LowerLimit);
 offset = 0 - LowerLimit;
 --palsize;
 for (i = (MakeLand_Height * MakeLand_Width) - 1; i > -1; i--)
 {
  index = (int) ((MakeLand_bmpHeights[i] + offset) * scalar);
  if (index > palsize)
   index = palsize;
  else if (index < 0)
   index = 0;
  MakeLand_bmpColors[i] = MakeLand_DefaultPalette[index];
 }
}

/////////////////////////////////
int MakeLand_GetTopHeight ()
{
 int j,
  top_value = -2147483647;
 //find highest value on bitmap:
 for (j = MakeLand_Width * MakeLand_Height - 1; j > -1; j--)
 {
  if (MakeLand_bmpHeights[j] > top_value)
   top_value = MakeLand_bmpHeights[j];
 }
 return top_value;
}

/*
   /////////////////////////////////
   //This doesn't look right in Linux because (I believe) bit order is different!
   unsigned int MakeLand_DefaultPalette[256] = {
   0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,
   0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,
   0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,
   0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,
   0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,
   0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,
   0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,
   0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff
   };
 */

/////////////////////////////////
//This doesn't look right in Linux because (I believe) bit order is different!
unsigned int MakeLand_DefaultPalette[256] = {
 0x291F17, 0x2D2319, 0x322216, 0x32251B, 0x33291D, 0x3A2514, 0x3A261B,
 0x3D2A15, 0x3C2A1D, 0x3D301F, 0x3A2C21, 0x3D3225, 0x42261C, 0x422A14,
 0x422C1D, 0x492A16, 0x4A2D1B, 0x45301D, 0x4B311D, 0x522E13, 0x502E1B,
 0x5A2D15, 0x5B2F18, 0x543016, 0x52321D, 0x56381F, 0x5C3015, 0x5B331C,
 0x5C381E, 0x402720, 0x422E21, 0x4B2E21, 0x443223, 0x443528, 0x463926,
 0x453A2B, 0x4B3323, 0x4B3628, 0x4C3825, 0x4C3A2B, 0x4B3D30, 0x523523,
 0x523629, 0x533925, 0x533B2A, 0x5A3622, 0x5C3728, 0x5B3A25, 0x5B3D2A,
 0x563E30, 0x603317, 0x62351C, 0x64381D, 0x6A351B, 0x6B391D, 0x71361A,
 0x73391D, 0x793C1D, 0x623621, 0x633C24, 0x623D29, 0x6B3620, 0x6A3D24,
 0x6A3E29, 0x633E30, 0x733D23, 0x733E29, 0x793E22, 0x4A402E, 0x4C4032,
 0x55402D, 0x5B4026, 0x5B412C, 0x5D482E, 0x544132, 0x564832, 0x5B4331,
 0x5A4538, 0x5C4932, 0x5C4A39, 0x644026, 0x63422C, 0x62482E, 0x6B4126,
 0x6B432C, 0x6C482E, 0x634431, 0x624639, 0x644933, 0x654C38, 0x6B4531,
 0x6C4933, 0x6B4C39, 0x665039, 0x6F5037, 0x6C513B, 0x744125, 0x73442C,
 0x74482D, 0x7B4225, 0x7A452B, 0x7D4827, 0x7B492D, 0x734630, 0x754738,
 0x744A32, 0x744D39, 0x7A4630, 0x784738, 0x7B4B32, 0x7B4D39, 0x755036,
 0x73513A, 0x7D5035, 0x7C5139, 0x7E583D, 0x6D5340, 0x794F40, 0x785642,
 0x814425, 0x824629, 0x834826, 0x834A2C, 0x8C4424, 0x884629, 0x8A4926,
 0x8A4C2C, 0x824731, 0x834D32, 0x824E39, 0x8A4E31, 0x884E38, 0x87502F,
 0x8C502E, 0x845034, 0x83533A, 0x84583D, 0x8B5134, 0x8B543A, 0x8E5837,
 0x8C583C, 0x914626, 0x904E2E, 0x994E2D, 0x904E31, 0x904F38, 0x9D4E31,
 0x93502E, 0x9B522E, 0x9E582F, 0x935233, 0x935539, 0x955835, 0x93593C,
 0x9B5534, 0x9B5638, 0x9C5835, 0x9C5A3A, 0x9C603E, 0xA1552E, 0xA45B3B,
 0xB05E38, 0xAA613D, 0xB1633D, 0x835541, 0x855A42, 0x865C48, 0x8B5640,
 0x8C5B42, 0x8B5D48, 0x915640, 0x935C42, 0x925E49, 0x9B5D41, 0x9B5E48,
 0x856046, 0x8C6046, 0x8D624A, 0x8E6650, 0x8F6852, 0x936045, 0x94634A,
 0x95684E, 0x9B6144, 0x9B644A, 0x9C684D, 0x926650, 0x946A52, 0x9B6551,
 0x9B6B52, 0x9E6F58, 0x9D7056, 0x9E7258, 0xA55E40, 0xB15E43, 0xA46142,
 0xA36549, 0xA56846, 0xA4694C, 0xAC6342, 0xAA6649, 0xAC6845, 0xAC6A4B,
 0xA36650, 0xA26C52, 0xA06F58, 0xAA6650, 0xAC6C51, 0xAB704E, 0xA47055,
 0xA47359, 0xA5785E, 0xAC7154, 0xA9755A, 0xAC785C, 0xB56947, 0xB66E51,
 0xBC714C, 0xB47253, 0xB57458, 0xB3795B, 0xBC7351, 0xB97658, 0xBC7955,
 0xBC7A59, 0xA67760, 0xA67B61, 0xA87760, 0xAB7A61, 0xB07D63, 0xBF7F60,
 0xC06C4B, 0xC1734D, 0xC37854, 0xC17F60, 0xAD8166, 0xAD8268, 0xB28067,
 0xB2856A, 0xB4886E, 0xB58970, 0xB98D72, 0xBC9076, 0xBF9278, 0xC9815B,
 0xD0855E, 0xC78262, 0xC1957B, 0xC4987E, 0xD38964, 0xDC9169, 0xC69B81,
 0xC89D83, 0xCBA086, 0xC9A78B, 0xCE825C
};

//////////////////////////////////////////////////////////////////
//IMPORTANT: the following erosion function counts matrices wrong 
//at the sides of bitmaps, among other things. But the result looks
//good, and it is at least 3 times as fast as the ErosionFine() 
//clean algo.
//////////////////////////////////////////////////////////////////
//the theory of this erosion: it does double-duty, both modifying the heightfield and
//doing a color-simulation of erosion based on existing folds in a landscape.
//In practice, it is best to do the first erosion with no color modification and,
//importantly, a POSITIVE cut value -- experience shows that this somehow reduces the
//biases in the landscape for the next run in which one does cut and color the landscape.
//IOW, it allows for longer erosion lines AND less prevailence of vert,horiz, and 
//45-degree lines.
//one interesting thing: the erosion is more interesting when actually MakeLand_Height modification
//is done immediately, instead of waiting until end to do it cleanly. But color
//erosion doesn't care about this; rather, color erosion has the problem of
//being too intense or nonexistent, so the summation array is created solely for color.
//
//The function works by sending out 8 rays out from each pixel to see which direction
//averages lower, then darkens (unless snow) the neighboring pixel coresponding
//to that direction. The real trick comes next: if a "lower direction" was found,
//the algorithm then starts again with that lower pixel -- even though it will
//eventually be selected specifically later when its altitude is being singled out.
//this has the effect of "drawing over and over" certain lines while hitting other
//lines only a few times; an amazing effect, but EXTREMELY time consuming. ANother
//thing to think about: the fact that in sending out 8 simple, uncorrected rays, 
//the diagonal rays are actually going much (1.41 times) further than the vert/horiz
//rays, meaning a kind of bias in introduced. On some landscapes this is very
//noticable as either numerous sharp 45-degree OR vert/horiz lines. THere are 
//obvious corrections, but tedious to implement and slower - just running a first-pass
//with a positive cut-value goes some extent toward fixing it.
//typical values for first and second passes:
//cut: 400, shade: 7, intensity: 8, raylength: 4
//cut: -750, shade:-55, intensity: 12, raylength: 4
//shade:negative darkens, positive whitens; intensity: contrast, darkness of lines
void MakeLand_Erosion (int cut, int shade, int intensity, int raylength,
                       int crispness)
{
 int x,
  y,
  totalsize = MakeLand_Width * MakeLand_Height;
 int index_compare,
  index_start;
 int cyclecount;
 int increment,
  temp_result = 0;
 int progress;
 int sum_old,
  i,
  sum_new,
  x_index,
  result = 0;
 int *storage = (int *) calloc (totalsize, sizeof (int));
 int original_height = MakeLand_GetTopHeight ();

 //zero array and scale up heights bitmap:
 int j = totalsize - 1;
 for (; j > -1; j--)
 {
  storage[j] = 0;
 }
 MakeLand_HeightsScaleMax (0xFFFFFF);
 MakeLand_HeightsSmooth (0, 1);

 //NOTE: next line starts processing erosion lines by pixel-altitude;
 //going from lowest to hightest pixel is essential for long lines
 //(doing the reverse results in very short erosion lines)
 j = 0;
 for (y = 0; y < MakeLand_Height; y++)
 {      //start y
  if (MakeLand_UserCallback)
   if (MakeLand_UserCallback (MakeLand_Width))
    break;
  for (x = 0; x < MakeLand_Width; x++)
  {     //start x
   cyclecount = 0;
   index_start = j;
   //DEBUG LINE:
   //if (j!=y*MakeLand_Width+x) { delete [] storage;j=1/cyclecount; return;}
   do
   {    //start progress from one pixel
    progress = FALSE;
    sum_old = MakeLand_bmpHeights[index_start] * raylength;
    //result=0;
    //START of unrolled rays
    //////////////////////////////////////
    //first do Up-Middle ray: y=-1, x=0;
    sum_new = 0;
    increment = -MakeLand_Width;
    index_compare = index_start;
    i = raylength;
    do
    {   //length I send the ray
     index_compare += increment;
     if (index_compare < 0)
      index_compare += totalsize;
     if (!sum_new)
      temp_result = index_compare;
     sum_new += MakeLand_bmpHeights[index_compare];
    }
    while (--i > 0);    //end i raylength
    if (sum_new < sum_old)
    {
     sum_old = sum_new;
     result = temp_result;
     progress = TRUE;
    }
    //////////////////////////////////////
    //////////////////////////////////////
    //now do Down-Middle ray: y=1, x=0;
    sum_new = 0;
    increment = MakeLand_Width;
    index_compare = index_start;
    i = raylength;
    do
    {   //length I send the ray
     index_compare += increment;
     if (index_compare >= totalsize)
      index_compare -= totalsize;
     if (!sum_new)
      temp_result = index_compare;
     sum_new += MakeLand_bmpHeights[index_compare];
    }
    while (--i > 0);    //end i raylength
    if (sum_new < sum_old)
    {
     sum_old = sum_new;
     result = temp_result;
     progress = TRUE;
    }
    //////////////////////////////////////
    //////////////////////////////////////
    //now do Left-Middle ray: y=0, x=-1;
    sum_new = 0;
    x_index = x;
    increment = -1;
    index_compare = index_start;
    i = raylength;
    do
    {   //length I send the ray
     if ((x_index += increment) < 0)
     {
      x_index = MakeLand_Width - 1;
      index_compare += x_index;
      if (index_compare >= totalsize)
       index_compare -= totalsize;
     }
     else
      index_compare += increment;
     if (index_compare < 0)
      index_compare += totalsize;
     if (!sum_new)
      temp_result = index_compare;
     sum_new += MakeLand_bmpHeights[index_compare];
    }
    while (--i > 0);    //end i raylength
    if (sum_new < sum_old)
    {
     sum_old = sum_new;
     result = temp_result;
     progress = TRUE;
    }
    //////////////////////////////////////
    //////////////////////////////////////
    //now do Right-Middle ray: y=0, x=1;
    sum_new = 0;
    x_index = x;
    increment = 1;
    index_compare = index_start;
    i = raylength;
    do
    {   //length I send the ray
     if ((x_index += increment) >= MakeLand_Width)
     {
      x_index = 0;
      index_compare -= (MakeLand_Width - 1);
      if (index_compare < 0)
       index_compare += totalsize;
     }
     else
      index_compare += increment;
     if (index_compare >= totalsize)
      index_compare -= totalsize;
     if (!sum_new)
      temp_result = index_compare;
     sum_new += MakeLand_bmpHeights[index_compare];
    }
    while (--i > 0);    //end i raylength
    if (sum_new < sum_old)
    {
     sum_old = sum_new;
     result = temp_result;
     progress = TRUE;
    }
    //////////////////////////////////////
    //////////////////////////////////////
    //now do Upper-Left ray: y=-1, x=-1;
    sum_new = 0;
    x_index = x;
    increment = -MakeLand_Width - 1;
    index_compare = index_start;
    i = raylength;
    do
    {   //length I send the ray
     if ((x_index += -1) < 0)
     {
      x_index = MakeLand_Width - 1;
      --index_compare;
     }
     else
      index_compare += increment;
     if (index_compare < 0)
      index_compare += totalsize;
     if (!sum_new)
      temp_result = index_compare;
     sum_new += MakeLand_bmpHeights[index_compare];
    }
    while (--i > 0);    //end i raylength
    if (sum_new < sum_old)
    {
     sum_old = sum_new;
     result = temp_result;
     progress = TRUE;
    }
    //////////////////////////////////////
    //////////////////////////////////////
    //now do Lower-Left ray: y=-1, x=-1;
    sum_new = 0;
    x_index = x;
    increment = MakeLand_Width - 1;
    index_compare = index_start;
    i = raylength;
    do
    {   //length I send the ray
     if ((x_index += -1) < 0)
     {
      x_index = MakeLand_Width - 1;
      index_compare += (MakeLand_Width << 1) - 1;
     }
     else
      index_compare += increment;
     if (index_compare >= totalsize)
      index_compare -= totalsize;
     if (!sum_new)
      temp_result = index_compare;
     sum_new += MakeLand_bmpHeights[index_compare];
    }
    while (--i > 0);    //end i raylength
    if (sum_new < sum_old)
    {
     sum_old = sum_new;
     result = temp_result;
     progress = TRUE;
    }
    //////////////////////////////////////
    //////////////////////////////////////
    //now do Upper-Right ray: y=-1, x=1;
    sum_new = 0;
    x_index = x;
    increment = -(MakeLand_Width - 1);
    index_compare = index_start;
    i = raylength;
    do
    {   //length I send the ray
     if ((x_index += 1) >= MakeLand_Width)
     {
      x_index = 0;
      index_compare -= ((MakeLand_Width << 1) - 1);
     }
     else
      index_compare += increment;
     if (index_compare < 0)
      index_compare += totalsize;
     if (!sum_new)
      temp_result = index_compare;
     sum_new += MakeLand_bmpHeights[index_compare];
    }
    while (--i > 0);    //end i raylength
    if (sum_new < sum_old)
    {
     sum_old = sum_new;
     result = temp_result;
     progress = TRUE;
    }
    //////////////////////////////////////
    //////////////////////////////////////
    //now do Lower-Right ray: y=1, x=1;
    sum_new = 0;
    x_index = x;
    increment = MakeLand_Width + 1;
    index_compare = index_start;
    i = raylength;
    do
    {   //length I send the ray
     if ((x_index += 1) >= MakeLand_Width)
     {
      x_index = 0;
      ++index_compare;
     }
     else
      index_compare += increment;
     if (index_compare >= totalsize)
      index_compare -= totalsize;
     if (!sum_new)
      temp_result = index_compare;
     sum_new += MakeLand_bmpHeights[index_compare];
    }
    while (--i > 0);    //end i raylength
    if (sum_new < sum_old)
    {
     sum_old = sum_new;
     result = temp_result;
     progress = TRUE;
    }
    //////////////////////////////////////
    //END of unrolled rays

    if (progress == TRUE)
    {
     if (++cyclecount > intensity)
      progress = FALSE;
     index_start = result;
     ++storage[index_start];
     //MakeLand_bmpHeights[index_start]+=cut;
     //if (MakeLand_bmpHeights[index_start]<llimit) MakeLand_bmpHeights[index_start]=llimit;
     //else if (MakeLand_bmpHeights[index_start]>ulimit) MakeLand_bmpHeights[index_start]=ulimit;
    }
   }
   while (progress == TRUE);    //end progress
   ++j;
  }     //end x
 }      //end y
 //copy and scale-up
 for (j = totalsize - 1; j > -1; j--)
 {
  // MakeLand_bmpHeights[j] = (MakeLand_bmpHeights[j] + cut * storage[j]) >> ML_SHIFT;
  // MakeLand_bmpColors[j] = MakeLand_RampPixel_rgb (MakeLand_bmpColors[j], (shade * storage[j]) >> ML_SHIFT);
  MakeLand_bmpHeights[j] = (MakeLand_bmpHeights[j] + cut * storage[j]);
  MakeLand_bmpColors[j] =
   MakeLand_RampPixel_rgb (MakeLand_bmpColors[j], (shade * storage[j]));
 }

 MakeLand_HeightsScaleMax (original_height);

 free (storage);
}       //end Erosion()

///////////////////////////////////
int MakeLand_RampPixel_rgb (int rgb, int value)
{
 int r = ((rgb & 0xff0000) >> 16) + value;
 if (r > 255)
  r = 255;
 else if (r < 0)
  r = 0;
 int g = ((rgb & 0xff00) >> 8) + value;
 if (g > 255)
  g = 255;
 else if (g < 0)
  g = 0;
 int b = (rgb & 0xff) + value;
 if (b > 255)
  b = 255;
 else if (b < 0)
  b = 0;
 return (0xff000000 | r << 16 | g << 8 | b);
}

///////////////////////////////////
void MakeLand_RampPixel_bmp (int *bitmap, int index, int value)
{
 int rgb = bitmap[index];
 int r = ((rgb & 0xff0000) >> 16) + value;
 int g = ((rgb & 0xff00) >> 8) + value;
 int b = (rgb & 0xff) + value;
 if (r > 255)
  r = 255;
 else if (r < 0)
  r = 0;
 if (g > 255)
  g = 255;
 else if (g < 0)
  g = 0;
 if (b > 255)
  b = 255;
 else if (b < 0)
  b = 0;
 bitmap[index] = (0xff000000 | r << 16 | g << 8 | b);
}

///////////////////////////////////
//source_weight: 0 means all pixels in 3x3 equally weighted
void MakeLand_HeightsSmooth (int source_weight, int iterations)
{
 if (source_weight < 0)
  source_weight = 0;
 double divisor = 1.0 / (source_weight + 9);
 ++source_weight;
 int AbortFlag = 0;
 int *HeightBuffer = MakeLand_bmpHeights;
 int totalsize = MakeLand_Width * MakeLand_Height;
 int *TempBuffer = (int *) calloc (totalsize, sizeof (int));
 int x,
  y,
  xi,
  yi,
  xm,
  ym,
  sum,
  index_start;
 int end = MakeLand_Width * iterations;
 for (; iterations > 0; iterations--)
 {
  for (x = 0; x < MakeLand_Width; x++)
  {
   if (MakeLand_UserCallback)
    if (MakeLand_UserCallback (end))
    {
     AbortFlag = 1;
     break;
    }
   for (y = 0; y < MakeLand_Height; y++)
   {
    sum =
     HeightBuffer[(index_start = y * MakeLand_Width + x)] * source_weight;
    for (xm = -1; xm < 2; xm++)
    {
     if ((xi = x + xm) < 0)
      xi += MakeLand_Width;
     else if (xi >= MakeLand_Width)
      xi -= MakeLand_Width;
     for (ym = -1; ym < 2; ym++)
     {
      if (!xm && !ym)
       ym = 1;
      if ((yi = y + ym) < 0)
       yi += MakeLand_Height;
      else if (yi >= MakeLand_Height)
       yi -= MakeLand_Height;
      sum += HeightBuffer[yi * MakeLand_Width + xi];
     }  //end ym
    }   //end xm 
    //////////////////////////////////////
    // HeightBuffer[index_start]=(int)(sum*0.111111111f+0.5f);
    // TempBuffer[index_start]=(int)(sum*0.111111111f+0.5f);
    TempBuffer[index_start] = (int) ((sum * divisor) + 0.5);
   }    //end y
  }     //end x
  if (!AbortFlag)
   MakeLand_blit (TempBuffer, HeightBuffer, 0, 0, 0, 0, MakeLand_Width,
                  MakeLand_Height);
 }      //end iterations
 free (TempBuffer);
}       //TerrainSmooth

/////////////////////////////////
void MakeLand_blit (int *pTempBitmapSource, int *pTempBitmapDest, int i,
                    int j, int x, int y, int Width, int Height)
{
 i = MakeLand_Width * MakeLand_Height - 1;
 do
  pTempBitmapDest[i] = pTempBitmapSource[i];
 while (--i > -1);
 i = MakeLand_Width * MakeLand_Height;
}

/////////////////////////////////
//finds highest point, scales it to new top, then uses same 
//discovered factor to scale everything else
void MakeLand_HeightsScaleMax (int top)
{
 int j,
  End = MakeLand_Width * MakeLand_Height / 256; //for user
 char progcount = 0;            //for user
 double HighestPoint = MakeLand_GetTopHeight ();
 if (HighestPoint == 0)
 {
  HighestPoint = 1;
 }
 double scale_factor = top / HighestPoint;
 //scale all pixels in image
 for (j = MakeLand_Width * MakeLand_Height - 1; j > -1; j--)
 {
  if (MakeLand_UserCallback && !progcount++)
  {
   if (MakeLand_UserCallback (End))
   {
    break;
   }
  }
  MakeLand_bmpHeights[j] =
   (int) ((MakeLand_bmpHeights[j] * scale_factor) + 0.5);
 }
}       //end scale_image()

/////////////////////////////////////////////////////////////////////
int MakeLand_FindClosestPalColor (int red, int green, int blue)
{
 int i,
  pRed,
  pGreen,
  pBlue,
  sum,
  PalEntry = 0,
  color_window = 0x7fffffff,
  rgb;
 for (i = 0; i < 256; i++)      //pick closest color out of 256
 {
  rgb = MakeLand_DefaultPalette[i];
  pBlue = blue - (rgb & 0xff);
  pGreen = green - ((rgb >> 8) & 0xff);
  pRed = red - ((rgb >> 16) & 0xff);
  sum = pRed * pRed * 299 + pGreen * pGreen * 587 + pBlue * pBlue * 114;

  if (color_window > sum)       //if this pick is better than the last
  {
   color_window = sum;
   PalEntry = i;
  }
 }      //end i
 return PalEntry;
}       //end FindClosestPalColor()

/////////////////////////////////////////////////////////////////////
// Blur - IMPORTANT: Higher source_weight means less blur
/////////////////////////////////////////////////////////////////////
void MakeLand_ColorsBlur (int source_weight, int iterations, int PalFlag)
{
 double divisor = 1.0 / (source_weight + 9);
 int *TempBitmap;
 TempBitmap =
  (int *) malloc (MakeLand_Width * MakeLand_Height * sizeof (int));
 int x,
  y,
  x_step,
  y_step,
  x_temp,
  y_temp,
  sum_red,
  sum_green,
  sum_blue,
  color;

 if (iterations < 1)
  iterations = 1;       //necessary for backward compatibility 
 int end = iterations * MakeLand_Width; //for user 

 while (--iterations > -1)
 {
  MakeLand_blit (MakeLand_bmpColors, TempBitmap, 0, 0, 0, 0, MakeLand_Width,
                 MakeLand_Height);
  for (x = 0; x < MakeLand_Width; x++)
  {     //start x
   if (MakeLand_UserCallback)
    if (MakeLand_UserCallback (end))
     break;
   for (y = 0; y < MakeLand_Height; y++)
   {    //start y
    sum_red = sum_green = sum_blue = 0; //reset sums for new average
    //start with upper left pixel of the 3x3
    for (x_step = -1; x_step < 2; x_step++)
    {
     x_temp = x + x_step;
     if (x_temp < 0)
      x_temp += MakeLand_Width;
     else if (x_temp >= MakeLand_Width)
      x_temp -= MakeLand_Width;
     for (y_step = -1; y_step < 2; y_step++)
     {
      y_temp = y + y_step;
      if (y_temp < 0)
       y_temp += MakeLand_Height;
      else if (y_temp >= MakeLand_Height)
       y_temp -= MakeLand_Height;
      //now I take the rgb info from the designated pixel:
      color = TempBitmap[y_temp * MakeLand_Width + x_temp];
      sum_red += ((color >> 16) & 0xff);
      sum_green += ((color >> 8) & 0xff);
      sum_blue += (color & 0xff);
      //now I emphasize the original pixel:
      if (x_step == 0 && y_step == 0 && source_weight != 0)
      {
       sum_red += (((color >> 16) & 0xff) * source_weight);
       sum_green += (((color >> 8) & 0xff) * source_weight);
       sum_blue += ((color & 0xff) * source_weight);
      }
     }  //y_temp end
    }   // x_temp end
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
    if (PalFlag != 0)
     MakeLand_bmpColors[y * MakeLand_Width + x] =
      MakeLand_DefaultPalette[MakeLand_FindClosestPalColor
                              (sum_red, sum_green, sum_blue)];
    else
     MakeLand_bmpColors[y * MakeLand_Width + x] =
      sum_red << 16 | sum_green << 8 | sum_blue;
   }    //y end
  }     //x end
 }      //end iterations
 free (TempBitmap);
}       //end Blur

//////////////////////////////////////////////////////////////
//source_weight: 0 means all pixels in 3x3 equally weighted
void MakeLand_TerrainSmooth (int source_weight, int iterations)
{
 if (source_weight < 0)
  source_weight = 0;
 double divisor = 1.0 / (source_weight + 9);
 ++source_weight;
 int AbortFlag = 0;
 int *HeightBuffer = MakeLand_bmpHeights;
 int *TempBuffer =
  (int *) malloc (MakeLand_Width * MakeLand_Height * sizeof (int));
 int x,
  y,
  xi,
  yi,
  xm,
  ym,
  sum,
  index_start;
 int end = MakeLand_Width * iterations;

 for (; iterations > 0; iterations--)
 {
  for (x = 0; x < MakeLand_Width; x++)
  {
   if (MakeLand_UserCallback)
    if (MakeLand_UserCallback (end))
    {
     AbortFlag = 1;
     break;
    }
   for (y = 0; y < MakeLand_Height; y++)
   {
    sum =
     HeightBuffer[(index_start = y * MakeLand_Width + x)] * source_weight;
    for (xm = -1; xm < 2; xm++)
    {
     if ((xi = x + xm) < 0)
      xi += MakeLand_Width;
     else if (xi >= MakeLand_Width)
      xi -= MakeLand_Width;
     for (ym = -1; ym < 2; ym++)
     {
      if (!xm && !ym)
       ym = 1;
      if ((yi = y + ym) < 0)
       yi += MakeLand_Height;
      else if (yi >= MakeLand_Height)
       yi -= MakeLand_Height;
      sum += HeightBuffer[yi * MakeLand_Width + xi];
     }  //end ym
    }   //end xm 
    //////////////////////////////////////
    // HeightBuffer[index_start]=(int)(sum*0.111111111f+0.5f);
    // TempBuffer[index_start]=(int)(sum*0.111111111f+0.5f);
    TempBuffer[index_start] = (int) ((sum * divisor) + 0.5);
   }    //end y
  }     //end x
  if (!AbortFlag)
   MakeLand_blit (TempBuffer, HeightBuffer, 0, 0, 0, 0, MakeLand_Width,
                  MakeLand_Height);
 }      //end iterations
 free (TempBuffer);
}       //TerrainSmooth

///////////////////////////////////////////////////////////////
//FOLLOWING WAS THE FIRST TRUE SHADING ALGO NOT 
//USING DERIVITIVE METHOD; USES VECTOR MATH INSTEAD
void MakeLand_ColorsShade (int intensity,
                           char direction,
                           int BlurIterations,
                           int BlurSourcePixelWeight, int Max)
{
 int start,
  val,
  x,
  y;
 int x1,
  x2,
  xs,
  xr;
 int y1,
  y2,
  ys,
  yr;
 int z1,
  z2,
  zs,
  zr;

 //set up sun coordinates:
 // xs=1,ys=1,zs=10;

 //positive x: lightsource right
 //positive y: lightsource above
 //following UP works very well:
 // xs= 0;
 // ys= 3;
 // zs=-2;

 zs = 0;

 switch (direction)
 {
 case 'd':     //"down"
  xs = 0;
  ys = -1;
  // zs=-2;
  break;

 case 'u':     //"up"
  xs = 0;
  ys = 1;
  // zs=-2;
  break;

 case 'l':     //"left"
  xs = -1;
  ys = 0;
  // zs=-2;
  break;

 default:      //default is 'r', or right
  xs = 1;
  ys = 0;
  // zs=-2;
  break;
 }

 int i = MakeLand_Width * MakeLand_Height - 1;
 int *TempBitmap =
  (int *) malloc (MakeLand_Width * MakeLand_Height * sizeof (int));
 for (; i > -1; i--)
  TempBitmap[i] = 0;

 for (x = MakeLand_Width - 1; x > -1; x--)
  for (y = MakeLand_Height - 1; y > -1; y--)
  {
   //first set up the heightfield vectors:
   start = y * MakeLand_Width + x;
   x1 = 1;
   x2 = 0;
   y1 = 0;
   y2 = 1;

   if ((x + 1) >= MakeLand_Width)
    z1 = MakeLand_bmpHeights[y * MakeLand_Width] - MakeLand_bmpHeights[start];
   else
    z1 = MakeLand_bmpHeights[start + 1] - MakeLand_bmpHeights[start];

   if ((y + 1) >= MakeLand_Height)
    z2 = MakeLand_bmpHeights[x] - MakeLand_bmpHeights[start];
   else
    z2 =
     MakeLand_bmpHeights[start + MakeLand_Width] - MakeLand_bmpHeights[start];

   //now do the cross-product:
   xr = y1 * z2 - z1 * y2;
   yr = z1 * x2 - x1 * z2;
   zr = x1 * y2 - y1 * x2;

   //now use dotproduct to convert angle in to some sort of magnitude:
   val = xs * xr + ys * yr + zs * zr;

   //now manifest the result:
   // RampPixel(ColorsBitmap,start,val*intensity);
   TempBitmap[y * MakeLand_Width + x] = val * intensity;
  }

 int *Holder = MakeLand_bmpHeights;
 MakeLand_bmpHeights = TempBitmap;
 MakeLand_TerrainSmooth (BlurSourcePixelWeight, BlurIterations);
 MakeLand_bmpHeights = Holder;
 for (i = MakeLand_Width * MakeLand_Height - 1; i > -1; i--)
 {
  // if (TempBitmap[i]!=0) RampPixel(ColorsBitmap,i,TempBitmap[i]);
  if (TempBitmap[i] != 0)
  {
   if (TempBitmap[i] > Max)
    TempBitmap[i] = Max;
   else if (TempBitmap[i] < -Max)
    TempBitmap[i] = -Max;
   MakeLand_RampPixel_bmp (MakeLand_bmpColors, i, TempBitmap[i]);
  }
 }

 free (TempBitmap);
 return;
}

///////////////////////////////////////////////////////////////
//
void MakeLand_ErosionFine (int cut, int shade, int intensity, int raylength,
                           int crispness)
{
#define ML_AFACTOR 724  //hypoteneuse, 0.707106781186547524400844362104849*(1<<ML_SHIFT)
#define ML_RAYOFFSET 300        //(1<<ML_SHIFT)-ML_AFACTOR: to ensure first summed pixel not starting pixel

 //#define ML_SHIFT 20
 //#define ML_AFACTOR 741455//724 //the length of the hypoteneuse ray, 724
 //#define ML_RAYOFFSET 307121//1048576-ML_AFACTOR: to ensure first summed pixel not starting pixel

 int x,
  y,
  totalsize = MakeLand_Width * MakeLand_Height;
 int xi,
  yi,
  xm,
  ym,
  xstep,
  ystep;
 int cyclecount;
 int temp_result = 0,
  rawindex;
 int progress = 0;
 int sum_old,
  i,
  sum_new,
  result = 0;
 int *storage = (int *) calloc (totalsize, sizeof (int));
 int original_height = MakeLand_GetTopHeight ();
 int start;
 int scaledwidth = MakeLand_Width << ML_SHIFT;
 int scaledheight = MakeLand_Height << ML_SHIFT;
 //zero array and scale up heights bitmap:

 for (i = totalsize - 1; i > -1; i--)
 {
  storage[i] = 0;
 }

 MakeLand_HeightsScaleMax (0xFFFFFF);
 MakeLand_HeightsSmooth (0, 1);

 //NOTE: next line starts processing erosion lines by pixel-altitude;
 //going from lowest to hightest pixel is essential for long lines
 //(doing the reverse results in very short erosion lines)
 for (x = 0; x < MakeLand_Width; x++)
 {      //start x
  //if (MakeLandUserCallback) if (MakeLandUserCallback(width)) break;
  for (y = 0; y < MakeLand_Height; y++)
  {     //start y
   rawindex = y * MakeLand_Width + x;
   cyclecount = 0;
   start = (x << 16) | y;
   do
   {    //start progress from one pixel
    progress = 0;
    sum_old = MakeLand_bmpHeights[rawindex] * raylength;
    for (xm = -1; xm < 2; xm++)
    {
     for (ym = -1; ym < 2; ym++)
     {
      if (!ym && !xm)
       ym = 1;
      sum_new = 0;
      i = raylength;
      //////////////////////////////
      if (ym && xm)
      { //doing an angle
       xi = ML_RAYOFFSET + ((start >> 16) << ML_SHIFT);
       yi = ML_RAYOFFSET + ((start & 0xffff) << ML_SHIFT);
       ystep = ym * ML_AFACTOR;
       xstep = xm * ML_AFACTOR;
       //////////////////////////////
       do
       {        //send the ray i
        if ((xi += xstep) < 0)
         xi += scaledwidth;
        else if (xi >= scaledwidth)
         xi -= scaledwidth;
        if ((yi += ystep) < 0)
         yi += scaledheight;
        else if (yi >= scaledheight)
         yi -= scaledheight;
        if (!sum_new)
         temp_result = (yi >> ML_SHIFT) | ((xi >> ML_SHIFT) << 16);
        sum_new +=
         MakeLand_bmpHeights[(yi >> ML_SHIFT) * MakeLand_Width +
                             (xi >> ML_SHIFT)];
       }
       while (--i > 0); //end i raylength
       if (sum_new < sum_old)
       {
        sum_old = sum_new;
        result = temp_result;
        progress = 1;
       }
       //////////////////////////////
      }
      else
      {
       xi = start >> 16;
       yi = start & 0xffff;
       do
       {        //send the ray i
        if ((xi += xm) < 0)
         xi += MakeLand_Width;
        else if (xi >= MakeLand_Width)
         xi -= MakeLand_Width;
        if ((yi += ym) < 0)
         yi += MakeLand_Height;
        else if (yi >= MakeLand_Height)
         yi -= MakeLand_Height;
        if (!sum_new)
         temp_result = yi | (xi << 16);
        sum_new += MakeLand_bmpHeights[yi * MakeLand_Width + xi];
       }
       while (--i > 0); //end i raylength
       if (sum_new < sum_old)
       {
        sum_old = sum_new;
        result = temp_result;
        progress = 1;
       }
       //////////////////////////////
      } //end non-angle
      //////////////////////////////
     }  //end ym
    }   //end xm 
    if (0 != progress)
    {
     if (++cyclecount > intensity)
      progress = 0;
     start = result;
     ++storage[(rawindex =
                (start & 0xffff) * MakeLand_Width + (start >> 16))];
    }
   }
   while (0 != progress);       //end progress
  }     //end y
 }      //end x

 //do crispness:
 int *Holder = MakeLand_bmpHeights;
 MakeLand_bmpHeights = storage;
 MakeLand_TerrainSmooth (crispness, 1);
 MakeLand_bmpHeights = Holder;

 //copy and scale-up
 for (i = totalsize - 1; i > -1; i--)
 {
  // MakeLand_bmpHeights[i]=(MakeLand_bmpHeights[i]+cut*storage[i])>>ML_SHIFT;
  // MakeLand_bmpColors[i]=RampPixel(MakeLand_bmpColors[i],(shade*storage[i])>>ML_SHIFT);
  MakeLand_bmpHeights[i] = (MakeLand_bmpHeights[i] + cut * storage[i]);
  MakeLand_bmpColors[i] =
   MakeLand_RampPixel_rgb (MakeLand_bmpColors[i], (shade * storage[i]));
 }

 MakeLand_HeightsScaleMax (original_height);
 free (storage);
}       //end Erosion()

///////////////////////////////////////////////////////////////
//
void MakeLand_Level (int height)
{
 int x,
  y;
 for (x = 0; x < MakeLand_Width; x++)
 {
  for (y = 0; y < MakeLand_Height; y++)
  {
   if (MakeLand_bmpHeights[y * MakeLand_Width + x] <= height)
    MakeLand_bmpHeights[y * MakeLand_Width + x] = height;
  }
 }
}

///////////////////////////////////////////////////////////////
//20101202: needed because GTK+ has a different byte order than the 
//Win32 i developed MakeLand and LandCaster on.
void MakeLand_ColorsRGBtoBGR ()
{
 int end = MakeLand_Height * MakeLand_Width;
 unsigned int r,
  g,
  b,
  p;
 int i;
 for (i = 0; i < end; i++)
 {
  r = ((MakeLand_bmpColors[i] >> 0) & 0xff);
  g = ((MakeLand_bmpColors[i] >> 8) & 0xff);
  b = ((MakeLand_bmpColors[i] >> 16) & 0xff);
  p = ((MakeLand_bmpColors[i] >> 24) & 0xff);
  MakeLand_bmpColors[i] =       //
   (b << 0) +   //
   (g << 8) +   //
   (r << 16) +  //
   (p << 24);
 }
}

/////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////
int MakeLand_getpixel_wrapped (int x, int y)
{
 if (x >= MakeLand_Width)
  do
   x -= MakeLand_Width;
  while (x >= MakeLand_Width);
 else if (x < 0)
  do
   x += MakeLand_Width;
  while (x < 0);
 if (y >= MakeLand_Height)
  do
   y -= MakeLand_Height;
  while (y >= MakeLand_Height);
 else if (y < 0)
  do
   y += MakeLand_Height;
  while (y < 0);
 return y * MakeLand_Width + x;
}

//////////////////////////////////////////////////////////
//general dimensions of a simple crater:
//(from http://www.ciw.edu/akir/Seminar/cratering.html )
//crater diameter: D
//crater depth: d = 0.2*D
//rim height: h = 0.04*D
//crater volume: V = (PI*d/6)*[0.75*D^2 + d^2]
//THIS FUNCTION LETS THE CENTER OF THE CRATER BE UNAFFECTED BY UNDERLYING 
//LANDSCAPE, INCREMENTALLY MORPHING TO LAND FROM THAT POINT OUTWARDS TO RIM
//UNTIL LANDSCAPE TAKES OVER COMPLETELY.
void MakeLand_TerrainCrater (int x, int y, int radius, int depth, int rimheight,        //Note: 0 is much faster than anything else
                             int OBSOLETE,      //##OBSOLETE 0 to 100; was percentage of underlying details erased
                             double rimdecay,   //pretty arbitrary; I guess 50 is good. Can't be 0 if there is a rimheight
                             double RZFactor,   //##OBSOLETE between .01 and 2; puts an end to rim decay tail-off 
                             int rimwidth)
{
 int p,
  r,
  rp,
  r2,
  avg = 0;
 if (rimheight < 0)
  rimheight = 0;
 if (radius < 1)
  radius = 1;

 //First add something to rimheight so I can subtract something at end to get it below zero
 //(otherwise it dangles above zero forever, thanks to 1/z++ decay):
 // RZFactor=3.0/rimdecay;
 RZFactor = 3.0 / rimdecay;
 double rh = rimheight * (1 + RZFactor);        //the 1 is needed for backward function compatibility

 //Get average perimeter height so i can cut crater at appropriate depth:
 if (radius < 2)
  avg = MakeLand_bmpHeights[MakeLand_getpixel_wrapped (x, y)];
 else
 {
  int xoff = 0,
   yoff = radius,
   balance = -radius,
   count = 0;
  avg = 0;
  do
  {
   //do sides of circle:
   avg += MakeLand_bmpHeights[MakeLand_getpixel_wrapped (x + xoff, y + yoff)];
   avg += MakeLand_bmpHeights[MakeLand_getpixel_wrapped (x - xoff, y + yoff)];
   avg += MakeLand_bmpHeights[MakeLand_getpixel_wrapped (x - xoff, y - yoff)];
   avg += MakeLand_bmpHeights[MakeLand_getpixel_wrapped (x + xoff, y - yoff)];
   //now top and bottom:
   avg += MakeLand_bmpHeights[MakeLand_getpixel_wrapped (x + yoff, y + xoff)];
   avg += MakeLand_bmpHeights[MakeLand_getpixel_wrapped (x - yoff, y + xoff)];
   avg += MakeLand_bmpHeights[MakeLand_getpixel_wrapped (x - yoff, y - xoff)];
   avg += MakeLand_bmpHeights[MakeLand_getpixel_wrapped (x + yoff, y - xoff)];
   count += 8;
   if ((balance += xoff++ << 1) >= 0)
    balance -= --yoff << 1;
  }
  while (xoff <= yoff);

  avg = avg / count;
 }      //end determine average perimeter height

 //Figure out how big to make the formation matrix to encompass rim decay:
 int xend = 0,
  yend,
  totalradius = radius + rimwidth;
 while ((rh * rh / (rh + (++xend) * rimdecay) - (rh - rimheight)) >= 1)
  ++totalradius;

 xend = x + totalradius + 1;
 yend = y + totalradius + 1;

 //now I can put together these important values:
 r2 = totalradius * totalradius + totalradius;
 r = radius * radius + radius;
 // rimwidth=((radius+rimwidth)*(radius+rimwidth)+(radius+rimwidth))-r;

 //figure out how deep to cut crater
 double dfactor = (depth + rimheight) / (double) (r);
 int i,
  j;

 //################
 //################
 //START CRATER MAKING LOOP:
 for (i = x - totalradius; i < xend; i++)
  for (j = y - totalradius; j < yend; j++)
  {
   p = MakeLand_getpixel_wrapped (i, j);        //Get index to point in array:
   //Get point's squared distance from center of matrix:
   rp = ((x - i) * (x - i)) + ((y - j) * (y - j));
   //###FIRST DO PARABOLIC CRATER-CUT:
   if (rp < r)
   {
    MakeLand_bmpHeights[p] =
     (int) (avg + rimheight - ((r - rp) * dfactor) +
            (rp * (MakeLand_bmpHeights[p] - avg) / (double) r) + 0.5);
   }    //###END PARABOLIC CRATER-CUT

   //###NOW DO RIM FORMATION (the hard part):
   else if (rp < r2)
   {
    //I'll need this a few times, so do it now:
    double true_rp = sqrt (rp);

    //first do rimwidth (for now simply a flat one):
    if ((true_rp -= rimwidth) < radius)
     true_rp = radius;

    //now attenuate rimheight according to inverse proportion of distance from rim:
    //following scales all craters identically, requiring the expensive sqrt; however,
    //it is the case that making small craters have longer decays looks more realistic.
    //logically, this should be implemented via TerrainCraters, not here.
    int rez =
     (int) (rh * rh / (rh + (true_rp - radius) * rimdecay) -
            (rh - rimheight));

    //if not done, sum result with original landscape:
    if (rez > 0)
     MakeLand_bmpHeights[p] += rez;

    //these were all very interesting variations on rim decay; don't erase, some may be 
    //handy one day (all at least worked, but fell aesthetically short)
    //     MakeLand_bmpHeights[p]+=(int)
    //best so far:
    //     (rimheight*rimheight/(rimheight+(Math.sqrt(rp)-radius)*30) );
    //Following simulates inverse proportion but with sin -- unfortunately not convincing-looking:
    //     (rimheight*(Math.sin((rp-r)*1.5707963267948966192313216916398/(r2-r)+3.1415926535897932384626433832795)+1)+.5);
    //following creates an inverse proportion decay curve, decreasing slope with increasing rim decay:
    //the problem with it: requires so much fiddling to get perfect. But it does look most real.
    //     (rimheight*(rimdecay/(rp-r+rimdecay))+.5);
    //following creates a linear, constant slope to rim decay:
    //     (rimheight*(1.0-(rp-r)/(double)(r2-r))+.5);
    //this half-wave method is easy on the eye, but doesn't really look like a crater:
    //     (rimheight*.5*(Math.cos((rp-r)*3.1415926535897932384626433832795/(r2-r))+1)+.5);
    //the above seems to do what this did, but above was faster (no sqrt):
    //(rimheight*.5*(Math.cos((Math.sqrt(rp)-radius)*3.1415926535897932384626433832795/rimwidth)+1)+.5);
   }    //###END RIM FORMATION

  }     //END CRATER MAKING LOOP
 //################

}       //end of Crater    

  //Following line gives v a normalized value proportional to distance away from rim
//   double v=((1+RZFactor)*rimdecay/(rp-r+rimdecay))-RZFactor;
   //now I use that normalize proportion to determine how much I attenuate rim height at given
   //distance from rim. I also use it to gradually blend/morph tail in to landscape for obliterated form
//   if (v>=0) MakeLand_bmpHeights[p]+=(int)(rimheight*v+0.5);

//################################
//TerrainCraters
//################################
void MakeLand_TerrainCraters (int NumCraters, int MaxRadius, int MinRadius, double DepthToRadiusRatio, double RimHeightToRadiusRatio, double RimWidthToRadiusRatio, double RimDecayFactor, double TailZeroFactor, int UnderlyingDetailRetention,        //percentage 0-100
                              int PowerLaw, int StopSize)
{       //stop making craters if one bigger than this appears
 if (MaxRadius < 1)
  MaxRadius = 1;
 if (MinRadius >= MaxRadius)
  MinRadius = MaxRadius;
 MaxRadius -= MinRadius;
 ++MaxRadius;

 while (--NumCraters > -1)
 {

  //interesting and very useable power-law:
  int i,
   r = rand () + 1;

  for (i = PowerLaw; i > 0; i--)
  {
   r = 1 + (rand () % r);
  }

  //Rounding does seem to give a bit more pow law in the 1 to 2 r 
  //craters, so I do it. But not sure it really matters in theory
  //here with rounding:
  r = (int) ((MaxRadius * r / (double) 0xfffff) + .5 + MinRadius);
  //here without rounding;
  //r=MaxRadius*r/0xfffff+MinRadius;

  //if (r<1) MakeLand_bmpHeights[(rand()%height)*width+(rand()%width)]-=(int)(2*DepthToRadiusRatio+.5);

  //RimDecayFactor=RimDecayFactor*r*.5;
  //if (RimDecayFactor<1) RimDecayFactor=1;

  int rwrr = (int) (r * RimWidthToRadiusRatio + .5);
  if (rwrr > 2)
   rwrr = 2;

  MakeLand_TerrainCrater (rand () % MakeLand_Width,
                          rand () % MakeLand_Height,
                          r,
                          (int) (r * DepthToRadiusRatio + .5),
                          (int) (r * RimHeightToRadiusRatio + .5),
                          UnderlyingDetailRetention,
                          RimDecayFactor, TailZeroFactor, rwrr);
  if (StopSize > -1 && r >= StopSize)
   break;       //got what user came for; abort
 }      //do numcraters
}
