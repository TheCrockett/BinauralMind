/*
   gnauralnet_lists.c
   Methods and variables for creating/accessing linked lists that run GnauralNet
   Depends on:
   gnauralnet.h
   gnauralnet_clocksync.c
   gnauralnet_lists.c
   gnauralnet_main.c
   gnauralnet_socket.c

   Copyright (C) 2008  Bret Logan

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

#include "gnauralnet.h"

//Todo:
//- make GN_FriendList_NextFriend() and GN_FriendList_RandomFriend() not 
//  have to count so much; they are inefficient, could be speeded up
//  by using placeholders. The effort is in ensuring the placeholders
//  never hold deleted info

//////////////////
//Linked FriendList routines:
/////////////////////////////////////////////////////
//This creates and adds a new GN_Friend to the
//end of an existing linked list, or starts new list if
//GN_My.FirstFriend == NULL. Most importantly, it gives
//the old last Friend's NextFriend the address to the new Friend,
//gives this new Friend's PrevFriend address to old lasts Friend,
//and sets new Friend's NextFriend to NULL.
//It will return NULL if malloc fails
//NOTE: Issue 20080115b, ID is a problem, since virgins don't know IDs,
//and Invites deliberately don't use them (to avoid abusers that could fake
//IDs) the solution is to make sure IP and Port haven't been
//seen before either:
GN_Friend *GN_FriendList_AddFriend (unsigned int ID,    //MUST BE IN NET ORDER
                                    unsigned int IP,    //MUST BE IN NET ORDER
                                    unsigned short Port)        //MUST BE IN NET ORDER
{
 GN_Friend *curFriend = GN_My.FirstFriend;
 GN_Friend *lastFriend = NULL;

 GN_My.FriendCount = 1; //starts at 1 to include new one being created

 GN_DBGOUT ("Create new friend entry");

 //find end of list:
 while (curFriend != NULL)
 {
  lastFriend = curFriend;
  curFriend = curFriend->NextFriend;
  ++GN_My.FriendCount;
 }

 GN_DBGOUT_INT ("Current friend count:", GN_My.FriendCount);

 //allot the memory:
 curFriend = (GN_Friend *) malloc (sizeof (GN_Friend));

 //make sure memory got alotted:
 if (NULL == curFriend)
 {
  return NULL;  //no harm done, just failed
 }

 //zero everything (since most things want to be zero at start):
 //THIS IS VERY IMPORTANT:
 memset (curFriend, 0, sizeof (GN_Friend));

 //set up the things I know now (caller must do rest):
 curFriend->ID = ID;
 curFriend->IP = IP;
 curFriend->Port = Port;
 g_get_current_time (&(curFriend->TimeOfDiscovery));

 //link the new Friend correctly with its Neighbors:
 curFriend->PrevFriend = lastFriend;
 curFriend->NextFriend = NULL;

 if (lastFriend != NULL)
 {
  lastFriend->NextFriend = curFriend;
 }
 else
 {
  GN_DBGOUT ("Creating a GN_My.FirstFriend");
  GN_My.FirstFriend = curFriend;
 }

 GN_FriendList_FriendCount ();  //do this any time one is added or deleted
 return curFriend;
}

/////////////////////////////////////////////////////
void GN_FriendList_DeleteAll ()
{
 GN_Friend *curFriend = GN_My.FirstFriend;
 GN_Friend *nextFriend;
 unsigned int count = 0;

 GN_My.FriendCount = GN_My.LoverCount = 0;
 if (curFriend == NULL)
 {
  return;
 }

 while (curFriend->NextFriend != NULL)
 {
  //  GN_DBGOUT_INT("Cleanup Friend", count);
  nextFriend = curFriend->NextFriend;
  free (curFriend);
  curFriend = nextFriend;
  ++count;
 }

 //still need to get the last one:
 if (curFriend != NULL)
 {
  GN_DBGOUT_INT ("Cleanup Last Friend", count);
  free (curFriend);
  ++count;
 }

 //critical:
 GN_My.FirstFriend = NULL;
 GN_DBGOUT_INT ("Friends Deleted:", count);
}

/////////////////////////////////////////////////////
void GN_FriendList_DeleteFriend (GN_Friend * curFriend)
{
 if (curFriend == NULL)
 {
  return;
 }

 //It IS legal to delete the ONLY point, but must be dealt with differently:
 //NOTE: I tried checking like this:
 // if (curFriend == GN_My.FirstFriend)
 //but finally realized that the logic was flawed because is not at all 
 //assumable that the first point is the only one left. This is correct:
 if (curFriend->NextFriend == NULL && curFriend->PrevFriend == NULL)
 {
  free (curFriend);
  //critical:
  GN_My.FirstFriend = NULL;
  GN_My.FriendCount = GN_My.LoverCount = 0;
  GN_DBGOUT ("Cleanedup Last Friend");
  return;
 }

 GN_Friend *prevFriend = curFriend->PrevFriend;

 //first check to see if GN_My.FirstFriend got deleted:
 if (prevFriend == NULL)
 {
  GN_My.FirstFriend = GN_My.FirstFriend->NextFriend;
  GN_My.FirstFriend->PrevFriend = NULL;
 }
 else
 {
  //Make previous Friend now link to the next Friend after the one to be deleted:
  prevFriend->NextFriend = curFriend->NextFriend;
  if (curFriend->NextFriend != NULL)    //i.e., if this point is not the last Friend in the llist:
  {
   //Make NextFriend now call the prevFriend PrevFriend:
   curFriend->NextFriend->PrevFriend = prevFriend;
  }
 }
 //Assuming I've linked everything remaining properly, can now free the memory:
 free (curFriend);
 curFriend = NULL;      //does this do anything??!
 GN_FriendList_FriendCount ();  //do this any time one is added or deleted
}

/////////////////////////////////////////////////////
//this resets GN_My.FriendCount and GN_My.LoverCount, and also returns GN_My.FriendCount
unsigned int GN_FriendList_FriendCount ()
{
 GN_Friend *curFriend = GN_My.FirstFriend;

 GN_My.FriendCount = 0;
 GN_My.LoverCount = 0;

 while (curFriend != NULL)
 {
  ++GN_My.FriendCount;
  if (0 != curFriend->RunningID && curFriend->RunningID == GN_My.RunningID)
  {
   ++GN_My.LoverCount;
  }
  //all list passes need this:
  curFriend = curFriend->NextFriend;
 }
 return GN_My.FriendCount;
}

///////////////////////////////////////
//Gnaurals that know only IP & Port, but not remote IDs, must start with this:
void GN_FriendList_Invite (unsigned int IP,     //MUST BE IN NET ORDER
                           unsigned short Port) //MUST BE IN NET ORDER)
{
 //now deal with special-cases, Virgins:
 //Philosophy is to send an official invitation hello without adding the friend:
 GN_Friend tmpFriend;

 //zero everything (since most things want to be zero at start):
 //THIS IS VERY IMPORTANT:
 memset (&tmpFriend, 0, sizeof (GN_Friend));

 //set up only the things GN_ProcessOutgoingData() needs:
 tmpFriend.ID = GNAURALNET_UNKNOWN;
 tmpFriend.IP = IP;
 tmpFriend.Port = Port;

 GN_DBGOUT ("Requesting a formal invitation:");
 GN_ProcessOutgoingData (&tmpFriend);
}

/////////////////////////////////////////////////////
//This is a very important function because it gets
//called any time a packet comes in, it must be fast,
//and needs to do some security/accuracy checks.
//Returns friend if ID is already on list, NULL otherwise
//NOTE: if ID is GNAURALNET_UNKNOWN, this function will
//check for IP/Port instead of ID; This is because
//Invites may want eventually need to check for IP/Port.
//IP/Port can prove you know someone, but can't prove that you don't.
GN_Friend *GN_FriendList_GetFriend (unsigned int ID,    //MUST BE IN NET ORDER
                                    unsigned int IP,    //MUST BE IN NET ORDER
                                    unsigned short Port)        //MUST BE IN NET ORDER
{
 GN_Friend *curFriend = GN_My.FirstFriend;

 if (GNAURALNET_UNKNOWN != ID)
  //first deal with regular, using IDs:
 {
  while (curFriend != NULL)
  {
   if (curFriend->ID == ID)
   {
    if (curFriend->IP != IP || curFriend->Port != Port)
    {
     GN_ERROUT ("ID found, but wrong IP/Port!");
     GN_ERROUT ("Incoming info:");
     GN_PrintAddressInfo (ID, IP, Port);
     GN_ERROUT ("FriendList info:");
     GN_PrintAddressInfo (curFriend->ID, curFriend->IP, curFriend->Port);
    }
    return curFriend;
   }
   //all list passes need this:
   curFriend = curFriend->NextFriend;
  }
 }
 else
  //Now look according to IP/Port:
 {
  while (curFriend != NULL)
  {
   if (curFriend->IP == IP && curFriend->Port == Port)
   {
    GN_DBGOUT ("Gave Friend her valid ID");
    curFriend->ID = ID; //necessary?
    return curFriend;
   }
   //all list passes need this:
   curFriend = curFriend->NextFriend;
  }
 }

 GN_DBGOUT ("Friend is NOT known");
 return NULL;
}

/////////////////////////////////////////////////////
//This returns a new Friend every call, or NULL if there 
//are no friends, rotating through the FriendList. 
//NOTE: I initially try to use a static
//GN_Friend holding last place; but that was very dangerous
//with the constantly adding and deleting friends. The
//approach here is inefficient, but never dangerous.
GN_Friend *GN_FriendList_NextFriend ()
{
 static unsigned int count = 0;
 unsigned int i = 0;
 GN_Friend *curFriend = GN_My.FirstFriend;

 while (curFriend != NULL)
 {
  if (i == count)
  {
   ++count;
   return curFriend;
  }
  ++i;
  //all list passes need this:
  curFriend = curFriend->NextFriend;
 }
 //if it got here, there are either no friends or count went over:
 if (NULL != GN_My.FirstFriend)
 {
  // GN_DBGOUT_INT ("NextFriend resetting at:", count);
 }
 count = 0;
 return GN_My.FirstFriend;
}

/////////////////////////////////////////////////////
//This returns a random Friend off FriendList, or NULL
//if there are less than 2 friends. 
//Currently there is no ensured good distribution. Tis a total hack
//Problems with it include not checking if GN_My.FriendCount is
//accurate, not checking if it's a repeat send to same guy, etc.
////20080115: excludeFriend added to not send invites to themselves.
//It can be anything if you don't care
GN_Friend *GN_FriendList_RandomFriend (GN_Friend * excludeFriend)
{
 if (2 > GN_My.FriendCount)
 {
  return NULL;
 }

 unsigned int count = 0;
 unsigned int i = rand () % GN_My.FriendCount;
 GN_Friend *curFriend = GN_My.FirstFriend;

 while (curFriend != NULL)
 {
  if (i == count)
  {
   if (curFriend != excludeFriend)
   {
    return curFriend;
   }
   else
   {
    GN_DBGOUT ("Recursively reentering GN_FriendList_RandomFriend()");
    //geeze:
    return GN_FriendList_RandomFriend (excludeFriend);
   }
  }
  ++count;
  //all list passes need this:
  curFriend = curFriend->NextFriend;
 }
 //Shouldn't get here:
 GN_DBGOUT_INT ("RandomFriend acting badly:", i);
 return NULL;
}
