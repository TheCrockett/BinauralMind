/*
   gnauralnet_flasher.c
   A hack to demonstrate how Gnaural can give information over net
   (the rgbbuf version has proven fastest)
   This is the rgbbuf version (interchangeable with the drawing_area version)
   Copyright (C) 2007  Bret Logan

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

#include <gtk/gtk.h>
#include "gnauralnet.h"
//program globals for gnauralnet_flasher.c:
int gdf_timeout_id = 0;

//flasher.c -- (GTK+2 program) a toggling image flipper, toggles between
//flasher_pixbuf1 and flasher_pixbuf2
//To use:
// gint w=1, h=1;
// main_FD = flasher_Init (w, h, 0xFF0000, 0x0000FF);
// (put it in your GtkHBox -- like this: gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (main_FD->drawing_area), TRUE, TRUE, 0);
// flasher_Cleanup (main_FD);

#include <stdlib.h>
#include <math.h>
#include "gnauralnet_flasher.h"
//////////////////////////////

//////////////////////////////
//this should ONLY be called with already cleaned flasher_Data
flasher_Data *flasher_Init (int WIDTH, int HEIGHT, int color1, int color2)
{
 flasher_Data *fd = (flasher_Data *) malloc (sizeof (flasher_Data));

 flasher_InitData (fd);
 fd->color1 = color1;
 fd->color2 = color2;
 // flasher_Cleanup (fd);
 fd->drawing_area = gtk_drawing_area_new ();
 gtk_drawing_area_size (GTK_DRAWING_AREA (fd->drawing_area), WIDTH, HEIGHT);
 //flasher_Render (fd); 

 // --- Signals used to handle backing rgbbuf 
 gtk_signal_connect (GTK_OBJECT (fd->drawing_area), "expose_event",
                     (GtkSignalFunc) flasher_expose_event, fd);
 gtk_signal_connect (GTK_OBJECT (fd->drawing_area), "configure_event",
                     (GtkSignalFunc) flasher_configure_event, fd);
 return fd;
}

//////////////////////////////
void flasher_Cleanup (flasher_Data * fd)
{
 //Free background if we created it
 if (NULL != fd->rgbbuf)
 {
  //fprintf (stderr, "deleting old rgbbuf\n");
  free (fd->rgbbuf);
 }

 if (NULL != fd)
 {
  free (fd);
  fd = NULL;
 }
}

//////////////////////////////
void flasher_Render (flasher_Data * fd)
{
 if (NULL == fd || NULL == fd->drawing_area->window)
 {
  GN_DBGOUT ("Shutting down, ignoring command to flash");
  return;
 }
 if (fd->size <= 0)
 {
  return;
 }

 fd->ToggleState = ~fd->ToggleState;
 flasher_Fill (fd);

 gdk_window_invalidate_rect (fd->drawing_area->window, NULL, FALSE);
 // gtk_widget_queue_draw_area (fd->drawing_area, 0, 0, fd->width,fd->height); 
 /*
    GdkRectangle update_rect;
    update_rect.x = 0;
    update_rect.y = 0;
    update_rect.width = fd->width;
    update_rect.height = fd->height;
    gtk_widget_draw (fd->drawing_area, &update_rect);
  */
}

//////////////////////////////
void flasher_InitData (flasher_Data * fd)
{
 fd->drawing_area = NULL;
 fd->ToggleState = 0;
 fd->width = 0;
 fd->height = 0;
 fd->size = 0;
 fd->rowstride = 0;
 fd->rgbbuf = NULL;
 fd->color1 = 0xffffffff;
 fd->color2 = 0;
}

//////////////////////////////
void flasher_Fill (flasher_Data * fd)
{
 // Set up the RGB buffer:
 guchar r,
  g,
  b;

 if (0 == fd->ToggleState)
 {
  r = (guchar) (fd->color1 >> 0) & 0xff;
  g = (guchar) (fd->color1 >> 8) & 0xff;
  b = (guchar) (fd->color1 >> 16) & 0xff;
 }
 else
 {
  r = (guchar) (fd->color2 >> 0) & 0xff;
  g = (guchar) (fd->color2 >> 8) & 0xff;
  b = (guchar) (fd->color2 >> 16) & 0xff;
 }

 guchar *pos = fd->rgbbuf;

 int i = fd->size;

 while (i > 1)
 {
  *pos++ = r;
  *pos++ = g;
  *pos++ = b;
  i -= 3;
 }
}

//////////////////////////////
gint flasher_expose_event (GtkWidget * widget, GdkEventExpose * event,
                           flasher_Data * fd)
{

 gdk_draw_rgb_image (widget->window, widget->style->fg_gc[GTK_STATE_NORMAL],
                     0, 0, fd->width, fd->height, GDK_RGB_DITHER_NONE,
                     fd->rgbbuf, fd->rowstride);
 return FALSE;
}

