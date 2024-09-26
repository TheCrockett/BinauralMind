/*
   ScheduleXML.h
   Copyright (C) 2006  Bret Logan

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
#ifndef _SCHEDULE_XML_
#define _SCHEDULE_XML_

//Set to 1 to abort parsing:
extern int ParserXML_AbortFlag;

///////////////////////////////////
//sets user's internal local functing that parser can call:
void SetUserParserXMLHandler (void (*NewUserParserXMLHandler) (const gchar * Element,   //always a valid string
                                                               const gchar * Attribute, //beware: this will equal NULL if there is no Attribute associated with this Element
                                                               const gchar * Value));   //beware: this will equal NULL to announce end of Element to user
//initiates parsing:
int ParserXML_parse_file_xml (const gchar * filename);
int ParserXML_parse_gchararray_xml (const gchar * contents, int length);

///////////////////////////////////
//the rest are internal use; they can generall be ignored:
void ParserXML_start_element_handler (GMarkupParseContext * context,
                                      const gchar * element_name,
                                      const gchar ** attribute_names,
                                      const gchar ** attribute_values,
                                      gpointer user_data, GError ** error);
void ParserXML_end_element_handler (GMarkupParseContext * context,
                                    const gchar * element_name,
                                    gpointer user_data, GError ** error);
void ParserXML_error_handler (GMarkupParseContext * context,
                              GError * error, gpointer user_data);
void ParserXML_passthrough_handler (GMarkupParseContext * context,
                                    const gchar * passthrough_text,
                                    gsize text_len,
                                    gpointer user_data, GError ** error);
void ParserXML_text_handler (GMarkupParseContext * context,
                             const gchar * text,
                             gsize text_len,
                             gpointer user_data, GError ** error);
void DefaultParserXMLHandler (const gchar * CurrentElement,
                              const gchar * Attribute, const gchar * Value);
//make this "main" for standalone:
int ParserXML_main (int argc, char *argv[]);
#endif
