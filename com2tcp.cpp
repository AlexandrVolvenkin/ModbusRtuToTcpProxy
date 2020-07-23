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

#include "com2tcp.h"
#include "precomp.h"
#include "telnet.h"
#include "rtu2tcp.h"
#include "Crc.h"

using namespace std;

unsigned char *pucSourceTemp;
///////////////////////////////////////////////////////////////
static SOCKET Accept(SOCKET hSockListen);
static void Disconnect(SOCKET hSock);
///////////////////////////////////////////////////////////////
static void TraceLastError(const char *pFmt, ...)
{
    DWORD err = GetLastError();
    va_list va;
    va_start(va, pFmt);
    vfprintf(stderr, pFmt, va);
    va_end(va);

    fprintf(stderr, " ERROR %s (%lu)\n", strerror(err), (unsigned long)err);
}
///////////////////////////////////////////////////////////////
static BOOL myGetCommState(HANDLE hC0C, DCB *dcb)
{
    dcb->DCBlength = sizeof(*dcb);

    if (!GetCommState(hC0C, dcb))
    {
        TraceLastError("GetCommState()");
        return FALSE;
    }
    return TRUE;
}

static BOOL mySetCommState(HANDLE hC0C, DCB *dcb)
{
    if (!SetCommState(hC0C, dcb))
    {
        TraceLastError("SetCommState()");
        return FALSE;
    }
    return TRUE;
}
///////////////////////////////////////////////////////////////
static void CloseEvents(int num, HANDLE *hEvents)
{
    for (int i = 0 ; i < num ; i++)
    {
        if (hEvents[i])
        {
            if (!::CloseHandle(hEvents[i]))
            {
                TraceLastError("CloseEvents(): CloseHandle()");
            }
            hEvents[i] = NULL;
        }
    }
}

