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
////////////////////////////////////////////////////////////
// ScheduleGUI.c, by Bret Logan (c) 2008-2011
//A user-manipulable plot for creating Gnaural Schedule files
//NOTES: 
// - SG_BackupDataPoints() is used for one-step Undo/Redo. It should be called by user usually; it
//   only gets called internally in cases that user can't access (mouse actions, mostly)
//To compile ScheduleGUI for standalone:
//g++ ScheduleGUI.cpp -o ScheduleGUI -D SCHEDULEGUI_FREESTANDING `pkg-config --cflags --libs gtk+-2.0`
//To use with another project, include ScheduleGUI.h and rename main();
////////////////////////////////////////////////////////////

//To compile:
//gcc -Wall -g ScheduleGUI.c -o ScheduleGUI.o -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -DSCHEDULEGUI_FREESTANDING `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0`

#include <stdlib.h>
#include <math.h>
#include <string.h>     //needed for memcpy()
#include <unistd.h>     //needed for sleep()
#include <gtk/gtk.h>

#include "ScheduleGUI.h"
//#include "ScheduleXML.h"
//#include "BinauralBeat.h"

#define ERROUT(a)   fprintf(stderr,"ScheduleGUI: #Error# %s\n",a);

//TODO:
// - have time portion of graph show hours when needed.
// - add to Parameter windows: type of voice, currently file, total time
// - offer means to change/specify type of voice, and implement ability to use files (CD, WAV, or MP3/Ogg) as sources for voices
// - offer means to change/specify type of voice, and implement ability to use files (CD, WAV, or MP3/Ogg) as sources for voices
// - solve problem of voice description parameter file vs. Copy/Paste incompatability. (Likewise, the sound file filename storage problem.)
// - make way to drag along a fixed x or y axis
// - Be able to hide or solo (view) single voices, with goal of editing one-at-a-time (but hearing all, if user wants).
// - be able to paste a clump exactly at the end of a current schedule, perhaps repeatedly in one action
// - open gnauralfiles at the commandline
// - Study SG_DeleteVoice() and SG_PasteDataPoints() to see if deleting voices should be legal (I think Voice IDs and copy/paste may make it a dangerous ability)
// - integrate balance and volume controls better in to DP Properties box (sliders?)
// - add LOOPS to schedulefile's overall parameters (how'd I forget it?!)
// - Be sure I have means to indicated whether noise voices are stereo or not, etc.
// - create a fast way to auto update whenever adjusting graphs (if number of DPs and Voices are the same, it should be easy to do)
// - implement SG_ProgressIndicatorFlag so user can see where they are.
// - make selecting last point select first point, since they are the same
// - organize keyboard shortcuts more logically, and document them carefully (since Graphsound may only use them instead of a GUI)
//Long Term:
// - make it so I don't have to send InsertDatapoint() a summed number (it is just getting diff anyway)
// - multiple undos
// - allow user to set font/colors for graph and a menu heading to reset graph to the current (real) data BB is using
// - hovering mouse over a datapoint should give a tooltip with the point's info
// - allow for space around graph so user can grab points easier
// - when user deletes rightmost point on graph it gets replaced by second to rightmost (actual last SG_DataPoint) values -- that it doesn't now is non-obvious for user.
// - make it so that when user right-clicks (or anyclick, I guess) a datapoint it changes color or in some way let's user know it is selected
// - create a zoom function, also scrolling ability (via side scrollbars)
// - provide a function that deletes redundant points (points in which beatfreq doesn't change over three points)
// - allow user to choose colors, get default background from configuration

//BUGS:
// - coloring current line of schedule is no longer functional (around line 473) because of multiple voices.
// - with IMMENSE schedules, it becomes clear that doing anything before anything else is done (like pasting then saving/moving/quiting before done) results in potential seg-faults or unpredicted behavior. Saving produces shorter files, for example. Should come up with a "Please Wait" window
// - updating voices duration to match total duration after a group-set requires a resize or something.
//- still might be places where totalscheduleduration could get set to 0 (which should cause divide-by-0 errors)
//- noticed that callback.cpp in main Gnaural tree isn't handling NULL returns from gtk_file_chooser_get_filename in Open and Save callbacks

//#define SG_PRECISIONTYPE double

//#define SG_DataPointSize 6
#define SG_GRAPHBOUNDARY 16
#define SG_GRIDY_MINSTEP  12
#define SG_GRIDY_MAXSTEP 32
#define SG_GRIDY_UNIT        1
#define SG_GRIDX_MINSTEP  32
#define SG_GRIDX_MAXSTEP 64
#define SG_GRIDX_UNIT        60
//#define SG_GraphFontSize  6
//#define SG_XTEXTOFFSET (SG_GraphFontSize+4)
//#define SG_XTEXTOFFSET 8

//global vars:
int SG_DebugFlag = FALSE; //set to true to spit-out debug info
SG_Voice *SG_FirstVoice = NULL;
SG_DataPoint *SG_CurrentDataPoint = NULL;
SG_BackupData SG_CopyPaste,
SG_UndoRedo;
SG_SelectionBox_type SG_SelectionBox;
double SG_TotalScheduleDuration = 1.0;
double SG_MaxScheduleBeatfrequency = 1.0;
double SG_MaxScheduleBasefrequency = 1.0;
int SG_GraphType = SG_GRAPHTYPE_BASEFREQ; //Informs program what type of graph to draw/gather-data-through; can be SG_GRAPHTYPE_BASEFREQ, SG_GRAPHTYPE_BEATFREQ, SG_GRAPHTYPE_VOLUME, SG_GRAPHTYPE_VOLUME_BALANCE.
gboolean SG_GraphHasChanged = FALSE; //lets any external code know when Graph appearance has changed (DP selection, hide/show voice); user must reset it manually
gboolean SG_DataHasChanged = FALSE; //lets any external code know when it should reload data from SG; user must reset it manually
gboolean SG_ProgressIndicatorFlag = TRUE; //to disallow progress indication if graph has been modified (out of sync with BB)
GdkPixmap *SG_pixmap = NULL; //main drawing area, gets set in SG_Init()
GdkGC *SG_gc = NULL;
PangoLayout *SG_PangoLayout = NULL; //controls the fonts associated with the main drawing area
int SG_GraphFontSize = 10;
int SG_DataPointSize = 6; //keep squaresize even (I guess):
gboolean SG_MagneticPointerflag = FALSE; //added 20101006 to get rid of Caps Lock hacks

/////////////////////////////////////////////////////
//this is slow as hell -- for repetitious things, inlined this code

void draw_text(GtkWidget * widget, gint x, gint y, gchar * s_utf8) {
    PangoLayout *layout;

    layout = pango_layout_new(gdk_pango_context_get());
    pango_layout_set_text(layout, s_utf8, -1);

    PangoFontDescription *fontdesc =
            pango_font_description_copy(widget->style->font_desc);
    //I can set the font here, but seems safer to work with what I know user has:
    //pango_font_description_set_family (fontdesc, "Sans Bold Italic");
    pango_font_description_set_size(fontdesc, PANGO_SCALE * 6);
    //pango_layout_set_alignment(layout,PANGO_ALIGN_RIGHT);
    pango_layout_set_font_description(layout, fontdesc);
    pango_font_description_free(fontdesc);

    gdk_draw_layout(SG_pixmap, widget->style->black_gc, x, y, layout);

    g_object_unref(layout);
}

/////////////////////////////////////////////////////
//Not used usually because it doesn't display info as readably

void SG_DrawGridFast(GtkWidget * widget, double y_var) {
    //NOTE: keeping this as an alternative: (much faster, but not as easy to read):
    //Fast but not as friendly-to-read graph:
    //First draw the horizontal marker lines:
#define SG_YSTEP 24
#define SG_XSTEP  64
    char graphtext[32]; //this is used for all text rendering
    int xtextoffset = widget->allocation.height - SG_GraphFontSize;
    int index = widget->allocation.height;
    double textstep = (SG_YSTEP * y_var / widget->allocation.height);
    double textindex = 0;

    while ((index -= SG_YSTEP) > -1) {
        gdk_draw_line(SG_pixmap, SG_gc, 0, index, widget->allocation.width, index);
        textindex += textstep;
        sprintf(graphtext, "%6.5gHz", textindex);
        pango_layout_set_text(SG_PangoLayout, graphtext, -1);
        gdk_draw_layout(SG_pixmap, widget->style->black_gc, 0, index,
                SG_PangoLayout);
    }

    //Next draw vertical lines:
    index = 0;
    textstep = (SG_XSTEP * SG_TotalScheduleDuration / widget->allocation.width);
    textindex = 0;
    while ((index += SG_XSTEP) < widget->allocation.width) {
        gdk_draw_line(SG_pixmap, SG_gc, index, 0, index,
                widget->allocation.height);
        textindex += textstep;
        sprintf(graphtext, "%dm %ds", ((int) (textindex + .5)) / 60,
                ((int) (textindex + .5)) % 60);
        pango_layout_set_text(SG_PangoLayout, graphtext, -1);
        gdk_draw_layout(SG_pixmap, widget->style->black_gc, index, xtextoffset,
                SG_PangoLayout);
    }
}

/////////////////////////////////////////////////////
//This used to be inlined in SG_DrawGraph(); here for clarity
//This function could be speeded up a lot.
//NOTE: X is always assumed to reflect duration

void SG_DrawGrid(GtkWidget * widget, double vscale, char *vtext) {
    static char graphtext[32]; //this is used for all text rendering -- does it really need to be static?
    int xtextoffset = widget->allocation.height - SG_GraphFontSize;
    double index;
    double textindex;
    double textstep;
    double step;

    //First draw "friendly-to-read" (and extremely computationally intensive) horizontal grid lines (Y axis):
    index = widget->allocation.height;
    textindex = 0;
    //deal with potential divide by zero issue:
    if (vscale == 0)
        vscale = 1.0;
    step = index / vscale;
    //deal with negative vscale:
    if (vscale < 0) {
        textindex = vscale;
        step /= -2;
    }
    textstep = SG_GRIDY_UNIT; //this is the basis of one "unit" of vertical climb
    //Bruteforce way to get grid-steps that are within a user-friendly range:
    //these next two uglies could be speeded up in a number of ways, no?
    if (step < SG_GRIDY_MINSTEP) {
        do {
            step *= 2;
            textstep *= 2;
        } while (step < SG_GRIDY_MINSTEP);
    } else if (step > SG_GRIDY_MAXSTEP) {
        do {
            step *= .5;
            textstep *= .5;
        } while (step > SG_GRIDY_MAXSTEP);
    }
    int ax;

    //draw y-axis text and horiz grid lines: 
    while ((ax = (int) ((index -= step) + .5)) > -1) {
        gdk_draw_line(SG_pixmap, SG_gc, 0, ax, widget->allocation.width, ax);
        textindex += textstep;
        sprintf(graphtext, "%g%s", textindex, vtext);
        pango_layout_set_text(SG_PangoLayout, graphtext, -1);
        gdk_draw_layout(SG_pixmap, widget->style->black_gc, 0, ax, SG_PangoLayout);
    }

    //Now draw "friendly-to-read" (and extremely computationally intensive) vertical grid lines along x-axis:
    index = 0;
    textindex = 0;
    textstep = SG_GRIDX_UNIT; //this is the basis of one "unit" of horizontal movement
    step = (SG_GRIDX_UNIT * widget->allocation.width) / SG_TotalScheduleDuration; //BUG: THIS WILL CRASH IF SG_TotalScheduleDuration IS ZERO
    int minutes,
            seconds;

    //Bruteforce way to get grid-steps that are within a user-friendly range:
    if (step < SG_GRIDX_MINSTEP) {
        do {
            step *= 2;
            textstep *= 2;
        } while (step < SG_GRIDX_MINSTEP);
    } else if (step > SG_GRIDX_MAXSTEP) {
        do {
            step *= .5;
            textstep *= .5;
        } while (step > SG_GRIDX_MAXSTEP);
    }
    while ((ax = (int) ((index += step) + .5)) < widget->allocation.width) {
        gdk_draw_line(SG_pixmap, SG_gc, ax, 0, ax, widget->allocation.height);
        textindex += textstep;
        //first handle if there are no minutes at all:
        if ((minutes = ((int) (textindex + .5)) / SG_GRIDX_UNIT) < 1)
            sprintf(graphtext, "%ds", ((int) (textindex + .5)));
        else if ((seconds = ((int) (textindex + .5)) % SG_GRIDX_UNIT) == 0)
            sprintf(graphtext, "%dm", minutes);
        else
            sprintf(graphtext, "%dm%ds", minutes, seconds);
        pango_layout_set_text(SG_PangoLayout, graphtext, -1);
        gdk_draw_layout(SG_pixmap, widget->style->black_gc, ax, xtextoffset,
                SG_PangoLayout);
    }
}

/////////////////////////////////////////////////////
//20100405 - separated from SG_DrawGraph so user could set it

void SG_FontSetup(GtkWidget * widget) {
    PangoFontDescription *fontdesc =
            pango_font_description_copy(widget->style->font_desc);
    //   pango_font_description_copy (NULL);
    //I can set the font here, but seems safer to work with what I know user has:
    //pango_font_description_set_family (fontdesc, "Sans Bold Italic");
    //  pango_font_description_set_size (fontdesc, PANGO_SCALE * SG_GraphFontSize);

    /*
       if (FALSE == pango_font_description_get_size_is_absolute (fontdesc))
       {
       double dpi = gdk_screen_get_resolution (gdk_screen_get_default ());
       SG_GraphFontSize = dpi * SG_GraphFontSize /  72.0;
       }
       SG_DBGOUT_INT ("New font size:", SG_GraphFontSize);
     */

    pango_font_description_set_absolute_size(fontdesc,
            PANGO_SCALE * SG_GraphFontSize);
    //pango_layout_set_alignment(layout,PANGO_ALIGN_RIGHT);
    pango_layout_set_font_description(SG_PangoLayout, fontdesc);
    pango_font_description_free(fontdesc);
}

/////////////////////////////////////////////////////
//If there is one function I should speed-up,
//this is it. Among other things, I could split
//it up in to separate functions, some of which
//could be left-out during high refresh periods
////
// Draw graph on the screen
/////////////////////////////////////////////////////
#define SG_TEXTYOFFSET 8

void SG_DrawGraph(GtkWidget * widget) {
    int x,
            y;

    //a20070702: needed so initializations don't cause a flood of GDK errors:
    if (SG_pixmap == NULL) {
        return;
    }

    // Create a GC to draw with if it isn't already created:
    if (SG_gc == NULL) //you delete something like this with g_object_unref(SG_gc);
    {
        SG_gc = gdk_gc_new(widget->window);
    }

    //Create Pango layout and set it up to write text with if it isn't already created:
    if (SG_PangoLayout == NULL) //you delete something like this with g_object_unref(gc);
    {
        //prepare for Text writing:
        SG_PangoLayout = pango_layout_new(gdk_pango_context_get());
        SG_FontSetup(widget);
    }

    //First paint the background:
    //NOTE: colors are between 0 and ~65000 in GDK
    GdkColor color;

    color.red = color.green = color.blue = 65000;
    gdk_gc_set_rgb_fg_color(SG_gc, &color);
    gdk_draw_rectangle(SG_pixmap,
            //        widget->style->white_gc,
            SG_gc, TRUE, 0, 0, widget->allocation.width,
            widget->allocation.height);

    color.red = color.green = color.blue = 55555;
    gdk_gc_set_rgb_fg_color(SG_gc, &color);

    SG_Voice *curVoice;

    //NOW draw grid:
    // ------START GRID-----
    switch (SG_GraphType) {
        case SG_GRAPHTYPE_BEATFREQ:
            SG_DrawGrid(widget, SG_MaxScheduleBeatfrequency, " hz");
            /*
               //now be sure voicetypes inappropriate for this graphtype aren't drawn:
               curVoice = SG_FirstVoice;
               while (curVoice != NULL) {
               if (curVoice->type != BB_VOICETYPE_BINAURALBEAT) curVoice->visibility = TRUE;
               else curVoice->visibility = FALSE;
               //advance to next voice:
               curVoice = curVoice->NextVoice;
               }        
             */
            break;

        case SG_GRAPHTYPE_BASEFREQ:
            SG_DrawGrid(widget, SG_MaxScheduleBasefrequency, " hz");
            break;

        case SG_GRAPHTYPE_VOLUME:
            SG_DrawGrid(widget, 1.0, " vol");
            break;

        case SG_GRAPHTYPE_VOLUME_BALANCE:
            SG_DrawGrid(widget, -1.0, " bal");
            break;
    }
    // ------END GRID-----

    SG_DrawCurrentPointInGraph(widget);

    //==
    //START presenting the data:
#define SG_ROUNDER +.5
    //#define SG_ROUNDER
    //First, draw the lines:
    //Start with all the easy points (where I have a valid current and next):
    curVoice = SG_FirstVoice;
    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) { //start "is voice visible?"

            if (curVoice->state == SG_SELECTED) {
                color.red = 65535;
                color.green = 0;
                color.blue = 65535;
            } else {
                color.red = 32768;
                color.green = 32768;
                color.blue = 65535;
            }
            gdk_gc_set_rgb_fg_color(SG_gc, &color);

            SG_DataPoint *curDP = curVoice->FirstDataPoint;
            int x_next = 0,
                    y_next = (int) (curDP->y SG_ROUNDER);

            while (curDP->NextDataPoint != NULL) {
                x = (int) (curDP->x SG_ROUNDER);
                y = (int) (curDP->y SG_ROUNDER);
                x_next = (int) (curDP->NextDataPoint->x SG_ROUNDER);
                y_next = (int) (curDP->NextDataPoint->y SG_ROUNDER);
                gdk_draw_line(SG_pixmap, SG_gc, x, y, x_next, y_next);

                //advance to next point:
                curDP = curDP->NextDataPoint;
            }
            ; //end of main connect the dots loop
            //======

            //Now draw last line from final graphpoint tranlated from the first datapoint:
            //x=(int)(SG_FirstDataPoint->x+widget->allocation.width+-1.5);
            x = (int) (curVoice->FirstDataPoint->x + widget->allocation.width);
            y = (int) (curVoice->FirstDataPoint->y SG_ROUNDER);
            gdk_draw_line(SG_pixmap, SG_gc, x_next, y_next, x, y);

            //Now start making the marker rectangles:
            //First do square for last graphpoint, since it is a special case and
            //the preparatory math already got done above:
            //decide last graphpoint square color (will be same as first datapoint's):
            if (curVoice->FirstDataPoint->state == SG_UNSELECTED)
                color.red = color.green = color.blue = 0;
            else {
                color.red = 65535;
                color.green = 32768;
                color.blue = 32768;
            }
            gdk_gc_set_rgb_fg_color(SG_gc, &color);
            gdk_draw_rectangle(SG_pixmap, SG_gc, FALSE,
                    x - (SG_DataPointSize >> 1),
                    y - (SG_DataPointSize >> 1), SG_DataPointSize,
                    SG_DataPointSize);

        } //end "is voice visible?"
        //advance to next voice:
        curVoice = curVoice->NextVoice;
    }

    // Now do the rest of the marker rectangles:
    //split all the loops on 20060414 to solve problem of pasted selected DPs hiding behind
    //unselected  parent DPs:
    //First do unselected:
    curVoice = SG_FirstVoice;
    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) { //start "is voice visible?"

            color.red = color.green = color.blue = 0;
            gdk_gc_set_rgb_fg_color(SG_gc, &color);
            SG_DataPoint *curDP = curVoice->FirstDataPoint;

            do {
                if (curDP->state == SG_UNSELECTED)
                    gdk_draw_rectangle(SG_pixmap, SG_gc, FALSE,
                        (int) (curDP->x SG_ROUNDER) -
                        (SG_DataPointSize >> 1),
                        (int) (curDP->y SG_ROUNDER) -
                        (SG_DataPointSize >> 1), SG_DataPointSize,
                        SG_DataPointSize);
                //advance to next point:
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        } //end "is voice visible?"
        //advance to next voice:
        curVoice = curVoice->NextVoice;
    }

    //Now do selected and draw highlighted line for BB's current entry:
    color.red = 65535;
    color.green = 32768;
    color.blue = 32768;
    gdk_gc_set_rgb_fg_color(SG_gc, &color);
    curVoice = SG_FirstVoice;
    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) { //start "is voice visible?"
            SG_DataPoint *curDP = curVoice->FirstDataPoint;

            do {
                //first check to see if progress line should be drawn:
                //START of highlighted line indicating schedule progress
                //20061124 BUG: THIS DOESN"T WORK WITH NEW MULTI-VOICE APPROACH:
                //~ if (SG_ProgressIndicatorFlag == TRUE && count++ == bb->ScheduleCount) {
                //~ color.red = 65535 >> 2;
                //~ color.green = 65535;
                //~ color.blue = 65535 >> 2;
                //~ gdk_gc_set_rgb_fg_color(SG_gc, &color);
                //~ if (curDP->NextDataPoint != NULL) {
                //~ x = (int)(curDP->x SG_ROUNDER);
                //~ y = (int)(curDP->y SG_ROUNDER);
                //~ x_next = (int)(curDP->NextDataPoint->x SG_ROUNDER);
                //~ y_next = (int)(curDP->NextDataPoint->y SG_ROUNDER);
                //~ } else {
                //~ x = (int)(SG_FirstDataPoint->x + widget->allocation.width);
                //~ y = (int)(SG_FirstDataPoint->y SG_ROUNDER);
                //~ x_next = (int)(curDP->x SG_ROUNDER);
                //~ y_next = (int)(curDP->y SG_ROUNDER);
                //~ }
                //~ gdk_draw_line (SG_pixmap, SG_gc, x, y, x_next, y_next);
                //~ color.red = 65535;
                //~ color.green = 32768;
                //~ color.blue = 32768;
                //~ gdk_gc_set_rgb_fg_color(SG_gc, &color);
                //~ } //END of highlighted line indicating schedule progress
                //now draw squares:
                if (curDP->state == SG_SELECTED)
                    gdk_draw_rectangle(SG_pixmap, SG_gc, FALSE,
                        (int) (curDP->x SG_ROUNDER) -
                        (SG_DataPointSize >> 1),
                        (int) (curDP->y SG_ROUNDER) -
                        (SG_DataPointSize >> 1), SG_DataPointSize,
                        SG_DataPointSize);
                //advance to next point:
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);

        } //end "is voice visible?"
        //advance to next voice:
        curVoice = curVoice->NextVoice;
    }

    //all done, ask manager to put it out there:
    gtk_widget_queue_draw_area(widget, 0, 0, widget->allocation.width,
            widget->allocation.height);
}

