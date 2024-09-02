#ifndef _DEVICE_ID_H_
#define _DEVICE_ID_H_

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

/* 6-byte (48-bit) unique device_id (or cloud comms_id) */
#define K_DEV_ID_LEN    (6)


void get_device_id(uint8_t *pui8_devi_id);


#ifdef __cplusplus
}
#endif


#endif // _DEVICE_ID_H_