//////////////////////////////
gint flasher_configure_event (GtkWidget * widget, GdkEventConfigure * event,
                              flasher_Data * fd)
{
 //erase any old rgb array:
 if (NULL != fd->rgbbuf)
 {
  //fprintf (stderr, "deleting old rgbbuf\n");
  free (fd->rgbbuf);
 }

 fd->width = widget->allocation.width;
 fd->height = widget->allocation.height;
 fd->rowstride = 3 * fd->width;
 fd->size = fd->rowstride * fd->height;
 //create new array to hold colors:
 fd->rgbbuf = (guchar *) malloc (fd->size);

 //fill it (optional):
 //flasher_Fill (fd);
 return TRUE;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
//START include from gnauralnet_client_example.c
#include "gnauralnet.h"

///////////////////////////////////////
//where incoming data gets processed
void flasher_GN_ProcessIncomingData (char *MsgBuf, int sizeof_MsgBuf,
                                     struct sockaddr_in *saddr_remote)
{
 main_ToggleFlasher (NULL);
 GN_DBGOUT ("Got data from:");
 GN_PrintAddressInfo (0, saddr_remote->sin_addr.s_addr,
                      saddr_remote->sin_port);
}

/////////////////////////////////////////////////////////////
//This is what this program set to get threaded out from GN 
int flasher_GN_MainLoop (void *arg)
{
 while (GNAURALNET_RUNNING == GN_My.State)
 {
  //Receive and process packets for GN_MainLoopInterval_usecs time:
  GN_Socket_RecvMessage (GN_My.Socket, (char *) &GN_RecvBuffer,
                         sizeof (GN_RecvBuffer), GN_MainLoopInterval_secs,
                         GN_MainLoopInterval_usecs);
 }
 GN_My.State = GNAURALNET_STOPPED;
 return GNAURALNET_SUCCESS;
}

////////////////////////////////////////
//start Gnauralnet:
int flasher_GN_main (int argc, char **argv)
{
 //Make zero to avoid all the debugging junk:
 GN_DebugFlag = 1;

#ifdef GNAURALNET_GTHREADS
 GN_DBGOUT ("Starting threaded approach");
 g_thread_init (NULL);
 gdk_threads_init ();   // Called to initialize internal mutex "gdk_threads_mutex".
#endif

 if (GNAURALNET_SUCCESS !=
     GN_start (flasher_GN_MainLoop, flasher_GN_ProcessIncomingData))
 {
  return GNAURALNET_FAILURE;
 }

 return GNAURALNET_SUCCESS;
}

//END include from gnauralnet_client_example.c
//////////////////////////////////////////////////
//////////////////////////////////////////////////

////////////////////////////////////////////////
//Following was in main.c, added here to reduce filecount:
// flasherdemo.c -- GdkPixbuf demo */
#include <stdlib.h>
#include <math.h>
#include "gnauralnet_flasher.h"
#include <gtk/gtk.h>

//globals:
//////////////////////////////////
flasher_Data *main_FD;

//////////////////////////////////
// standard handlers
gint main_delete_event (GtkWidget * widget, GdkEvent event, gpointer data)
{
 return FALSE;
}

/////////////////////////////
void main_end_program (GtkWidget * widget, gpointer data)
{
 gtk_main_quit ();
}

//////////////////////////////////
//uncomment the line to make the timer run flasher
gboolean gdf_timeout_func (gpointer data)
{
 //main_ToggleFlasher (pointer);
 return TRUE;;
}

//////////////////////////////////
gboolean main_ToggleFlasher (gpointer data)
{
 flasher_Render (main_FD);
 // flasher_RenderImage(flasher_Pixbuf);
 // flasher_DisplayImage (main_FD);
 return TRUE;
}

/////////////////////////////////////////////////////////////
void main_InterceptCtrl_C (int sig)
{
 GN_DBGOUT ("Caught Ctrl-C");
 gtk_main_quit ();
}

/////////////////////////////////////////////////////////////
void main_cleanup ()
{
 g_source_remove (gdf_timeout_id);
 GN_stop ();    //since this is unthreaded, this waits unnecessarily
 flasher_Cleanup (main_FD);
}

//////////////////////////////////
int main (int argc, char **argv)
{
 GtkWidget *window;
 GtkHBox *hbox;

 // initialize GTK+, create a window, attach handlers
 gtk_init (&argc, &argv);

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

 gtk_window_get_size (GTK_WINDOW (window), &w, &h);
 //some color possibilities:
 // main_FD = flasher_Init (w, h, 0xFF0000, 0x0000FF);
 // main_FD = flasher_Init (w, h, 0x8f8f8f, 0x6f6f6f);
 // main_FD = flasher_Init (w, h, 0xFFFF00, 0x00FFFF);
 main_FD = flasher_Init (w, h, 0x111111, 0x00df00);
 gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (main_FD->drawing_area), TRUE,
                     TRUE, 0);
 gtk_widget_show_all (GTK_WIDGET (window));

 //add GN functionality here:
 flasher_GN_main (argc, argv);
 //GN_Socket_SetBlocking (GN_My.Socket,0);

 int delay = 1000 / 2;
 //not used anymore, but kept for when i want to test flasher manually:
 gdf_timeout_id = g_timeout_add (delay, gdf_timeout_func, NULL);
 gtk_main ();
 main_cleanup ();
 return 0;
}

//END included main.c 
/////////////////////////
