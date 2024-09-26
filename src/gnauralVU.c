/*
   gnauralVU.c

   Copyright (C) 20101014  Bret Logan

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "main.h"
#include "BinauralBeat.h"
#include "gnauralVU.h"

GtkWidget *gnauralVU_VolL = NULL;
GtkWidget *gnauralVU_VolR = NULL;

////////////////////////////////////////////
//added 20101014
void gnauralVU_init (GtkBuilder * xml)
{
 gnauralVU_VolL =
  GTK_WIDGET (gtk_builder_get_object (xml, "progressbar_VolL"));
 gnauralVU_VolR =
  GTK_WIDGET (gtk_builder_get_object (xml, "progressbar_VolR"));
}

/////////////////////////////////////////////////////
//added 20101014
void main_UpdateVU ()
{
 float left;
 float right;
 
 left = BB_PeakL * 0.000030518;
 if (left > 1.0) left = 1.0;
 right = BB_PeakR * 0.000030518;
 if (right > 1.0) right = 1.0;
 
 gtk_progress_bar_set_fraction ((GtkProgressBar *) gnauralVU_VolL, left);
 gtk_progress_bar_set_fraction ((GtkProgressBar *) gnauralVU_VolR, right);
 BB_PeakR*=.6;
 BB_PeakL*=.6;
}