static BOOL PrepareEvents(int num, HANDLE *hEvents, OVERLAPPED *overlaps)
{
    memset(hEvents, 0, num * sizeof(HANDLE));
    memset(overlaps, 0, num * sizeof(OVERLAPPED));

    for (int i = 0 ; i < num ; i++)
    {
        overlaps[i].hEvent = hEvents[i] = ::CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!hEvents[i])
        {
            TraceLastError("PrepareEvents(): CreateEvent()");
            CloseEvents(i, hEvents);
            return FALSE;
        }
    }
    return TRUE;
}
///////////////////////////////////////////////////////////////
static void InOut(
    HANDLE hC0C,
    SOCKET hSock,
    Protocol &protocol,
    BOOL ignoreDSR,
    SOCKET hSockListen = INVALID_SOCKET)
{
    printf("InOut() START\n");

    protocol.Clean();

    BOOL stop = FALSE;

    enum
    {
        EVENT_READ,
        EVENT_SENT,
        EVENT_RECEIVED,
        EVENT_WRITTEN,
        EVENT_STAT,
        EVENT_CLOSE,
        EVENT_ACCEPT,
        EVENT_NUM
    };

    HANDLE hEvents[EVENT_NUM];
    OVERLAPPED overlaps[EVENT_NUM];

    if (!PrepareEvents(EVENT_NUM, hEvents, overlaps))
        stop = TRUE;

    if (!SetCommMask(hC0C, EV_DSR))
    {
        TraceLastError("InOut(): SetCommMask()");
        stop = TRUE;
    }
    WSAEventSelect(hSock, hEvents[EVENT_CLOSE], FD_CLOSE);

    if (hSockListen != INVALID_SOCKET)
        WSAEventSelect(hSockListen, hEvents[EVENT_ACCEPT], FD_ACCEPT);

    DWORD not_used;

    BYTE cbufCocToBuff[64];
    BOOL waitingRead = FALSE;

    BYTE cbufBuffToSock[64];
    int cbufBuffToSockSize = 0;
    int cbufBuffToSockDone = 0;
    BOOL waitingSend = FALSE;

    BYTE cbufSockToBuff[64];
    BOOL waitingRecv = FALSE;

    BYTE cbufBuffToCoc[64];
    int cbufBuffToCocSize = 0;
    int cbufBuffToCocDone = 0;
    BOOL waitingWrite = FALSE;

    BOOL waitingStat = FALSE;
    int DSR = -1;

    while (!stop)
    {
        if (!waitingSend)
        {
            if (!cbufBuffToSockSize)
            {
                cbufBuffToSockSize = protocol.Read(cbufBuffToSock, sizeof(cbufBuffToSock));
                if (cbufBuffToSockSize < 0)
                {
                    break;
                }

            }

            DWORD num = cbufBuffToSockSize - cbufBuffToSockDone;

            if (num)
            {
                if (!WriteFile((HANDLE)hSock, cbufBuffToSock + cbufBuffToSockDone, num, &not_used, &overlaps[EVENT_SENT]))
                {
                    if (::GetLastError() != ERROR_IO_PENDING)
                    {
                        TraceLastError("InOut(): WriteFile(hSock)");
                        cout << "GetLastError !WriteFile hSock" << endl;
                        break;
                    }
                }
                waitingSend = TRUE;
            }
        }
        if (!waitingRead && !protocol.isSendFull())
        {
            if (!ReadFile(hC0C, cbufCocToBuff, sizeof(cbufCocToBuff), &not_used, &overlaps[EVENT_READ]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("InOut(): ReadFile(hC0C)");
                    cout << "GetLastError !ReadFile hC0C" << endl;
                    break;
                }
            }
            waitingRead = TRUE;
        }

        if (!waitingWrite)
        {
            if (!cbufBuffToCocSize)
            {
                cbufBuffToCocSize = protocol.Recv(cbufBuffToCoc, sizeof(cbufBuffToCoc));
                if (cbufBuffToCocSize < 0)
                {
                    cout << "!waitingWrite cbufBuffToCocSize < 0" << endl;
                    break;
                }
            }

            DWORD num = cbufBuffToCocSize - cbufBuffToCocDone;

            if (num)
            {
                if (!WriteFile(hC0C, cbufBuffToCoc + cbufBuffToCocDone, num, &not_used, &overlaps[EVENT_WRITTEN]))
                {
                    if (::GetLastError() != ERROR_IO_PENDING)
                    {
                        TraceLastError("InOut(): WriteFile(hC0C)");
                        cout << "GetLastError WriteFile hC0C" << endl;
                        break;
                    }
                }
                waitingWrite = TRUE;
            }
        }
        if (!waitingRecv && !protocol.isWriteFull())
        {
            if (!ReadFile((HANDLE)hSock, cbufSockToBuff, sizeof(cbufSockToBuff), &not_used, &overlaps[EVENT_RECEIVED]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("InOut(): ReadFile(hSock)");
                    cout << "GetLastError ReadFile hSock" << endl;
                    break;
                }
            }
            waitingRecv = TRUE;
        }

        if (!waitingStat)
        {
            if (!WaitCommEvent(hC0C, &not_used, &overlaps[EVENT_STAT]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("InOut(): WaitCommEvent()");
                    cout << "GetLastError ERROR_IO_PENDING" << endl;
                    break;
                }
            }
            waitingStat = TRUE;

            DWORD stat;

            if (!GetCommModemStatus(hC0C, &stat))
            {
                TraceLastError("InOut(): GetCommModemStatus()");
                cout << "!GetCommModemStatus" << endl;
                break;
            }

            if (!(stat & MS_DSR_ON))
            {
                if (DSR != 0)
                {
                    printf("DSR is OFF\n");
                    DSR = 0;
                }
                if (!ignoreDSR)
                {
                    if (waitingSend)
                        Sleep(1000);
                    cout << "!ignoreDSR" << endl;
                    break;
                }
            }
            else
            {
                if (DSR != 1)
                {
                    printf("DSR is ON\n");
                    DSR = 1;
                }
            }
        }

        if ((waitingRead || waitingSend) && (waitingRecv || waitingWrite) && waitingStat)
        {
            DWORD done;
            switch (WaitForMultipleObjects(EVENT_NUM, hEvents, FALSE, 50000))
            {
            case WAIT_OBJECT_0 + EVENT_READ:
                if (!GetOverlappedResult(hC0C, &overlaps[EVENT_READ], &done, FALSE))
                {
                    if (::GetLastError() != ERROR_OPERATION_ABORTED)
                    {
                        TraceLastError("InOut(): GetOverlappedResult(EVENT_READ)");
                        cout << "WAIT_OBJECT_0 + EVENT_READ" << endl;
                        stop = TRUE;
                        break;
                    }
                }
                ResetEvent(hEvents[EVENT_READ]);
                waitingRead = FALSE;
                protocol.Send(cbufCocToBuff, done);
                break;
            case WAIT_OBJECT_0 + EVENT_SENT:
                if (!GetOverlappedResult((HANDLE)hSock, &overlaps[EVENT_SENT], &done, FALSE))
                {
                    if (::GetLastError() != ERROR_OPERATION_ABORTED)
                    {
                        TraceLastError("InOut(): GetOverlappedResult(EVENT_SENT)");
                        cout << "WAIT_OBJECT_0 + EVENT_SENT" << endl;
                        stop = TRUE;
                        break;
                    }
                    done = 0;
                }
                ResetEvent(hEvents[EVENT_SENT]);
                cbufBuffToSockDone += done;
                if (cbufBuffToSockDone >= cbufBuffToSockSize)
                    cbufBuffToSockDone = cbufBuffToSockSize = 0;
                waitingSend = FALSE;
                break;
            case WAIT_OBJECT_0 + EVENT_RECEIVED:
                if (!GetOverlappedResult((HANDLE)hSock, &overlaps[EVENT_RECEIVED], &done, FALSE))
                {
                    if (::GetLastError() != ERROR_OPERATION_ABORTED)
                    {
                        TraceLastError("InOut(): GetOverlappedResult(EVENT_RECEIVED)");
                        cout << "WAIT_OBJECT_0 + EVENT_RECEIVED" << endl;
                        stop = TRUE;
                        break;
                    }
                    done = 0;
                }
                else if (!done)
                {
                    ResetEvent(hEvents[EVENT_RECEIVED]);
                    printf("Received EOF\n");
                    break;
                }
                ResetEvent(hEvents[EVENT_RECEIVED]);
                waitingRecv = FALSE;
                protocol.Write(cbufSockToBuff, done);
                break;
            case WAIT_OBJECT_0 + EVENT_WRITTEN:
                if (!GetOverlappedResult(hC0C, &overlaps[EVENT_WRITTEN], &done, FALSE))
                {
                    if (::GetLastError() != ERROR_OPERATION_ABORTED)
                    {
                        TraceLastError("InOut(): GetOverlappedResult(EVENT_WRITTEN)");
                        cout << "WAIT_OBJECT_0 + EVENT_WRITTEN" << endl;
                        stop = TRUE;
                        break;
                    }
                    done = 0;
                }
                ResetEvent(hEvents[EVENT_WRITTEN]);
                cbufBuffToCocDone += done;
                if (cbufBuffToCocDone >= cbufBuffToCocSize)
                    cbufBuffToCocDone = cbufBuffToCocSize = 0;
                waitingWrite = FALSE;
                break;
            case WAIT_OBJECT_0 + EVENT_STAT:
                if (!GetOverlappedResult(hC0C, &overlaps[EVENT_STAT], &done, FALSE))
                {
                    if (::GetLastError() != ERROR_OPERATION_ABORTED)
                    {
                        TraceLastError("InOut(): GetOverlappedResult(EVENT_STAT)");
                        cout << "WAIT_OBJECT_0 + EVENT_STAT" << endl;
                        stop = TRUE;
                        break;
                    }
                }
                waitingStat = FALSE;
                break;
            case WAIT_OBJECT_0 + EVENT_CLOSE:
                ResetEvent(hEvents[EVENT_CLOSE]);
                printf("EVENT_CLOSE\n");
                if (waitingWrite)
                    Sleep(1000);
                stop = TRUE;
                break;
            case WAIT_OBJECT_0 + EVENT_ACCEPT:
            {
                ResetEvent(hEvents[EVENT_ACCEPT]);
                printf("EVENT_ACCEPT\n");

                SOCKET hSockTmp = Accept(hSockListen);

                if (hSockTmp != INVALID_SOCKET)
                {
                    char msg[] = "*** Serial port is busy ***\n";

                    send(hSockTmp, msg, strlen(msg), 0);
                    Disconnect(hSockTmp);
                }
                break;
            }
            case WAIT_TIMEOUT:
                break;
            default:
                TraceLastError("InOut(): WaitForMultipleObjects()");
                stop = TRUE;
            }
        }
    }

    CancelIo(hC0C);
    CancelIo((HANDLE)hSock);

    if (hSockListen != INVALID_SOCKET)
    {
        WSAEventSelect(hSockListen, hEvents[EVENT_ACCEPT], 0);

        u_long blocking = 0;

        ioctlsocket(hSockListen, FIONBIO, &blocking);
    }

    CloseEvents(EVENT_NUM, hEvents);

    printf("InOut() - STOP\n");
}

