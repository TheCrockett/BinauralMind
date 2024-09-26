//gnauralscape.c, main file of project vulcan by Bret Logan
//2010 (c) Bret Byron Logan
//NOTE:
//define "GS_STANDALONE" to run this standalone.
#define GS_STANDALONE

#include <stdlib.h>
#include <math.h>
#include "MakeLand.h"
#include "LandCaster.h"

#include <gtk/gtk.h>

#define VULCAN_FPS 25   //this is the animation speed
#define VULCAN_TARGETS 20

//Game messages/data:
#define VULCAN_HIT 1
#define VULCAN_GROUND 2
#define VULCAN_FIRE 4
#define VULCAN_WIN 8
#define VULCAN_NEWGAME 16
#define VULCAN_MAXSPEED 10
#define VULCAN_MISSILE_LIFE 80
#define VULCAN_FORWARD 0
#define VULCAN_BEHIND 1
#define VULCAN_RIGHT 2
#define VULCAN_LEFT 3
#define VULCAN_NORMAL 0
#define VULCAN_FOLLOW 1
#define VULCAN_CIRCLE 2
#define VULCAN_CHASE 3
#define VULCAN_BLIND 4
#define VC_SPEED 14

//globals:
//////////////////////////////////
typedef struct
{
 GtkWidget *drawing_area;
 int *BmpDest;                  //GTK+ prefers rgb char triplets buffers, but LandCaster was set up for int-sized rgbp data
 int width;
 int height;
 int size;
 gint rowstride;                //drawing_area need this; it is 3*width for an RGB bitmap, 4*width for an int-sized RGBP
 double FPS;                    //frames-per-second animation speed);
 guint GUIUpdateHandle;         //to keep track of it's own timer
 int Turn;
 int Lift;
 int GameObjects;
 double GameTime;               // holds duration from whenever main_Stopwatch() was last called
 int FireCount;
 int pixelfly;                  //if non-0, renders in flat mode
} vulcan_Data;

vulcan_Data *main_VD = NULL;

gchar main_msgstring[1024];     //use this for any string stuff
void main_GameCallback (int message);
void main_NewGame (vulcan_Data * vd);
void main_Cleanup ();           //assumes global main_VD

//////////////////////////////////
// standard handlers
gint main_delete_event (GtkWidget * widget, GdkEvent event, gpointer data)
{
 printf ("main_delete_event\n");
 //kill GUI updating if here:
 if (NULL != main_VD && 0 != main_VD->GUIUpdateHandle)
 {
  g_source_remove (main_VD->GUIUpdateHandle);
  main_VD->GUIUpdateHandle = 0;
 }

 return FALSE;
}

/////////////////////////////
void main_end_program (GtkWidget * widget, gpointer data)
{
 printf ("main_end_program\n");
 main_Cleanup ();
#ifdef GS_STANDALONE
 gtk_main_quit ();
#endif
}

/*
   //////////////////////////////
   //this is an example only to demonstrate how to fill the buffer
   void main_Fill (vulcan_Data * vd)
   {
   // Set up the RGB buffer:
   guchar r,
   g,
   b;

   static unsigned int color1 = 0;

   color1 = 1 + color1 * 1.1;

   r = (guchar) ((color1 >> 0) & 0xff);
   g = (guchar) ((color1 >> 8) & 0xff);
   b = (guchar) ((color1 >> 16) & 0xff);

   guchar *pos = vd->rgbbuf;
   int i = vd->size;
   while (i > 1)
   {
   *pos++ = r;
   *pos++ = g;
   *pos++ = b;
   i -= 3;
   }
   }
 */