///////////////////////////////
//20070710: taken from GnauralJavaApplet:

void SG_DrawCurrentPointInGraph(GtkWidget * widget) {
    if (BB_TotalDuration <= 0) {
        return;
    }
    GdkColor color;

    color.red = 65535;
    color.green = 0;
    color.blue = 0;
    gdk_gc_set_rgb_fg_color(SG_gc, &color);
    int x =
            (int) ((widget->allocation.width / BB_TotalDuration) *
            (BB_CurrentSampleCount / BB_AUDIOSAMPLERATE) + .5f);
    gdk_draw_line(SG_pixmap, SG_gc, x, 0, x, widget->allocation.height);
}

/////////////////////////////////////////////////////
//HANDLER CODE:

gboolean SG_button_release_event(GtkWidget * widget, GdkEventButton * event) {
    if (event->button != 1 || SG_pixmap == NULL) {
        return TRUE;
    }
    //this was added 20060414. It is sort of arbitrary that it is here, so keep aware of it:
    //removed in 20070731; the function just doesn't work very well
    //SG_DeleteDuplicateDataPoints (widget);
    //=============
    //first see if we're doing bounding box selecting:
    if (SG_SelectionBox.status != 0) {
        SG_SelectionBox.status = 0; //tell renderer not to draw box
        SG_DrawGraph(widget); //erases last rendering of box
        return TRUE; //don't do any dp processing
    }

    // if (SG_CurrentDataPoint!=NULL && event->button == 1 && SG_pixmap_Graph != NULL)

    if (SG_CurrentDataPoint != NULL) {
        //First the easy one: convert all Y values to Hz:
        SG_ConvertYToData_SelectedPoints(widget);

        //User done moving, so now finally convert all X values to Durations:
        //Can't move first data point along X axis://NOTE: I think it is no longer necessary to do this here (other functions are safe on this point). But safest to just leave it...
        // if (SG_CurrentDataPoint!=SG_FirstDataPoint)
        // {
        //Now the hard one: convert SG_CurrentDataPoint's x to duration value. Hard because
        //I also need to change PrevDataPoint's duration:
        if (SG_TotalScheduleDuration == 0 || widget->allocation.width == 0) {
            return TRUE;
        }
        SG_ConvertXToDuration_AllPoints(widget);
        // }

        SG_CurrentDataPoint = NULL;
        SG_ConvertDataToXY(widget); // NOTE: I need to call this both to set limits and to bring XY's outside of graph back in
        SG_DrawGraph(widget);
        SG_GraphHasChanged = SG_DataHasChanged = TRUE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////
//Tests if a mouse-click landed on a square
//NOTE: DP's Voice must be visible to be returned

SG_DataPoint *SG_TestXYForDataPoint(gdouble x, gdouble y, int graphwidth) {
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            //make all points undraggable:
            SG_DataPoint *curDP = curVoice->FirstDataPoint;

            //first check to see if last square is clicked (since it is REALLY the first one):
            if ((x - SG_DataPointSize) < (curDP->x + graphwidth) &&
                    (x + SG_DataPointSize) > (curDP->x + graphwidth) &&
                    (y - SG_DataPointSize) < curDP->y && (y + SG_DataPointSize) > curDP->y)
                return curDP;

            //wasn't last square, so now trudge through all of them:
            do {
                if ((x - SG_DataPointSize) < curDP->x &&
                        (x + SG_DataPointSize) > curDP->x &&
                        (y - SG_DataPointSize) < curDP->y
                        && (y + SG_DataPointSize) > curDP->y)
                    return curDP;
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        }
        //advance to next voice:
        curVoice = curVoice->NextVoice;
    }
    //########
    //If it got here, user missed all existant points -- therefore it is
    //probably a NEW DATA POINT (unless it had the same x as an existant point).
    //########
    return NULL;
}

/////////////////////////////////////////////////////
//if SG_Voice * == NULL, returns currently selected voice or
//or NULL if it can't find one (so be sure to check!!!).
//Otherwise deselects all voices, then selects submitted one:

SG_Voice *SG_SelectVoice(SG_Voice * voice) {
    SG_Voice *curVoice = SG_FirstVoice;

    //if user passed NULL, just look for current selected voice:
    if (voice == NULL) {
        while (curVoice != NULL) {
            if (curVoice->state == SG_SELECTED) {
                return curVoice;
            }
            curVoice = curVoice->NextVoice;
        }
        //if it got here, no selected voice was found. So 
        //try to find ANY visible voice, and if not, return NULL:
        curVoice = SG_FirstVoice;
        while (curVoice != NULL) {
            if (curVoice->hide == FALSE) {
                curVoice->state = SG_SELECTED;
                return curVoice;
            }
            curVoice = curVoice->NextVoice;
        }
        //if it got here, there was NO visible voice:
        return NULL;
    }

    //if got here, set selected voice, but first check to see if it's 
    //visible (if not, arbitrarily look for another):
    if (voice->hide == TRUE) {
        voice->state = SG_UNSELECTED;
        //interesting, no? this should tell it to either find a valid
        //selected voice or to choose an arbitary new one.
        return SG_SelectVoice(NULL);
    }

    curVoice = SG_FirstVoice;
    while (curVoice != NULL) {
        curVoice->state = SG_UNSELECTED;
        curVoice = curVoice->NextVoice;
    }
    voice->state = SG_SELECTED;
    return (voice);
}

/////////////////////////////////////////////////////
//now I figure out which data point got clicked, if any:

gboolean SG_button_press_event(GtkWidget * widget, GdkEventButton * event) {
    if (SG_pixmap == NULL)
        return TRUE;

    //############################
    //Check for button 1: [Select point or create point]
    //############################
    if (event->button == 1) {
        //Button 1, if over a point, select it, (making it and other 
        //selected draggable. Otherwise unselect all and potentially create new point.
        //Test if mouse clicked an existing point, and if so make point selected:
        SG_CurrentDataPoint =
                SG_TestXYForDataPoint(event->x, event->y, widget->allocation.width);

        //If !NULL, it found a suitable point:
        if (SG_CurrentDataPoint != NULL) {
            //Backup points:
            SG_BackupDataPoints(widget); //a20070620
            //First, make this SG_Voice selected:
            SG_SelectVoice((SG_Voice *) (SG_CurrentDataPoint->parent));
            //OVERVIEW: If point is already selected, see if Ctrl key is pressed to unselect it. If Ctrl isn't pressed,
            //just leave the selected point alone so that motion_notify_event can be allowed to move a clump of
            //points. But If point is not selected, deselect all points then select this one.
            if (SG_CurrentDataPoint->state == SG_SELECTED) {
                if ((event->state & GDK_CONTROL_MASK) != 0)
                    SG_CurrentDataPoint->state = SG_UNSELECTED;
                return TRUE;
            }
            if ((event->state & GDK_SHIFT_MASK) == 0) {
                SG_DeselectDataPoints();
            }
            SG_CurrentDataPoint->state = SG_SELECTED;
            SG_DrawGraph(widget);
            return TRUE;
        }

        //must check for SHIFT and CTRL keys again in this context,
        //just in case use is dragging a selection box:
        if (0 == (event->state & GDK_SHIFT_MASK) && 0 == (event->state & GDK_CONTROL_MASK)) //lock mask added 20100727
        {
            //Backup points:
            SG_BackupDataPoints(widget); //a20070620 - this often just backs up selections
            if (FALSE == SG_MagneticPointerflag) //added 20101006 to stop unselecting during Magnetic Pointer operations
            {
                SG_DeselectDataPoints();
            }
        }

        //Mouse clicked a non-existant point, so first see if it was a double-click (create a new point):
        if (event->type == GDK_2BUTTON_PRESS) {
            // SG_CurrentDataPoint=SG_InsertNewDataPointXY(widget, event->x,event->y+.5);
            // 20060714 I think this is working now, but I said "THIS ISN'T WORKING YET!!!!! NOT FINISHED 20060504"
            SG_Voice *tmpVoice = SG_SelectVoice(NULL);

            if (tmpVoice == NULL) {
                //got rid of next two lines 20070731:
                //    tmpVoice = SG_FirstVoice;
                //    SG_FirstVoice->state = SG_SELECTED;
                return TRUE; // not legal to create new point
            }
            //Backup points:
            SG_BackupDataPoints(widget); //a20070620
            SG_CurrentDataPoint =
                    SG_InsertNewDataPointXY(widget, tmpVoice, event->x, event->y + .5);
            if (SG_CurrentDataPoint != NULL) {
                SG_ProgressIndicatorFlag = FALSE;
                //Point exists, so convert it's Y to real data (X got determined at creation):
                SG_ConvertYToData_SelectedPoints(widget);
                SG_DrawGraph(widget);
                SG_GraphHasChanged = SG_DataHasChanged = TRUE;
                return TRUE;
            }
        } //END double-click portion

        //if it got here, use XY as start of a selection bounding box:
        SG_SelectionBox.status = 1;
        SG_SelectionBox.startX = (int) (event->x);
        SG_SelectionBox.startY = (int) (event->y);
        SG_DrawGraph(widget); //I don't think this is necessary
        return TRUE;
    }        //############################
        //Check for button 2 [delete point]:
        //############################
    else if (event->button == 2) {
        SG_Voice *curVoice = SG_FirstVoice;

        while (curVoice != NULL) {
            if (curVoice->hide == FALSE) {
                SG_DataPoint *curDP = curVoice->FirstDataPoint;

                do {
                    if ((event->x - SG_DataPointSize) < curDP->x &&
                            (event->x + SG_DataPointSize) > curDP->x &&
                            (event->y - SG_DataPointSize) < curDP->y &&
                            (event->y + SG_DataPointSize) > curDP->y) {
                        SG_ProgressIndicatorFlag = FALSE;
                        //Backup points:
                        SG_BackupDataPoints(widget); //a20070620
                        if ((event->state & GDK_SHIFT_MASK) == 0) {
                            SG_DeleteDataPoint(curDP, FALSE);
                        } else {
                            SG_DeleteDataPoint(curDP, TRUE);
                        }
                        SG_ConvertDataToXY(widget);
                        SG_DrawGraph(widget);
                        SG_DataHasChanged = SG_GraphHasChanged = TRUE; //added 20070803
                        return TRUE;
                    }
                    curDP = curDP->NextDataPoint;
                } while (curDP != NULL);

                //If I got here, it must be the last square (which is REALLY the first SG_DataPoint):
                curDP = curVoice->FirstDataPoint;
                if ((event->x + SG_DataPointSize) > (curDP->x + widget->allocation.width) && (event->y - SG_DataPointSize) < curDP->y && (event->y + SG_DataPointSize) > curDP->y && curVoice->FirstDataPoint->NextDataPoint != NULL) //to be sure we don't end up trying to delete the ONLY point
                {
                    //Backup points:
                    SG_BackupDataPoints(widget); //a20070620
                    // if ((event->state & GDK_SHIFT_MASK)==0) SG_DeleteDataPoint(curDP);
                    // else SG_DeleteDataPoint(curDP, TRUE);
                    SG_DeleteDataPoint(curDP, FALSE);
                    SG_ConvertDataToXY(widget);
                    SG_DrawGraph(widget);
                    return TRUE;
                }
            }
            curVoice = curVoice->NextVoice;
        }
        //shouldn't actually ever get here...
        return TRUE;
    }        //############################
        //Not button 1 or 2, so check for button 3 [bring-up a properties box for the SG_DataPoint]:
        //############################
    else if (event->button == 3) {
        SG_DataPoint *curDP;
        SG_Voice *curVoice = SG_FirstVoice;

        while (curVoice != NULL) {
            if (curVoice->hide == FALSE) {
                curDP = curVoice->FirstDataPoint;
                //run through all the DataPoints to find the one clicked (if any):
                //NOTE: we ignore trying to click on last visible datapoint because it isn't a real one (rather, 'tis the first one)
                do {
                    if ((event->x - SG_DataPointSize) < curDP->x &&
                            (event->x + SG_DataPointSize) > curDP->x &&
                            (event->y - SG_DataPointSize) < curDP->y &&
                            (event->y + SG_DataPointSize) > curDP->y) {
                        SG_BackupDataPoints(widget);
                        SG_DataPointPropertiesDialog(widget, curDP);
                        SG_ProgressIndicatorFlag = FALSE;
                        SG_ConvertDataToXY(widget);
                        SG_DrawGraph(widget);
                        SG_GraphHasChanged = SG_DataHasChanged = TRUE;
                        return TRUE;
                    }
                    curDP = curDP->NextDataPoint;
                } while (curDP != NULL);
            }
            curVoice = curVoice->NextVoice;
        }
        //if I got here, the Properties box will affect ALL selected DPs:
        //Following part scans all selected DPs for each field category below
        // to see if ALL already are the same, so that value can be
        // used instead of -1 (to inform user that this is the case):
        //NOTE: -1 means:
        //   outgoing: "don't show anythign in box."
        //   return:"don't change the DP value associated with that box"
        gboolean firstselflag = FALSE;
        SG_DataPoint propDP,
                tempDP;

        tempDP.basefreq = tempDP.beatfreq = tempDP.volume_left =
                tempDP.volume_right = tempDP.duration = -1;
        curVoice = SG_FirstVoice;
        while (curVoice != NULL) {
            if (curVoice->hide == FALSE) {
                curDP = curVoice->FirstDataPoint;
                do {
                    if (curDP->state == SG_SELECTED) {
                        if (firstselflag == FALSE) {
                            firstselflag = TRUE;
                            propDP.basefreq = tempDP.basefreq = curDP->basefreq;
                            propDP.beatfreq = tempDP.beatfreq = curDP->beatfreq;
                            propDP.volume_left = tempDP.volume_left = curDP->volume_left;
                            propDP.volume_right = tempDP.volume_right = curDP->volume_right;
                            propDP.duration = tempDP.duration = curDP->duration;
                        }
                        if (tempDP.basefreq != curDP->basefreq && propDP.basefreq != -1)
                            propDP.basefreq = -1;
                        if (tempDP.beatfreq != curDP->beatfreq && propDP.beatfreq != -1)
                            propDP.beatfreq = -1;
                        if (tempDP.volume_left != curDP->volume_left &&
                                propDP.volume_left != -1)
                            propDP.volume_left = -1;
                        if (tempDP.volume_right != curDP->volume_right &&
                                propDP.volume_right != -1)
                            propDP.volume_right = -1;
                        if (tempDP.duration != curDP->duration && propDP.duration != -1)
                            propDP.duration = -1;
                    }
                    curDP = curDP->NextDataPoint;
                } while (curDP != NULL);
            }
            curVoice = curVoice->NextVoice;
        }

        propDP.parent = NULL; //this tells function to use different form of properites dialog
        //now run through all the DataPoints to adjust all selected DPs (if any):
        if (TRUE == SG_DataPointPropertiesDialog(widget, &propDP)) {
            SG_BackupDataPoints(widget);
            curVoice = SG_FirstVoice;
            while (curVoice != NULL) {
                if (curVoice->hide == FALSE) {
                    curDP = curVoice->FirstDataPoint;
                    do {
                        if (curDP->state == SG_SELECTED) {
                            if (propDP.basefreq != -1)
                                curDP->basefreq = propDP.basefreq;
                            if (propDP.volume_left != -1)
                                curDP->volume_left = propDP.volume_left;
                            if (propDP.volume_right != -1)
                                curDP->volume_right = propDP.volume_right;
                            if (propDP.duration != -1)
                                curDP->duration = propDP.duration;
                            if (propDP.beatfreq != -1)
                                curDP->beatfreq = propDP.beatfreq;
                        }
                        curDP = curDP->NextDataPoint;
                    } while (curDP != NULL);
                }
                curVoice = curVoice->NextVoice;
            }
            SG_ProgressIndicatorFlag = FALSE;
            SG_ConvertDataToXY(widget);
            SG_DrawGraph(widget);
            SG_GraphHasChanged = SG_DataHasChanged = TRUE;
        }
        return TRUE;
    }
    return TRUE;
}

//////////////////////////////////////
//this counts voices, dps, selected dps, and also how many voices have dps.
//NOTE: voicecount_selecteddp is actually count of voices with selected DPs

void SG_CountAllData(int *voicecount_all, int *voicecount_selecteddp,
        int *dpcount_all, int *dpcount_selected) {
    //do some chores to get some numbers:
    *dpcount_all = 0;
    *dpcount_selected = 0;
    *voicecount_all = 0;
    *voicecount_selecteddp = 0;
    gboolean newvflag;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        ++(*voicecount_all);
        newvflag = TRUE;
        SG_DataPoint *curDP = curVoice->FirstDataPoint;

        while (curDP != NULL) {
            ++(*dpcount_all);
            if (curDP->state == SG_SELECTED) {
                ++(*dpcount_selected);
                if (newvflag == TRUE) {
                    ++(*voicecount_selecteddp);
                }
                newvflag = FALSE;
            }
            curDP = curDP->NextDataPoint;
        }
        curVoice = curVoice->NextVoice;
    }
}

//////////////////////////////////////
//this counts dps and selected dps in given voice

void SG_CountVoiceDPs(SG_Voice * curVoice, int *dpcount_all,
        int *dpcount_selected) {
    *dpcount_all = 0;
    *dpcount_selected = 0;
    if (curVoice != NULL) {
        SG_DataPoint *curDP = curVoice->FirstDataPoint;

        while (curDP != NULL) {
            ++(*dpcount_all);
            if (curDP->state == SG_SELECTED) {
                ++(*dpcount_selected);
            }
            curDP = curDP->NextDataPoint;
        }
    }
}

/////////////////////////////////////////////////////
//If curDP->parent == NULL, properties box is tailored
//for multiple DPs, otherwise for just one