///////////////////////////////////////////////////////////////
static void ModbusInOut(
    SOCKET hRtuSock,
    SOCKET hTcpSock,
    Protocol &protocol,
    const ComParams &comParams,
    SOCKET hSockListen = INVALID_SOCKET)
{
    printf("ModbusInOut() START\n");

    protocol.Clean();

    BOOL stop = FALSE;

    enum
    {
        EVENT_READ,
        EVENT_SENT,
        EVENT_RECEIVED,
        EVENT_WRITTEN,
        EVENT_STAT,
        EVENT_CLOSE,
        EVENT_ACCEPT,
        EVENT_NUM
    };

    HANDLE hEvents[EVENT_NUM];
    OVERLAPPED overlaps[EVENT_NUM];

    if (!PrepareEvents(EVENT_NUM, hEvents, overlaps))
        stop = TRUE;

//    if (!SetCommMask(hC0C, EV_DSR))
//    {
//        TraceLastError("InOut(): SetCommMask()");
//        stop = TRUE;
//    }
    WSAEventSelect(hRtuSock, hEvents[EVENT_CLOSE], FD_CLOSE);
    WSAEventSelect(hTcpSock, hEvents[EVENT_CLOSE], FD_CLOSE);

    if (hSockListen != INVALID_SOCKET)
        WSAEventSelect(hSockListen, hEvents[EVENT_ACCEPT], FD_ACCEPT);

    DWORD not_used;
    BOOL waitingStat = FALSE;

    enum
    {
        MODBUS_COMM_TO_BUFF,
        MODBUS_BUFF_TO_COMM,
        MODBUS_SOCK_TO_BUFF,
        MODBUS_BUFF_TO_SOCK
    };

    uint8_t ucModbusFsmFlowControl;
    uint8_t ucModbusSockToCommBuff[MODBUS_TCP_MAX_ADU_LENGTH];
    uint8_t ucModbusCommToSockBuff[MODBUS_TCP_MAX_ADU_LENGTH];
    uint8_t ucMessageLength;
    DWORD ulBytesReceived;
    uint32_t ulEventsTimeOut;
    uint16_t usTransactionID;
    uint16_t usMBAPLength;
    uint16_t usCrc;
    const uint32_t MODBUS_35_TIMEOUT = ((((1000000UL / (comParams.BaudRate())) * 8UL * 4UL) / 1000UL) + 1) * 2;
    const uint32_t MODBUS_RECEIVE_TIMEOUT = _RECEIVE_TIMEOUT;

    ucMessageLength = 0;
    ulBytesReceived = 0;
    usTransactionID = 0;
    ulEventsTimeOut = MODBUS_RECEIVE_TIMEOUT;
    ucModbusFsmFlowControl = MODBUS_COMM_TO_BUFF;

    while (!stop)
    {
        switch (ucModbusFsmFlowControl)
        {
        case MODBUS_COMM_TO_BUFF:
            if (!ReadFile((HANDLE)hRtuSock,
                          (&ucModbusCommToSockBuff[_MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE + ucMessageLength]),
                          (sizeof(ucModbusCommToSockBuff) - ucMessageLength),
                          &not_used,
                          &overlaps[EVENT_READ]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("InOut(): ReadFile((HANDLE)hRtuSock)");
                    cout << "GetLastError !ReadFile (HANDLE)hRtuSock" << endl;
                    break;
                }
            }

            ucMessageLength += ulBytesReceived;
            break;

        case MODBUS_BUFF_TO_SOCK:
            if (!ReadFile((HANDLE)hTcpSock,
                          (ucModbusSockToCommBuff),
                          (sizeof(ucModbusSockToCommBuff)),
                          &not_used,
                          &overlaps[EVENT_RECEIVED]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("InOut(): ReadFile(hTcpSock)");
                    cout << "GetLastError ReadFile hTcpSock" << endl;
                    break;
                }
            }

            if (usTransactionID < UINT16_MAX)
            {
                usTransactionID++;
            }
            else
            {
                usTransactionID = 0;
            }

            ucModbusCommToSockBuff[0] = usTransactionID >> 8;
            ucModbusCommToSockBuff[1] = usTransactionID & 0x00ff;

            ucModbusCommToSockBuff[2] = 0;
            ucModbusCommToSockBuff[3] = 0;

            usMBAPLength = ucMessageLength -
                           _MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE -
                           _MODBUS_RTU_CHECKSUM_LENGTH;

            ucModbusCommToSockBuff[4] = usMBAPLength >> 8;
            ucModbusCommToSockBuff[5] = usMBAPLength & 0x00FF;

            ucMessageLength = ucMessageLength +
                              _MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE;

            if (!WriteFile((HANDLE)hTcpSock,
                           (ucModbusCommToSockBuff),
                           ucMessageLength,
                           &not_used,
                           &overlaps[EVENT_SENT]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("InOut(): WriteFile(hTcpSock)");
                    cout << "GetLastError !WriteFile hTcpSock" << endl;
                    break;
                }
            }

            break;
        case MODBUS_SOCK_TO_BUFF:
            break;

        case MODBUS_BUFF_TO_COMM:
            if (!ReadFile((HANDLE)hRtuSock,
                          (&ucModbusCommToSockBuff[_MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE]),
                          (sizeof(ucModbusCommToSockBuff)),
                          &not_used,
                          &overlaps[EVENT_READ]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("InOut(): ReadFile((HANDLE)hRtuSock)");
                    cout << "GetLastError !ReadFile (HANDLE)hRtuSock" << endl;
                    break;
                }
            }

            ucMessageLength = ucMessageLength -
                              _MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE;

            usCrc =
                usCrc16(&ucModbusSockToCommBuff[_MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE],
                        ucMessageLength);

            ucModbusSockToCommBuff[_MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE +
                                   ucMessageLength] = (uint8_t)(usCrc >> 8);
            ucModbusSockToCommBuff[_MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE +
                                   ucMessageLength + 1] = (uint8_t)usCrc;

            if (!WriteFile((HANDLE)hRtuSock,
                           (&ucModbusSockToCommBuff[_MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE]),
                           ucMessageLength + _MODBUS_RTU_CHECKSUM_LENGTH,
                           &not_used,
                           &overlaps[EVENT_WRITTEN]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("InOut(): WriteFile((HANDLE)hRtuSock)");
                    cout << "GetLastError !WriteFile (HANDLE)hRtuSock" << endl;
                    break;
                }
            }
            break;

        default:
            break;
        }

        switch (WaitForMultipleObjects(EVENT_NUM, hEvents, FALSE, ulEventsTimeOut))
        {
        case WAIT_OBJECT_0 + EVENT_READ:
            if (!GetOverlappedResult((HANDLE)hRtuSock, &overlaps[EVENT_READ], &ulBytesReceived, FALSE))
            {
                if (::GetLastError() != ERROR_OPERATION_ABORTED)
                {
                    TraceLastError("InOut(): GetOverlappedResult(EVENT_READ)");
                    cout << "WAIT_OBJECT_0 + EVENT_READ" << endl;
                    stop = TRUE;
                    break;
                }
            }

            ResetEvent(hEvents[EVENT_READ]);
            if (ucMessageLength >= (MODBUS_TCP_MAX_ADU_LENGTH - _MODBUS_TCP_HEADER_LENGTH))
            {
                ucMessageLength = 0;
            }
            else
            {
                ulEventsTimeOut = MODBUS_35_TIMEOUT;
            }

            break;
        case WAIT_OBJECT_0 + EVENT_SENT:
            if (!GetOverlappedResult((HANDLE)hTcpSock, &overlaps[EVENT_SENT], &ulBytesReceived, FALSE))
            {
                if (::GetLastError() != ERROR_OPERATION_ABORTED)
                {
                    TraceLastError("InOut(): GetOverlappedResult(EVENT_SENT)");
                    cout << "WAIT_OBJECT_0 + EVENT_SENT" << endl;
                    stop = TRUE;
                    break;
                }
                ulBytesReceived = 0;
            }

            ResetEvent(hEvents[EVENT_SENT]);
            ucMessageLength = 0;
            ulBytesReceived = 0;
            ulEventsTimeOut = _RESPONSE_TIMEOUT;
            ucModbusFsmFlowControl = MODBUS_SOCK_TO_BUFF;

            break;
        case WAIT_OBJECT_0 + EVENT_RECEIVED:
            if (!GetOverlappedResult((HANDLE)hTcpSock, &overlaps[EVENT_RECEIVED], &ulBytesReceived, FALSE))
            {
                if (::GetLastError() != ERROR_OPERATION_ABORTED)
                {
                    TraceLastError("InOut(): GetOverlappedResult(EVENT_RECEIVED)");
                    cout << "WAIT_OBJECT_0 + EVENT_RECEIVED" << endl;
                    stop = TRUE;
                    break;
                }
                ulBytesReceived = 0;
            }
            else if (!ulBytesReceived)
            {
                ResetEvent(hEvents[EVENT_RECEIVED]);
                printf("Received EOF\n");

                ucMessageLength = 0;
                ulBytesReceived = 0;
                ulEventsTimeOut = MODBUS_RECEIVE_TIMEOUT;
                ucModbusFsmFlowControl = MODBUS_COMM_TO_BUFF;
                break;
            }

            ResetEvent(hEvents[EVENT_RECEIVED]);

            if (!ReadFile((HANDLE)hTcpSock,
                          (ucModbusSockToCommBuff + ucMessageLength),
                          (sizeof(ucModbusSockToCommBuff) - ucMessageLength),
                          &not_used,
                          &overlaps[EVENT_RECEIVED]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("InOut(): ReadFile(hTcpSock)");
                    cout << "GetLastError ReadFile hTcpSock" << endl;
                    break;
                }
            }

            ucMessageLength += ulBytesReceived;

            if (ucMessageLength >= (MODBUS_TCP_MAX_ADU_LENGTH - _MODBUS_TCP_HEADER_LENGTH))
            {
                ucMessageLength = 0;
                ulBytesReceived = 0;
                ulEventsTimeOut = MODBUS_RECEIVE_TIMEOUT;
                ucModbusFsmFlowControl = MODBUS_COMM_TO_BUFF;
            }
            else
            {
                ulEventsTimeOut = MODBUS_35_TIMEOUT;
            }
            break;
        case WAIT_OBJECT_0 + EVENT_WRITTEN:
            if (!GetOverlappedResult((HANDLE)hRtuSock, &overlaps[EVENT_WRITTEN], &ulBytesReceived, FALSE))
            {
                if (::GetLastError() != ERROR_OPERATION_ABORTED)
                {
                    TraceLastError("InOut(): GetOverlappedResult(EVENT_WRITTEN)");
                    cout << "WAIT_OBJECT_0 + EVENT_WRITTEN" << endl;
                    stop = TRUE;
                    break;
                }
                ulBytesReceived = 0;
            }

            ResetEvent(hEvents[EVENT_WRITTEN]);
            ucMessageLength = 0;
            ulBytesReceived = 0;
            ulEventsTimeOut = MODBUS_RECEIVE_TIMEOUT;
            ucModbusFsmFlowControl = MODBUS_COMM_TO_BUFF;

            break;
        case WAIT_OBJECT_0 + EVENT_STAT:
            if (!GetOverlappedResult((HANDLE)hRtuSock, &overlaps[EVENT_STAT], &ulBytesReceived, FALSE))
            {
                if (::GetLastError() != ERROR_OPERATION_ABORTED)
                {
                    TraceLastError("InOut(): GetOverlappedResult(EVENT_STAT)");
                    cout << "WAIT_OBJECT_0 + EVENT_STAT" << endl;
                    stop = TRUE;
                    break;
                }
            }
            ulBytesReceived = 0;
            waitingStat = FALSE;
            break;
        case WAIT_OBJECT_0 + EVENT_CLOSE:
            ResetEvent(hEvents[EVENT_CLOSE]);
            printf("EVENT_CLOSE\n");
            Sleep(1000);
            stop = TRUE;
            break;
        case WAIT_OBJECT_0 + EVENT_ACCEPT:
        {
            ResetEvent(hEvents[EVENT_ACCEPT]);
            printf("EVENT_ACCEPT\n");

            SOCKET hSockTmp = Accept(hSockListen);

            if (hSockTmp != INVALID_SOCKET)
            {
                char msg[] = "*** Serial port is busy ***\n";

                send(hSockTmp, msg, strlen(msg), 0);
                Disconnect(hSockTmp);
            }
            break;
        }
        case WAIT_TIMEOUT:
            switch (ucModbusFsmFlowControl)
            {
            case MODBUS_COMM_TO_BUFF:
                if (ucMessageLength < _MIN_MESSAGE_LENGTH)
                {
                    ucMessageLength = 0;
                    ulBytesReceived = 0;
                    ulEventsTimeOut = MODBUS_RECEIVE_TIMEOUT;
                    ucModbusFsmFlowControl = MODBUS_COMM_TO_BUFF;
                }
                else
                {
                    if (usCrc16(&ucModbusCommToSockBuff[_MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE],
                                ucMessageLength - _MODBUS_RTU_CHECKSUM_LENGTH) ==
                            ((ucModbusCommToSockBuff[ucMessageLength +
                                                     _MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE - 2] << 8) |
                             ucModbusCommToSockBuff[ucMessageLength +
                                                    _MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE - 1]))
                    {
                        ulEventsTimeOut = _RESPONSE_TIMEOUT;
                        ucModbusFsmFlowControl = MODBUS_BUFF_TO_SOCK;

                    }
                    else
                    {
                        ucMessageLength = 0;
                        ulBytesReceived = 0;
                        ulEventsTimeOut = MODBUS_RECEIVE_TIMEOUT;
                        ucModbusFsmFlowControl = MODBUS_COMM_TO_BUFF;
                    }
                }
                break;
            case MODBUS_BUFF_TO_SOCK:
                ucMessageLength = 0;
                ulBytesReceived = 0;
                ulEventsTimeOut = MODBUS_RECEIVE_TIMEOUT;
                ucModbusFsmFlowControl = MODBUS_COMM_TO_BUFF;
                break;
            case MODBUS_SOCK_TO_BUFF:
                if (ucMessageLength < _MIN_MESSAGE_LENGTH)
                {
                    ucMessageLength = 0;
                    ulBytesReceived = 0;
                    ulEventsTimeOut = MODBUS_RECEIVE_TIMEOUT;
                    ucModbusFsmFlowControl = MODBUS_COMM_TO_BUFF;
                }
                else
                {
                    ulEventsTimeOut = _RESPONSE_TIMEOUT;
                    ucModbusFsmFlowControl = MODBUS_BUFF_TO_COMM;
                }

                break;
            case MODBUS_BUFF_TO_COMM:
                ucMessageLength = 0;
                ulBytesReceived = 0;
                ulEventsTimeOut = MODBUS_RECEIVE_TIMEOUT;
                ucModbusFsmFlowControl = MODBUS_COMM_TO_BUFF;
                break;
            default:
                ucMessageLength = 0;
                ulBytesReceived = 0;
                ulEventsTimeOut = MODBUS_RECEIVE_TIMEOUT;
                ucModbusFsmFlowControl = MODBUS_COMM_TO_BUFF;
                break;
            }
            break;
        default:
            TraceLastError("InOut(): WaitForMultipleObjects()");
            stop = TRUE;
            break;
        }
    }

    CancelIo((HANDLE)hRtuSock);
    CancelIo((HANDLE)hTcpSock);

    if (hSockListen != INVALID_SOCKET)
    {
        WSAEventSelect(hSockListen, hEvents[EVENT_ACCEPT], 0);

        u_long blocking = 0;

        ioctlsocket(hSockListen, FIONBIO, &blocking);
    }

    CloseEvents(EVENT_NUM, hEvents);

    printf("InOut() - STOP\n");
}
///////////////////////////////////////////////////////////////
static BOOL WaitComReady(HANDLE hC0C, BOOL ignoreDSR, const BYTE *pAwakSeq)
{
    BOOL waitAwakSeq = (pAwakSeq && *pAwakSeq);
    BOOL waitDSR = (!ignoreDSR && !waitAwakSeq);

    if (!waitAwakSeq && !waitDSR)
        return TRUE;

    enum
    {
        EVENT_READ,
        EVENT_STAT,
        EVENT_NUM
    };

    HANDLE hEvents[EVENT_NUM];
    OVERLAPPED overlaps[EVENT_NUM];

    if (!PrepareEvents(EVENT_NUM, hEvents, overlaps))
        return FALSE;

    BOOL fault = FALSE;

    if (!SetCommMask(hC0C, EV_DSR))
    {
        TraceLastError("WaitComReady(): SetCommMask()");
        fault = TRUE;
    }

    DWORD not_used;

    const BYTE *pAwakSeqNext = pAwakSeq;

    BYTE cbufCocToBuff[1];
    BOOL waitingRead = !waitAwakSeq;
    BOOL waitingStat = !waitDSR;

    while (!fault)
    {
        if (!waitingRead)
        {
            if (!pAwakSeqNext || !*pAwakSeqNext)
                break;

            if (!ReadFile(hC0C, cbufCocToBuff, sizeof(cbufCocToBuff), &not_used, &overlaps[EVENT_READ]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("WaitComReady(): ReadFile()");
                    break;
                }
            }
            waitingRead = TRUE;
        }

        if (!waitingStat)
        {
            if (!WaitCommEvent(hC0C, &not_used, &overlaps[EVENT_STAT]))
            {
                if (::GetLastError() != ERROR_IO_PENDING)
                {
                    TraceLastError("WaitComReady(): WaitCommEvent()");
                    fault = TRUE;
                    break;
                }
            }
            waitingStat = TRUE;

            DWORD stat;

            if (!GetCommModemStatus(hC0C, &stat))
            {
                TraceLastError("WaitComReady(): GetCommModemStatus()");
                fault = TRUE;
                break;
            }

            if (stat & MS_DSR_ON)
            {
                printf("DSR is ON\n");

                Sleep(1000);

                if (!GetCommModemStatus(hC0C, &stat))
                {
                    TraceLastError("WaitComReady(): GetCommModemStatus()");
                    fault = TRUE;
                    break;
                }

                if (stat & MS_DSR_ON)
                    break;                // OK if DSR is still ON

                printf("DSR is OFF\n");
            }
        }

        if (waitingRead && waitingStat)
        {
            DWORD done;

            switch (WaitForMultipleObjects(EVENT_NUM, hEvents, FALSE, 5000))
            {
            case WAIT_OBJECT_0 + EVENT_READ:
                if (!GetOverlappedResult(hC0C, &overlaps[EVENT_READ], &done, FALSE))
                {
                    TraceLastError("WaitComReady(): GetOverlappedResult(EVENT_READ)");
                    fault = TRUE;
                }
                ResetEvent(hEvents[EVENT_READ]);
                if (done && pAwakSeqNext)
                {
                    if (*pAwakSeqNext == *cbufCocToBuff)
                    {
                        pAwakSeqNext++;
                    }
                    else
                    {
                        pAwakSeqNext = pAwakSeq;
                        if (*pAwakSeqNext == *cbufCocToBuff)
                            pAwakSeqNext++;
                    }
                    printf("Skipped character 0x%02.2X\n", (int)*cbufCocToBuff);
                }
                waitingRead = FALSE;
                break;
            case WAIT_OBJECT_0 + EVENT_STAT:
                if (!GetOverlappedResult(hC0C, &overlaps[EVENT_STAT], &not_used, FALSE))
                {
                    TraceLastError("WaitComReady(): GetOverlappedResult(EVENT_STAT)");
                    fault = TRUE;
                }
                waitingStat = FALSE;
                break;
            case WAIT_TIMEOUT:
                break;
            default:
                TraceLastError("WaitComReady(): WaitForMultipleObjects()");
                fault = TRUE;
            }
        }
    }

    CancelIo(hC0C);

    CloseEvents(EVENT_NUM, hEvents);

    printf("WaitComReady() - %s\n", fault ? "FAIL" : "OK");

    return !fault;
}
///////////////////////////////////////////////////////////////
static HANDLE OpenC0C(const char *pPath, const ComParams &comParams)
//static HANDLE OpenC0C(LPCWSTR pPath, const ComParams &comParams)
{
    HANDLE hC0C = CreateFile(pPath,
                             GENERIC_READ|GENERIC_WRITE,
                             0,
                             NULL,
                             OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED,
                             NULL);

    if (hC0C == INVALID_HANDLE_VALUE)
    {
        TraceLastError("OpenC0C(): CreateFile(\"%s\")", pPath);
        return INVALID_HANDLE_VALUE;
    }

    DCB dcb;

    if (!myGetCommState(hC0C, &dcb))
    {
        CloseHandle(hC0C);
        return INVALID_HANDLE_VALUE;
    }

    if (comParams.BaudRate() > 0)
        dcb.BaudRate = (DWORD)comParams.BaudRate();

    if (comParams.ByteSize() > 0)
        dcb.ByteSize = (BYTE)comParams.ByteSize();

    if (comParams.Parity() >= 0)
        dcb.Parity = (BYTE)comParams.Parity();

    if (comParams.StopBits() >= 0)
        dcb.StopBits = (BYTE)comParams.StopBits();

    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = !comParams.IgnoreDSR();
    dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
    dcb.fDtrControl = comParams.ConnectDTR() ? DTR_CONTROL_DISABLE : DTR_CONTROL_ENABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = TRUE;
    dcb.XonChar = 0x11;
    dcb.XoffChar = 0x13;
    dcb.XonLim = 100;
    dcb.XoffLim = 100;
    dcb.fParity = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;
    dcb.fErrorChar = FALSE;

    if (!mySetCommState(hC0C, &dcb))
    {
        CloseHandle(hC0C);
        return INVALID_HANDLE_VALUE;
    }

    COMMTIMEOUTS timeouts;

    if (!GetCommTimeouts(hC0C, &timeouts))
    {
        TraceLastError("OpenC0C(): GetCommTimeouts()");
        CloseHandle(hC0C);
        return INVALID_HANDLE_VALUE;
    }

    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = MAXDWORD - 1;
    timeouts.ReadIntervalTimeout = MAXDWORD;

    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;

    if (!SetCommTimeouts(hC0C, &timeouts))
    {
        TraceLastError("OpenC0C(): SetCommTimeouts()");
        CloseHandle(hC0C);
        return INVALID_HANDLE_VALUE;
    }

    printf("OpenC0C(\"%s\", baud=%ld, data=%ld, parity=%s, stop=%s) - OK\n",
           pPath,
           (long)dcb.BaudRate,
           (long)dcb.ByteSize,
           ComParams::ParityStr(dcb.Parity),
           ComParams::StopBitsStr(dcb.StopBits));

    return hC0C;
}
///////////////////////////////////////////////////////////////
static const char *pProtoName = "tcp";