//////////////////////////////
//this is a timer; set vd->running to FALSE to exit
gboolean main_Render (gpointer data)
{
#define DUR 300
 static int toggle = 0;
 if (NULL == data)
  return TRUE;

 vulcan_Data *vd = (vulcan_Data *) data;
 if (vd->size <= 0)
 {
  return TRUE;
 }

 if (NULL != vd->BmpDest)
 {
  if (++toggle > DUR)
  {
   toggle = 0;
  }

  vd->pixelfly = 0;
  if (0 == vd->pixelfly)
  {
   if (!(7 & toggle))   //to just do followtheleader occasionally
   {
    //printf ("Toggles %d, %x\n", toggle, toggle);
    LC_TargetFollowTheLeader ();
   }
   LC_LandscapeRollSkyFlyingObjects ();
   if (0 != LC_AP.running)
   {
    LC_AutoPilot ((toggle > (DUR >> 1)) ? -1 : 1, 30);
    vd->Turn = LC_AP.Turn;
    vd->Lift = LC_AP.Lift;
    //printf ("LC_Lift: %d, LC_Turn: %d\n", LC_Lift, LC_Turn);
   }
  }
  else
  {
   //LC_ShowColorsBitmap ();
   LC_PixelFly ();
   if (10 > LC_Altitude)
    LC_Altitude = 10;
  }

  LC_Turn = vd->Turn >> 2;
  LC_Tilt = (vd->height >> 1) + vd->Lift;
  LC_Lift = (vd->Lift >> 4);

  if (0 != vd->pixelfly)
  {
   LC_Turn = -LC_Turn;
  }

  LC_SetRoll ((LC_CircleRez >> 1) + (LC_Turn << 2));
  LC_DoMovement ((0 == vd->pixelfly) ? 1 : 0);
  gdk_window_invalidate_rect (vd->drawing_area->window, NULL, FALSE);
 }

 return TRUE;
}

//////////////////////////////
void main_CleanupArrays (vulcan_Data * vd)
{
 if (NULL == vd)
  return;

 //Free background if we created it
 if (NULL != vd->BmpDest)
 {
  //fprintf (stderr, "deleting old rgbbuf\n");
  free (vd->BmpDest);
  vd->BmpDest = NULL;
 }
}

//////////////////////////////
//this cleans up global main_VD and MakeLand and LandCaster resources
void main_Cleanup ()
{
 LC_LandCaster_Cleanup ();
 MakeLand_Cleanup ();
 if (NULL != main_VD)
 {
  main_CleanupArrays (main_VD);
  free (main_VD);
  main_VD = NULL;       // have to set the real one 
 }
}

//////////////////////////////
//called only from configure_event:
void main_InitRenderer (vulcan_Data * vd)
{
 //init MakeLand:
 /////////////////////////////
 //creates the color and heights bitmap:
 MakeLand_SeedRand (time (NULL), 0);
 MakeLand_Init_InternalBitmaps (8);     //size is given as power of 2, since bitmap must be
 MakeLand_Clear (MakeLand_bmpHeights, 0);
 MakeLand_HeightsCreate ();
 //MakeLand_TerrainCraters (8, MakeLand_Height>>5, 1, 0, 0, 0, 0, 0, 0, 7, -1);    //lava
 //MakeLand_TerrainCraters (8, 512, 1, 8, 4, .08, 20, .3, 0, 10, -1);  //craters
 MakeLand_TranslateHeightsToColors (256);
 //MakeLand_ErosionFine (-400, -10, 4, 4, 3);
 MakeLand_HeightsSmooth (2, 1);
 MakeLand_HeightsBottomReference (0);
 MakeLand_HeightsScaleMax (256 + 64);
 MakeLand_HeightsSmooth (1, 1);
 MakeLand_HeightsBottomReference (0);
 MakeLand_ColorsBlur (3, 1, 1);
 MakeLand_ColorsShade (20, 'u', 5, 3, 62);
 MakeLand_Level (64);
 //MakeLand_TerrainCraters (512, MakeLand_Height, 1, 0, 0, 0, 0, 0, 0, 7, -1);    //lava
 //MakeLand_TerrainCraters (4096, 512, 1, 8, 4, .08, 20, .3, 0, 10, -1);  //craters
 MakeLand_HeightsBottomReference (0);
 //MakeLand_HeightsScaleMax (1);
 MakeLand_ColorsRGBtoBGR ();

 //init LandCaster:

 LC_LandCaster_Initialize (vd->BmpDest,
                           vd->width,
                           vd->height,
                           MakeLand_bmpHeights,
                           MakeLand_Width,
                           MakeLand_Height,
                           MakeLand_bmpColors,
                           MakeLand_Width, MakeLand_Height, 5, 8, 12);
 // ColorsShade(pColors, pHeights, width, height, 10, 10, 1, 64, -32); 
 // ColorsShadow(pColors, pHeights, width, height,(M_PI*1)/4,10,-64,2); 
 //PrepareLC_BmpHeights(0xFF); // not necessary to call, just convenient 
 LC_SetCallbackFast (main_GameCallback);
 LC_SetRoll (LC_CircleRez >> 1);
 LC_Lift = -1;
 LC_Altitude = 5;
 LC_Tilt = vd->height >> 1;
 //LC_SetNumberOfFlyingObjects (12);
 //LC_SetLC_Raylength(2000); 
 LC_Turn = 0;
 LC_Speed = 10;
 main_NewGame (vd);
}

