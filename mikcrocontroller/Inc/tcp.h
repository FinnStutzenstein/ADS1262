/*
 * tcp.h
 *
 *  Created on: Oct 15, 2018
 *      Author: finn
 */

#ifndef TCP_H_
#define TCP_H_

#include "stdint.h"
#include "connection.h"

uint8_t handle_TCP(connection_t* connection, uint8_t* data, uint16_t len);

#endif /* TCP_H_ */
