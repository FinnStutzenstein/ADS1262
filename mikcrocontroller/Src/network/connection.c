/*
 * connection.c
 *
 *  Created on: Nov 5, 2018
 *      Author: finn
 */
#include "connection.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "network.h"
#include "http.h"
#include "websocket.h"
#include "string.h"
#include "tcp.h"

DEFINE_POOL_IN_SECTION(connection_data_pool, MAX_CONNECTIONS, connection_data_t, ".extsram");

static uint8_t get_connection_type(connection_t* connection, uint8_t* data, uint16_t len);

/**
 * Inits the connection module
 */
void connections_init() {
	pool_init(connection_data_pool);
}

uint16_t connection_id_counter = 0;

/**
 * The task for every open connection. Recieves data and processes it through ADCP.
 */
void connection_task_function(void const *argument) {
	uint16_t id = connection_id_counter++; // To keep track of all prints..
	connection_t* connection = (connection_t*) argument;

	err_t recv_err;
	struct netbuf* recv;
	uint8_t* data;
	uint16_t len;

	uint8_t exit = NOEXIT;
	while (exit == NOEXIT && (recv_err = netconn_recv(connection->conn, &recv)) == ERR_OK) {
		osThreadYield();

		osMutexWait(connection->write_mutex, osWaitForever);

		//connection->conn->pcb.tcp->flags |= TF_NODELAY | TF_ACK_NOW;
		//connection->conn->pcb.tcp->flags |= TF_ACK_NOW;
		//connection->conn->pcb.tcp->flags |= TF_ACK_DELAY;
		connection->conn->pcb.tcp->flags |= TF_NODELAY | TF_ACK_DELAY;

		// get access to the data
		netbuf_data(recv, (void**)&data, &len);

		// If this is the first message, guess the connection type.
		if (connection->type == CONNECTION_TYPE_UNKNOWN) {
			if (!get_connection_type(connection, data, len)) {
				// Could not get the type, throw http error and exit
				char* response = "HTTP/1.1 400 Bad request"CRLF"Connection: close"CRLF"Content-Length: 34"CRLF
						CRLF"Request too short.";
				netconn_write(connection->conn, response, strlen(response), NETCONN_COPY);
				exit = EXIT;
			}
		}

		// If until everything was fine, dispatch the message.
		if (exit == NOEXIT) {
			switch(connection->type) {
			case CONNECTION_TYPE_HTTP:
				exit = handle_HTTP(connection, data, len);
				break;
			case CONNECTION_TYPE_WEBSOCKET:
				exit = handle_WebSocket(connection, data, len);
				break;
			case CONNECTION_TYPE_TCP:
				exit = handle_TCP(connection, data, len);
				break;
			default:
				Error_Handler();
			}
		}

		osMutexRelease(connection->write_mutex);
		netbuf_delete(recv);
	}

	// Connection ended
	if (EXIT == exit) {
		printf("Connection %u will be closed by the server\n", id);
	} else {
		switch (recv_err) {
		case ERR_ABRT:
			printf("Connection %u aborted.\n", id);
			break;
		case ERR_RST:
			printf("Connection %u reset.\n", id);
			break;
		case ERR_CLSD:
			printf("Connection %u closed.\n", id);
			break;
		default:
			printf("A critical connection %u error occurred: %d\n", id, recv_err);
			break;
		}
	}

	// Close connection and discard connection identifier.
	netconn_close(connection->conn);
	netconn_delete(connection->conn);
	connection->conn = NULL;

	// Clean up mutex
	osMutexDelete(connection->write_mutex);
	connection->write_mutex = NULL;

	// terminate this thread, free the conneciton data and release connection semaphore to allow new connections.
	osThreadId this_thread = connection->thread;
	pool_free(connection_pool, (void*) connection);
	osSemaphoreRelease(connection_semaphore);
	osThreadTerminate(this_thread);
}

/**
 * Determinate the connection type. Sets the type and send_type attributes of the connection struct.
 * Return 0 if too few bytes are given.
 */
static uint8_t get_connection_type(connection_t* connection, uint8_t* data, uint16_t len) {
	if (len < 2) {
		return 0;
	}
	if (data[0] == CONNECT_MAGIC1 && data[1] == CONNECT_MAGIC2) {
		connection->type = CONNECTION_TYPE_TCP;
		connection->send_type = data[2];
	} else { // Everything else should be HTTP request. If they are not, the http module will send an error.
		connection->type = CONNECTION_TYPE_HTTP;
		connection->send_type = SEND_TYPE_NONE;
	}
	return 1;
}

/**
 * Formats statistics into the given buffer as a string with respect to max_length.
 * Returns the length of the written string (excl. null terminator)
 */
uint16_t format_connections_stats(uint8_t* data, uint16_t max_length) {
	char* pos = (char*)data;

	pos += snprintf(pos, max_length - ((char*)data - pos), "Connections:\n");
	connection_t **connections = (connection_t**)pool_get_entries(connection_pool);
	for (uint32_t i = 0; i < connection_pool->entrycount; i++) {
		connection_t* c = connections[i];

		if (NULL == c) {
			pos += snprintf(pos, max_length - ((char*)data - pos), "%lu: -\n", i);
		} else if (NULL == c->conn) {
			pos += snprintf(pos, max_length - ((char*)data - pos), "%lu: Error: no connection\n", i);
		} else {
			pos += snprintf(pos, max_length - ((char*)data - pos), "%lu: Connectiontype ", i);
			switch (c->type) {
			case CONNECTION_TYPE_UNKNOWN:
				pos += snprintf(pos, max_length - ((char*)data - pos), "Unknown, Datatype");
				break;
			case CONNECTION_TYPE_HTTP:
				pos += snprintf(pos, max_length - ((char*)data - pos), "HTTP, Datatype");

				break;
			case CONNECTION_TYPE_WEBSOCKET:
				pos += snprintf(pos, max_length - ((char*)data - pos), "WS, Datatype");

				break;
			case CONNECTION_TYPE_TCP:
				pos += snprintf(pos, max_length - ((char*)data - pos), "TCP, Datatype");

				break;
			}

			if (c->send_type == SEND_TYPE_NONE) {
				pos += snprintf(pos, max_length - ((char*)data - pos), " None,");
			}
			if (c->send_type & SEND_TYPE_DEBUG) {
				pos += snprintf(pos, max_length - ((char*)data - pos), " Debug,");

			}
			if (c->send_type & SEND_TYPE_STATUS) {
				pos += snprintf(pos, max_length - ((char*)data - pos), " Status,");

			}
			if (c->send_type & SEND_TYPE_DATA) {
				pos += snprintf(pos, max_length - ((char*)data - pos), " Data,");

			}
			if (c->send_type & SEND_TYPE_FFT) {
				pos += snprintf(pos, max_length - ((char*)data - pos), " FFT,");
			}
			// Make the last comma a newline for the next loop.
			*(pos-1) = '\n';
		}
	}
	return strlen((char*)data);
}
