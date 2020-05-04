
/*
 * $Id$
 *
 * Copyright (c) 2005-2008 Vyacheslav Frolov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * $Log$
 * Revision 1.11  2008/02/22 12:56:42  vfrolov
 * Implemented --connect-dtr option
 *
 * Revision 1.10  2006/11/16 12:51:43  vfrolov
 * Added ability to set COM port parameters
 *
 * Revision 1.9  2005/11/25 13:49:23  vfrolov
 * Implemented --interface option for client mode
 *
 * Revision 1.8  2005/10/18 09:53:36  vfrolov
 * Added EVENT_ACCEPT
 *
 * Revision 1.7  2005/10/03 13:48:08  vfrolov
 * Added --ignore-dsr and listen options
 *
 * Revision 1.6  2005/06/10 15:55:10  vfrolov
 * Implemented --terminal option
 *
 * Revision 1.5  2005/06/08 15:48:17  vfrolov
 * Implemented --awak-seq option
 *
 * Revision 1.4  2005/06/07 10:06:37  vfrolov
 * Added ability to use port names
 *
 * Revision 1.3  2005/06/06 15:20:46  vfrolov
 * Implemented --telnet option
 *
 * Revision 1.2  2005/05/30 12:17:32  vfrolov
 * Fixed resolving problem
 *
 * Revision 1.1  2005/05/30 10:02:33  vfrolov
 * Initial revision
 *
 */

#ifndef COM2TCP_H_INCLUDED
#define COM2TCP_H_INCLUDED


#include "precomp.h"
#include "telnet.h"

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char *pucSourceTemp;

extern const char *pProtoName;

extern SOCKET Accept(SOCKET hSockListen);
extern void Disconnect(SOCKET hSock);
extern void TraceLastError(const char *pFmt, ...);
extern BOOL myGetCommState(HANDLE hC0C, DCB *dcb);
extern BOOL mySetCommState(HANDLE hC0C, DCB *dcb);
extern void CloseEvents(int num, HANDLE *hEvents);
extern BOOL PrepareEvents(int num, HANDLE *hEvents, OVERLAPPED *overlaps);
extern void InOut(
    HANDLE hC0C,
    SOCKET hSock,
    Protocol &protocol,
    BOOL ignoreDSR,
    SOCKET hSockListen);
extern void ModbusInOut(
    SOCKET hRtuSock,
    SOCKET hTcpSock,
    Protocol &protocol,
    const ComParams &comParams,
    SOCKET hSockListen);
extern BOOL WaitComReady(HANDLE hC0C, BOOL ignoreDSR, const BYTE *pAwakSeq);
extern HANDLE OpenC0C(const char *pPath, const ComParams &comParams);
extern BOOL SetAddr(struct sockaddr_in &sn, const char *pAddr, const char *pPort);
extern SOCKET Socket(
    const char *pIF,
    const char *pPort);
extern void Disconnect(SOCKET hSock);
extern SOCKET Accept(SOCKET hSockListen);
extern int tcp2com(
    const char *pPath,
    const ComParams &comParams,
    const char *pIF,
    const char *pPort,
    Protocol &protocol);
extern SOCKET Connect(
    const char *pIF,
    const char *pAddr,
    const char *pPort);
extern int com2tcp(
    const char *pPath,
    const ComParams &comParams,
    const char *pIF,
    const char *pAddr,
    const char *pPort,
    Protocol &protocol,
    const BYTE *pAwakSeq);

extern int rtu2tcp(
    const char *pListenPort,
    const ComParams &comParams,
    const char *pIF,
    const char *pAddr,
    const char *pPort,
    Protocol &protocol,
    const BYTE *pAwakSeq);

extern void Usage(const char *pProgName);

#ifdef __cplusplus
}
#endif

#endif // COM2TCP_H_INCLUDED