gboolean SG_DataPointPropertiesDialog(GtkWidget * widget,
        SG_DataPoint * curDP) {
    gboolean res = FALSE;
    char tmpstring[256];

    if (curDP == NULL)
        return FALSE;
    if (curDP->parent != NULL)
        sprintf(tmpstring, "Properties: SG_DataPoint");
    else
        sprintf(tmpstring, "Properties: Selected DataPoints");

    GtkWidget *dialog = gtk_dialog_new_with_buttons(tmpstring,
            NULL,
            (GtkDialogFlags)
            (GTK_DIALOG_MODAL |
            GTK_DIALOG_DESTROY_WITH_PARENT),
            GTK_STOCK_OK,
            GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL,
            GTK_RESPONSE_REJECT,
            NULL);

    //add some stuff:
    GtkWidget *label_dur = gtk_label_new("Event Duration (sec.):");
    GtkWidget *entry_Input_dur = gtk_entry_new();
    GtkWidget *label_volume_left =
            gtk_label_new("Volume Right (range 0 - 1.0):");
    GtkWidget *entry_Input_volume_left = gtk_entry_new();
    GtkWidget *label_volume_right =
            gtk_label_new("Volume Left (range 0 - 1.0):");
    GtkWidget *entry_Input_volume_right = gtk_entry_new();
    GtkWidget *label_starts = gtk_label_new("STARTING VALUES:");
    GtkWidget *label_beatfreq = gtk_label_new("Beat Frequency (hz):");
    GtkWidget *entry_Input_beatfreq = gtk_entry_new();
    GtkWidget *label_basefreq = gtk_label_new("Base Frequency (hz):");
    GtkWidget *entry_Input_basefreq = gtk_entry_new();

    if (curDP->duration != -1) {
        sprintf(tmpstring, "%g", curDP->duration);
        gtk_entry_set_text(GTK_ENTRY(entry_Input_dur), tmpstring);
    }
    if (curDP->beatfreq != -1) {
        sprintf(tmpstring, "%g", curDP->beatfreq);
        gtk_entry_set_text(GTK_ENTRY(entry_Input_beatfreq), tmpstring);
    }
    if (curDP->basefreq != -1) {
        sprintf(tmpstring, "%g", curDP->basefreq);
        gtk_entry_set_text(GTK_ENTRY(entry_Input_basefreq), tmpstring);
    }
    if (curDP->volume_left != -1) {
        sprintf(tmpstring, "%g", curDP->volume_left);
        gtk_entry_set_text(GTK_ENTRY(entry_Input_volume_left), tmpstring);
    }
    if (curDP->volume_right != -1) {
        sprintf(tmpstring, "%g", curDP->volume_right);
        gtk_entry_set_text(GTK_ENTRY(entry_Input_volume_right), tmpstring);
    }

    // Add the label, and show everything we've added to the dialog.
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_dur);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
            entry_Input_dur);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_starts);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
            label_beatfreq);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
            entry_Input_beatfreq);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
            label_basefreq);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
            entry_Input_basefreq);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
            label_volume_left);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
            entry_Input_volume_left);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
            label_volume_right);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
            entry_Input_volume_right);

    if (curDP->parent != NULL) {
        double times =
                curDP->x * (SG_TotalScheduleDuration / widget->allocation.width);
        sprintf(tmpstring, "\tStart time:\t%d min. \t%d sec.\t",
                ((int) (times + .5)) / 60, ((int) (times + .5)) % 60);
        GtkWidget *label_start = gtk_label_new(tmpstring);

        sprintf(tmpstring, "\tEnd time: \t%d min. \t%d sec.\t",
                ((int) (times + curDP->duration + .5)) / 60,
                ((int) (times + curDP->duration + .5)) % 60);
        GtkWidget *label_end = gtk_label_new(tmpstring);

        sprintf(tmpstring, "X:\t%g", curDP->x);
        GtkWidget *label_x = gtk_label_new(tmpstring);

        sprintf(tmpstring, "Y:\t%g", (widget->allocation.height - curDP->y));
        GtkWidget *label_y = gtk_label_new(tmpstring);

        sprintf(tmpstring, "SG_Voice ID# %d", ((SG_Voice *) curDP->parent)->ID);
        GtkWidget *label_voice = gtk_label_new(tmpstring);

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_start);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_end);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_x);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_y);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_voice);
    } else {
        //do some chores to get some numbers:
        int dpcount = 0;
        int seldpcount = 0;
        int voicecount = 0;
        int selvoicecount = 0;

        SG_CountAllData(&voicecount, &selvoicecount, &dpcount, &seldpcount);

        /*
           gboolean newvflag;
           SG_Voice *curVoice = SG_FirstVoice;
           while (curVoice != NULL)
           {
           ++voicecount;
           newvflag = TRUE;
           SG_DataPoint *curDP = curVoice->FirstDataPoint;
           while (curDP != NULL)
           {
           ++dpcount;
           if (curDP->state == SG_SELECTED)
           {
           ++seldpcount;
           if (newvflag == TRUE)
           ++selvoicecount;
           newvflag = FALSE;
           }
           curDP = curDP->NextDataPoint;
           }
           curVoice = curVoice->NextVoice;
           }
         */

        if (seldpcount == 0) {
            gtk_widget_destroy(dialog);
            return FALSE;
        }

        sprintf(tmpstring, "Total Schedule Duration:\t%d min. \t%d sec.",
                ((int) (SG_TotalScheduleDuration + .5)) / 60,
                ((int) (SG_TotalScheduleDuration + .5)) % 60);
        GtkWidget *label_start = gtk_label_new(tmpstring);

        sprintf(tmpstring, "Total DataPoints: %d", dpcount);
        GtkWidget *label_end = gtk_label_new(tmpstring);

        sprintf(tmpstring, "Total Selected DataPoints: %d", seldpcount);
        GtkWidget *label_x = gtk_label_new(tmpstring);

        sprintf(tmpstring, "Total Voices: %d", voicecount);
        GtkWidget *label_y = gtk_label_new(tmpstring);

        sprintf(tmpstring, "\tTotal Voices with Selected DataPoints: %d\t",
                selvoicecount);
        GtkWidget *label_voice = gtk_label_new(tmpstring);

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_start);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_end);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_x);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_y);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label_voice);
    }

    gtk_widget_show_all(dialog);

    //block until I get a response:
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));

    switch (result) {
        case GTK_RESPONSE_ACCEPT:
        {
            double tmpd;
            const gchar *restr;

            restr = gtk_entry_get_text(GTK_ENTRY(entry_Input_dur));
            tmpd = (restr[0] != '\0') ? atof(restr) : -1.0;
            curDP->duration = tmpd;

            restr = gtk_entry_get_text(GTK_ENTRY(entry_Input_beatfreq));
            tmpd = (restr[0] != '\0') ? atof(restr) : -1.0;
            curDP->beatfreq = tmpd;

            restr = gtk_entry_get_text(GTK_ENTRY(entry_Input_volume_left));
            tmpd = (restr[0] != '\0') ? atof(restr) : -1.0;
            curDP->volume_left = tmpd;
            if (curDP->volume_left > 1.0)
                curDP->volume_left = 1.0;

            restr = gtk_entry_get_text(GTK_ENTRY(entry_Input_volume_right));
            tmpd = (restr[0] != '\0') ? atof(restr) : -1.0;
            curDP->volume_right = tmpd;
            if (curDP->volume_right > 1.0)
                curDP->volume_right = 1.0;

            restr = gtk_entry_get_text(GTK_ENTRY(entry_Input_basefreq));
            tmpd = (restr[0] != '\0') ? atof(restr) : -1.0;
            curDP->basefreq = tmpd;
            res = TRUE;
            SG_GraphHasChanged = SG_DataHasChanged = TRUE; //added 20070109
            break;
        }

        default:
            res = FALSE;
            break;
    }
    gtk_widget_destroy(dialog);
    return res;
}

//////////////////////////////////////////
//this inserts srcDP to the left of destDP.

gboolean SG_InsertLink(SG_DataPoint * srcDP, SG_DataPoint * destDP) { //swap two links:
    if (srcDP == NULL || destDP == NULL)
        return FALSE;

    SG_DataPoint *destprevDP = destDP->PrevDataPoint;
    SG_DataPoint *srcprevDP = srcDP->PrevDataPoint;
    SG_DataPoint *srcnextDP = srcDP->NextDataPoint;

    //     SG_DataPoint * destnextDP=destDP->NextDataPoint;

    //curDP is FirstDataPoint; there is nothing logical to swap-left with, so:
    if (destprevDP == NULL)
        return FALSE;

    destDP->PrevDataPoint = srcDP;
    destprevDP->NextDataPoint = srcDP;
    srcDP->NextDataPoint = destDP;
    srcDP->PrevDataPoint = destprevDP;
    srcprevDP->NextDataPoint = srcnextDP;
    //deal with special case where srcDP is LastDataPoint, in which case 2nd-to-last becomes last:
    if (srcnextDP != NULL)
        srcnextDP->PrevDataPoint = srcprevDP;

    return TRUE;
}

//////////////////////////////////////////
//a20070724: Swaps links, but pays no attention to X values (that's your job)
//returns false if it fails, I guess. This this was a bear

gboolean SG_SwapLinks(SG_DataPoint * curDP1, SG_DataPoint * curDP2) { //swap two links:
    if (curDP1 == NULL || curDP2 == NULL || curDP1 == curDP2) {
        return FALSE;
    }
    //first the easy part:
    SG_DataPoint *curDP1_prev = curDP1->PrevDataPoint;
    SG_DataPoint *curDP1_next = curDP1->NextDataPoint;

    curDP1->PrevDataPoint = curDP2->PrevDataPoint;
    curDP1->NextDataPoint = curDP2->NextDataPoint;
    curDP2->PrevDataPoint = curDP1_prev;
    curDP2->NextDataPoint = curDP1_next;

    //now the hard part (checking if what we did is vaid, and correcting if not):
    int i;
    SG_DataPoint *curDP = curDP1;
    SG_DataPoint *otherDP = curDP2;

    for (i = 0; i < 2; i++) {
        if (i != 0) {
            curDP = curDP2;
            otherDP = curDP1;
        }

        //first deal with possibility that DPs were adjacent:
        if (curDP->NextDataPoint == curDP) {
            curDP->NextDataPoint = otherDP;
            otherDP->PrevDataPoint = curDP;
        }

        if (curDP->PrevDataPoint == curDP) {
            curDP->PrevDataPoint = otherDP;
            otherDP->NextDataPoint = curDP;
        }

        //now deal with logical adjacent DPs:
        //see if there is a new FirstDataPoint:
        if (curDP->PrevDataPoint == NULL) {
            ((SG_Voice *) curDP->parent)->FirstDataPoint = curDP;
        } else {
            curDP->PrevDataPoint->NextDataPoint = curDP;
        }
        //see if there is a new last DataPoint:
        if (curDP->NextDataPoint == NULL) {
        } else {
            curDP->NextDataPoint->PrevDataPoint = curDP;
        }
    }
    return TRUE;
}

//////////////////////////////////////////
//this swaps link you pass to it with neighbor to it's "left"
//(so if you want to want to swap something with it's
//neighbor to the right, pass the neighbor instead)
//NOTE: this works well for the visual needs of the graph, but
//is unsuitable for really determining final values for data points
//because it can't compensate dur/beatfreq values losing their
//original reference to neighbors. Use "InsertDatapoint"
//after mouse-click is released to get real final values from
//the x,y

gboolean SG_SwapLinks_left(SG_DataPoint * curDP) { //swap two links:
    if (curDP == NULL)
        return FALSE;
    SG_DataPoint *prevDP = curDP->PrevDataPoint;
    SG_DataPoint *nextDP = curDP->NextDataPoint;

    if (prevDP == NULL) { //curDP is FirstDataPoint; there is nothing logical to swap-left with, so:
        return FALSE;
    }
    SG_DataPoint *prevprevDP = curDP->PrevDataPoint->PrevDataPoint;

    if (prevprevDP == NULL) {
        return FALSE;
    }
    curDP->NextDataPoint = prevDP;
    curDP->PrevDataPoint = prevprevDP;
    prevDP->NextDataPoint = nextDP;
    prevDP->PrevDataPoint = curDP;
    //deal with special case where curDP is LastDataPoint, in which case 2nd-to-last becomes last:
    if (nextDP != NULL) {
        nextDP->PrevDataPoint = prevDP;
    }
    prevprevDP->NextDataPoint = curDP;
    return TRUE;
}

/*
   //////////////////////////////////////////
   //THIS IS ONLY HERE AS AN EXAMPLE OF THE INVERSE
   //OF THE ABOVE SWAP FUNCTION:
   //this swaps link you pass to it with neighbor to it's "right"
   //(so if you want to want to swap something with it's
   //neighbor to the left, you could pass the neighbor instead)
   gboolean SG_SwapLinks_right(SG_DataPoint * curDP)
   {//swap two links:
   if (curDP==NULL) return FALSE;
   SG_DataPoint * prevDP=curDP->PrevDataPoint;
   SG_DataPoint * nextDP=curDP->NextDataPoint;
   if (nextDP==NULL || prevDP==NULL) return FALSE;
   SG_DataPoint * nextnextDP=curDP->NextDataPoint->NextDataPoint;
   if (nextnextDP==NULL) return FALSE;
   curDP->NextDataPoint=nextnextDP;
   curDP->PrevDataPoint=nextDP;
   nextDP->PrevDataPoint=prevDP;
   nextDP->NextDataPoint=curDP;
   prevDP->NextDataPoint=nextDP;
   nextnextDP->PrevDataPoint=curDP;
   return TRUE;
   }
 */

//TODO: Figure out why I must convert the values to ints for "is_hint"
/////////////////////////////////////////////////////

gboolean SG_motion_notify_event(GtkWidget * widget, GdkEventMotion * event) {
    int x,
            y;
    GdkModifierType state;

    if (event->is_hint)
        gdk_window_get_pointer(event->window, &x, &y, &state);
    else {
        x = (int) (event->x + .5);
        y = (int) (event->y + .5);
        state = (GdkModifierType) event->state;
    }

    if ((state & GDK_BUTTON1_MASK) == 0 || SG_pixmap == NULL)
        return TRUE;

    //=============
    //see whether to do bounding box or drag point(s):
    //first, is is a bounding box?:
    if (SG_SelectionBox.status != 0) {
        //first draw graph (to erase last bounding box draw):
        SG_DrawGraph(widget);

        //20100727: if main_MagneticPointerflag == TRUE, do 
        //"Magnetic Pointer", meaning  move selected points on the pointer's
        //Y axis to the pointer:
        //  if (event->state & GDK_MOD1_MASK)
        //  if (event->state & GDK_LOCK_MASK)
        if (TRUE == SG_MagneticPointerflag) {
            //   SG_DBGOUT ("CAPS LOCK ON");
            if (0 != SG_MagneticPointer(widget, x, y)) {
                SG_DrawGraph(widget);
            }
            return TRUE;
        }

        //now draw a box over it:
        GdkColor color;

        color.red = color.green = color.blue = 0;
        gdk_gc_set_rgb_fg_color(SG_gc, &color);
        gdk_gc_set_line_attributes(SG_gc, 1, GDK_LINE_ON_OFF_DASH,
                (GdkCapStyle) 0, (GdkJoinStyle) 0);
        int startX = SG_SelectionBox.startX;
        int startY = SG_SelectionBox.startY;
        int endX = x - startX;
        int endY = y - startY;

        if (endX < 0) {
            startX = x;
            endX = SG_SelectionBox.startX - x;
        }
        if (endY < 0) {
            startY = y;
            endY = SG_SelectionBox.startY - y;
        }
        gdk_draw_rectangle(SG_pixmap, SG_gc, FALSE, startX, startY, endX, endY);
        if ((event->state & GDK_CONTROL_MASK) == 0) {
            SG_SelectDataPoints(startX, startY, startX + endX, startY + endY, TRUE);
        } else {
            SG_SelectDataPoints(startX, startY, startX + endX, startY + endY, FALSE);
        }
        //all done, put it out there:
        //NOTE: I don't need to do this, for some reason:
        //  gtk_widget_queue_draw_area (widget,startX,startY,endX, endY);
        //  gtk_widget_queue_draw_area (widget,0,0,widget->allocation.width, widget->allocation.height);
        gdk_gc_set_line_attributes(SG_gc, 1, GDK_LINE_SOLID, (GdkCapStyle) 0,
                (GdkJoinStyle) 0);
    }        //=============
        //Not bounding selection box, so deal with possibility that user was trying to move some points:
        //the user must always have one live SG_CurrentDataPoint to move any points, and it must
        //be selected, because it's movement will be the reference for all other other selected points to move
    else if (SG_CurrentDataPoint != NULL &&
            SG_CurrentDataPoint->state == SG_SELECTED) {
        SG_ProgressIndicatorFlag = FALSE;
        //Determine how far X and Y have moved, and move all selected points accordingly:
        SG_MoveSelectedDataPoints(widget, x - SG_CurrentDataPoint->x,
                y - SG_CurrentDataPoint->y);

        //Finally, put the graphical result out there (save translating in to dur/beatfreq for mouse release; way too slow):
        SG_DrawGraph(widget);
    }
    return TRUE;
}

/*
   /////////////////////////////////////////////////////
   //THIS ONE ONLY WORKS DISREGARDING SELECTED/UNSELECTED STATUS
   //give the existing datapoint and the desired new x, this will
   //move it to the proper place on graph AND in the linked-list.
   //returns FALSE if it something illegal is being asked-for:
   gboolean  SG_MoveDataPoint_simple(GtkWidget *widget, SG_DataPoint * curDP, double newx, double newy)
   {
   //first do y:
   if (newy > widget->allocation.height)  newy = widget->allocation.height;
   curDP->y=newy; 

   //now do x:  
   if (curDP!=SG_FirstDataPoint) 
   {
   //first limit-check x axis:
   if (newx >= 0 && newx < widget->allocation.width) curDP->x=newx;
   else return FALSE;

   //now see if point moved past a neighbor:
   do {
   if (newx < curDP->PrevDataPoint->x)  SG_SwapLinks(curDP);
   //Must treat the last real SG_DataPoint differently, for two reasons: 
   //1) it can't be pulled further than right-edge of graph
   //2) it's NextDataPoint equals NULL, so can't swap-right
   else  if (curDP->NextDataPoint==NULL) break;//last DP can't swap with anything to its right
   if (newx > curDP->NextDataPoint->x)   SG_SwapLinks(curDP->NextDataPoint);
   } while (curDP->NextDataPoint!=NULL &&
   (newx < curDP->PrevDataPoint->x    || 
   newx > curDP->NextDataPoint->x));
   } else return FALSE;
   return TRUE;
   }
 */

/*
   /////////////////////////////////////////////////////
   //THIS ONE TAKES IN TO CONSIDERATION SELECTED/UNSELECTED STATUS
   //give the existing datapoint and the desired new x, this will
   //move it to the proper place on graph AND in the linked-list.
   //NOTE: this function quadrupled in complexity once it had to
   //differentiate selected and unselected points and allow user to 
   //expand length of schedule by dragging past right edge
   void  SG_MoveDataPoint(GtkWidget *widget, SG_DataPoint * curDP, double newx, double newy)
   {
   //first do y: 
   if (newy > widget->allocation.height) newy = widget->allocation.height;//this means all Hz below 0 become 0

   curDP->y=newy; 

   //now do x:
   //first, make sure it isn't the first data point, since that can't move sideways:
   if (curDP->PrevDataPoint==NULL) return;

   //first limit-check x axis:
   // if (newx < 0 || newx >= widget->allocation.width) return FALSE;
   if (newx < 0) newx=0;
   //else if (newx >= widget->allocation.width) newx=widget->allocation.width; //TODO: eventually, I should allow user to pull past final point

   //now get difference between new point and old, then assign new value:
   double diff=curDP->x - newx;
   curDP->x=newx;
   // fprintf(stderr,"diff: %g\n",diff);

   //Now see if the new X means it passed a neighbor:
   //Positive diff means test if point moved past an unselected neighbor to the left:
   SG_DataPoint * tempDP;
   gboolean swapflag;
   if (diff>0)  do {
   //First try to find an unselected neighbor:
   tempDP=curDP;
   swapflag=FALSE;
   do {
   tempDP=tempDP->PrevDataPoint;
   if (tempDP->PrevDataPoint==NULL) return;//first DP, therefore no unselected neighbor to switch with
   }while (tempDP->state==SG_SELECTED);
   if (newx < tempDP->x) {    SG_InsertLink(curDP, tempDP); swapflag=TRUE;    }
   //   if (newx < tempDP->x) {    SG_SwapLinks(curDP); swapflag=TRUE;    }
   } while (swapflag==TRUE);

   //Negative diff means test if point moved past an unselected neighbor to the right:
   else  if (diff<0 && curDP->NextDataPoint!=NULL)  do  { //remember, last DP can't swap with anything to its right) 
   //First try to find an unselected neighbor:
   tempDP=curDP;
   swapflag=FALSE;
   do {
   tempDP=tempDP->NextDataPoint;
   if (tempDP==NULL) return;//no unselected neighbor to switch with
   }while (tempDP->state==SG_SELECTED);
   if (newx > tempDP->x) { SG_InsertLink(tempDP,curDP); swapflag=TRUE; }
   //   if (newx > tempDP->x) { SG_SwapLinks(curDP->NextDataPoint); swapflag=TRUE; }
   } while (swapflag==TRUE);
   else {
   //=====
   //here comes the most complicated set of possibilities: dealing with the last DP:
   //First the easy one: if I am merely moving the last data point within the current graph size,
   //there is nothing to do:
   if (curDP->x <= widget->allocation.width)  return;
   //But if I got here, it means that the user is pulling 
   //last DP beyond size of graph, which turns out to be quite a complex 
   //situation:

   //Problem: x and duration get out of sync. I can't know this crosser's original starting point before move, 
   //which leaves one unknown unselected DP with an invalid duration but a valid x. Thus I need to translate x 
   //in to dur for ALL DPs before I can increment SG_TotalScheduleDuration (the latter which is what actually
   //causes graph to expand), then finally translate all the DP durations back to x's (to correctly draw graph):
   //First, x to duration conversion:
   double conversion_factor=SG_TotalScheduleDuration/widget->allocation.width;
   SG_Voice * curVoice=SG_FirstVoice;
   while (curVoice !=NULL) {
   tempDP=curVoice->FirstDataPoint;
   while (tempDP->NextDataPoint != NULL) {
   tempDP->duration = (tempDP->NextDataPoint->x - tempDP->x) * conversion_factor;     
   tempDP=tempDP->NextDataPoint;
   }
   //do last datapoint:
   tempDP->duration = 0;//zero because it reached the edge of the graph
   //Now increment SG_TotalScheduleDuration:
   SG_TotalScheduleDuration+=(curDP->x - widget->allocation.width) *conversion_factor;
   curVoice=curVoice->NextVoice; 
   }   

   //Now, convert them back (dur to x conversion):
   //NOTE: New problem here, since multiple points pulled in the same past-edge drag 
   //(i.e., no button-up) will now be "moved further" for the same x-move than the first crosser, 
   //because x scale has changed (and subsequent same-move crossers would normally just get the 
   //original, un-rescaled x move). This apparently ONLY matters for multiple voices, since it 
   //seems to only affect relationship between last and second-to-last DPs (which gets desynced between voices).
   //The most independent solution is just to have calling function check to see if TotalDuration has changed
   //in-between DP moves, then rescale the x move accordingly.
   conversion_factor=widget->allocation.width/SG_TotalScheduleDuration;
   curVoice=SG_FirstVoice;
   while (curVoice !=NULL) {
   double currentx=0;
   tempDP=curVoice->FirstDataPoint; 
   //this is necessary because each DP's duration is now out-of-correct proportion with it's X
   //Durations are correct, while X has become arbitrary -- so here I make X agree with duration:
   do{
   tempDP->x=currentx;
   currentx+=tempDP->duration*conversion_factor;
   tempDP=tempDP->NextDataPoint;
   }while (tempDP != NULL);
   //now get new limits (not necessary because I incremented SG_TotalScheduleDuration directly):
   //    SG_GetScheduleLimits();
   curVoice=curVoice->NextVoice; 
   }   

   }
   return;
   }
 */

/////////////////////////////////////////////////////
//THIS ONE TAKES IN TO CONSIDERATION SELECTED/UNSELECTED STATUS
//give the existing datapoint and the desired new x, this will
//move it to the proper place on graph AND in the linked-list.
//NOTE: this function quadrupled in complexity once it had to
//differentiate selected and unselected points and allow user to
//expand length of schedule by dragging past right edge

