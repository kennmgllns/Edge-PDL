/*****************************************************************************************//**
* \file         crc16.h
*
* \brief        CRC16 application library header file.
* \details      CRC16 application library header file.
*
* \author       Keith A. Beja
* \version      v00.01.00
* \date         20160314
*
* \copyright    &copy; 2016 Power Quality Engineering Ltd. <BR>
*               www.edgeelectrons.com
*********************************************************************************************/
// *INDENT-OFF*
#ifndef __CRC16_H__
#define __CRC16_H__
// *INDENT-ON*

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Include .h Library Files
 */
#include "crc16_cfg.h"

/*
 * Global Constants
 */
/* none */

/*
 * Public Function Prototypes
 */
uint16_t crc16CalcByte(uint16_t ui16_crc16, uint8_t ui8_data);
uint16_t crc16CalcBlock(uint8_t *pui8_data, uint16_t ui16_length);
uint16_t crc16GetSeed(void);

#ifdef __cplusplus
}
#endif

#endif/* end of crc16.h */
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------
