/*
   gnauralXML.h - declarations for gnauralXML.c

   Copyright (C) 20100608  Bret Logan

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

#ifndef _GNAURALXML_H_
#define _GNAURALXML_H_

//Increments this whenever a change to file format obsoletes old gnaural formats:
//#define gxml_XMLFILEVERSION "1.20060924"
//#define gxml_XMLFILEVERSION "1.20070124"
//#define gxml_VERSION_GNAURAL_XMLFILE "1.20100606"
#define gxml_VERSION_GNAURAL_XMLFILE "1.20101006"

extern int gxml_ParserXML_VoiceCount;   //this is a *true* count of Voices (as opposed to listed count in file) taken from  pre-reads of the XML file; has priority
extern int gxml_ParserXML_EntryCount;   //this is a *true* count of Entries (as opposed to listed count in file) taken from  pre-reads of the XML file; has priority
extern int gxml_GnauralFile;    //just a way to know internally if a file being opened isn't a valid Gnaurl2 file
extern void gxml_XMLWriteFile (char *filename);
extern int gxml_XMLReadFile (char *filename, GtkWidget * widget, gboolean MergeRestore);        //returns 0 on success
extern void gxml_XMLParser (const gchar * CurrentElement, const gchar * Attribute, const gchar * Value);        //internal use
extern int gxml_XMLEventDataParser (const gchar * DataType, const gchar * Value, const int internal_EntryCount);        //internal use
extern void gxml_XMLParser_counter (const gchar * CurrentElement, const gchar * Attribute, const gchar * Value);        //always must give valid strings. Also BEWARE: Attribute will equal NULL if there are none
#endif