void SG_MoveDataPoint(GtkWidget * widget, SG_DataPoint * curDP, double newx,
        double newy) {
    //first do y:
    if (newy > widget->allocation.height)
        newy = widget->allocation.height; //this means all Hz below 0 become 0

    curDP->y = newy;

    //now do x:
    //first, make sure it isn't the first data point, since that can't move sideways:
    if (curDP->PrevDataPoint == NULL)
        return;

    //first limit-check x axis:
    // if (newx < 0 || newx >= widget->allocation.width) return FALSE;
    if (newx < 0)
        newx = 0;
    //else if (newx >= widget->allocation.width) newx=widget->allocation.width; //TODO: eventually, I should allow user to pull past final point

    //now get difference between new point and old, then assign new value:
    double diff = curDP->x - newx;

    curDP->x = newx;
    // fprintf(stderr,"diff: %g\n",diff);

    //Now see if the new X means it passed a neighbor:
    //Positive diff means test if point moved past an unselected neighbor to the left:
    SG_DataPoint *tempDP;
    gboolean swapflag;

    if (diff > 0)
        do {
            //First try to find an unselected neighbor:
            tempDP = curDP;
            swapflag = FALSE;
            do {
                tempDP = tempDP->PrevDataPoint;
                if (tempDP->PrevDataPoint == NULL)
                    return; //first DP, therefore no unselected neighbor to switch with
            } while (tempDP->state == SG_SELECTED);
            if (newx < tempDP->x) {
                SG_InsertLink(curDP, tempDP);
                swapflag = TRUE;
            }
            //   if (newx < tempDP->x) {    SG_SwapLinks(curDP); swapflag=TRUE;    }
        } while (swapflag == TRUE);

        //Negative diff means test if point moved past an unselected neighbor to the right:
    else if (diff < 0 && curDP->NextDataPoint != NULL)
        do { //remember, last DP can't swap with anything to its right)
            //First try to find an unselected neighbor:
            tempDP = curDP;
            swapflag = FALSE;
            do {
                tempDP = tempDP->NextDataPoint;
                if (tempDP == NULL)
                    return; //no unselected neighbor to switch with
            } while (tempDP->state == SG_SELECTED);
            if (newx > tempDP->x) {
                SG_InsertLink(tempDP, curDP);
                swapflag = TRUE;
            }
            //   if (newx > tempDP->x) { SG_SwapLinks(curDP->NextDataPoint); swapflag=TRUE; }
        } while (swapflag == TRUE);
    else {
        //=====
        //here comes the most complicated set of possibilities: dealing with the last DP:
        //First the easy one: if I am merely moving the last data point within the current graph size,
        //there is nothing to do:
        if (curDP->x <= widget->allocation.width)
            return;
        //But if I got here, it means that the user is pulling
        //last DP beyond size of graph, which turns out to be quite a complex
        //situation:

        //Problem: x and duration get out of sync. I can't know this crosser's original starting point before move,
        //which leaves one unknown unselected DP with an invalid duration but a valid x. But I need valid durations
        //in order to correctly rescale graph. Thus I need to translate x in to dur for ALL DPs before I can increment
        //SG_TotalScheduleDuration (the latter which is what actually causes graph to expand), then finally
        //translate all the DP durations back to x's (to correctly draw graph):
        //First, x to duration conversion:
        double conversion_factor =
                SG_TotalScheduleDuration / widget->allocation.width;
        SG_Voice *curVoice = SG_FirstVoice;

        while (curVoice != NULL) {
            tempDP = curVoice->FirstDataPoint;
            while (tempDP->NextDataPoint != NULL) {
                tempDP->duration =
                        (tempDP->NextDataPoint->x - tempDP->x) * conversion_factor;
                tempDP = tempDP->NextDataPoint;
            }
            //do last datapoint:
            tempDP->duration = 0; //zero because it reached the edge of the graph
            //Now increment SG_TotalScheduleDuration:
            SG_TotalScheduleDuration +=
                    (curDP->x - widget->allocation.width) * conversion_factor;
            curVoice = curVoice->NextVoice;
        }

        //Now, convert them back (dur to x conversion):
        //NOTE: New problem here, since multiple points pulled in the same past-edge drag
        //(i.e., no button-up) will now be "moved further" for the same x-move than the first crosser,
        //because x scale has changed (and subsequent same-move crossers would normally just get the
        //original, un-rescaled x move). This apparently ONLY matters for multiple voices, since it
        //seems to only affect relationship between last and second-to-last DPs (which gets desynced between voices).
        //The most independent solution is just to have calling function check to see if TotalDuration has changed
        //in-between DP moves, then rescale the x move accordingly.
        conversion_factor = widget->allocation.width / SG_TotalScheduleDuration;
        curVoice = SG_FirstVoice;
        while (curVoice != NULL) {
            double currentx = 0;

            tempDP = curVoice->FirstDataPoint;
            //this is necessary because each DP's duration is now out-of-correct proportion with it's X
            //Durations are correct, while X has become arbitrary -- so here I make X agree with duration:
            do {
                tempDP->x = currentx;
                currentx += tempDP->duration * conversion_factor;
                tempDP = tempDP->NextDataPoint;
            } while (tempDP != NULL);
            //now get new limits (not necessary because I incremented SG_TotalScheduleDuration directly):
            //    SG_GetScheduleLimits();
            curVoice = curVoice->NextVoice;
        }

    }
    return;
}

/////////////////////////////////////////////////////
//recalculates durations for all datapoints according to their x position on the graph

void SG_ConvertXToDuration_AllPoints(GtkWidget * widget) {
    double secs_per_pixel = SG_TotalScheduleDuration / widget->allocation.width;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {

        SG_DataPoint *curDP = curVoice->FirstDataPoint;

        while (curDP->NextDataPoint != NULL) {
            curDP->duration = (curDP->NextDataPoint->x - curDP->x) * secs_per_pixel;

            // SG_DBGOUT_INT("Dur:",(int)(curDP->duration+.5));

            //this must be in every linked list loop:
            curDP = curDP->NextDataPoint;
        };

        //do last datapoint:
        curDP->duration = SG_TotalScheduleDuration - ((curDP->x) * secs_per_pixel);
        // SG_DBGOUT_INT("LastDur:",(int)(curDP->duration+.5));
        curVoice = curVoice->NextVoice;
    }
}

/*
   /////////////////////////////////////////////////////
   //NOTE: should change name of this function; it actually requires user
   //to know duration of a point if it is going to be the new last point.
   //This translates the X of an existing datapoint in to the correct
   //duration in relationship to it's neighbors:
   //at some point before calling this, SG_GetScheduleLimits()
   //should have been called once to correctly set two key vars:
   //SG_TotalScheduleDuration and
   //SG_MaxScheduleBeatfrequency
   void SG_ConvertXYToDurHZ(GtkWidget *widget, SG_DataPoint * curDP) {
   //First, convert Y to Hz:
   //NOTE: might want to figure out which point had the highest value again before doing this (lastDataPoint=SG_GetScheduleLimits())
   //curDP->beatfreq = (widget->allocation.height-curDP->y)*(SG_MaxScheduleBeatfrequency/widget->allocation.height);
   curDP->beatfreq = ((widget->allocation.height - curDP->y) * SG_MaxScheduleBeatfrequency) / widget->allocation.height;

   //Now convert X to Duration:
   //The hard part: figuring out new time relationships between neighboring pixels:
   //NOTE: rounding errors and pixel resolution have made getting this right a bit of a nightmare,
   //propagating/summing small errors along total duration...
   if (curDP->PrevDataPoint == NULL)
   return ;
   //NEW WAY (20060409):
   //Benefit: this one doesn't introduce NEARLY as much rounding-error/x-rez  noise
   double oldprevdur = curDP->PrevDataPoint->duration;
   curDP->PrevDataPoint->duration = (curDP->x - curDP->PrevDataPoint->x) * (SG_TotalScheduleDuration / (double)widget->allocation.width);
   curDP->duration = oldprevdur - curDP->PrevDataPoint->duration;
   }
 */

// SG_Voice * curVoice=SG_FirstVoice;
// while (curVoice !=NULL) {   curVoice=curVoice->NextVoice; }

/////////////////////////////////////////////////////
//this creates and adds a new SG_DataPoint to the existing linked list based on X Y mouse event,
//assigning a duration relative to neighbor-proximity or (in the case of a point placed after
//formerly last point) distance from end as related to Total Duration (if x was within graph width).
//"Y" values will be set to merely whatever a left-neighboring DP has to offer, if one is available - therefore
//it is essential for user to call SG_ConvertYToData_SingleDP() or similar after this if
//a graph-view context or otherwise is used for value determination (not the case for Paste, for example).
//IMPORTANT: the duration it returns will be assigned 0 if datapoint x was beyond graph width. This
//is why the calling function must be responsible for checking if an x sent here is beyond the graph
//width, and thus will need to give the DP returned a valid duration (and then probably then do a
//convertDataToXY() to rescale graph to contain last-point's duration).
//NEW 20060412: now user can create data points with same x ("0 duration" points). Became
//a moot point as dragging points past each other created 0 duration points.
//NOTE: 20070731: be aware that this doesn't discrimintate between visible/invisible parent voices.
//I don't think I should make it aware, since so many things call it

SG_DataPoint *SG_InsertNewDataPointXY(GtkWidget * widget, SG_Voice * voice,
        double x, double y) {
    SG_DataPoint *newDP = NULL;
    SG_DataPoint *curDP = voice->FirstDataPoint;

    //a20070919:
    SG_DataPoint *nextDP = NULL;
    double diff;

    if (curDP == NULL) {
        return NULL;
    }

    //now see if DP is in-between two valid DPs:
    while (curDP->NextDataPoint != NULL) {
        if (curDP->x <= x && x <= curDP->NextDataPoint->x) {
            break;
        }
        curDP = curDP->NextDataPoint;
    }

    //if curDP->NextDataPoint == NULL, I DP was assumably to the right of the last DP.
    //Which gives me two possibilities: either the new DP's x is within graph
    //width (easy), or bigger than graph width (unsolvable)

    //allot the memory for the new point:
    newDP = (SG_DataPoint *) malloc(sizeof (SG_DataPoint));

    if (newDP == NULL) {
        return NULL;
    }

    //first make all neighboring DP's link up right:
    if (curDP->NextDataPoint == NULL) { //DP had no neighbor to right, so make it last DP:
        newDP->NextDataPoint = NULL;
        //a20070919: needed to interpolate new DP's Y values:
        nextDP = voice->FirstDataPoint;
        ;
        diff = widget->allocation.width - curDP->x;
    } else { //DP was between two neighbors
        newDP->NextDataPoint = curDP->NextDataPoint;
        newDP->NextDataPoint->PrevDataPoint = newDP;
        //a20070919: needed to interpolate new DP's Y values:
        nextDP = curDP->NextDataPoint;
        diff = (nextDP->x - curDP->x);
    }

    //both conditions need this -- just don't change the order in which Linked List
    //stuff gets assigned, makes for terribly confusing bugs:
    newDP->PrevDataPoint = curDP;
    curDP->NextDataPoint = newDP;

    //Now fill newDP's data. Because it is new, I can reference it to it's neighbor's x
    //without introducing any (or at least minimal) resolution/rounding errors:
    newDP->x = x;
    newDP->y = y;
    newDP->parent = voice;
    newDP->state = SG_SELECTED;

    //a20070919: needed to interpolate new DP's Y values:
    //assign Y data according to neighboring DPs:
    //first figure how close to first DP this is:
    double fac = 0;

    if (0 <= diff) //should never be negative, right?
    {
        fac = (x - curDP->x) / diff;
    }
    newDP->beatfreq =
            (fac * (nextDP->beatfreq - curDP->beatfreq)) + curDP->beatfreq;
    newDP->basefreq =
            (fac * (nextDP->basefreq - curDP->basefreq)) + curDP->basefreq;
    newDP->volume_left =
            (fac * (nextDP->volume_left - curDP->volume_left)) + curDP->volume_left;
    newDP->volume_right =
            (fac * (nextDP->volume_right - curDP->volume_right)) + curDP->volume_right;

    //Determine duration for the new point from it's X:
    double oldprevdur = newDP->PrevDataPoint->duration;

    newDP->PrevDataPoint->duration = (newDP->x - newDP->PrevDataPoint->x) *
            (SG_TotalScheduleDuration / (double) widget->allocation.width);
    newDP->duration = oldprevdur - newDP->PrevDataPoint->duration;

    //now do the remaining stuff that is different for the two conditions (between DPs, or last DP):
    if (curDP->NextDataPoint == NULL) {
        //this next "if/else" is a bit disconcerting in one way: it says that the new DP will be assigned a zero-duration
        //if it is larger than graph width. This means it is up to the calling function to recognize that the
        //x went past, and then give it a more appropriate duration (if necessary). Importantly, if the calling
        //function is adding multiple points to the end, it only needs to assign a duration and convert it for the very
        //last point added, since the previously added  "last" ones (initially given a 0 duration) get valid durations
        //once they get relegated to 2nd-to-last:
        if (x > widget->allocation.width) { //not solveable, since X is outside graph bounds:
            newDP->duration = 0; //if a valid duration is desired, calling function needs to recognize point went past right edge, then manually set duration
        }
    }

    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
    return newDP;
}

/*
   //20060714 SAVING THIS UNTIL I AM SURE THE REPLACEMENT ABOVE ACTUALLY WORKS 
   /////////////////////////////////////////////////////
   //this creates and adds a new SG_DataPoint to the existing linked list based on X Y mouse event,
   //assigning a duration relative to neighbor-proximity or (in the case of a point placed after
   //formerly last point) distance from end as related to Total Duration (if x was within graph width).
   //"Y" values are merely whatever a left-neighboring DP has to offer, if one is available - therefore
   //it is essential for user to call SG_ConvertYToData_SelectedPoints() or similar after this if
   //a graph-view context or otherwise is being implemented.
   //IMPORTANT: the duration it returns will be assigned 0 if datapoint x was beyond graph width. This
   //is why the calling function must be responsible for checking if an x sent here is beyond the graph
   //width, and thus will need to give the DP returned a valid duration (and then probably then do a
   //convertDurHzToXY() to rescale graph to contain last-point's duration).
   //NEW 20060412: now user can create data points with same x ("0 duration" points). Became
   //a moot point as dragging points past each other created 0 duration points.
   SG_DataPoint * SG_InsertNewDataPointXY(GtkWidget *widget, SG_Voice * voice, double x, double y) {
   SG_DataPoint * newDP = NULL;
   SG_DataPoint * curDP = voice->FirstDataPoint;
   while (curDP->NextDataPoint != NULL) {
   if (curDP->x <= x && x <= curDP->NextDataPoint->x) {
   //allot the memory for the new point:
   newDP = (SG_DataPoint *)malloc(sizeof(SG_DataPoint));

   //insert it between the earlier and later points:
   newDP->NextDataPoint = curDP->NextDataPoint;
   newDP->NextDataPoint->PrevDataPoint = newDP;
   newDP->PrevDataPoint = curDP;
   curDP->NextDataPoint = newDP;

   //fill it's data. Because it is new, I can reference it to it's neighbor's x
   //without introducing any (or at least minimal) resolution/rounding errors:
   newDP->x = x;
   newDP->y = y;
   newDP->parent = voice;
   newDP->state = SG_SELECTED;

   //assign a Y data according to neighboring previous DP, (known in this function as "curDP"):
   newDP->beatfreq = curDP->beatfreq;
   newDP->basefreq = curDP->basefreq;
   newDP->volume_left = curDP->volume_left;
   newDP->volume_right = curDP->volume_right;

   //now for the hard one, X data (duration):
   double oldprevdur = newDP->PrevDataPoint->duration;
   newDP->PrevDataPoint->duration = (newDP->x - newDP->PrevDataPoint->x) *
   (SG_TotalScheduleDuration / (double)widget->allocation.width);
   newDP->duration = oldprevdur - newDP->PrevDataPoint->duration;

   //found the point's position and filled all its data: all done:
   return newDP;
   }
   //putc('n',stderr);
   curDP = curDP->NextDataPoint;
   }
   //if it got here, the new DP's x was greater than the formerly-last DP's x (second-to-last graphpoint).
   //So, need to deal with two possibilities: whether new DP's x is within graph width (easy), or bigger than graph
   // width (hard).
   //  if (curDP->x <= x && x <= widget->allocation.width) //second conditional is not needed?
   if (curDP->x <= x) {
   //allot the memory for the new point:
   newDP = (SG_DataPoint *) malloc(sizeof(SG_DataPoint));

   //put it after the last real datapoint and link it up to the first:
   newDP->NextDataPoint = NULL; //it is the real "last one", so NULL NextDataPoint
   curDP->NextDataPoint = newDP;
   newDP->PrevDataPoint = curDP;

   //fill it's data:
   newDP->x = x;
   newDP->y = y;
   newDP->parent = voice;
   newDP->state = SG_SELECTED;

   //assign a Y data according to neighboring previous DP, (known in this function as "curDP"):
   newDP->beatfreq = curDP->beatfreq;
   newDP->basefreq = curDP->basefreq;
   newDP->volume_left = curDP->volume_left;
   newDP->volume_right = curDP->volume_right;

   //now for the hard one, duration:
   double oldprevdur = newDP->PrevDataPoint->duration;
   newDP->PrevDataPoint->duration = (newDP->x - newDP->PrevDataPoint->x) *
   (SG_TotalScheduleDuration / (double)widget->allocation.width);
   //this next "if/else" is a bit disconcerting in one way: it says that the new DP will be assigned a zero-duration
   //if it is larger than graph width. This means it is up to the calling function to recognize that the
   //x went past, and then give it a more appropriate duration (if necessary). Importantly, if the calling
   //function is adding multiple points to the end, it only needs to assign a duration and convert it for the very
   //last point added, since the previously added  "last" ones (initially given a 0 duration) get valid durations
   //once they get relegated to 2nd-to-last:
   if (x <= widget->allocation.width)
   newDP->duration = oldprevdur - newDP->PrevDataPoint->duration;
   else {
   //    SG_DBGOUT_INT("x is bigger than window-width",(int)newDP->x);
   newDP->duration = 0; //if a valid duration is desired, calling function needs to recognize point went past right edge, then manually set duration
   }
   return newDP;
   }
   //Shouldn't ever get here; will be NULL if it does (which will at least give me a bug to locate the problem)
   return newDP;
   }
 */

/////////////////////////////////////////////////////
//this creates and adds a new SG_DataPoint to the
//end of an existing linked list, or starts new one if
//curDP == NULL. Most importantly, it gives
//the curDP's (if existant) NextDataPoint the address  to this new
//point, gives this new DP's PrevDataPoint address to curDP,
//and sets new DP's NextDataPoint to NULL. FInally, if
//the curDP you send it == NULL, it tries to attach the alloted
//new DP to the DP's parent voice, if possible.

SG_DataPoint *SG_AddNewDataPointToEnd(SG_DataPoint * curDP,
        SG_Voice * voice,
        double duration,
        double beatfreq,
        double volume_left,
        double volume_right, double basefreq,
        int state) {
    SG_DataPoint *lastDP = NULL;

    //find end of list:
    while (curDP != NULL) {
        lastDP = curDP;
        curDP = curDP->NextDataPoint;
        //  SG_DBGOUT_INT("Stuck in Add New DP loop",0)
    }

    curDP = (SG_DataPoint *) malloc(sizeof (SG_DataPoint));
    curDP->PrevDataPoint = lastDP;
    curDP->duration = duration;
    curDP->beatfreq = beatfreq;
    curDP->x = 0;
    curDP->y = 0;
    curDP->volume_left = volume_left; //negative means "no change"
    curDP->volume_right = volume_right; //negative means "no change"
    curDP->basefreq = basefreq;
    curDP->parent = voice;
    curDP->state = state;
    curDP->NextDataPoint = NULL;
    if (lastDP != NULL) {
        lastDP->NextDataPoint = curDP;
    } else if (curDP->parent != NULL) {
        ((SG_Voice *) curDP->parent)->FirstDataPoint = curDP;
    }
    return curDP;
}

/*
   //START OF SAVED BLOCK
   //DON'T TOSS THESE Backup/Restore METHODS:
   //May be useful in future. ISSUES:
   //They work great, problem is that this approach to undo/redo depends (in the process
   //of creating duplicate linked lists) on swaping-out SG_FirstVoice to "Undo" -- which 
   //unfortunately means that Paste, among other things, loses context for which Voices it's DPs 
   //belong too. Which would be solvable, but basically a hacky way to approach it. instead, now
   //undo/redo will rely on the Copy/Paste methods, since they can use less room (but are a bit slower)
   /////////////////////////////////////////////////////
   //20060324
   //Creates a duplicate of the main linked-list
   //Implemented as a rudimentary one-step, toggle "Undo" feature
   //when used with a SG_RestoreDataPoints().
   void SG_BackupDataPoints()
   {
   //first free any previous backup:
   SG_CleanupVoices(SG_FirstBakVoice);
   SG_FirstBakVoice=NULL;

   SG_Voice * curVoice = SG_FirstVoice;
   SG_Voice * curBakVoice;
   SG_Voice * lastBakVoice = NULL;//MUST equal NULL!
   SG_DataPoint * curDP;
   SG_DataPoint * curBakDP;
   SG_DataPoint * lastBakDP = NULL;//MUST equal NULL!
   while (curVoice != NULL) {
   //Found a voice, so allot a new voice for backing it up:
   curBakVoice = SG_AddNewVoiceToEnd(lastBakVoice, BB_VOICETYPE_BINAURALBEAT);
   //copy voice to bakvoice:
   memcpy(curBakVoice, curVoice, sizeof(SG_Voice));
   //(re)set the critical items of backup voice to point to the right places:
   curBakVoice->NextVoice = NULL;
   curBakVoice->PrevVoice = lastBakVoice;
   //prepare lastBakVoice for next time it gets here:
   lastBakVoice=curBakVoice;
   //give it a first datapoint:
   curBakVoice->FirstDataPoint = SG_AddNewDataPointToEnd(NULL,curBakVoice, 10,1,-1.0, -1.0,0,0);
   //set SG_FirstBakVoice if we're doing first one:
   if (SG_FirstBakVoice == NULL)  SG_FirstBakVoice = curBakVoice;

   //now that we have the information, set DPs for loop:
   curDP = curVoice->FirstDataPoint;
   curBakDP = curBakVoice->FirstDataPoint;
   while (curDP != NULL) {
   //copy entire contents of curDP to curBakDP:
   memcpy(curBakDP, curDP,sizeof(SG_DataPoint));
   //now re-set curBakDP->Prev to point to it's real predecessor, and 
   //curBakDP->Next to NULL (critical! Allows AddNewDP() to know where to add next point)
   curBakDP->NextDataPoint = NULL;
   curBakDP->PrevDataPoint = lastBakDP;
   //now set lastBakDP to remember next DP's predecessor:
   lastBakDP = curBakDP;
   //also for DPs crucial to reset it's parent pointer to point to its own parent:
   curBakDP->parent = curBakVoice;
   //advance to next DP:
   curDP = curDP->NextDataPoint;
   //if not at end of curDP list, allot some more memory and advance curBakDP to it:
   if (curDP != NULL) {
   curBakDP->NextDataPoint = SG_AddNewDataPointToEnd(curBakVoice->FirstDataPoint,curBakVoice,10,1,-1.0, -1.0,0,0);
   curBakDP = curBakDP->NextDataPoint;
   }
   }
   //advance to next voice:
   curVoice = curVoice->NextVoice; 
   }
   }//END SG_BackupDataPoints()

   //SG_Voice * curVoice=SG_FirstVoice;
   //while (curVoice !=NULL) {  curVoice=curVoice->NextVoice; }

   /////////////////////////////////////////////////////
   //20060324
   //Restores a backup (if existant) of main linked-list
   //Implemented as a rudimentary one-step,toggle "Undo" feature
   //when used with a SG_BackupDataPoints()
   void SG_RestoreDataPoints(GtkWidget * widget)
   {
   //first validate SG_FirstBackupDataPoint:
   if (SG_FirstBakVoice == NULL || SG_FirstVoice == NULL) return;

   //delete old graph data:
   //SG_CleanupDataPoints(SG_FirstDataPoint);
   //20060328 -- instead of deleting old graph data (above), smarter to make 
   //it the "Backup" data. But it is up to implementation to make clear that
   //restoring this "Undo" is actually a "Redo" after an "Undo":
   SG_Voice * tmpVoice = SG_FirstVoice;
   SG_FirstVoice = SG_FirstBakVoice;
   SG_FirstBakVoice = tmpVoice;
   SG_CurrentDataPoint = NULL; 
   SG_ConvertDataToXY(widget);
   SG_DrawGraph(widget); 
   }
 */
