/*
 * http.h
 *
 *  Created on: Oct 13, 2018
 *      Author: finn
 */

#ifndef HTTP_H_
#define HTTP_H_

#include "stdint.h"
#include "connection.h"
#include "sys/cdefs.h"

#define CRLF		"\r\n"

#ifdef __cplusplus
 extern "C" {
#endif


typedef struct {
	char* host;
	char* upgrade;
	char* connection;
	char* sec_websocket_key;
	char* origin;
	char* sec_websocket_version;
} request_headers_t;

extern volatile uint8_t http_permitted;

void http_init();
uint8_t handle_HTTP(connection_t* connection, uint8_t* _data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_H_ */
