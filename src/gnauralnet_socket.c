/*
   gnauralnet_socket.c
   low-level network code to access/create GnauralNet
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

/////////////////////////////////////////////////////////////
//IMPORTANT: IP and Port must already be in network order 
//(i.e. had htons or htonl done to them by caller), since 
//GN_Friend stores them that way
int GN_Socket_SendMessage (SOCKET sok,  //
                           char *MsgBuf,        //question
                           int sizeof_MsgBuf,   //in bytes
                           unsigned int IP,     //IN NET ORDER htons
                           unsigned short port) //IN NET ORDER htonl
{
 //put the address in:
 struct sockaddr_in saddr_remote;       //holds temp addresses for send and recv

 saddr_remote.sin_family = PF_INET;
 saddr_remote.sin_port = port;  //it is already in net order
 saddr_remote.sin_addr.s_addr = IP;     //it is already in net order

 //send the buffer:
 // GN_DBGOUT_STR ("Sending to IP:", inet_ntoa (saddr_remote.sin_addr));
 // GN_DBGOUT_INT ("         Port:", ntohs (port));
 if (GNAURAL_IS_SOCKET_ERROR
     (sendto
      (sok, MsgBuf, sizeof_MsgBuf, 0, (struct sockaddr *) &saddr_remote,
       sizeof (saddr_remote))))
 {
  perror ("sendto() error");
 }
 return GNAURALNET_SUCCESS;
}

///////////////////////////////////////
//NOTE: I NEVER DEBUGGED THE ODD TIME REPORTING HERE; uncomment the dbgout lines to see
//This spends usecs time receiving messages; if successful
//it guarantees secs + usecs period before returning unless SIGALRM 
//gets sent across the system (see select())
//NOTE: usecs is always a fractional amount under 1e6
int GN_Socket_RecvMessage (SOCKET sok,  //normally GN_My.Socket
                           char *MsgBuf,        //what to receive into
                           int sizeof_MsgBuf, unsigned int secs,
                           unsigned int usecs)
{
 int count = 0;                 //for debugging only
 int recvsize = 0;
 struct sockaddr_in saddr_remote;

#ifdef GNAURAL_WIN32
 int iAddrLen = sizeof (saddr_remote);
#else
 unsigned int iAddrLen = sizeof (saddr_remote);
#endif

 //see if socket is ready to be read:
 GTimeVal time_started;
 GTimeVal time_now;
 unsigned int target_usecs = (secs * 1e6) + usecs;
 unsigned int elapsed_usecs = 0;
 int interval_secs = secs;
 int interval_usecs = usecs;

 //this is one way of returning at a fixed interval:
 g_get_current_time (&time_started);
 do
 {
  if (0 !=
      GN_Socket_IsReadyForRead (GN_My.Socket, interval_secs, interval_usecs))
  {
   GN_DBGOUT_INT ("Recv packet", ++count);
   if (0 ==
       GNAURAL_IS_SOCKET_ERROR ((recvsize =
                                 recvfrom (sok, MsgBuf, sizeof_MsgBuf, 0,
                                           (struct sockaddr *) &saddr_remote,
                                           &iAddrLen))))
   {    //was a good receive:
    GN_ProcessIncomingData (MsgBuf, sizeof_MsgBuf, &saddr_remote);
   }
   else
   {
    GN_DBGOUT ("Socket error");
    //return GNAURALNET_FAILURE;
   }
  }
  //see if it has taken long enough:
  g_get_current_time (&time_now);

  elapsed_usecs += (((time_now.tv_sec - time_started.tv_sec) * 1e6) +
                    (time_now.tv_usec - time_started.tv_usec));

  //first get total interval in usecs:
  interval_usecs = target_usecs - elapsed_usecs;
  //now separate it in to fractional sec and usec components:
  if (interval_usecs < 1e6)
  {
   interval_secs = 0;
  }
  else
  {
   interval_secs = interval_usecs / 1e6;
   interval_usecs = interval_usecs % 1000000;
  }

  //  GN_DBGOUT_INT ("Time elapsed_usecs:", elapsed_usecs);
  //  GN_DBGOUT_INT ("Time to go, secs:", interval_secs);
  //  GN_DBGOUT_INT ("Time to go, usecs:", interval_usecs);
 }
 while (target_usecs > elapsed_usecs);
 return GNAURALNET_SUCCESS;
}

/////////////////////////////////////////////////////////////////
//Makes a Datagram (UDP) socket
/////////////////////////////////////////////////////////////////
SOCKET GN_Socket_MakeUDP (int port)
{
 SOCKET sok = socket (PF_INET, SOCK_DGRAM, 0);

 if (GNAURAL_IS_INVALID_SOCKET (sok))
 {
  // error occurred on socket call
  //Windows:
  //  printf("Error on socket(), error code=%d",WSAGetLastError());
  //Unix:
  perror ("socket");
  return -1;
 }
 // set up local address and bind() socket to wild card IP address
 // and input port number
 struct sockaddr_in temp_saddr;

 temp_saddr.sin_family = PF_INET;
 temp_saddr.sin_port = htons (port);
 temp_saddr.sin_addr.s_addr = htonl (INADDR_ANY);
 memset (&(temp_saddr.sin_zero), '\0', 8);      // zero the rest of the struct
 if (GNAURAL_IS_SOCKET_ERROR
     (bind (sok, (struct sockaddr *) &temp_saddr, sizeof (struct sockaddr))))
 {
  //Windows:
  //sprintf(szMessageText1,"Error on server bind(), error code=%d",WSAGetLastError());
  perror ("bind");
  return -1;
 }
 return sok;
}

/////////////////////////////////////////////////////////////////////
void GN_Socket_SetBlocking (SOCKET sok, int yesflag)
//yesflag == 0 sets the socket to non-blocking
{
 if (0 == yesflag)
 {
 GN_DBGOUT ("Setting socket for Non-Blocking")}
 else
 {
 GN_DBGOUT ("Setting socket for Blocking")};

#ifdef GNAURAL_WIN32
 unsigned long ioctl_opt;

 if (0 == yesflag)
  ioctl_opt = 1;
 else
  ioctl_opt = 0;

 if (SOCKET_ERROR == ioctlsocket (sok, FIONBIO, &ioctl_opt))
 {
  perror ("ioctlsocket FAILED");
 }
#else
 int flags = fcntl (sok, F_GETFL, 0);

 if (flags != -1)
 {
  if (0 == yesflag)
   flags |= O_NONBLOCK;
  else
   flags &= ~O_NONBLOCK;

  fcntl (sok, F_SETFL, flags);
 }
#endif
}

/////////////////////////////////////////////////////////////////////
//If successful, sets globals (in network order!) GN_My.IP, GN_My.Port, 
//and GN_My.ID, returning SUCCESS:
int GN_Socket_SetLocalInfo ()
{
 struct sockaddr_in s_in;

#ifdef GNAURAL_WIN32
 int namelen = sizeof (s_in);
#else
 unsigned int namelen = sizeof (s_in);
#endif

 if (getsockname (GN_My.Socket, (struct sockaddr *) &s_in, &namelen) < 0)
 {
  GN_DBGOUT_STR ("Can't get local port: %s\n", strerror (errno));
  return GNAURALNET_FAILURE;
 }

 GN_DBGOUT ("Setting local info");
 GN_My.IP = s_in.sin_addr.s_addr;
 GN_My.Port = s_in.sin_port;
 GN_My.ID = rand ();
 return GNAURALNET_SUCCESS;
}

////////////////////////////////////////
//waits timeout microsecs for data to arrive on socket
int GN_Socket_IsReadyForRead (int sd, unsigned int secs, unsigned int usecs)
{
 fd_set socketReadSet;

 FD_ZERO (&socketReadSet);
 FD_SET (sd, &socketReadSet);
 struct timeval tv;

 tv.tv_sec = secs;
 tv.tv_usec = usecs;

 if (GNAURAL_IS_SOCKET_ERROR (select (sd + 1, &socketReadSet, 0, 0, &tv)))
 {
  return 0;
 }
 return (FD_ISSET (sd, &socketReadSet) != 0);
}
