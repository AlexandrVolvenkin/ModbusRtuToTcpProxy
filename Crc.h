#ifndef CRC_H_INCLUDED
#define CRC_H_INCLUDED


//-----------------------------------------------------------------------------
// crc16 constants.
//-----------------------------------------------------------------------------
#define START_VAL_CRC16 0xFFFF;
#define CRC16_LENGTH 2;
//-----------------------------------------------------------------------------


unsigned short usCrcSummTwoByteCalculation(unsigned char *pucSource, unsigned int nuiNbyte);
int iCrcSummTwoByteCompare(unsigned char *pucSource, unsigned int nuiNbyte);
int iCrcSummOneByteCompare(unsigned char *pucSource, unsigned int nuiNbyte);
unsigned short usCrc16(unsigned char *pucSource, unsigned short nusLength);
int iCrc16Check(unsigned char *pucSource, unsigned short nusLength);
unsigned short CRC16 (const unsigned char *pucSource, unsigned short nusLength);

#endif // CRC_H_INCLUDED
