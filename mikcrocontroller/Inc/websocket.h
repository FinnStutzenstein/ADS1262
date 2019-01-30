/*
 * websocket.h
 *
 *  Created on: Nov 5, 2018
 *      Author: finn
 */

#ifndef WEBSOCKET_H_
#define WEBSOCKET_H_

#include "connection.h"
#include "http.h"

#ifdef __cplusplus
 extern "C" {
#endif

#define WEBSOCKET_KEY_LENGTH    24
#define WEBSOCKET_ACCEPT_LENGTH 28
#define WEBSOCKET_MAGIC         "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WEBSOCKET_MAGIC_LENGTH  38

#define WEBSOCKET_STATUS_MESSAGE_TOO_BIG		1009
#define WEBSOCKET_STATUS_FULFILLMENT_PREVENTED	1011

uint8_t handle_WebSocket(connection_t* connection, uint8_t* data, uint16_t len);
uint8_t websocket_handshake(connection_t* connection, request_headers_t* headers);
uint8_t* websocket_write_header(uint8_t* payload, uint16_t playload_length, uint16_t* package_length);

#ifdef __cplusplus
}
#endif


#endif /* WEBSOCKET_H_ */