//////////////////////////////////////////////////////////////////////

////////////////////////////////////
//quick way to put up a message:
void main_messagebox (gchar * message)
{
 GtkWidget *dialog;
 dialog = gtk_message_dialog_new (NULL,
                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_INFO,
                                  GTK_BUTTONS_CLOSE, "%s", message);
 gtk_dialog_run (GTK_DIALOG (dialog));
 gtk_widget_destroy (dialog);
}

/////////////////////////////////////////////////////////////////////
void main_HideFlyingObject (int id)
{       //needed to make this because erasing Object is hard outside of this class
 LC_FlyingObject[id].Visible = 0;
 int pos = LC_FlyingObject[id].SizeY;
 if (pos > 0)
 {
  do
   LC_BmpFlyingObjects[LC_FlyingObject[id].pErase[--pos]] = 0;  //erase old positions
  while (pos);
 }
 return;
}

/////////////////////////////////////////////////////////////////////
//only counts visible targets
int main_GetLiveTargetCount ()
{
 int i,
  j = 0;
 for (i = LC_FlyingObjectCount - 1; i > 0; i--)
 {
  if (LC_FlyingObject[i].Type == LC_TARGET && LC_FlyingObject[i].Visible)
   ++j;
 }
 // printf ("Flying Target Objects: %d\n", j);
 return j;
}

/*
   //////////////////////////////////////////////
   //THIS IS THE BEST ONE, but clock_gettime can't run on Windows
   //20101109: Returns elapsed time since previous call. First call it 
   //returns 0.
   //NOTE: used to use gettimeofday (&newtime, NULL), but this is better.
   double main_Stopwatch_hirez ()
   {
   #define BILLION 1000000000L;
   struct timespec newtime;
   static struct timespec oldtime;
   double result;

   if (clock_gettime (CLOCK_REALTIME, &newtime) == -1)
   {
   return 0;
   }

   result = (newtime.tv_sec - oldtime.tv_sec)
   + (double) (newtime.tv_nsec - oldtime.tv_nsec) / (double) BILLION;

   //deal with first-time call
   if (0 == oldtime.tv_nsec && 0 == oldtime.tv_sec)
   {
   result = 0;
   }

   oldtime.tv_sec = newtime.tv_sec;
   oldtime.tv_nsec = newtime.tv_nsec;

   return result;
   }
 */

