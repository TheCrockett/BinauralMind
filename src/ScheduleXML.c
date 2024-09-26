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
// ScheduleXML.c, by Bret Logan (c) 2006
//Bare-bones XML reading for Schedule file.
//Goal: take a valid Schedule file and put it's data in to a form acceptable to BinauralBeat.
//standalone compile command:
// g++ ParserXML.cpp -o  ParserXML `pkg-config --cflags --libs gtk+-2.0`
//or this for C:
//gcc -c -Wall -g ScheduleXML.c -o ScheduleXML.o -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -DSCHEDULEGUI_FREESTANDING `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0`

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <stdio.h>
#include <stdlib.h> //for free()
#include <ctype.h> //for islanum()

#include <string.h>
#include <glib.h>

#include "ScheduleXML.h"

#define DBGOUT_STR(a,b)  //  fprintf(stderr,"ParserXML: %s %s\n",a, b);
#define DBGOUT_INT(a,b)  //  fprintf(stderr,"ParserXML: %s %d\n",a, b);
#define DBGOUT_FLT(a,b)  //  fprintf(stderr,"ParserXML: %s %g\n",a, b);
#define DBGOUT_PNT(a,b)  //  fprintf(stderr,"ParserXML: %s %p\n",a,b);
#define DBGOUT(a)        //  fprintf(stderr,"ParserXML: %s\n",a);
#define ERROUT(a)          fprintf(stderr,"ParserXML: #Error# %s\n",a);

/*
This is the old format of the data file:
<schedule>
  <date></date>
  <gnauralfile_version></gnauralfile_version>
  <gnaural_version></gnaural_version>
  <title></title>
  <description></description>
  <author></author>
  <totaltime></totaltime>
  <voicecount></voicecount>
  <totalentrycount></totalentrycount>
  <loops></loops>
  <voice>
    <description></description>
    <id></id>
    <type></type>
    <voice_state></voice_state>
    <entrycount></entrycount>
    <entries>
      <entry>
        <parent></parent>
        <duration></duration>
        <volume_left></volume_left>
        <volume_right></volume_right>
        <beatfreq></beatfreq>  //this will hold different data depending on the voice type 
        <basefreq></basefreq>   //this will hold different data depending on the voice type 
        <state></state>    //just whether the DP was selected or not
      </entry>
    </entries>
  </voice>
</schedule>
 
This is the new format:
<schedule>
<gnauralfile_version>1.20060924</gnauralfile_version>
<gnaural_version>N/A [ScheduleGUI]</gnaural_version>
<date>Sun Sep 24 14:16:07 2006
</date>
<title>Very Boring Title</title>
<schedule_description>Test Built-in Schedule</schedule_description>
<author>Bret Logan</author>
<totaltime>300</totaltime>
<voicecount>1</voicecount>
<totalentrycount>1</totalentrycount>
<voice>
 <description>[none]</description>
 <id>0</id>
 <type>1</type>
 <voice_state>1</voice_state>
 <entrycount>10</entrycount>
   <entries>
     <entry parent="0" duration="0.0492881" volume_left="0.132812" volume_right="0.132812" beatfreq="53.1694" basefreq="1164" state="0"/>
    </entries>
  </voice>
</schedule>
*/

///////////////////////////////////////////////
//Global variables:
int depth = 0;
gchar ParserXML_CurrentElement[512];
int ParserXML_AbortFlag = 0;
void (*UserParserXMLHandler)(const gchar *, const gchar *, const gchar *) = DefaultParserXMLHandler;
GMarkupParser parser = {
                         ParserXML_start_element_handler,
                         ParserXML_end_element_handler,
                         ParserXML_text_handler,
                         ParserXML_passthrough_handler,
                         ParserXML_error_handler
                       };
//End globals
///////////////////////////////////////////////




///////////////////////////////////////////////
//user calls this to set PaserXML to directly invoke user's internal code:
void SetUserParserXMLHandler(void (*NewUserParserXMLHandler)(const gchar *, const gchar *, const gchar *)) {
  UserParserXMLHandler = NewUserParserXMLHandler;
}


///////////////////////////////////////////////
//this is an example of how to implement your local/weird/proprietary code:
void DefaultParserXMLHandler(const gchar * CurrentElement, //this must always be a valid string
                             const gchar * Attribute, //beware: this will equal NULL if there are no Atrributes with this Element
                             const gchar * Value) //beware: this will equal NULL to announce end of Element to user
{
  fprintf(stderr, "ParserXML: Element: %s, ", CurrentElement);
  if (Attribute != NULL) fprintf(stderr, "Atribute: %s, ", Attribute);
  if (Value != NULL) fprintf(stderr, "Value: %s\n", Value);
}


///////////////////////////////////////////////
void ParserXML_start_element_handler (GMarkupParseContext *context,
                                      const gchar *element_name,
                                      const gchar **attribute_names,
                                      const gchar **attribute_values,
                                      gpointer user_data,
                                      GError **error) {
  if (ParserXML_AbortFlag != 0) return;
  ++depth;
  strcpy(ParserXML_CurrentElement, element_name);
  DBGOUT_STR("START ", element_name);

//Process any attributes in this xml (if there are any):
  int i = 0;
  while (attribute_names[i] != NULL) {
    UserParserXMLHandler(ParserXML_CurrentElement, attribute_names[i], attribute_values[i]);
    DBGOUT_STR(attribute_names[i], attribute_values[i]);
    ++i;
  }
}