//END OF SAVE BLOCK

//SG_Voice * curVoice=SG_FirstVoice;
//while (curVoice !=NULL) {  curVoice=curVoice->NextVoice; }

/////////////////////////////////////////////////////
//This implements a one-step "Undo" function, taking a
//snapshop that SG_RestoreDataPoints can restore.

void SG_BackupDataPoints(GtkWidget * widget) {
    SG_DBGOUT_INT("SG_BackupDataPoints...", 0);
    SG_CopyDataPoints(widget, &SG_UndoRedo, FALSE);
    if (SG_UndoRedo.DPdata == NULL) {
        SG_DBGOUT_INT("...Failed to fill Undo buffer", 0);
    }
}

/////////////////////////////////////////////////////
//This was designed to implement a one-step "Undo" function, 
//restoring the snapshop taken by SG_BackupDataPoints. However,
//it has also proven to be the most efficient way to load new data
//from other sources (files, hard-coded, etc.) in to SG. Why? 
//Because this treates a restore as a total reload from scratch. 
//And to work, it only needs the global SG_BackupData
//variable "SG_UndoRedo" to be allocated and filled. Specifically, 
//*DPdata and *Voice need to be allocated, then these fields filled:
//SG_UndoRedo:
//-  TotalDataPoints [Notably, I don't believe TotalVoices needs to be filled for the restore]
// Voice[]:
//-  type
//-  state
//-  hide
//-  mute
//-  mono [added 20100614]
//-  description
// DPdata[]:
//-  duration
//-  beatfreq
//-  basefreq
//-  volume_left
//-  volume_right
//-  state
//-  parent
//NOTE: if SG_MergeRestore == TRUE, adds restore to whatever is already
//in SG; if FALSE, erases everything currently in SG

void SG_RestoreDataPoints(GtkWidget * widget, gboolean MergeRestore) {
    if (SG_UndoRedo.DPdata == NULL || SG_UndoRedo.TotalDataPoints < 1) {
        return;
    }

    //first steal SG_UndoRedo's Undo data, then make SG_UndoRedo pose as empty to refill it with Redo data:
    SG_BackupData tmpUndoRedo;

    memcpy(&tmpUndoRedo, &SG_UndoRedo, sizeof (SG_BackupData));
    SG_UndoRedo.DPdata = NULL;
    //added following two lines on 20060604:
    SG_UndoRedo.Voice = NULL;
    SG_CopyDataPoints(widget, &SG_UndoRedo, FALSE);

    SG_Voice *curVoice;

    if (FALSE == MergeRestore) {
        //Delete EVERYTHING:
        SG_CleanupVoices(SG_FirstVoice);
        SG_FirstVoice = curVoice = NULL;
    } else {
        curVoice = SG_FirstVoice;
    }

    //Now reload EVERYTHING:

    void *placerpointer = NULL; // parent pointer is treated strictly as arbitrary but unique number here, so it can be loaded with either a pointer or an incrementing int
    SG_DataPoint *curDP = NULL;
    int voices_used_count = -1;
    gboolean newDPflag = TRUE;
    int i;

    for (i = 0; i < tmpUndoRedo.TotalDataPoints; i++) {
        if (placerpointer != tmpUndoRedo.DPdata[i].parent) {
            placerpointer = tmpUndoRedo.DPdata[i].parent;
            ++voices_used_count;
            curVoice =
                    SG_AddNewVoiceToEnd(tmpUndoRedo.Voice[voices_used_count].type,
                    tmpUndoRedo.Voice[voices_used_count].ID);
            SG_StringAllocateAndCopy(&(curVoice->description),
                    tmpUndoRedo.Voice[voices_used_count].
                    description);
            curVoice->state = tmpUndoRedo.Voice[voices_used_count].state;
            curVoice->hide = tmpUndoRedo.Voice[voices_used_count].hide;
            curVoice->mute = tmpUndoRedo.Voice[voices_used_count].mute;
            curVoice->mono = tmpUndoRedo.Voice[voices_used_count].mono; //[added 20100614]
            if (SG_FirstVoice == NULL) {
                SG_FirstVoice = curVoice;
            }
            newDPflag = TRUE;
            curDP = NULL;
            SG_DBGOUT_INT("Found new voice, ID:", curVoice->ID);
        }
        curDP = SG_AddNewDataPointToEnd(curDP, curVoice, tmpUndoRedo.DPdata[i].duration, //duration
                tmpUndoRedo.DPdata[i].beatfreq, //beatfreq
                tmpUndoRedo.DPdata[i].volume_left, //volume_left
                tmpUndoRedo.DPdata[i].volume_right, //volume_right
                tmpUndoRedo.DPdata[i].basefreq, //basefreq
                tmpUndoRedo.DPdata[i].state); //state
        if (newDPflag == TRUE) {
            newDPflag = FALSE;
            curVoice->FirstDataPoint = curDP;
            SG_DBGOUT_PNT("Gave new voice a FirstDataPoint",
                    curVoice->FirstDataPoint);
        }
    }

    SG_ConvertDataToXY(widget);
    SG_DrawGraph(widget);
    SG_DBGOUT_INT("Finished Restoration, Pasted Points:",
            tmpUndoRedo.TotalDataPoints);

    //cleanup the original Undo data:
    //free(tmpUndoRedo.DPdata);
    //changed above to following on 20060604:
    SG_CleanupBackupData(&tmpUndoRedo);
    if (TRUE == MergeRestore) {
        SG_VoiceTestLegalSelection();
    }
    SG_DataHasChanged = SG_GraphHasChanged = TRUE;
}

/////////////////////////////////////////////////////

void SG_CopySelectedDataPoints(GtkWidget * widget) {
    SG_CopyDataPoints(widget, &SG_CopyPaste, TRUE);
}

/////////////////////////////////////////////////////

void SG_PasteSelectedDataPoints(GtkWidget * widget, gboolean predeselect) {
    SG_PasteDataPoints(widget, &SG_CopyPaste, predeselect);
}

/////////////////////////////////////////////////////
//Cleans the 4 SG_BackupData members that require variable 
//amounts of storage (allocated by SG_AllocateBackupData)

void SG_CleanupBackupData(SG_BackupData * Bdata) {
    if (Bdata == NULL) {
        return;
    }
    SG_DBGOUT_PNT("Cleaningup SG_BackupData:", Bdata);
    if (Bdata->DPdata != NULL) {
        free(Bdata->DPdata);
        SG_DBGOUT_PNT("\tFreed ->DPdata", Bdata->DPdata);
        Bdata->DPdata = NULL;
    }

    //first free all the description string data:
    if (Bdata->Voice != NULL) {
        int i;

        for (i = 0; i < Bdata->TotalVoices; i++) {
            if (Bdata->Voice[i].description != NULL) {
                SG_DBGOUT_STR("Desc.:", Bdata->Voice[i].description);
                free(Bdata->Voice[i].description);
                SG_DBGOUT_PNT("\tFreed ->VoiceDescription[]",
                        Bdata->Voice[i].description);
            }
        }

        //Now free all the remaining Voice data:
        free(Bdata->Voice);
        SG_DBGOUT_PNT("\tFreed ->Voice", Bdata->Voice);
        Bdata->Voice = NULL;
    }

    Bdata->TotalDataPoints = 0;
}

/////////////////////////////////////////////////////////////
//20070622: This allocates memory for the SG_BackupData 
//data members that require variable amounts of storage.
//Call SG_CleanupBackupData to clean what this does.

int SG_AllocateBackupData(SG_BackupData * Bdata, int TotalVoiceCount,
        int TotalDataPointCount) {
    Bdata->TotalDataPoints = TotalDataPointCount;
    Bdata->TotalVoices = TotalVoiceCount;

    //allocate the memory:
    Bdata->DPdata =
            (SG_DataPoint *) calloc(Bdata->TotalDataPoints, sizeof (SG_DataPoint));
    Bdata->Voice = (SG_Voice *) calloc(Bdata->TotalVoices, sizeof (SG_Voice));
    //make sure everything allocated properly:

    if (Bdata->DPdata == NULL || Bdata->Voice == NULL) {
        SG_CleanupBackupData(Bdata);
        return 0;
    }
    return 1;
}

/////////////////////////////////////////////////////
//20060412
//Copies any selected points to a simple array to create
//a paste-able mass when used with a SG_PasteDataPoints().

void SG_CopyDataPoints(GtkWidget * widget, SG_BackupData * Bdata,
        gboolean selected_only) {
    //First count how many DPs to copy. One reason to do this first is to be sure
    //there really are selected DPs before throwing away the old buffer:
    SG_DataPoint *curDP;
    int DP_count = 0;
    int voice_count = 0;
    int voices_used_count = 0;
    gboolean newvoiceflag;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        curDP = curVoice->FirstDataPoint;
        ++voice_count;
        newvoiceflag = TRUE;
        while (curDP != NULL) {
            if ((selected_only == FALSE) || (curDP->state == SG_SELECTED)) {
                ++DP_count;
                if (newvoiceflag == TRUE) {
                    ++voices_used_count;
                }
                newvoiceflag = FALSE;
            }
            //all list loops need this:
            curDP = curDP->NextDataPoint;
        }
        curVoice = curVoice->NextVoice;
    }

    SG_DBGOUT_INT("Copy: DataPoints to copy:", DP_count);
    SG_DBGOUT_INT("Copy: Total Voices:", voice_count);
    SG_DBGOUT_INT("Copy: Voices used by selected DPs:", voices_used_count);

    //if there are no selected DPs, don't do anything:
    if (DP_count < 1) {
        return;
    }

    //erase backup structure to prepare for new data:
    SG_CleanupBackupData(Bdata);

    //Now get scaling vars required to correctly paste data regardless of how
    //drawingarea dimensions may have changed:
    //Calling SG_GetScheduleLimits() just to be safe (it fills the DurHz vars below):
    SG_GetScheduleLimits();
    Bdata->OrigSG_TotalScheduleDuration = SG_TotalScheduleDuration;
    // Bdata->OrigSG_MaxScheduleBeatfrequency = SG_MaxScheduleBeatfrequency;
    Bdata->OrigWidth = widget->allocation.width;
    Bdata->OrigHeight = widget->allocation.height;
    Bdata->TotalDataPoints = DP_count;
    // Bdata->TotalVoices = voice_count;//not sure why this was here; commented 20070630
    Bdata->TotalVoices = voices_used_count;

    SG_AllocateBackupData(Bdata, Bdata->TotalVoices, Bdata->TotalDataPoints);

    // Now put all the selected DPs in to the copy buffer:
    DP_count = 0;
    voice_count = 0;
    voices_used_count = -1;
    curVoice = SG_FirstVoice;
    while (curVoice != NULL) {
        curDP = curVoice->FirstDataPoint;
        ++voice_count;
        newvoiceflag = TRUE;
        while (curDP != NULL) {
            if ((selected_only == FALSE) || (curDP->state == SG_SELECTED)) {
                memcpy(&(Bdata->DPdata[DP_count]), curDP, sizeof (SG_DataPoint));
                ++DP_count;
                if (newvoiceflag == TRUE) {
                    ++voices_used_count; //NOTE: I think the array count is wrong here; one-ahead of where it should be
                    newvoiceflag = FALSE;
                    Bdata->Voice[voices_used_count].type =
                            ((SG_Voice *) curDP->parent)->type;
                    Bdata->Voice[voices_used_count].ID = ((SG_Voice *) curDP->parent)->ID;
                    Bdata->Voice[voices_used_count].state =
                            ((SG_Voice *) curDP->parent)->state;
                    Bdata->Voice[voices_used_count].hide =
                            ((SG_Voice *) curDP->parent)->hide;
                    Bdata->Voice[voices_used_count].mute =
                            ((SG_Voice *) curDP->parent)->mute;
                    Bdata->Voice[voices_used_count].mono = ((SG_Voice *) curDP->parent)->mono; //[added 20100614]
                    //now deal with the annoying Voice Description:
                    SG_StringAllocateAndCopy(&(Bdata->Voice[voices_used_count].description),
                            ((SG_Voice *) curDP->parent)->description);
                    SG_DBGOUT_STR("SG_CopyDataPoints copied string:",
                            Bdata->Voice[voices_used_count].description);
                }
            }
            //all list loops need this:
            curDP = curDP->NextDataPoint;
        }
        curVoice = curVoice->NextVoice;
    }
    SG_DBGOUT_INT("Copied DataPoints:", Bdata->TotalDataPoints);
} //END SG_CopySelectedDataPoints()

/////////////////////////////////////////////////////
//20060412
//Pastes any previously selected points copied with SG_CopyDataPoints().
//BUG: Unfortunately, Paste requires each DP's knowledge of previous parent SG_Voice.
//So if parent SG_Voice gets erased previous to a Paste, some horrible things will happen

void SG_PasteDataPoints(GtkWidget * widget, SG_BackupData * Bdata,
        gboolean predeselect) {
    if (Bdata->DPdata == NULL || Bdata->TotalDataPoints < 1) {
        return;
    }

    SG_ProgressIndicatorFlag = FALSE;

    //Not sure what user would want:
    if (predeselect == TRUE) {
        SG_DeselectDataPoints();
    }

    double xscale = widget->allocation.width / (double) Bdata->OrigWidth;

    //  double yscale = widget->allocation.height / (double)Bdata->OrigHeight;
    int count;
    SG_DataPoint *curDP;

    //NOTE 20060711: since being able to change graph views, it now has to be assumed necessary to
    //insert each data point as if the original XY context for the points is no gone. This means
    //I can let InsertNewDatapointXY() do all the "current graph view" sorting:

    //Easiest thing to do would be just to paste the mess with the first point at x=0, but it would be
    //really nice to try and paste it "where it was", time-wise, in case user really just changed a few details
    //Scale all x's appropriately and calculate appropriate durations:
    xscale *= Bdata->OrigSG_TotalScheduleDuration / SG_TotalScheduleDuration;
    SG_DBGOUT_FLT("Paste context \%", xscale);
    SG_Voice *curVoice;
    int voices_used_count = -1;
    void *placerpointer = NULL;

    for (count = 0; count < Bdata->TotalDataPoints; count++) {
        //this was hacked to make pasting independent of pointer addresses:
        curVoice = SG_FirstVoice;
        //see if this is a new (as in not yet seen) voice:
        if (Bdata->DPdata[count].parent != placerpointer) {
            placerpointer = Bdata->DPdata[count].parent;
            ++voices_used_count;
            //these aren't necessary because user may not be restoring, just pasting a few points:
            //   curVoice->type = Bdata->Voice[voices_used_count].type;
            //   curVoice->state = Bdata->Voice[voices_used_count].state;
            //   curVoice->hide = Bdata->Voice[voices_used_count].hide;
            //   curVoice->mute = Bdata->Voice[voices_used_count].mute;
            //   curVoice->mono = Bdata->Voice[voices_used_count].mono; 
            SG_DBGOUT_PNT("Paste: DP changing Voices", placerpointer);
        }
        while (curVoice != NULL &&
                curVoice->ID != Bdata->Voice[voices_used_count].ID) {
            curVoice = curVoice->NextVoice;
        }
        if (curVoice != NULL) {
            //old way (Still seeing if new one works in all cases)
            curDP = SG_InsertNewDataPointXY(widget, curVoice, Bdata->DPdata[count].x * xscale, 0); //NOTE: I MANUALLY SET THIS
            //now set the stuff that SG_InsertNewDataPointXY() doesn't:
            curDP->beatfreq = Bdata->DPdata[count].beatfreq;
            curDP->volume_left = Bdata->DPdata[count].volume_left;
            curDP->volume_right = Bdata->DPdata[count].volume_right;
            curDP->basefreq = Bdata->DPdata[count].basefreq;
            curDP->state = Bdata->DPdata[count].state; //state should always be SG_SELECTED, correct?
            if (curDP->x > widget->allocation.width) { //deal with x's beyond current graph width:
                SG_DBGOUT_FLT("Paste: Went past right-edge; rescaling", curDP->x);
                curDP->duration = Bdata->DPdata[count].duration;
            }
            //end old way

            /*
               //alternate way (20060511): TODO: This is a brute force approach, written because I'm not convinced that the old approach above handles last DPs right
               double newx = Bdata->DPdata[count].x * xscale;
               if (newx >= widget->allocation.width) {//deal with x's beyond current graph width:
               SG_AddNewDataPointToEnd(curVoice->FirstDataPoint, 
               curVoice, 
               Bdata->DPdata[count].duration, 
               Bdata->DPdata[count].beatfreq, 
               Bdata->DPdata[count].volume, 
               Bdata->DPdata[count].basefreq, 
               Bdata->DPdata[count].state); //state should always be SG_SELECTED, correct?
               } else {
               curDP = SG_InsertNewDataPointXY(widget, 
               curVoice,
               newx,
               0); //NOTE: I manually set this via SG_ConvertDataToXY()
               //now set the stuff that SG_InsertNewDataPointXY() doesn't:
               curDP->beatfreq = Bdata->DPdata[count].beatfreq;
               curDP->volume = Bdata->DPdata[count].volume;
               curDP->basefreq = Bdata->DPdata[count].basefreq;
               curDP->state = Bdata->DPdata[count].state; //state should always be SG_SELECTED, correct?
               }
               //end alternate way (20060511)
             */
        }
    }

    //20060421: I've observed that it is wise to NOT call this function unless absolutely
    //necessary; it can introduce subtle but insidiously accumulating rounding errors:
    SG_ConvertDataToXY(widget);

    SG_DBGOUT_INT("Pasted Points:", count);

    //draw it:
    SG_DrawGraph(widget);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
} //END SG_PasteSelectedDataPoints()

/////////////////////////////////////////////////////
//this offers a means for the user to cull invisible duplicate DPs.
//NOTE: This just never really worked very well; I guess rounding errors in
//floats are just too insidious. It is better to let user use a proximity
//algorithm when they might want to clean up.
//20070731: It had to be updated to account for multiple graph views

void SG_DeleteDuplicateDataPoints(GtkWidget * widget) {
    int count = 0;
    SG_DataPoint *curDP;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        curDP = curVoice->FirstDataPoint;
        if (curDP != NULL)
            while (curDP->NextDataPoint != NULL) {
                if (curDP->x == curDP->NextDataPoint->x &&
                        curDP->y == curDP->NextDataPoint->y &&
                        curDP->duration == curDP->NextDataPoint->duration &&
                        curDP->volume_left == curDP->NextDataPoint->volume_left &&
                        curDP->volume_right == curDP->NextDataPoint->volume_right &&
                        curDP->basefreq == curDP->NextDataPoint->basefreq &&
                        curDP->beatfreq == curDP->NextDataPoint->beatfreq) {
                    SG_DeleteDataPoint(curDP->NextDataPoint, FALSE);
                    ++count;
                } else
                    curDP = curDP->NextDataPoint;
            }
        curVoice = curVoice->NextVoice;
    }

    if (count > 0) {
        SG_DBGOUT_INT("Duplicates deleted:", count);
        SG_DataHasChanged = SG_GraphHasChanged = TRUE;
    }
}

/////////////////////////////////////////////////////

gboolean SG_MessageBox(char *question) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("A question for you...",
            NULL,
            (GtkDialogFlags)
            (GTK_DIALOG_MODAL |
            GTK_DIALOG_DESTROY_WITH_PARENT),
            GTK_STOCK_OK,
            GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL,
            GTK_RESPONSE_REJECT,
            NULL);

    //add some stuff:
    GtkWidget *label_question = gtk_label_new(question);

    // Add the label, and show everything we've added to the dialog.
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),
            label_question);

    gtk_widget_show_all(dialog);

    //block until I get a response:
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));

    switch (result) {
        case GTK_RESPONSE_ACCEPT:
        {
            result = TRUE;
            break;
        }

        default:
            result = FALSE;
            break;
    }
    gtk_widget_destroy(dialog);
    return result;
}

/////////////////////////////////////////////////////