//////////////////////////////////////////////
//20101129: Returns elapsed time since previous call. First call it 
//returns 0.
//This version uses crappy time () so it will run on windows (and clock () is WAY too broken)
double main_Stopwatch ()
{
 static clock_t oldtime = 0;
 clock_t newtime;
 double result;
 newtime = time (&newtime);

 result = (double) (newtime - oldtime);
 //result = difftime(newtime, oldtime)/CLOCKS_PER_SEC;

 //deal with first-time call
 if (0 == oldtime)
 {
  result = 0;
 }

 oldtime = newtime;
 return result;
}

/////////////////////////
void main_NewGame (vulcan_Data * vd)
{
 main_HideFlyingObject (0);
 LC_SetNumberOfFlyingObjects (vd->GameObjects);
 vd->GameTime = main_Stopwatch ();
 vd->FireCount = 0;
 LC_PositionX = LC_FlyingObject[1].PosX =
  MakeLand_Rand () % (LC_BmpColors.w << 10);
 LC_PositionY = LC_FlyingObject[1].PosY =
  MakeLand_Rand () % (LC_BmpColors.h << 10);
}

/////////////////////////////////////////////////////////////////////
void main_GameCallback (int message)
{
 /////////////////////////////////////////////////////////////////////
 // GameMessages - LANDSCAPE'S way of sending fast messages 
 // from within game or rendering loops
 /////////////////////////////////////////////////////////////////////
 switch (message)
 {
 case LC_OBJECTHIT:
  if (LC_FlyingObject[LC_UserData1].Type == LC_TARGET)  //was a target
  {
   main_HideFlyingObject (LC_UserData1);
   LC_FlyingObject[LC_UserData1].Speed = 0;     //freeze the target
   // UserCallback(VULCAN_HIT);

   //see if user won:
   if (!main_GetLiveTargetCount ())
   {
    sprintf (main_msgstring,
             "You destroyed %d targets with %d shots in %g seconds",
             main_VD->GameObjects - 1, main_VD->FireCount, main_Stopwatch ());
    main_messagebox (main_msgstring);
    main_NewGame (main_VD);
   }
   return;
  }

 case LC_GROUND:
  // UserCallback(VULCAN_GROUND);
  return;

 default:
  return;
 }
}

//////////////////////////////
gint main_drawing_area_expose_event (GtkWidget * widget,
                                     GdkEventExpose * event, vulcan_Data * vd)
{
 gdk_draw_rgb_32_image (widget->window,
                        widget->style->fg_gc[GTK_STATE_NORMAL],
                        0, 0, vd->width, vd->height,
                        // GDK_RGB_DITHER_MAX,
                        GDK_RGB_DITHER_NONE,
                        (guchar *) (vd->BmpDest), vd->width << 2);

 /*
    //20101202: disabled rgbbuf because changing LandCaster and MakeLand 
    //from ints proved nightmarish. Kept this as an example of how to do it:
    //translate vd->BmpDest to vd->rgbbuf:
    int x;
    int y;
    int y_index = 0;
    int indexrgb = 0;              //(vd->rowstride)
    int color;

    y = vd->width * vd->height;
    for (x = 0; x < y; x++)
    {
    color = vd->BmpDest[x];
    vd->rgbbuf[indexrgb++] = ((color >> 16) & 0xff);
    vd->rgbbuf[indexrgb++] = ((color >> 8) & 0xff);
    vd->rgbbuf[indexrgb++] = ((color >> 0) & 0xff);
    }
    gdk_draw_rgb_image (widget->window,
    widget->style->fg_gc[GTK_STATE_NORMAL],
    0,
    0,
    vd->width,
    vd->height,
    GDK_RGB_DITHER_NONE, vd->rgbbuf, vd->rowstride);
  */
 return FALSE;
}