static BOOL SetAddr(struct sockaddr_in &sn, const char *pAddr, const char *pPort)
{
    memset(&sn, 0, sizeof(sn));
    sn.sin_family = AF_INET;

    if (pPort)
    {
        struct servent *pServEnt;

        pServEnt = getservbyname(pPort, pProtoName);

        sn.sin_port = pServEnt ? pServEnt->s_port : htons((u_short)atoi(pPort));
    }

    sn.sin_addr.s_addr = pAddr ? inet_addr(pAddr) : INADDR_ANY;

    if (sn.sin_addr.s_addr == INADDR_NONE)
    {
        const struct hostent *pHostEnt = gethostbyname(pAddr);

        if (!pHostEnt)
        {
            TraceLastError("SetAddr(): gethostbyname(\"%s\")", pAddr);
            return FALSE;
        }

        memcpy(&sn.sin_addr, pHostEnt->h_addr, pHostEnt->h_length);
    }
    return TRUE;
}

static SOCKET Socket(
    const char *pIF,
    const char *pPort = NULL)
{
    const struct protoent *pProtoEnt;

    pProtoEnt = getprotobyname(pProtoName);

    if (!pProtoEnt)
    {
        TraceLastError("Socket(): getprotobyname(\"%s\")", pProtoName);
        return INVALID_SOCKET;
    }

    SOCKET hSock = socket(AF_INET, SOCK_STREAM, pProtoEnt->p_proto);

    if (hSock == INVALID_SOCKET)
    {
        TraceLastError("Socket(): socket()");
        return INVALID_SOCKET;
    }

    if (pIF || pPort)
    {
        struct sockaddr_in sn;

        if (!SetAddr(sn, pIF, pPort))
            return INVALID_SOCKET;

        if (bind(hSock, (struct sockaddr *)&sn, sizeof(sn)) == SOCKET_ERROR)
        {
            TraceLastError("Socket(): bind(\"%s\", \"%s\")", pIF, pPort);
            closesocket(hSock);
            return INVALID_SOCKET;
        }

        printf("Listen port:(%d) - OK\n", ntohs(sn.sin_port));

    }

    return hSock;
}