void SG_CleanupVoice(SG_Voice * curVoice) {
    if (curVoice == NULL) {
        return;
    }
    if (curVoice->description != NULL) {
        SG_DBGOUT_STR("Cleaning Voice Description:", curVoice->description);
        free(curVoice->description);
        curVoice->description = NULL;
    }
    SG_CleanupDataPoints(curVoice->FirstDataPoint);
    //curVoice->FirstDataPoint=NULL; //Normally, you must do this!
    free(curVoice);
}

/////////////////////////////////////////////////////
//this is a bit of a scary function, but necessary for
//user to remove a voice from their Schedule. Scary
//for a lot of reasons that could introduce bugs, like:
// - gaps in SG_Voice ID count
// - unpredictable with Copy/Paste
//A workaround (if this does prove to have real bugs)
//might be to copy everything to a SG_BackupData,
//then do SG_RestoreDataPoints (restore completely from scratch), but
//simply skipping the designated "deleted" voice.

void SG_DeleteVoice(SG_Voice * curVoice) {
    if (curVoice == NULL) {
        return;
    }
    SG_Voice *prevVoice = curVoice->PrevVoice;
    SG_Voice *nextVoice = curVoice->NextVoice;

    if (prevVoice == NULL && nextVoice == NULL) {
        ERROUT("Deleting only voice not permitted");
        return;
    }

    //Make sure user knows what they're doing:
    // if (FALSE == SG_MessageBox ("\nDo you want to delete this voice?\n")) { return; }

    SG_DBGOUT_INT("Deleting SG_Voice ", curVoice->ID);

    SG_CleanupVoice(curVoice);

    if (prevVoice != NULL) {
        prevVoice->NextVoice = nextVoice;
        //Check to see if we now have a new SG_FirstVoice:
    } else {
        SG_FirstVoice = nextVoice;
        SG_DBGOUT_PNT("New SG_FirstVoice", SG_FirstVoice);
    }

    if (nextVoice != NULL) {
        nextVoice->PrevVoice = prevVoice;
    }
    //NOTE: I might eventually need to do the following if bugs are evident.
    //Pasting with a missing voice is not understood, could be ugly until I take the
    //time to have Paste create a new voice if it can't find existing one.
    // SG_CleanupBackupData(&SG_CopyPaste);
    // SG_CleanupBackupData(&SG_UndoRedo);

    // SG_DrawGraph(widget); //decided not to do this here because user needs to find new SG_SELECTED voice
    SG_GraphHasChanged = SG_DataHasChanged = TRUE; //added 20070731
}

/////////////////////////////////////////////////////
//User must reset SG_First*SG_Voice manually after calling this!
//like SG_FirstVoice=NULL, SG_FirstBakVoice=NULL, etc.

void SG_CleanupVoices(SG_Voice * FirstVoice) {
    if (FirstVoice == NULL) {
        return;
    }
    SG_Voice *curVoice = FirstVoice;
    SG_Voice *nextVoice;
    unsigned int count = 0;

    while (curVoice->NextVoice != NULL) {
        nextVoice = curVoice->NextVoice;
        SG_CleanupVoice(curVoice);
        curVoice = nextVoice;
        ++count;
    }

    //still need to get the last one:
    if (curVoice != NULL) {
        //a20070621: was manually cleaning here instead of calling SG_CleanupVoice,
        //  SG_DBGOUT("Cleaning last voice:");
        SG_CleanupVoice(curVoice);
        ++count;
    }
    //User must NULL their FirstVoice after calling this!!!! i.e., "FirstVoice=NULL;"
}

/////////////////////////////////////////////////////
//20060326: this was made to be able to clean any linked list of
//data points (so both main and backup sets can use this func)
//NOTE: making this general meant that it is now up to the calling
//function to NULL FirstDataPoint sent here after done here!

void SG_CleanupDataPoints(SG_DataPoint * curDP) {
    if (curDP == NULL) {
        return;
    }
    SG_DataPoint *nextDP;
    unsigned int count = 0;

    while (curDP->NextDataPoint != NULL) {
        //  SG_DBGOUT_INT("Cleanup DP", count);
        nextDP = curDP->NextDataPoint;
        free(curDP);
        curDP = nextDP;
        ++count;
    }

    //still need to get the last one:
    if (curDP != NULL) {
        //   SG_DBGOUT_INT("Cleanup Last DP", count);
        free(curDP);
        ++count;
    }

    SG_DBGOUT_INT("DataPoints Deleted:", count);
    //NOTE: caller must now be sure to NULL the FirstDataPoint sent here!
}

/////////////////////////////////////////////////////
//this fills global variables SG_MaxScheduleBeatfrequency
//and SG_TotalScheduleDuration. It can be
//called without a widget, making it a bit more general than
//related workhorse, SG_ConvertDataToXY()

inline void SG_GetScheduleLimits() {
    //SG_DBGOUT_INT("Got to SG_GetScheduleLimits\n",0);
    SG_TotalScheduleDuration = 0;
    SG_MaxScheduleBeatfrequency = 0;
    SG_MaxScheduleBasefrequency = 0;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        SG_DataPoint *curDP = curVoice->FirstDataPoint;
        double totalscheduleduration = 0;

        do {
            totalscheduleduration += curDP->duration;
            if (FALSE == curVoice->hide) //20110519
            {
                if (SG_MaxScheduleBeatfrequency < curDP->beatfreq)
                    SG_MaxScheduleBeatfrequency = curDP->beatfreq;
                if (SG_MaxScheduleBasefrequency < curDP->basefreq)
                    SG_MaxScheduleBasefrequency = curDP->basefreq;
            }
            curDP = curDP->NextDataPoint;
        } while (curDP != NULL);
        if (totalscheduleduration > SG_TotalScheduleDuration)
            SG_TotalScheduleDuration = totalscheduleduration;
        curVoice = curVoice->NextVoice;
    }

    //hacks to make sure we don't have divide-by-zero issues:
    if (SG_TotalScheduleDuration < .001)
        SG_TotalScheduleDuration = .001;
    if (SG_MaxScheduleBeatfrequency < .001)
        SG_MaxScheduleBeatfrequency = .001;
    if (SG_MaxScheduleBasefrequency < .001)
        SG_MaxScheduleBasefrequency = .001;
    SG_GraphHasChanged = TRUE; //20110519
}

/*
   /////////////////////////////////////////////////////
   void SG_ConvertDataToXY(GtkWidget *widget) {

   SG_GetScheduleLimits();

   switch (SG_GraphType) {
   case SG_GRAPHTYPE_BEATFREQ:
   SG_ConvertDataToXY(widget);
   break;

   case SG_GRAPHTYPE_BASEFREQ:
   SG_ConvertDataToXY(widget);
   break;

   case SG_GRAPHTYPE_VOLUME:
   SG_ConvertDataToXY(widget);
   break;

   case SG_GRAPHTYPE_VOLUME_BALANCE:
   SG_ConvertDataToXY(widget);
   break;

   default:
   SG_GraphType = SG_GRAPHTYPE_BEATFREQ;
   break;

   }
   }
 */

void SG_TestDataPointGraphLimits(GtkWidget * widget, SG_DataPoint * curDP) {
    if (curDP->y > widget->allocation.height) {
        curDP->y = widget->allocation.height;
    } else if (curDP->y < 0) {
        curDP->y = 0;
    }
    if (curDP->x > widget->allocation.width) {
        //this one isn't necessary; it is legal to go beyond end
        // curDP->x = widget->allocation.width; 
    } else if (curDP->x < 0) {
        curDP->x = 0;
    }
}

/////////////////////////////////////////////////////
//recalculates durations for SELECTED datapoints according to their x position on the graph

void SG_ConvertYToData_SelectedPoints(GtkWidget * widget) {
    //double y_scaling_factor=widget->allocation.height/SG_MaxScheduleBeatfrequency;
    // SG_CurrentDataPoint->duration=SG_CurrentDataPoint->x_interval/x_scaling_factor;
    //First the easy one: convert SG_CurrentDataPoint's y to Hz value:
    //SG_CurrentDataPoint->beatfreq = ((widget->allocation.height-SG_CurrentDataPoint->y)*SG_MaxScheduleBeatfrequency)/widget->allocation.height;
    double unit_per_pixel;

    switch (SG_GraphType) {
        case SG_GRAPHTYPE_BEATFREQ:
            unit_per_pixel = SG_MaxScheduleBeatfrequency / widget->allocation.height;
            break;

        case SG_GRAPHTYPE_BASEFREQ:
            unit_per_pixel = SG_MaxScheduleBasefrequency / widget->allocation.height;
            break;

        case SG_GRAPHTYPE_VOLUME:
            unit_per_pixel = 1.0 / widget->allocation.height;
            break;

        case SG_GRAPHTYPE_VOLUME_BALANCE:
            unit_per_pixel = 1.0 / widget->allocation.height;
            break;

        default:
            SG_GraphType = SG_GRAPHTYPE_BEATFREQ;
            unit_per_pixel = 0.001;
            break;
    }

    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        SG_DataPoint *curDP = curVoice->FirstDataPoint;

        do {
            if (curDP->state == SG_SELECTED) {
                switch (SG_GraphType) {
                    case SG_GRAPHTYPE_BEATFREQ:
                        curDP->beatfreq =
                                (widget->allocation.height - curDP->y) * unit_per_pixel;
                        break;

                    case SG_GRAPHTYPE_BASEFREQ:
                        curDP->basefreq =
                                (widget->allocation.height - curDP->y) * unit_per_pixel;
                        break;

                    case SG_GRAPHTYPE_VOLUME:
                        SG_TestDataPointGraphLimits(widget, curDP); //to make sure Y is within limits
                        //idea here: changing volume sets absolute level of higher of the two channels, while
                        //the lower gets set to a new value according to the original proportion between the two.
                        if (curDP->volume_left > curDP->volume_right) {
                            double ratio = curDP->volume_right / curDP->volume_left;

                            curDP->volume_left =
                                    ((widget->allocation.height - curDP->y) * unit_per_pixel);
                            curDP->volume_right = curDP->volume_left * ratio;
                        } else if (curDP->volume_left < curDP->volume_right) {
                            double ratio = curDP->volume_left / curDP->volume_right;

                            curDP->volume_right =
                                    ((widget->allocation.height - curDP->y) * unit_per_pixel);
                            curDP->volume_left = curDP->volume_right * ratio; //20070728 FIXED STUPID BUG (volume_right was volume_left)
                        } else {
                            curDP->volume_left = curDP->volume_right =
                                    ((widget->allocation.height - curDP->y) * unit_per_pixel);
                        }
                        break;

                    case SG_GRAPHTYPE_VOLUME_BALANCE:
                        SG_TestDataPointGraphLimits(widget, curDP); //to make sure Y is within limits
                        //###################### BUG HERE ###########################
                        //idea here is to only decrease one side of the Volume pair depending on
                        //whether the DP is above ("left") or below ("right") centerline, in which
                        //case it becomes a proportion of the side that doesn't get changed. the only
                        //tricky part is that if the user crosses the midpoint of the graph, this would
                        //start to deplete the larger valued channel as a proportion of the smaller --
                        //meaning that it would change (reduce) the overall volume. Thus, I must first check to
                        //see if I am changing the larger -- and if so, give it's value to the formerly
                        //smaller channell... yikes.
                    {
                        //Totally hacky solution to problem of keeping a valid ratio when both vars == 0:
                        if (curDP->volume_right == 0 && curDP->volume_left == 0) {
                            curDP->volume_right = curDP->volume_left = 1e-16;
                        }
                        double tmpval =
                                ((widget->allocation.height - curDP->y) * unit_per_pixel) - .5;
                        if (tmpval > 0) {
                            //this means I am changing "right" channel (because "left" is bigger):
                            //first see if right channel WAS the bigger (because if it is, I need to give left channel it's value!):
                            if (curDP->volume_right > curDP->volume_left) {
                                curDP->volume_left = curDP->volume_right;
                                //DBGOUT("Crossed Midline to Left");
                            }
                            curDP->volume_right = (1.0 - (tmpval / .5)) * curDP->volume_left;
                        } else { //this handles right > left, and also right == left:
                            if (curDP->volume_right < curDP->volume_left) {
                                curDP->volume_right = curDP->volume_left;
                                //DBGOUT("Crossed Midline to Right");
                            }
                            curDP->volume_left = (1.0 - (-tmpval / .5)) * curDP->volume_right;
                        }
                    }
                        break;
                }
            }
            curDP = curDP->NextDataPoint;
        } while (curDP != NULL);
        curVoice = curVoice->NextVoice;
    }
}

/////////////////////////////////////////////////////
//NOTE: The following unified conversion functions turn out to be
//about the most critical code to the entire functionality of
//ScheduleGUI, being called virtually every time anything a graph view is presented
//or anything is done to the points. It is worth really studying this one to see
//where/if they introduce subtle but accumulating rounding-errors or graphpoint/datapoint
//resolution errors (since for many operations, a SG_DataPoint's x determined here
//gets used to recalibrate that SG_DataPoint's duration).
////////
//This function first determines global duration and beatfreq limits, then uses
//that info in combination with drawingarea dimensions to give exact XY
//placement for DataPoints. Used after a drawingarea resize, or any time
//a change in graphical context invalidates datapoints' xy's
/////////////////////////////////////////////////////

void SG_ConvertDataToXY(GtkWidget * widget) {
    SG_GetScheduleLimits();

    double x_scaling_factor =
            widget->allocation.width / SG_TotalScheduleDuration;
    double y_scaling_factor;

    switch (SG_GraphType) {
        case SG_GRAPHTYPE_BEATFREQ:
            y_scaling_factor = widget->allocation.height / SG_MaxScheduleBeatfrequency;
            break;

        case SG_GRAPHTYPE_BASEFREQ:
            y_scaling_factor = widget->allocation.height / SG_MaxScheduleBasefrequency;
            break;

        case SG_GRAPHTYPE_VOLUME:
            y_scaling_factor = widget->allocation.height;
            break;

        case SG_GRAPHTYPE_VOLUME_BALANCE:
            y_scaling_factor = .5 * widget->allocation.height;
            break;

        default:
            y_scaling_factor = 0;
            SG_GraphType = SG_GRAPHTYPE_BEATFREQ;
            break;
    }

    //Now lays all the points along a running sum along the x axis proportional to the durations:
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        SG_DataPoint *curDP = curVoice->FirstDataPoint;
        double currentx = 0;

        do {
            curDP->x = currentx;
            currentx += curDP->duration * x_scaling_factor;

            switch (SG_GraphType) {
                case SG_GRAPHTYPE_BEATFREQ:
                    curDP->y =
                            widget->allocation.height - (curDP->beatfreq * y_scaling_factor);
                    break;

                case SG_GRAPHTYPE_BASEFREQ:
                    curDP->y =
                            widget->allocation.height - (curDP->basefreq * y_scaling_factor);
                    break;

                case SG_GRAPHTYPE_VOLUME:
                    //philosophy here: Y is proportional to the higher of the two channels:
                {
                    double higher =
                            (curDP->volume_left >
                            curDP->volume_right) ? curDP->volume_left : curDP->volume_right;
                    curDP->y = widget->allocation.height - (higher * y_scaling_factor);
                }
                    break;

                case SG_GRAPHTYPE_VOLUME_BALANCE:
                    //philosophy here: Y is determined by the proportion between the lower
                    //to the higher of the two channels. Confusingly, if the DP is above half-graph,
                    //it means that the right channel is the lower than right; and vice-versa.
                {
                    if (curDP->volume_left > curDP->volume_right) { // see if left is bigger
                        curDP->y = y_scaling_factor * curDP->volume_right / curDP->volume_left;
                        // SG_DBGOUT_FLT("Left Bigger:", (curDP->volume_right / curDP->volume_left));
                    } else if (curDP->volume_left < curDP->volume_right) { // see if right is bigger:
                        curDP->y =
                                y_scaling_factor * (2 - (curDP->volume_left / curDP->volume_right));
                        // SG_DBGOUT_FLT("Right Bigger:", (curDP->volume_left / curDP->volume_right));
                    } else
                        //logically, they must be equal -- but I don't know if volume was zero:
                    {
                        curDP->y = y_scaling_factor;
                    }
                }
                    break;

                default:
                    curDP->y = 0;
                    SG_GraphType = SG_GRAPHTYPE_BEATFREQ;
                    break;
            }

            curDP = curDP->NextDataPoint;
        } while (curDP != NULL);
        curVoice = curVoice->NextVoice;
    }
    SG_GraphHasChanged = TRUE; //added 20070731
}

/////////////////////////////////////////////////////
//disregards voice visibility

void SG_DeselectDataPoints() {
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        SG_DataPoint *curDP = curVoice->FirstDataPoint;

        do {
            curDP->state = SG_UNSELECTED;
            //all list loops need this:
            curDP = curDP->NextDataPoint;
        } while (curDP != NULL);
        curVoice = curVoice->NextVoice;
    }
    SG_GraphHasChanged = TRUE;
}

/////////////////////////////////////////////////////
//currently ONLY selects VISIBLE voices

void SG_SelectDataPoints_All(gboolean select) {
    if (select == FALSE) {
        SG_DeselectDataPoints();
    }
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            SG_DataPoint *curDP = curVoice->FirstDataPoint;

            do {
                curDP->state = SG_SELECTED;
                //all list loops need this:
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        }
        curVoice = curVoice->NextVoice;
    }
    SG_GraphHasChanged = TRUE;
}

/////////////////////////////////////////////////////
//if select is TRUE, selects all points in given 
//voice; otherwise deselects

void SG_SelectDataPoints_Voice(SG_Voice * curVoice, gboolean select) {
    if (curVoice == NULL) {
        return;
    }
    SG_DataPoint *curDP = curVoice->FirstDataPoint;

    while (curDP != NULL) {
        curDP->state = ((select == TRUE) ? SG_SELECTED : SG_UNSELECTED);
        //all list loops need this:
        curDP = curDP->NextDataPoint;
    }
    SG_GraphHasChanged = TRUE;
}

/////////////////////////////////////////////////////
//if next is TRUE, selects neighbors to the right; FALSE means left.
//if deselect=TRUE, it deselects the original DataPoints when selecting their neighbors

void SG_SelectNeighboringDataPoints(gboolean next, gboolean deselect) {
    SG_DataPoint *curDP;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            curDP = curVoice->FirstDataPoint;

            if (next == FALSE) {
                do {
                    if (curDP->state == SG_SELECTED && curDP->PrevDataPoint != NULL) {
                        curDP->PrevDataPoint->state = SG_SELECTED;
                    }
                    if (deselect == TRUE)
                        curDP->state = SG_UNSELECTED;
                    //all list loops need this:
                    curDP = curDP->NextDataPoint;
                } while (curDP != NULL);
            } else {
                //first find end:
                while (curDP->NextDataPoint != NULL)
                    curDP = curDP->NextDataPoint;
                do {
                    if (curDP->state == SG_SELECTED && curDP->NextDataPoint != NULL) {
                        curDP->NextDataPoint->state = SG_SELECTED;
                    }
                    if (deselect == TRUE)
                        curDP->state = SG_UNSELECTED;
                    //all list loops need this:  curVoice=curVoice->NextVoice;
                    curDP = curDP->PrevDataPoint;
                } while (curDP != NULL);
            }
        }
        curVoice = curVoice->NextVoice;
    }
}

/////////////////////////////////////////////////////
// if DeleteTime==FALSE (default) this function adds time from deleted point
//to the previous point. TRUE deletes that time, making it essential
//to call SG_GetScheduleLimits() at some point before
//rendering, etc. (advisable anyway).

void SG_DeleteDataPoint(SG_DataPoint * curDP, gboolean DeleteTime) {
    if (curDP == NULL) {
        return;
    }

    //to be sure we don't end up trying to delete the ONLY point:
    if (curDP->NextDataPoint == NULL && curDP->PrevDataPoint == NULL) {
        return;
    }

    SG_DataPoint *prevDP = curDP->PrevDataPoint;

    //first check to see if SG_FirstDataPoint is being deleted:
    if (prevDP == NULL) {
        //  SG_FirstDataPoint=SG_FirstDataPoint->NextDataPoint;
        ((SG_Voice *) curDP->parent)->FirstDataPoint = curDP->NextDataPoint;
        ((SG_Voice *) curDP->parent)->FirstDataPoint->PrevDataPoint = NULL;
    } else {
        //sum curDP's duration in to previous DataPoints:
        if (DeleteTime == FALSE)
            prevDP->duration += curDP->duration;
        //Make previous SG_DataPoint now link to the next SG_DataPoint after the one to be deleted:
        prevDP->NextDataPoint = curDP->NextDataPoint;
        if (curDP->NextDataPoint != NULL) //i.e., if this point is not the last SG_DataPoint in the llist:
        {
            //Make NextDataPoint now call the prevDataPoint PrevDataPoint:
            curDP->NextDataPoint->PrevDataPoint = prevDP;
        }
    }
    //Assuming I've linked everything remaining properly, can now free the memory:
    free(curDP);
    //recalibrate everything: [TOO SLOW! Let calling function do this, in case it is calling 8000 points]
    // SG_ConvertDataToXY(widget);
    // SG_DrawGraph(widget);
}

/////////////////////////////////////////////////////
//this can delete all except FIRST datapoint.

void SG_DeleteDataPoints(GtkWidget * widget, gboolean DeleteTime,
        gboolean selected_only) {
    SG_ProgressIndicatorFlag = FALSE;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        //NOTE: I start with 2nd DP just in case user is deleting all DPs -- in which case
        //it would be useful to have the last (undeletable) DP be the original First DP.
        SG_DataPoint *curDP = curVoice->FirstDataPoint->NextDataPoint;

        SG_DataPoint *tmpDP;

        while (curDP != NULL) {
            if (selected_only == FALSE ||
                    (selected_only == TRUE && curDP->state == SG_SELECTED)) {
                //since I'm about to obliterate it, I need to advance now:
                tmpDP = curDP;
                curDP = curDP->NextDataPoint;
                SG_DeleteDataPoint(tmpDP, DeleteTime);
            } else
                curDP = curDP->NextDataPoint;
        }
        //get last one:
        if (selected_only == FALSE ||
                (selected_only == TRUE
                && curVoice->FirstDataPoint->state == SG_SELECTED)) {
            SG_DeleteDataPoint(curVoice->FirstDataPoint, DeleteTime);
        }
        //advance to next voice:
        curVoice = curVoice->NextVoice;
    }

    SG_ConvertDataToXY(widget);
    SG_DrawGraph(widget);
    //SG_DBGOUT_INT("after SG_ShiftFlag",SG_ShiftFlag);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