//////////////////////////////
gint main_drawing_area_button_press_event (GtkWidget * widget,
                                           GdkEventButton * event,
                                           vulcan_Data * vd)
{
 //Fire missile:
 vd->FireCount++;
 LC_FlyingObject[0].Visible = VULCAN_MISSILE_LIFE;      //gets it outta sight
 LC_FlyingObject[0].PosX = LC_PositionX;        //position == mine
 LC_FlyingObject[0].PosY = LC_PositionY;
 LC_FlyingObject[0].Direction = LC_Direction;   //direction==mine
 LC_FlyingObject[0].Alt = LC_Altitude;
 LC_FlyingObject[0].Lift = LC_Lift << 1;        //takes my sink or rise once 
 return TRUE;
}

//////////////////////////////
gint main_drawing_area_button_release_event (GtkWidget * widget,
                                             GdkEventButton * event,
                                             vulcan_Data * vd)
{
 //printf ("clock(): %d time: %g\n", (int) clock (), main_Stopwatch ());
 return TRUE;
}

//////////////////////////////
gint main_drawing_area_key_press_event (GtkWidget * widget,
                                        GdkEventKey * event, vulcan_Data * vd)
{
 return TRUE;
}

//////////////////////////////
gint main_drawing_area_key_release_event (GtkWidget * widget,
                                          GdkEventKey * event,
                                          vulcan_Data * vd)
{
 return TRUE;
}

//////////////////////////////
gint main_drawing_area_motion_notify_event (GtkWidget * widget,
                                            GdkEventMotion * event,
                                            vulcan_Data * vd)
{
 //get the coords of the mouse:
 int x;
 int y;
 GdkModifierType state;

 if (event->is_hint)
  gdk_window_get_pointer (event->window, &x, &y, &state);
 else
 {
  x = (int) (event->x + .5);
  y = (int) (event->y + .5);
  state = (GdkModifierType) event->state;
 }

 vd->Turn = (((vd->width) >> 1) - x);
 vd->Lift = (((vd->height) >> 1) - y);

 return TRUE;
}

//////////////////////////////
gint main_drawing_area_enter_notify_event (GtkWidget * widget,
                                           GdkEventCrossing * event,
                                           vulcan_Data * vd)
{
 LC_AP.running = 0;
 return TRUE;
}

//////////////////////////////
gint main_drawing_area_leave_notify_event (GtkWidget * widget,
                                           GdkEventCrossing * event,
                                           vulcan_Data * vd)
{
 LC_AP.running = 1;
 return TRUE;
}

//////////////////////////////
//THIS is where MakeLand and LandCaster get setup (cleaned if already existant)
gint main_drawing_area_configure_event (GtkWidget * widget,
                                        GdkEventConfigure * event,
                                        vulcan_Data * vd)
{
 //kill GUI updating if here:
 if (0 != vd->GUIUpdateHandle)
 {
  g_source_remove (vd->GUIUpdateHandle);
  vd->GUIUpdateHandle = 0;
 }

 //fprintf (stderr, "Got here %d\n", __LINE__);

 //make sure timer isn't doing anything illegal:
 //erase any old rgb array:
 main_CleanupArrays (vd);

 vd->width = widget->allocation.width;
 vd->height = widget->allocation.height;
 //fprintf (stderr, "Line %d: %d %d\n", __LINE__, vd->width, vd->height);

 vd->rowstride = 3 * vd->width;
 vd->size = vd->rowstride * vd->height;
 // vd->rgbbuf = (guchar *) malloc (vd->size * sizeof (guchar));
 vd->BmpDest = (int *) malloc (vd->width * vd->height * sizeof (int));  //the main drawing surface

 //Setup Lanscape and Makeland:
 main_InitRenderer (vd);
 //setup the main GUI timer:
 vd->GUIUpdateHandle =
  g_timeout_add (1000 / VULCAN_FPS, main_Render, (gpointer) main_VD);

 return TRUE;
}

