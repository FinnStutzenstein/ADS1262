/*
 * tcp.c
 *
 *  Created on: Oct 15, 2018
 *      Author: finn
 */

#include "tcp.h"
#include "adcp.h"
#include "lwip/api.h"
#include "stdio.h"

// Used to give responses, if no memory could be allocated.
static uint8_t TCP_no_mem_error_code = RESPONSE_NO_MEMORY;

/**
 * Handles incoming message data with length len on the given connection. Passes the message
 * through ADCP and writes the response.
 */
uint8_t handle_TCP(connection_t* connection, uint8_t* data, uint16_t len) {
	// Try to get some memory
	connection_data_t* response = (connection_data_t*) pool_alloc(connection_data_pool);
	if (NULL == response) {
		netconn_write(connection->conn, &TCP_no_mem_error_code, 1, NETCONN_COPY);
		return EXIT;
	}

	// use the connection buffer for the response.
	uint8_t* out_data = (uint8_t*)response->buffer;
	uint16_t out_len = 0;
	uint8_t exit = adcp_handle_command(connection, data, len, out_data, &out_len, CONNECTION_BUFFER_SIZE);

	// Write message.
	netconn_write(connection->conn, out_data, out_len, NETCONN_COPY);

	pool_free(connection_data_pool, response);
	return exit;
}
