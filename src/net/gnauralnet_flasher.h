/*
   flasher.h
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

#ifndef _GNAURALNET_FLASHER_H_
#define _GNAURALNET_FLASHER_H_

#include <gtk/gtk.h>

//////////////////////////////////////
typedef struct
{
 GtkWidget *drawing_area;
 guchar *rgbbuf;
 int width;
 int height;
 int size;
 gint rowstride;
 int ToggleState;
 int color1;
 int color2;
} flasher_Data;

extern flasher_Data *flasher_Init (int WIDTH, int HEIGHT, int color1, int color2);      //call after flasher_InitData
extern void flasher_Cleanup (flasher_Data * fd);
extern void flasher_Fill (flasher_Data * fd);
extern void flasher_Render (flasher_Data * fd);
extern void flasher_InitData (flasher_Data * fd);       //user shouldn't call, just zeros the struct.
extern gint flasher_expose_event (GtkWidget * widget, GdkEventExpose * event,
                                  flasher_Data * fd);
extern gint flasher_configure_event (GtkWidget * widget,
                                     GdkEventConfigure * event,
                                     flasher_Data * fd);
extern gboolean main_ToggleFlasher (gpointer data);
extern void main_cleanup ();
extern gboolean gdf_timeout_func (gpointer pointer);
#endif