/////////////////////////////////////////////////////
//if select is TRUE, selects datapoints in visible voices 
//within a rectangular region of graph; otherwise deselects

void SG_SelectDataPoints(int startX, int startY, int endX, int endY,
        gboolean select) {
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            SG_DataPoint *curDP = curVoice->FirstDataPoint;

            do {
                if (curDP->x > startX && curDP->x < endX &&
                        curDP->y > startY && curDP->y < endY) {
                    if (select == TRUE)
                        curDP->state = SG_SELECTED;
                    else
                        curDP->state = SG_UNSELECTED;
                } else {
                    //No need to deselect points, because the initial button_press_event handler
                    //will have already deselected them if SHIFT isn't pressed.
                    //curDP->state=SG_UNSELECTED;
                }
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        } //end "is voice visible?"
        curVoice = curVoice->NextVoice;
    }
    SG_GraphHasChanged = TRUE;
}

/*
   /////////////////////////////////////////////////////
   gboolean SG_key_release_event (GtkWidget  *widget, GdkEventKey  *event)
   {
   //if (event->state & GDK_CONTROL_MASK)  SG_DBGOUT_INT("KeyRelease",event->keyval);//g_print ("The control key is pressed\n");
   // SG_DBGOUT_INT("KeyRelease",event->keyval);
   // SG_ShiftFlag=(event->state & GDK_SHIFT_MASK);
   // SG_DBGOUT_INT("Shiftflag release:",event->state & GDK_SHIFT_MASK);
   return FALSE;
   }
 */

/////////////////////////////////////////////////////

void SG_MoveSelectedDataPoints(GtkWidget * widget, double moveX,
        double moveY) {
    //these two vars deal with out-of-bounds points:
    double YFlagOffset;
    double XFlagOffset;
    SG_Voice *curVoice;
    SG_DataPoint *curDP = NULL;
    SG_DataPoint *tempDP;
    double tmpmoveX = moveX;
    double tmpmoveY = moveY;
    gboolean swapflag;

    //Theory: I do two passes through the points if any point in the cluster go out of bounds, using
    //whichever point went "most out of bounds" as the reference to "re-move" things back within
    //bounds on the second pass.

    //First go through all selected points and increment them according to moveX and moveY,
    //keeping clusters from moving past left and bottom edges, but free of top and right:
    while (1) { //START out-of-bounds feedback loop. Point of this is to determine max inbounds move
        YFlagOffset = 0.0;
        XFlagOffset = 0.0;
        curVoice = SG_FirstVoice;
        while (curVoice != NULL) {
            curDP = curVoice->FirstDataPoint; //there is always a firstDP, correct?
            do {
                if (curDP->state == SG_SELECTED) {
                    //first do Y:
                    curDP->y += tmpmoveY;
                    //Theory here: if DP's new Y is less than zero (i.e., above the graph),
                    //see if it is currently "the most below zero." if ultimately (after checking all) it is, it becomes
                    //the reference (limit) for moving all the DPs in the cluster on the required second pass through here:
                    if (curDP->y < 0.0 && curDP->y < YFlagOffset) {
                        YFlagOffset = curDP->y;
                    }                        //now do similarly in checking if DP is below the graph:
                    else if (curDP->y > widget->allocation.height &&
                            (curDP->y - widget->allocation.height) > YFlagOffset) {
                        YFlagOffset = curDP->y - widget->allocation.height;
                    }
                    //now do X:
                    curDP->x += tmpmoveX;
                    //be sure first DP hasn't been moved (I am assuming here that after a Ctrl-A, user wants to move everything BUT first DP):
                    if (curDP->PrevDataPoint == NULL) {
                        curDP->x = 0.0;
                    } else if (curDP->x < 0.0 && curDP->x < XFlagOffset) {
                        XFlagOffset = curDP->x;
                    } else if (curDP->x > widget->allocation.width &&
                            (curDP->x - widget->allocation.width) > XFlagOffset) {
                        XFlagOffset = curDP->x - widget->allocation.width;
                    }
                }
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
            //advance to next voice:
            curVoice = curVoice->NextVoice;
        }

        //Now with that info, use FlagOffsets to do math to re-move clusters to be back within bounds:
        //First see if x has gone past left edge of graph, and if so act accordingly:
        if (XFlagOffset < 0.0) {
            tmpmoveX = -XFlagOffset;
            moveX += tmpmoveX;
        } else
            tmpmoveX = 0.0; //this gives green-light for user to pull points past right-edge

        //Now do Y:
        //First deal with graph views in which DPs can be pulled above graph:
        if (SG_GraphType == SG_GRAPHTYPE_BASEFREQ ||
                SG_GraphType == SG_GRAPHTYPE_BEATFREQ) {
            //check if Y has gone past bottom:
            if (YFlagOffset > 0.0) {
                tmpmoveY = -YFlagOffset;
                moveY += tmpmoveY;
            } else
                tmpmoveY = 0.0; //this gives the green-light for user to move above graph
            //see if we're done here:
            if (XFlagOffset >= 0.0 && YFlagOffset <= 0.0)
                break;
        } else
            //deal with Volume graph views, where user can't go above OR below graph:
        {
            if (YFlagOffset != 0.0) {
                tmpmoveY = -YFlagOffset;
                moveY += tmpmoveY;
            }
            if (XFlagOffset >= 0.0 && YFlagOffset == 0.0)
                break;
        }
    } //END out-of-bounds feedback loop

    // ======

    //Now see if the new X passed a neighbor, or went beyond a boundary:
    curVoice = SG_FirstVoice;
    while (curVoice != NULL) { //START of Voices
        //assume that there is always a firstDP, and that it can't move:
        curDP = curVoice->FirstDataPoint->NextDataPoint;
        while (curDP != NULL) {
            if (curDP->state == SG_SELECTED) {
                //Negative moveX means test if point moved past an unselected neighbor to the left:
                if (moveX < 0)
                    do {
                        //First try to find an unselected neighbor:
                        tempDP = curDP;
                        swapflag = FALSE;
                        do {
                            tempDP = tempDP->PrevDataPoint;
                            if (tempDP->PrevDataPoint == NULL)
                                goto SkipToNext; //first DP, therefore no unselected neighbor to switch with
                        } while (tempDP->state == SG_SELECTED);
                        if (curDP->x < tempDP->x) {
                            SG_InsertLink(curDP, tempDP);
                            swapflag = TRUE;
                        }
                    } while (swapflag == TRUE);

                    //Positive moveX means test if point moved past an unselected neighbor to the right:
                else if (moveX > 0 && curDP->NextDataPoint != NULL)
                    do { //remember, last DP can't swap with anything to its right)
                        //First try to find an unselected neighbor:
                        tempDP = curDP;
                        swapflag = FALSE;
                        do {
                            tempDP = tempDP->NextDataPoint;
                            if (tempDP == NULL)
                                goto SkipToNext; //no unselected neighbor to switch with
                        } while (tempDP->state == SG_SELECTED);
                        if (curDP->x > tempDP->x) {
                            SG_InsertLink(tempDP, curDP);
                            swapflag = TRUE;
                        }
                    } while (swapflag == TRUE);
            } //END of "is DP selected?"
SkipToNext:
            ;
            curDP = curDP->NextDataPoint;
        }
        //advance to next voice:
        curVoice = curVoice->NextVoice;
    } //END of Voices

    // ==========

    //Now deal with point dragged past right edge ("Time Stretching"):
    if (XFlagOffset > 0.0) { //START of "went past right edge of graph" section
        //=====
        //The most complicated set of possibilities:  user is pulling the
        //last DP beyond size of graph. Turns out to be somewhat complex:
        //Problem: x and duration get out of sync. I can't know this crosser's original starting point before move,
        //which leaves one unknown unselected DP with an invalid duration but a valid x. But I need valid durations
        //in order to correctly rescale graph. Thus I need to translate x in to dur for ALL DPs before I can increment
        //SG_TotalScheduleDuration (the latter which is what actually causes graph to expand), then finally
        //translate all the DP durations back to x's (to correctly draw graph):
        //First, x to duration conversion:
        double conversion_factor =
                SG_TotalScheduleDuration / widget->allocation.width;
        curVoice = SG_FirstVoice;
        while (curVoice != NULL) {
            tempDP = curVoice->FirstDataPoint;
            while (tempDP->NextDataPoint != NULL) {
                tempDP->duration =
                        (tempDP->NextDataPoint->x - tempDP->x) * conversion_factor;
                tempDP = tempDP->NextDataPoint;
            }
            //do last datapoint:
            tempDP->duration = 0.0; //zero because it reached the edge of the graph
            //Now increment SG_TotalScheduleDuration:
            //   SG_TotalScheduleDuration+=(curDP->x - widget->allocation.width) *conversion_factor;
            curVoice = curVoice->NextVoice;
        }

        //Set total schedule duration to rescale graph's X axis:
        SG_TotalScheduleDuration += XFlagOffset * conversion_factor;

        //Now, convert them back (dur to x conversion):
        //NOTE: New problem here, since multiple points pulled in the same past-edge drag
        //(i.e., no button-up) will now be "moved further" for the same x-move than the first crosser,
        //because x scale has changed (and subsequent same-move crossers would normally just get the
        //original, un-rescaled x move). This apparently ONLY matters for multiple voices, since it
        //seems to only affect relationship between last and second-to-last DPs (which gets desynced between voices).
        //The most independent solution is just to have calling function check to see if TotalDuration has changed
        //in-between DP moves, then rescale the x move accordingly.
        conversion_factor = widget->allocation.width / SG_TotalScheduleDuration;
        curVoice = SG_FirstVoice;
        while (curVoice != NULL) {
            double currentx = 0.0;

            tempDP = curVoice->FirstDataPoint;
            //this is necessary because each DP's duration is now out-of-correct proportion with it's X
            //Durations are correct, while X has become arbitrary -- so here I make X agree with duration:
            do {
                tempDP->x = currentx;
                currentx += tempDP->duration * conversion_factor;
                tempDP = tempDP->NextDataPoint;
            } while (tempDP != NULL);
            //now get new limits (not necessary because I incremented SG_TotalScheduleDuration directly):
            //    SG_GetScheduleLimits();
            curVoice = curVoice->NextVoice;
        }
    } //END of "went past right-edge of graph" section

    // ===============
    //Now deal with graph views allowing points pulled above top edge (Rescaling graph to new top Hz):
    if (YFlagOffset < 0.0 &&
            (SG_GraphType == SG_GRAPHTYPE_BASEFREQ ||
            SG_GraphType == SG_GRAPHTYPE_BEATFREQ)) { //START of "went above top edge of graph" section
        //First do Y to Hz translation for SELECTED points according to current graph scale:
        double conversion_factor;

        //first figure out conversion factor and new max:
        switch (SG_GraphType) {
            case SG_GRAPHTYPE_BEATFREQ:
                conversion_factor =
                        SG_MaxScheduleBeatfrequency / widget->allocation.height;
                SG_MaxScheduleBeatfrequency =
                        (widget->allocation.height - YFlagOffset) * conversion_factor;
                break;

            case SG_GRAPHTYPE_BASEFREQ:
                conversion_factor =
                        SG_MaxScheduleBasefrequency / widget->allocation.height;
                SG_MaxScheduleBasefrequency =
                        (widget->allocation.height - YFlagOffset) * conversion_factor;
                break;

            default:
                SG_GraphType = SG_GRAPHTYPE_BEATFREQ;
                conversion_factor =
                        SG_MaxScheduleBeatfrequency / widget->allocation.height;
                SG_MaxScheduleBeatfrequency =
                        (widget->allocation.height - YFlagOffset) * conversion_factor;
                break;
        }

        //now go through all selected points and scale them
        curVoice = SG_FirstVoice;
        while (curVoice != NULL) {
            curDP = curVoice->FirstDataPoint;
            do {
                if (curDP->state == SG_SELECTED) {
                    switch (SG_GraphType) {
                        case SG_GRAPHTYPE_BEATFREQ:
                            curDP->beatfreq =
                                    (widget->allocation.height - curDP->y) * conversion_factor;
                            break;

                        case SG_GRAPHTYPE_BASEFREQ:
                            curDP->basefreq =
                                    (widget->allocation.height - curDP->y) * conversion_factor;
                            break;

                        default:
                            SG_GraphType = SG_GRAPHTYPE_BEATFREQ;
                            curDP->beatfreq =
                                    (widget->allocation.height - curDP->y) * conversion_factor;
                            break;
                    }

                }
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
            curVoice = curVoice->NextVoice;
        }

        //now set new conversion factor:
        switch (SG_GraphType) {
            case SG_GRAPHTYPE_BEATFREQ:
                conversion_factor =
                        widget->allocation.height / SG_MaxScheduleBeatfrequency;
                break;

            case SG_GRAPHTYPE_BASEFREQ:
                conversion_factor =
                        widget->allocation.height / SG_MaxScheduleBasefrequency;
                break;

            default:
                SG_GraphType = SG_GRAPHTYPE_BEATFREQ;
                conversion_factor =
                        widget->allocation.height / SG_MaxScheduleBeatfrequency;
                break;
        }

        //Now do Hz to Y translation for ALL points to fit new graph scale:
        curVoice = SG_FirstVoice;
        while (curVoice != NULL) {
            curDP = curVoice->FirstDataPoint;
            do {
                switch (SG_GraphType) {
                    case SG_GRAPHTYPE_BEATFREQ:
                        curDP->y =
                                widget->allocation.height - (curDP->beatfreq * conversion_factor);
                        break;

                    case SG_GRAPHTYPE_BASEFREQ:
                        curDP->y =
                                widget->allocation.height - (curDP->basefreq * conversion_factor);
                        break;

                    default:
                        SG_GraphType = SG_GRAPHTYPE_BEATFREQ;
                        curDP->y =
                                widget->allocation.height - (curDP->beatfreq * conversion_factor);
                        break;
                }
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
            curVoice = curVoice->NextVoice;
        }
    } //END of "went above top edge of graph" section

} //whew!

/////////////////////////////////////////////////////

void SG_Cleanup() {
    //done writing text, so discard this:
    if (SG_PangoLayout != NULL) {
        g_object_unref(SG_PangoLayout);
    }
    if (SG_gc != NULL) {
        g_object_unref(SG_gc);
    }
    if (SG_pixmap != NULL) {
        g_object_unref(SG_pixmap);
    }
    SG_pixmap = NULL;
    SG_gc = NULL;
    SG_PangoLayout = NULL;
    // SG_CleanupVoices(SG_FirstBakVoice);
    // SG_FirstBakVoice=NULL;
    SG_CleanupVoices(SG_FirstVoice);
    SG_FirstVoice = NULL;
    SG_CleanupBackupData(&SG_CopyPaste);
    SG_CleanupBackupData(&SG_UndoRedo);
    SG_DBGOUT("Exiting ScheduleGUI Normally");
}

/////////////////////////////////////////////////////
//Allots memory for a new SG_Voice and puts it at end of list
//IMPORTANTLY, if you send it a "First SG_Voice", such as
//SG_FirstVoice or SG_FirstBakVoice, you will still need to explicity
//set that variable to the return from this.
//
//NOTE: FirstDataPoint DOES NOT get alloted here (== NULL).
//[Maybe it should be? Decided NO because it doesn't serve paste or init well]
//So you must call SG_AddNewDataPointToEnd() after this before
//using.
//20100625: ID added to try to maintain consistency across restores

SG_Voice *SG_AddNewVoiceToEnd(int type, int ID) {
    SG_Voice *lastVoice = NULL;
    int count = 0;

    gboolean UseID = TRUE; //20100625
    SG_Voice *curVoice = SG_FirstVoice; //20100625

    //find end of list and check if ID is valid:
    while (curVoice != NULL) {
        if (ID == curVoice->ID)
            UseID = FALSE;
        count = curVoice->ID + 1;
        lastVoice = curVoice;
        curVoice = curVoice->NextVoice;
    }

    if (TRUE == UseID) {
        count = ID;
        //SG_DBGOUT_INT ("Using User ID ", count);
    } else {
        //SG_DBGOUT_INT ("Not using User ID, using ", count);
    }

    curVoice = (SG_Voice *) malloc(sizeof (SG_Voice));
    curVoice->PrevVoice = lastVoice;
    curVoice->type = type; //masks: BB_VOICETYPE_BINAURALBEAT, BB_VOICETYPE_PINKNOISE, BB_VOICETYPE_PCM
    curVoice->ID = count;
    curVoice->hide = FALSE;
    curVoice->mute = FALSE;
    curVoice->mono = FALSE; //[added 20100614]
    curVoice->state = SG_UNSELECTED;
    curVoice->description = NULL; //NOTE: calling function can choose to allot a string to this
    curVoice->NextVoice = NULL;
    curVoice->FirstDataPoint = NULL; //NOTE: calling function must allot this

    if (lastVoice != NULL) {
        lastVoice->NextVoice = curVoice;
    }
    return curVoice;
}

/*
   /////////////////////////////////////////////////////
   //20070621:
   //This allocates, but user must free result on their own.
   char *SG_StringAllocateAndCopy (const char *tmpstr)
   {
   char *dest = NULL;
   if (tmpstr != NULL)
   {
   dest = calloc ((strlen (tmpstr) + 1), 1);
   if (dest != NULL)
   {
   strcpy (dest, tmpstr);
   }
   }
   else
   //this kindly returns an empty valid string if tmpstr was equal to NULL:
   {
   dest = calloc (1, 1);
   if (dest != NULL)
   {
   dest[0] = '\0';
   }
   }
   // SG_DBGOUT_STR ("SG_StringAllocateAndCopy:", dest);
   // SG_DBGOUT_PNT ("SG_StringAllocateAndCopy address:", dest);
   return dest;
   }
 */

/////////////////////////////////////////////////////
//20070621:
//give this the address of a char pointer, ie "&(char * dest)", 
//and this fills it with src.
//IMPORTANT: if value of (*dest) is not NULL, this free's it. 
//So be sure to NULL it if it is "fresh" (no data)

void SG_StringAllocateAndCopy(char **dest, const char *src) {
    if (NULL != (*dest)) {
        SG_DBGOUT_STR("Freeing", (*dest));
        free((*dest));
        (*dest) = NULL;
    }
    if (NULL != src) {
        (*dest) = calloc((strlen(src) + 1), 1);
        if (NULL != (*dest)) {
            strcpy((*dest), src);
        }
    } else
        //this kindly returns an empty valid string if src was equal to NULL:
    {
        (*dest) = calloc(1, 1);
        if (NULL != (*dest)) {
            SG_DBGOUT("Creating empty string");
            (*dest)[0] = '\0';
        }
    }
}

/////////////////////////////////////////////////////
//THIS should be called one time at start of program.
//just to put something inside SG. It can, technically, 
//be called before SG_Init(), since no calls to a pixmap happen.
/////////////////////////////////////////////////////

void SG_SetupDefaultDataPoints(int numberofvoices) {
    SG_SelectionBox.status = 0;
    SG_CleanupBackupData(&SG_CopyPaste);
    SG_CleanupBackupData(&SG_UndoRedo);
    SG_CleanupVoices(SG_FirstVoice);
    SG_FirstVoice = NULL;

    //this section just loads arbitrary crap in ScheduleGUI; needed so that SG is never left empty;
    //START arbitrary data creation:
    int entry;
    SG_Voice *curVoice;
    SG_DataPoint *curDP = NULL;
    int voice;

    for (voice = 0; voice < numberofvoices; voice++) {
        if (voice == 0) {
            curVoice = SG_AddNewVoiceToEnd(BB_VOICETYPE_BINAURALBEAT, 0);
            SG_StringAllocateAndCopy(&(curVoice->description), "Voice 0");
        } else {
            curVoice = SG_AddNewVoiceToEnd(BB_VOICETYPE_PINKNOISE, 0);
            SG_StringAllocateAndCopy(&(curVoice->description), "Pink Noise Voice");
        }
        if (curVoice == NULL) {
            exit(0);
        }
        if (SG_FirstVoice == NULL) {
            SG_FirstVoice = curVoice; // this is getting cumbersome -- maybe I should pass address of pointer instead?!
        }
        for (entry = 0; entry < 1; entry++) {
            if (curVoice->type == BB_VOICETYPE_PINKNOISE) {
                curDP = SG_AddNewDataPointToEnd(curVoice->FirstDataPoint, curVoice, 60, //duration
                        0, //beatfreq
                        .1, //volume_left
                        .1, //volume_right
                        0, //basefreq
                        SG_UNSELECTED); //state
            } else {
                curDP = SG_AddNewDataPointToEnd(curVoice->FirstDataPoint, curVoice, 60, //duration
                        1 * (1 + voice), //beatfreq
                        .5, //volume_left
                        .5, //volume_right
                        180 * (1 + voice), //basefreq
                        SG_UNSELECTED); //state
            }
            if (curVoice->FirstDataPoint == NULL)
                curVoice->FirstDataPoint = curDP;
        }
    }
    //END arbitrary data creation

    //I do this to give SG_MaxScheduleBeatfrequency, SG_MaxScheduleBasefrequency and
    //SG_TotalScheduleDuration some reasonable starting
    //points (they change whenever size of drawingarea changes):
    SG_GetScheduleLimits();

    //just general housecleaning:
    SG_DeselectDataPoints();
    SG_SelectVoice(SG_FirstVoice);
}

/*
   /////////////////////////////////////////////////////
   int SG_CountVoices() {
   int voicecount = 0;
   SG_Voice * curVoice = SG_FirstVoice;
   while (curVoice != NULL) {
   ++voicecount;
   curVoice = curVoice->NextVoice;
   }
   return voicecount;
   }

   /////////////////////////////////////////////////////
   //Dumps whatever is in graph to an array readable by SG_SetupDefaultDataPoints();
   void SG_DumpGraphToC() {
   // int voicecount = SG_CountVoices();
   SG_Voice * curVoice = SG_FirstVoice;
   while (curVoice != NULL) {
   if (curVoice->hide == TRUE) {
   SG_DataPoint * curDP = curVoice->FirstDataPoint;
   do {
   curDP->state = SG_SELECTED;
   //all list loops need this:
   curDP = curDP->NextDataPoint;
   } while (curDP != NULL);
   }
   curVoice = curVoice->NextVoice;
   }
   }
 */

/////////////////////////////////////////////////////
//call this first, then perhaps SG_SetupDefaultDataPoints(2)

void SG_Init(GdkPixmap * pixmap) {
    SG_pixmap = pixmap;
    if (SG_FirstVoice == NULL) {
        //I must NULL all these so that they can safely be fed to SG_CleanupBackupData():
        SG_CopyPaste.DPdata = NULL;
        SG_CopyPaste.Voice = NULL;
        SG_UndoRedo.DPdata = NULL;
        SG_UndoRedo.Voice = NULL;
    }
    // SG_SetupDefaultDataPoints(2);
}

/////////////////////////////////////////////////////
//a20070625:

int SG_VoiceCount() {
    int voicecount = 0;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        ++voicecount;
        curVoice = curVoice->NextVoice;
    }
    return voicecount;
}

/////////////////////////////////////////////////////
//THIS REALLY ISN'T WORKING SO GOOD
//a20070626 This deselects voices AND their DPs if 
//the voices are NOT visible, and also checks if
//a voice is selected but NOT visible. It also checks if
//more than one voice is selected.

void SG_VoiceTestLegalSelection() {
    gboolean illegalflag = FALSE;
    gboolean VoiceAlreadySelected = FALSE;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == TRUE) {
            //deselect this voices DPs:
            SG_SelectDataPoints_Voice(curVoice, FALSE);
            if (curVoice->state == SG_SELECTED) {
                curVoice->state = SG_UNSELECTED;
                illegalflag = TRUE;
            }
        } else if (SG_SELECTED == curVoice->state) {
            if (TRUE == VoiceAlreadySelected) {
                curVoice->state = SG_UNSELECTED;
            } else {
                VoiceAlreadySelected = TRUE;
            }
        }
        curVoice = curVoice->NextVoice;
    }
    if (illegalflag != TRUE) {
        return;
    }
    //if we got here, need to select any visible voice, which:
    //SG_SelectVoice() will do atomatically:
    SG_SelectVoice(SG_FirstVoice);
    // SG_GraphHasChanged = TRUE;
}