///////////////////////////////////////////////
void ParserXML_end_element_handler (GMarkupParseContext *context,
                                    const gchar *element_name,
                                    gpointer user_data,
                                    GError **error) {
  if (ParserXML_AbortFlag != 0) return;
  --depth;

//tell user end of element occurred:
//NOTE: it is essential to pass local element_name instead of
//ParserXML_CurrentElement, because there is no guarantee that
//ParserXML_CurrentElement hadn't changed (many times) since the
//particular element ending here is announced for user:
  UserParserXMLHandler(element_name, NULL, NULL);

  DBGOUT_STR("END ", element_name);

  //Erase CurrentElement's text:
  ParserXML_CurrentElement[0] = '\0';
}



///////////////////////////////////////////////
void ParserXML_text_handler (GMarkupParseContext *context,
                             const gchar *text,
                             gsize text_len,
                             gpointer user_data,
                             GError **error) {

  if (ParserXML_AbortFlag != 0) return;

  if (text == NULL) {
    ERROUT("ParserXML_text_handler: text == NULL!");
    return;
  }


//this is actually a critical line; not sure it is really ready for all platforms as-is:
  if (text[0] == '\0' ||
      text[0] == '\n' ||
      text[0] == '\r' ||
      text[0] == '\t' ||
      text[0] == ' ')
    return ;

// DBGOUT_STR(ParserXML_CurrentElement, text);

  //==================
//send to user handler code:
  UserParserXMLHandler(ParserXML_CurrentElement, NULL, text);
  //==================
}


///////////////////////////////////////////////
void ParserXML_passthrough_handler (GMarkupParseContext *context,
                                    const gchar *passthrough_text,
                                    gsize text_len,
                                    gpointer user_data,
                                    GError **error) {
  if (ParserXML_AbortFlag != 0) return;

  DBGOUT_STR ("[passthrough junk]", passthrough_text);
}


///////////////////////////////////////////////
void ParserXML_error_handler (GMarkupParseContext *context,
                              GError *error,
                              gpointer user_data) {
  if (ParserXML_AbortFlag != 0) return;

  DBGOUT_STR("XML Parsing Error", error->message);
}


///////////////////////////////////////////////
int ParserXML_parse_file_xml (const gchar *filename) {
  gchar *contents;
  gsize length;
  GError *error;
  GMarkupParseContext *context;

  error = NULL;
  depth = 0;
  ParserXML_AbortFlag = 0;
  ParserXML_CurrentElement[0] = '\0';

//load entire file in to contents:
  if (FALSE == g_file_get_contents (filename,
                                    &contents,
                                    &length,
                                    &error)) {
    fprintf (stderr, "%s\n", error->message);
    g_error_free (error);
    return 1;
  }

//CHOICE: you can parse the entire file at once (and risk having
//it abort at the first error and skip all subsequent data)
//OR you can parse it in chunks (meaning it keep trying to
//parse more data after errors):
//To parse the entire file at once, do this:
//create the GMarkup context:

  context = g_markup_parse_context_new (&parser, (GMarkupParseFlags) 0, NULL, NULL);

  if (FALSE == g_markup_parse_context_parse (context, contents, length, NULL)) {
    g_markup_parse_context_free (context);
    return 1;
  }

  if (FALSE == g_markup_parse_context_end_parse (context, NULL)) {
    g_markup_parse_context_free (context);
    return 1;
  }

  g_markup_parse_context_free (context);
  g_free(contents);
  /*
  //To parse the file's contents in chunks:
   #define chunk_size 2048 // you can pick basically anything for this; basically determines how much you are willing to discard on failure
   context = g_markup_parse_context_new (&parser, (GMarkupParseFlags) 0, NULL, NULL);
    unsigned int i = 0;
    while (i < length)
      {
        int this_chunk = MIN (length - i, chunk_size);
   
        if (FALSE == g_markup_parse_context_parse (context,
                                           contents + i,
                                           this_chunk,
                                           NULL))
          {
            g_markup_parse_context_free (context);
            return 1;
          }
        i += this_chunk;
      }
        
    if (FALSE == g_markup_parse_context_end_parse (context, NULL))
      {
        g_markup_parse_context_free (context);
        return 1;
      }
    g_markup_parse_context_free (context);
    g_free(contents);
      */

  if (ParserXML_AbortFlag == 0) {
    DBGOUT_INT("Net Count: ", depth);
  }
  return ParserXML_AbortFlag;
}

///////////////////////////////////////////////
int ParserXML_main (int argc, gchar *argv[]) {
  SetUserParserXMLHandler(DefaultParserXMLHandler); //same as this: UserParserXMLHandler = DefaultParserXMLHandler;
  if (argc > 1)
    return ParserXML_parse_file_xml (argv[1]);
  else {
    fprintf (stderr, "Give a markup file on the command line\n");
    return 1;
  }
}


///////////////////////////////////////////////
//added 20070130 to parse an already loaded file or
//pre-defined XML array:
int ParserXML_parse_gchararray_xml (const char *contents, int length) {
  GMarkupParseContext *context;
  depth = 0;
  ParserXML_AbortFlag = 0;
  ParserXML_CurrentElement[0] = '\0';

//create the GMarkup context:
  context = g_markup_parse_context_new (&parser, (GMarkupParseFlags) 0, NULL, NULL);

  if (FALSE == g_markup_parse_context_parse (context, contents, length, NULL)) {
    g_markup_parse_context_free (context);
    return 1;
  }

  if (FALSE == g_markup_parse_context_end_parse (context, NULL)) {
    g_markup_parse_context_free (context);
    return 1;
  }

  g_markup_parse_context_free (context);

  if (ParserXML_AbortFlag == 0) {
    DBGOUT_INT("Net Count: ", depth);
  }
  return ParserXML_AbortFlag;
}