//////////////////////////////
//this should ONLY be called with already cleaned vulcan_Data
vulcan_Data *main_Init (int WIDTH, int HEIGHT, int FPS)
{
 vulcan_Data *vd = (vulcan_Data *) malloc (sizeof (vulcan_Data));

 vd->GameObjects = VULCAN_TARGETS + 1;  //remember, object[0] is the missile
 vd->GameTime = 0;
 vd->FireCount = 0;
 vd->pixelfly = 0;
 vd->GUIUpdateHandle = 0;       //handle to the timer that updates the GUI
 vd->width = WIDTH;
 vd->height = HEIGHT;
 vd->FPS = FPS;
 vd->size = 0;  //gets set in configure_event
 vd->rowstride = 0;     //gets set in configure_event
 vd->BmpDest = NULL;    //gets set in configure_event
 vd->drawing_area = gtk_drawing_area_new ();
 gtk_widget_set_size_request (vd->drawing_area, WIDTH, HEIGHT);

 gtk_widget_add_events (GTK_WIDGET (vd->drawing_area),
                        GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK |
                        GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK |
                        GDK_KEY_RELEASE_MASK | GDK_ENTER_NOTIFY_MASK |
                        GDK_LEAVE_NOTIFY_MASK);

 //now setup all the signals:
 gtk_signal_connect (GTK_OBJECT (vd->drawing_area), "expose_event",
                     (GtkSignalFunc) main_drawing_area_expose_event, vd);

 gtk_signal_connect (GTK_OBJECT (vd->drawing_area), "configure_event",
                     (GtkSignalFunc) main_drawing_area_configure_event, vd);

 g_signal_connect (vd->drawing_area, "button_press_event",
                   G_CALLBACK (main_drawing_area_button_press_event), vd);

 g_signal_connect (vd->drawing_area, "button_release_event",
                   G_CALLBACK (main_drawing_area_button_release_event), vd);

 g_signal_connect (vd->drawing_area, "key_press_event",
                   G_CALLBACK (main_drawing_area_key_press_event), vd);

 g_signal_connect (vd->drawing_area, "key_release_event",
                   G_CALLBACK (main_drawing_area_key_release_event), vd);

 g_signal_connect (vd->drawing_area, "motion_notify_event",
                   G_CALLBACK (main_drawing_area_motion_notify_event), vd);

 g_signal_connect (vd->drawing_area, "enter_notify_event",
                   G_CALLBACK (main_drawing_area_enter_notify_event), vd);

 g_signal_connect (vd->drawing_area, "leave_notify_event",
                   G_CALLBACK (main_drawing_area_leave_notify_event), vd);

 return vd;
}

//////////////////////////////////
#ifdef GS_STANDALONE
int main (int argc, char **argv)
#else
int gnauralscape_main ()
#endif
{
 GtkWidget *window;
 GtkHBox *hbox;

 // initialize GTK+, create a window, attach handlers
#ifdef GS_STANDALONE
 gtk_init (&argc, &argv);
#endif

 window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 //gtk_window_fullscreen (GTK_WINDOW(window));

 // create a new HBox, pack the image and vboxes above
 hbox = g_object_new (GTK_TYPE_HBOX, NULL);
 // pack everything into the window, show everything, start GTK+ main loop
 gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (hbox));

 // attach standard event handlers
 g_signal_connect (window, "delete_event", G_CALLBACK (main_delete_event),
                   NULL);
 g_signal_connect (window, "destroy", G_CALLBACK (main_end_program), NULL);
 gint w = 1,
  h = 1;
 gtk_window_resize (GTK_WINDOW (window), 512, 512);
 gtk_window_get_size (GTK_WINDOW (window), &w, &h);
 gtk_window_set_policy (GTK_WINDOW (window), TRUE, FALSE, FALSE);
 main_VD = main_Init (w, h, VULCAN_FPS);

 //INIT MAKELAND AND LANSCAPE HERE:

 gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (main_VD->drawing_area), TRUE,
                     TRUE, 0);
 gtk_widget_show_all (GTK_WIDGET (window));

#ifdef GS_STANDALONE
 gtk_main ();
#endif
 return 0;
}