static void Disconnect(SOCKET hSock)
{
    if (shutdown(hSock, SD_BOTH) != 0)
        TraceLastError("Disconnect(): shutdown()");

    if (closesocket(hSock) != 0)
        TraceLastError("Disconnect(): closesocket()");

    printf("Disconnect() - OK\n");
}
///////////////////////////////////////////////////////////////

static SOCKET Accept(SOCKET hSockListen)
{
    struct sockaddr_in sn;
    int snlen = sizeof(sn);
    SOCKET hSock = accept(hSockListen, (struct sockaddr *)&sn, &snlen);

    if (hSock == INVALID_SOCKET)
    {
        TraceLastError("tcp2com(): accept()");
        return INVALID_SOCKET;
    }

    u_long addr = ntohl(sn.sin_addr.s_addr);

    printf("Accept(%d.%d.%d.%d) - OK\n",
           (addr >> 24) & 0xFF,
           (addr >> 16) & 0xFF,
           (addr >>  8) & 0xFF,
           addr        & 0xFF);

    return hSock;
}

static int tcp2com(
    const char *pPath,
    const ComParams &comParams,
    const char *pIF,
    const char *pPort,
    Protocol &protocol)
{
    SOCKET hSockListen = Socket(pIF, pPort);

    if (hSockListen == INVALID_SOCKET)
        return 2;

    if (listen(hSockListen, SOMAXCONN) == SOCKET_ERROR)
    {
        TraceLastError("tcp2com(): listen(\"%s\", \"%s\")", pIF, pPort);
        closesocket(hSockListen);
        return 2;
    }

    for (;;)
    {
        SOCKET hSock = Accept(hSockListen);

        if (hSock == INVALID_SOCKET)
            break;

        HANDLE hC0C = OpenC0C(pPath, comParams);

        if (hC0C != INVALID_HANDLE_VALUE)
        {
            if (comParams.ConnectDTR())
                EscapeCommFunction(hC0C, SETDTR);

            InOut(hC0C, hSock, protocol, comParams.IgnoreDSR(), hSockListen);

            if (comParams.ConnectDTR())
                EscapeCommFunction(hC0C, CLRDTR);

            CloseHandle(hC0C);
        }

        Disconnect(hSock);
    }

    closesocket(hSockListen);

    return 2;
}
///////////////////////////////////////////////////////////////
static SOCKET Connect(
    const char *pIF,
    const char *pAddr,
    const char *pPort)
{
    struct sockaddr_in sn;

    if (!SetAddr(sn, pAddr, pPort))
        return INVALID_SOCKET;

    SOCKET hSock = Socket(pIF);

    if (hSock == INVALID_SOCKET)
        return INVALID_SOCKET;

    if (connect(hSock, (struct sockaddr *)&sn, sizeof(sn)) == SOCKET_ERROR)
    {
        TraceLastError("Connect(): connect(\"%s\", \"%s\")", pAddr, pPort);
        closesocket(hSock);
        return INVALID_SOCKET;
    }

    printf("Connect(\"%s\", \"%s\") - OK\n", pAddr, pPort);

    return hSock;
}

