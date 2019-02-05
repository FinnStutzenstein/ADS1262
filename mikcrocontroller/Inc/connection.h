/*
 * connection.h
 *
 *  Created on: Nov 5, 2018
 *      Author: finn
 */

#ifndef CONNECTION_H_
#define CONNECTION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "pool.h"
#include "network.h"

#define CONNECT_MAGIC1		0x10 /*PREFIX_CONNECTION*/
#define CONNECT_MAGIC2		0x00 /*CONNECTION_SET_TYPE*/

// The types of data, a client has subscribed to.
#define SEND_TYPE_NONE		0x00
#define SEND_TYPE_DEBUG		0x01
#define SEND_TYPE_STATUS	0x02
#define SEND_TYPE_DATA		0x04
#define SEND_TYPE_FFT		0x08

#define CONNECTION_BUFFER_SIZE	((1<<16)-1) // 64K

// All connection types, a TCP connection can have.
typedef enum {
	CONNECTION_TYPE_UNKNOWN,
	CONNECTION_TYPE_HTTP,
	CONNECTION_TYPE_WEBSOCKET,
	CONNECTION_TYPE_TCP
} ConnectionType;

typedef volatile struct connection {
	struct netconn* conn;
	osThreadId thread;
	osMutexId write_mutex;
	volatile ConnectionType type;
	volatile uint8_t send_type;
} connection_t;

typedef struct {
	char buffer[CONNECTION_BUFFER_SIZE];
	char filename[255 + 7 + 1]; // used by http. max length is 155 plus these characters: 0:/www/<name>\0
} connection_data_t;

extern Pool* connection_data_pool;

void connections_init();
void connection_task_function(void const *argument);
uint16_t format_connections_stats(uint8_t* data, uint16_t max_length);

#ifdef __cplusplus
}
#endif

#endif /* CONNECTION_H_ */
