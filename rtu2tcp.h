#ifndef RTU2TCP_H_INCLUDED
#define RTU2TCP_H_INCLUDED

#include "com2tcp.h"
#include "precomp.h"
#include "telnet.h"

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------------------------------
#define _MIN_MESSAGE_LENGTH 4

#define _RESPONSE_TIMEOUT    1000
#define _RECEIVE_TIMEOUT    15000

#define MODBUS_RTU_MAX_ADU_LENGTH  256
#define MODBUS_TCP_MAX_ADU_LENGTH  260

#define _MODBUS_TCP_HEADER_LENGTH      7

#define _MODBUS_TCP_CHECKSUM_LENGTH    0

#define _MODBUS_RTU_35_TIMEOUT        (1000000UL / MODBUS_RTU_BAUD_RATE) * 8UL * 4UL

#define _MODBUS_RTU_HEADER_LENGTH      1
#define _MODBUS_RTU_CHECKSUM_LENGTH    2

#define _MODBUS_TCP_RTU_HEADER_LENGTH_DIFFERENCE    (_MODBUS_TCP_HEADER_LENGTH - _MODBUS_RTU_HEADER_LENGTH)

//-----------------------------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif


#endif // RTU2TCP_H_INCLUDED