static int com2tcp(
    const char *pPath,
    const ComParams &comParams,
    const char *pIF,
    const char *pAddr,
    const char *pPort,
    Protocol &protocol,
    const BYTE *pAwakSeq)
{
    HANDLE hC0C = OpenC0C(pPath, comParams);

    if (hC0C == INVALID_HANDLE_VALUE)
    {
        return 2;
    }

    while (WaitComReady(hC0C, comParams.IgnoreDSR(), pAwakSeq))
    {
        SOCKET hSock = Connect(pIF, pAddr, pPort);
        if (hSock == INVALID_SOCKET)
        {
            //break;
        }
        else
        {
            if (comParams.ConnectDTR())
                EscapeCommFunction(hC0C, SETDTR);
            InOut(hC0C, hSock, protocol, comParams.IgnoreDSR());
            if (comParams.ConnectDTR())
                EscapeCommFunction(hC0C, CLRDTR);
            Disconnect(hSock);
        }

        Sleep(1000);
    }

    CloseHandle(hC0C);

    return 2;
}

static int rtu2tcp(
    const char *pListenPort,
    const ComParams &comParams,
    const char *pIF,
    const char *pAddr,
    const char *pPort,
    Protocol &protocol,
    const BYTE *pAwakSeq)
{
    uint16_t nui16FaultsCounter;

    nui16FaultsCounter = 600;
    while (nui16FaultsCounter--)
    {
        SOCKET hSockListen = Socket(pIF, pListenPort);

        if (hSockListen == INVALID_SOCKET)
        {
            cout << "hSockListen INVALID_SOCKET" << endl;
            return 2;
        }

        if (listen(hSockListen, SOMAXCONN) == SOCKET_ERROR)
        {
            TraceLastError("tcp2com(): listen(\"%s\", \"%s\")", pIF, pPort);
            closesocket(hSockListen);
            return 2;
        }

        while (1)
        {
            SOCKET hTcpSock = Connect(pIF, pAddr, pPort);
            if (hTcpSock == INVALID_SOCKET)
            {
                cout << "hTcpSock INVALID_SOCKET" << endl;
				cout << "Please check ModbusTCP side connection!" << endl;
                break;
            }

            SOCKET hRtuSock = Accept(hSockListen);

            if (hRtuSock == INVALID_SOCKET)
            {
                cout << "hRtuSock INVALID_SOCKET" << endl;
				cout << "Please check ModbusRTU side connection!" << endl;
                break;
            }

            ModbusInOut(hRtuSock, hTcpSock, protocol, comParams, hSockListen);
            Disconnect(hRtuSock);
            Disconnect(hTcpSock);

            Sleep(1000);
        }

        closesocket(hSockListen);
    }
    return 2;
}

