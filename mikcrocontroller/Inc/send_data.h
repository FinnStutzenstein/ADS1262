/*
 * send_data.h
 *
 *  Created on: Oct 24, 2018
 *      Author: finn
 */

#ifndef SEND_DATA_H_
#define SEND_DATA_H_

#include "stdint.h"
#include "network.h"
#include "connection.h"
#include "sys/cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_QUEUE_SIZE				16
#define STATUS_QUEUE_SIZE				16
#define DATA_QUEUE_SIZE					64
#define FFT_QUEUE_SIZE					16

#define DATA_DESCRIPTOR_POOL_SIZE		(DEBUG_QUEUE_SIZE + STATUS_QUEUE_SIZE + DATA_QUEUE_SIZE + FFT_QUEUE_SIZE)
#define DATA_DESCRIPTOR_BUFFER_SIZE		(3*1460) /* =4380. This is three times the MTU and about 4.2K. This allows
													to send 4K of raw data incl. some header information. */
#define DATA_DESCRIPTOR_BUFFER_RESERVED	7 /* 3 for ADCP, 4 for WS */
#define DATA_DESCRIPTOR_USER_SPACE		(DATA_DESCRIPTOR_BUFFER_SIZE - DATA_DESCRIPTOR_BUFFER_RESERVED)

typedef struct __packed {
	uint8_t type;
	uint8_t data[DATA_DESCRIPTOR_BUFFER_SIZE];
	uint16_t adcp_len;
	uint16_t ws_len;
	uint8_t* adcp_dataptr;
	uint8_t* ws_dataptr;
	uint8_t connections[MAX_CONNECTIONS];
	void (*callback)(void*);
	void* cb_argument;
} DataDescriptor;

void send_data_init();

void send_debug_data(char* buffer, uint16_t len);
uint8_t send_data(uint8_t send_type, uint8_t* data, uint16_t len);
uint8_t send_data_non_copy(uint8_t send_type, uint8_t* data, uint32_t len, void (*callback)(void*), void* cb_argument);
void send_queue_flush_and_update_status();

#ifdef __cplusplus
}
#endif

#endif