/////////////////////////////////////////////////////
//ONLY inverts VISIBLE voices

void SG_SelectInvertDataPoints_All() {
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            SG_SelectInvertDataPoints_Voice(curVoice);
        }
        curVoice = curVoice->NextVoice;
    }
}

/////////////////////////////////////////////////////
//currently ONLY selects VISIBLE voices

void SG_SelectInvertDataPoints_Voice(SG_Voice * curVoice) {
    if (curVoice == NULL) {
        return;
    }
    SG_DataPoint *curDP = curVoice->FirstDataPoint;

    do {
        if (curDP->state == SG_SELECTED) {
            curDP->state = SG_UNSELECTED;
        } else {
            curDP->state = SG_SELECTED;
        }
        curDP = curDP->NextDataPoint;
    } while (curDP != NULL);
    SG_GraphHasChanged = TRUE;
}

/////////////////////////////////////////////////////
//ONLY inverts VISIBLE voices

void SG_SelectIntervalDataPoints_All(int interval, gboolean select,
        gboolean inverse_on_others) {
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            SG_SelectIntervalDataPoints_Voice(curVoice, interval, select,
                    inverse_on_others);
        }
        curVoice = curVoice->NextVoice;
    }
}

/////////////////////////////////////////////////////
//currently ONLY selects VISIBLE voices

void SG_SelectIntervalDataPoints_Voice(SG_Voice * curVoice, int interval,
        gboolean select,
        gboolean inverse_on_others) {
    if (curVoice == NULL) {
        return;
    }

    if (interval < 2) {
        SG_SelectDataPoints_Voice(curVoice, select);
        return;
    }

    SG_DataPoint *curDP = curVoice->FirstDataPoint;
    int count = 0;

    do {
        if (count == 0) {
            curDP->state = (select == TRUE) ? SG_SELECTED : SG_UNSELECTED;
        } else {
            if (inverse_on_others == TRUE) {
                curDP->state = (select == TRUE) ? SG_UNSELECTED : SG_SELECTED;
            }
        }
        ++count;
        if (count >= interval) {
            count = 0;
        }
        //all list loops need this:
        curDP = curDP->NextDataPoint;
    } while (curDP != NULL);
    SG_GraphHasChanged = TRUE;
}

/////////////////////////////////////////////////////
//20101008: changed from old type to new. 
//NEW: Looks for first and last selected data points in ALL voices
//and aligns all OTHER SELECTED datapoints in that voice between them
//OLD WAY: looked for the first two selected data points in a voice, 
//puts ALL between them in line.

void SG_AlignDataPoints(GtkWidget * widget) {
    double count; //holds # of selected DPs to determine slope
    double facX;
    double facY;

    SG_DataPoint *startDP;
    SG_DataPoint *endDP;
    SG_DataPoint *innerDP;

    //Go through all voices
    SG_Voice *curVoice = SG_FirstVoice;
    SG_DataPoint *curDP = NULL;
    while (curVoice != NULL) {
        count = 0;
        startDP = NULL;
        endDP = NULL;
        innerDP = NULL;
        curDP = curVoice->FirstDataPoint;

        //start going through all DPs in given Voice to
        //find each's first and last selected DPs 
        while (curDP != NULL) {
            if (curDP->state == SG_SELECTED) {
                ++count;
                //get first selected DP:
                if (NULL == startDP) {
                    startDP = curDP;
                } else {
                    endDP = curDP;
                }
            }
            curDP = curDP->NextDataPoint;
        }
        //Done going through DPs looking for first and last selected

        //Now see if we have three or more selected datapoints to play
        //with, and if so, align them:
        if (2 < count) {
            facX = (endDP->x - startDP->x);
            facY = (endDP->y - startDP->y);
            innerDP = startDP;
            while (NULL != (innerDP = innerDP->NextDataPoint)) {
                if (innerDP == endDP) {
                    break;
                }
                if (SG_SELECTED == innerDP->state) {
                    innerDP->y =
                            (startDP->y + ((1.0 - ((endDP->x - innerDP->x) / facX)) * facY));
                }
            }
        }
        //FINALLY: do it all again for next voice:
        curVoice = curVoice->NextVoice;
    }

    //all done, translate the coordinates to graph units and draw:
    SG_ConvertYToData_SelectedPoints(widget);
    SG_ConvertXToDuration_AllPoints(widget);
    SG_DrawGraph(widget);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

/////////////////////////////////////////////////////
//20100727: Change selected datapoints Y to match mouse pointer Y
//returns !0 if anything got changed

int SG_MagneticPointer(GtkWidget * widget, int x, int y) {
#define THRESH 3
    int xl = x - THRESH;
    int xr = x + THRESH;
    gboolean flag = FALSE;
    SG_DataPoint *curDP;
    SG_Voice *curVoice = SG_FirstVoice;
    while (NULL != curVoice) {
        if (FALSE == curVoice->hide) {
            curDP = curVoice->FirstDataPoint;
            do {
                if (SG_SELECTED == curDP->state) {
                    if (xl < curDP->x && xr > curDP->x) {
                        if (y != curDP->y) {
                            curDP->y = y;
                            flag = TRUE;
                        }
                    }
                }
                curDP = curDP->NextDataPoint;
            } while (NULL != curDP);
        }
        curVoice = curVoice->NextVoice;
    }
    //
    if (TRUE == flag) {
        SG_ConvertYToData_SelectedPoints(widget);
        SG_GraphHasChanged = SG_DataHasChanged = TRUE;
        return 1;
    }
    return 0;
}

/////////////////////////////////////////////////////

void SG_ScaleDataPoints_Time(GtkWidget * widget, double scalar) {
    SG_DataPoint *curDP;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            curDP = curVoice->FirstDataPoint;
            do {
                if (curDP->state == SG_SELECTED) {
                    curDP->duration *= scalar;
                }
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        }
        curVoice = curVoice->NextVoice;
    }
    SG_ConvertDataToXY(widget);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

/////////////////////////////////////////////////////

void SG_ScaleDataPoints_Y(GtkWidget * widget, double scalar) {
    gboolean scalegraphflag = FALSE;
    SG_DataPoint *curDP;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            curDP = curVoice->FirstDataPoint;
            do {
                if (curDP->state == SG_SELECTED) {
                    curDP->y =
                            widget->allocation.height -
                            ((widget->allocation.height - curDP->y) * scalar);
                    if (curDP->y > widget->allocation.height) {
                        curDP->y = widget->allocation.height;
                    }
                    if (curDP->y < 0) {
                        scalegraphflag = TRUE;
                    }
                }
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        }
        curVoice = curVoice->NextVoice;
    }
    SG_ConvertYToData_SelectedPoints(widget);
    if (scalegraphflag == TRUE) {
        SG_ConvertDataToXY(widget);
    }
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

/////////////////////////////////////////////////////
//adds value (or random number between +/- limit) to 
//y axis and/or duration of selected datapoints:

void SG_AddToDataPoints(GtkWidget * widget, double limit,
        gboolean x_flag, gboolean y_flag, gboolean rand_flag) {
    gboolean scalegraphflag = FALSE;
    SG_DataPoint *curDP;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            curDP = curVoice->FirstDataPoint;
            do {
                if (curDP->state == SG_SELECTED) {
                    if (y_flag == TRUE) {
                        curDP->y += (TRUE == rand_flag) ? (limit * BB_Rand_pm()) : -limit;
                        if (curDP->y > widget->allocation.height) {
                            curDP->y = widget->allocation.height;
                        }
                        if (curDP->y < 0) {
                            scalegraphflag = TRUE;
                        }
                    }

                    //reminder: you can't just change X, you have to change durations:
                    if (x_flag == TRUE) {
                        curDP->duration +=
                                (TRUE == rand_flag) ? (limit * BB_Rand_pm()) : limit;
                        if (curDP->duration < 0) {
                            curDP->duration = 0;
                        }
                    }
                }
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        }
        curVoice = curVoice->NextVoice;
    }

    if (y_flag == TRUE) {
        SG_ConvertYToData_SelectedPoints(widget);
    }
    if (scalegraphflag == TRUE || x_flag == TRUE) {
        SG_ConvertDataToXY(widget);
    }
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

///////////////////////////////////
//20070722:

void SG_DuplicateSelectedVoice() {
    SG_Voice *srcVoice = SG_SelectVoice(NULL);

    if (srcVoice == NULL) {
        return;
    }
    SG_Voice *destVoice = SG_AddNewVoiceToEnd(srcVoice->type, 0);

    SG_StringAllocateAndCopy(&(destVoice->description), srcVoice->description);
    SG_DataPoint *srcDP = srcVoice->FirstDataPoint;
    SG_DataPoint *destDP = destVoice->FirstDataPoint;
    destVoice->mute = srcVoice->mute; //[20100616]
    destVoice->mono = srcVoice->mono; //[20100616]

    do {
        destDP = SG_AddNewDataPointToEnd(destVoice->FirstDataPoint, destVoice, srcDP->duration, //duration
                srcDP->beatfreq, //beatfreq
                srcDP->volume_left, //volume_left
                srcDP->volume_right, //volume_right
                srcDP->basefreq, //basefreq
                srcDP->state); //state
        srcDP = srcDP->NextDataPoint;
        destDP = destDP->NextDataPoint;
    } while (srcDP != NULL);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

///////////////////////////////////
//20070724: selects last DP in all visible voices

void SG_SelectLastDP_All() {
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            SG_SelectLastDP(curVoice);
        }
        curVoice = curVoice->NextVoice;
    }
}

///////////////////////////////////
//20070724: selects last DP in given voice

void SG_SelectLastDP(SG_Voice * curVoice) {
    if (curVoice == NULL) {
        return;
    }
    SG_DataPoint *curDP = curVoice->FirstDataPoint;

    while (curDP->NextDataPoint != NULL) {
        //advance to next point:
        curDP = curDP->NextDataPoint;
    }
    curDP->state = SG_SELECTED;
    SG_GraphHasChanged = TRUE;
}

///////////////////////////////////
//20070920: selects first DP in visible voices

void SG_SelectFirstDP_All() {
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            curVoice->FirstDataPoint->state = SG_SELECTED;
        }
        curVoice = curVoice->NextVoice;
    }
    SG_GraphHasChanged = TRUE;
}

///////////////////////////////////
//20070724:

void SG_ReverseVoice(GtkWidget * widget) {
    SG_Voice *curVoice = SG_SelectVoice(NULL);

    if (curVoice == NULL || curVoice->FirstDataPoint == NULL ||
            curVoice->FirstDataPoint->NextDataPoint == NULL) {
        return;
    }

    //Backup points:
    // SG_BackupDataPoints (widget);

    int dpcount = 0;
    int seldpcount = 0;

    SG_CountVoiceDPs(curVoice, &dpcount, &seldpcount);

    //first set x values:
    SG_DataPoint *curDP = curVoice->FirstDataPoint->NextDataPoint;

    while (curDP != NULL) {
        curDP->x = widget->allocation.width - curDP->x;
        //advance to next point:
        curDP = curDP->NextDataPoint;
    }

    //Now find end:
    SG_DataPoint *endDP = curVoice->FirstDataPoint;

    while (endDP->NextDataPoint != NULL) {
        if (curDP != curVoice->FirstDataPoint) {
            //advance to next point:
            endDP = endDP->NextDataPoint;
        }
    }

    //now go forwards and backwards:
    SG_DataPoint *nextcurDP;
    SG_DataPoint *nextendDP;

    curDP = curVoice->FirstDataPoint->NextDataPoint;
    int i;

    for (i = 0; i < ((dpcount - 1) >> 1); i++) {
        nextcurDP = curDP->NextDataPoint;
        nextendDP = endDP->PrevDataPoint;
        SG_SwapLinks(curDP, endDP);
        //advance to next point:
        curDP = nextcurDP;
        endDP = nextendDP;
    }

    SG_ConvertXToDuration_AllPoints(widget);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

///////////////////////////////////
//20070724: Cuts (or grows) a schedule to an exact entime

void SG_TruncateSchedule(GtkWidget * widget, double endtime) {
    SG_Voice *curVoice = SG_FirstVoice;
    gboolean cutflag;

    while (curVoice != NULL) {
        cutflag = FALSE;
        SG_DataPoint *lastDP;
        SG_DataPoint *curDP = curVoice->FirstDataPoint;
        double currentduration = 0;

        do {
            lastDP = curDP;
            currentduration += curDP->duration;
            if (currentduration >= endtime) {
                cutflag = TRUE;
                break;
            }
            curDP = curDP->NextDataPoint;
        } while (curDP != NULL);

        if (cutflag == TRUE) {
            lastDP->duration -= (currentduration - endtime);
            //delete all the rest of the DPs:
            curDP = lastDP->NextDataPoint;
            lastDP->NextDataPoint = NULL;
            while (curDP != NULL) {
                //since I'm about to obliterate it, I need to advance now:
                lastDP = curDP;
                curDP = curDP->NextDataPoint;
                SG_DeleteDataPoint(lastDP, TRUE);
            }
        } else {
            //lengthen the last DP to fit schedule
            lastDP->duration += (endtime - currentduration);
        }
        curVoice = curVoice->NextVoice;
    }
    //SG_GetScheduleLimits();
    SG_ConvertDataToXY(widget);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

////////////////////////////////////////

SG_DataPoint *SG_GetLeftmostSelectedDP() {
    SG_DataPoint *curDP,
            *leftmostDP = NULL;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            curDP = curVoice->FirstDataPoint;
            while (curDP->NextDataPoint != NULL) {
                if (curDP->state == SG_SELECTED) {
                    if (leftmostDP == NULL || curDP->x < leftmostDP->x) {
                        leftmostDP = curDP;
                        break;
                    }
                }
                //advance to next point:
                curDP = curDP->NextDataPoint;
            }
        }
        curVoice = curVoice->NextVoice;
    }
    return leftmostDP;
}

////////////////////////////////////////
//This works on the principle that pasted points will be the 
//only selected points, then moves them to the end

void SG_PasteDataPointsAtEnd(GtkWidget * widget) {
    if (SG_CopyPaste.DPdata == NULL || SG_CopyPaste.TotalDataPoints < 1) {
        return;
    }
    SG_PasteSelectedDataPoints(widget, TRUE);
    //find "leftmost selected point":
    SG_DataPoint *leftmostDP = SG_GetLeftmostSelectedDP();

    if (leftmostDP == NULL) {
        return;
    }
    //move all points distance leftmost DP is from right end of graph:
    SG_MoveSelectedDataPoints(widget, widget->allocation.width - leftmostDP->x,
            0);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

////////////////////////////////////////

void SG_InvertY(GtkWidget * widget) {
    SG_DataPoint *curDP;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            curDP = curVoice->FirstDataPoint;
            do {
                if (curDP->state == SG_SELECTED) {
                    curDP->y = widget->allocation.height - curDP->y;
                }
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        }
        curVoice = curVoice->NextVoice;
    }
    SG_ConvertYToData_SelectedPoints(widget);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

////////////////////////////////////////
//if duration is positive, looks for all visible DPs with
//durations >= duration; if negative, for all <= -duration.

void SG_SelectDuration(GtkWidget * widget, double duration) {
    SG_DataPoint *curDP;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            curDP = curVoice->FirstDataPoint;
            do {
                if (duration >= 0) {
                    if (curDP->duration >= duration) {
                        curDP->state = SG_SELECTED;
                    }
                } else if (curDP->duration <= (-duration)) {
                    curDP->state = SG_SELECTED;
                }
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        }
        curVoice = curVoice->NextVoice;
    }
    SG_ConvertYToData_SelectedPoints(widget);
    SG_GraphHasChanged = TRUE;
}

/////////////////////////////////////////////////////
//selects all data points that are within a given proximity
//to each other.
//this is actually a hellaciously complex problem, if I
//really feel like implementing it right. Brute Force:

void SG_SelectProximity_All(double threshold) {
    SG_DataPoint *curDP;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            curDP = curVoice->FirstDataPoint;
            do {
                //###start inner loop
                SG_SelectProximity_SingleDP(curDP, threshold);
                //###end inner loop
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        }
        curVoice = curVoice->NextVoice;
    }
}

/////////////////////////////////////////////////////
//this takes one DP and selects all DPs within it's vicinty

void SG_SelectProximity_SingleDP(SG_DataPoint * innerDP, double threshold) {
    SG_DataPoint *curDP;
    SG_Voice *curVoice = SG_FirstVoice;

    while (curVoice != NULL) {
        if (curVoice->hide == FALSE) {
            curDP = curVoice->FirstDataPoint;
            do {
                if (innerDP != curDP) {
                    if (sqrt((curDP->x - innerDP->x) *
                            (curDP->x - innerDP->x) +
                            (curDP->y - innerDP->y) *
                            (curDP->y - innerDP->y)) <= threshold) {
                        curDP->state = SG_SELECTED;
                        innerDP->state = SG_SELECTED;
                        SG_GraphHasChanged = TRUE;
                    }
                }
                //###end inner loop
                curDP = curDP->NextDataPoint;
            } while (curDP != NULL);
        }
        curVoice = curVoice->NextVoice;
    }
}

/////////////////////////////////////////////////////
//rounds to "roundingval":  1=1th, 10=10th, 100=100th
//parameter can be one of:
//0: duration
//1: volume_left
//2: volume_right
//3: basefreq
//4: beatfreq

void SG_RoundValues_Voice(GtkWidget * widget,
        SG_Voice * curVoice,
        double roundingval, int parameter) {
    if (curVoice == NULL || 0 >= roundingval) {
        return;
    }
    double *proxyvar;
    int val;
    SG_DataPoint *curDP = curVoice->FirstDataPoint;

    do {
        if (SG_SELECTED == curDP->state) {
            switch (parameter) {
                case 0:
                    proxyvar = &curDP->duration;
                    break;

                case 1:
                    proxyvar = &curDP->volume_left;
                    break;

                case 2:
                    proxyvar = &curDP->volume_right;
                    break;

                case 3:
                    proxyvar = &curDP->basefreq;
                    break;

                case 4:
                    proxyvar = &curDP->beatfreq;
                    break;

                default:
                    return;
            }

            val = (int) (*proxyvar * roundingval + .5);
            *proxyvar = (double) (val / roundingval);
            if (*proxyvar < 0)
                *proxyvar = 0;
        }
        //all list loops need this:
        curDP = curDP->NextDataPoint;
    } while (curDP != NULL);
    SG_ConvertDataToXY(widget);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;
}

///////////////////////////////////
//rounds to "roundingval":  1=1th, 10=10th, 100=100th
//parameter can be one of:
//0: duration
//1: volume_left
//2: volume_right
//3: basefreq
//4: beatfreq

void SG_RoundValues_All(GtkWidget * widget,
        double roundingval, int parameter) {
    SG_Voice *curVoice = SG_FirstVoice;

    while (NULL != curVoice) {
        if (curVoice->hide == FALSE) {
            SG_RoundValues_Voice(widget, curVoice, roundingval, parameter);
        }
        curVoice = curVoice->NextVoice;
    }
}