///////////////////////////////////////////////////////////////
static void Usage(const char *pProgName)
{
    fprintf(stderr, "Usage (client mode):\n");
    fprintf(stderr, "    %s [options] \\\\.\\<com port> <host addr> <host port>\n", pProgName);
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage (server mode):\n");
    fprintf(stderr, "    %s [options] \\\\.\\<listen port> <host addr> <host port>\n", pProgName);
    fprintf(stderr, "\n");
    fprintf(stderr, "Common options:\n");
    fprintf(stderr, "    --telnet              - use Telnet protocol.\n");
    fprintf(stderr, "    --terminal <type>     - use terminal <type> (RFC 1091).\n");
    fprintf(stderr, "    --help                - show this help.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "COM port options:\n");
    fprintf(stderr, "    --baud <b>            - set baud rate to <b> (default is %ld),\n",
            (long)ComParams().BaudRate());
    fprintf(stderr, "                            where <b> is %s.\n",
            ComParams::BaudRateLst());
    fprintf(stderr, "    --data <d>            - set data bits to <d> (default is %ld), where <d> is\n",
            (long)ComParams().ByteSize());
    fprintf(stderr, "                            %s.\n",
            ComParams::ByteSizeLst());
    fprintf(stderr, "    --parity <p>          - set parity to <p> (default is %s), where <p> is\n",
            ComParams::ParityStr(ComParams().Parity()));
    fprintf(stderr, "                            %s.\n",
            ComParams::ParityLst());
    fprintf(stderr, "    --stop <s>            - set stop bits to <s> (default is %s), where <s> is\n",
            ComParams::StopBitsStr(ComParams().StopBits()));
    fprintf(stderr, "                            %s.\n",
            ComParams::StopBitsLst());
    fprintf(stderr, "    --ignore-dsr          - ignore DSR state (do not wait DSR to be ON before\n");
    fprintf(stderr, "                            connecting to host, do not close connection after\n");
    fprintf(stderr, "                            DSR is OFF and do not ignore any bytes received\n");
    fprintf(stderr, "                            while DSR is OFF).\n");
    fprintf(stderr, "    --connect-dtr         - set DTR to ON/OFF on opening/closing connection to\n");
    fprintf(stderr, "                            host.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    The value d[efault] above means to use current COM port settings.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Client mode options:\n");
    fprintf(stderr, "    --awak-seq <sequence> - wait for awakening <sequence> from com port\n"
            "                            before connecting to host. All data before\n"
            "                            <sequence> and <sequence> itself will not be sent.\n");
    fprintf(stderr, "    --interface <if>      - use interface <if> for connecting.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Server mode options:\n");
    fprintf(stderr, "    --interface <if>      - use interface <if> for listening.\n");
    exit(1);
}
///////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    enum {prNone, prTelnet} protocol = prNone;
    const char *pTermType = NULL;
    const BYTE *pAwakSeq = NULL;
    const char *pIF = NULL;
    char **pArgs = &argv[1];
    ComParams comParams;

    while (argc > 1)
    {
        if (**pArgs != '-')
            break;

        if (!strcmp(*pArgs, "--help"))
        {
            Usage(argv[0]);
        }
        else if (!strcmp(*pArgs, "--telnet"))
        {
            protocol = prTelnet;
            pArgs++;
            argc--;
        }
        else if (!strcmp(*pArgs, "--ignore-dsr"))
        {
            pArgs++;
            argc--;
            comParams.SetIgnoreDSR(TRUE);
        }
        else if (!strcmp(*pArgs, "--connect-dtr"))
        {
            pArgs++;
            argc--;
            comParams.SetConnectDTR(TRUE);
        }
        else if (argc < 3)
        {
            Usage(argv[0]);
        }
        else if (!strcmp(*pArgs, "--terminal"))
        {
            pArgs++;
            argc--;
            pTermType = *pArgs;
            pArgs++;
            argc--;
        }
        else if (!strcmp(*pArgs, "--baud"))
        {
            pArgs++;
            argc--;
            comParams.SetBaudRate(*pArgs);
            pArgs++;
            argc--;
        }
        else if (!strcmp(*pArgs, "--data"))
        {
            pArgs++;
            argc--;
            comParams.SetByteSize(*pArgs);
            pArgs++;
            argc--;
        }
        else if (!strcmp(*pArgs, "--parity"))
        {
            pArgs++;
            argc--;
            if (!comParams.SetParity(*pArgs))
            {
                fprintf(stderr, "Unknown parity value %s\n", *pArgs);
                exit(1);
            }
            pArgs++;
            argc--;
        }
        else if (!strcmp(*pArgs, "--stop"))
        {
            pArgs++;
            argc--;
            if (!comParams.SetStopBits(*pArgs))
            {
                fprintf(stderr, "Unknown stop bits value %s\n", *pArgs);
                exit(1);
            }
            pArgs++;
            argc--;
        }
        else if (!strcmp(*pArgs, "--awak-seq"))
        {
            pArgs++;
            argc--;
            pAwakSeq = (const BYTE *)*pArgs;
            pArgs++;
            argc--;
        }
        else if (!strcmp(*pArgs, "--interface"))
        {
            pArgs++;
            argc--;
            pIF = *pArgs;
            pArgs++;
            argc--;
        }
        else
        {
            fprintf(stderr, "Unknown option %s\n", *pArgs);
            exit(1);
        }
    }

    if (argc < 3 || argc > 4)
        Usage(argv[0]);

    WSADATA wsaData;

    WSAStartup(MAKEWORD(1, 1), &wsaData);

    Protocol *pProtocol;

    switch (protocol)
    {
    case prTelnet:
        pProtocol = new TelnetProtocol(10, 10);
        ((TelnetProtocol *)pProtocol)->SetTerminalType(pTermType);
        break;
    default:
        pProtocol = new Protocol(10, 10);
    };

    int res;

    if (argc == 4)
    {
//        cout << "ModbusRtuToTcpProxy" << endl;
        cout << "Copyright (c) 2020-2025 Alexandr Volvenkin" << endl;
        cout << "elghost36@gmail.com" << endl;
        cout << "" << endl;

        res = rtu2tcp(pArgs[0], comParams, pIF, pArgs[1], pArgs[2], *pProtocol, pAwakSeq);
    }
    else
    {
        cout << "tcp2com" << endl;
        res = tcp2com(pArgs[0], comParams, pIF, pArgs[1], *pProtocol);
    }

    delete pProtocol;

    WSACleanup();
    return res;
}
///////////////////////////////////////////////////////////////
