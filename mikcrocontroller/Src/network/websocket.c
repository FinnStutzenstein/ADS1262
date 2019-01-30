/*
 * websocket.c
 *
 *  Created on: Nov 5, 2018
 *      Author: finn
 */
#include "websocket.h"
#include "sha1.h"
#include "string.h"
#include "stdio.h"
#include "base64.h"
#include "http.h"
#include "utils.h"
#include "string.h"
#include "lwip/api.h"
#include "adcp.h"

#define WEBSOCKET_BINARY_PACKAGE	0b10000010

// Websocket packet with 1 byte of payload: RESPONSE_NO_MEMORY.
// The wirst two bytes are FIN, opcode=2 (binary frame) and payloadlength = 1.
const uint8_t  WebSocket_no_mem_error_code[] = {
		WEBSOCKET_BINARY_PACKAGE, 1, RESPONSE_NO_MEMORY
};

static void send_pong(connection_t* connection, uint8_t* data, uint16_t len);
static void send_close(connection_t* connection);
static uint8_t handle_frame(connection_t* connection, uint8_t* data, uint16_t len);
static uint8_t verify_headers(request_headers_t* headers);
static void unmask_payload(uint8_t* payload, uint8_t* masking_key, uint16_t len);
static void create_accept_header_value(char* key, char* accept_header);
static uint8_t is_valid_key(const char* key);

uint8_t handle_WebSocket(connection_t* connection, uint8_t* data, uint16_t len) {
	// We do need at least 6 bytes: Header(2 bytes) and masking key (4 bytes)
	if (len < 6) {
		Error_Handler(); // TODO: fail the connection
	}

	uint8_t FIN_set = data[0] & 0x80;
	uint8_t RSV_set = data[0] & 0x70;
	uint8_t opcode = data[0] & 0x0F;
	uint8_t MASK_set = data[1] & 0x80;

	if (!FIN_set) {
		Error_Handler(); // We do not support fragmentation
	}

	if (RSV_set) {
		Error_Handler(); // Should not be set.
	}

	if (!MASK_set) {
		Error_Handler(); // A client message has to be masked.
	}

	uint8_t offset = 2; // First two bytes are finished.

	uint16_t payload_length = data[1] & 0x7F;
	if (payload_length == 126) {
		// get length from the following two bytes: The extended payload length
		// But first check, if we got them -> We need two more bytes.
		if (len < 8) {
			Error_Handler();
		}

		// in data[2] and data[3] is the length in little endian.
		payload_length = data[3] | (data[2] << 8);
		offset += 2;
	} else if (payload_length == 127) {
		Error_Handler(); // We do not support payloads above 64K of size.
	}

	uint8_t* masking_key = data + offset;
	offset += 4;

	// check if we got exactly payload_length data.
	if ((len - offset) != payload_length) {
		Error_Handler();
	}

	uint8_t* payload = data + offset;
	unmask_payload(payload, masking_key, payload_length);

	uint8_t exit = NOEXIT;
	switch(opcode) {
	case 1: // text frame
		// Not supported.
		Error_Handler();
	case 2: // binary frame
		handle_frame(connection, payload, payload_length);
		break;
	case 8: // close
		send_close(connection);
		exit = EXIT;
		break;
	case 9: // ping
		send_pong(connection, payload, payload_length);
		break;
	case 10: // pong
		// What? Did we ever send a ping? Just ignore this..
		break;
	default:
		Error_Handler(); // These are reserved
	}

	return exit;
}

/**
 * Send a pong response to a ping. Tricky: We must return all payload data, so we need to reserve some
 * buffer space.
 */
static void send_pong(connection_t* connection, uint8_t* data, uint16_t len) {
	// Try to get some memory
	connection_data_t* response = (connection_data_t*) pool_alloc(connection_data_pool);
	if (NULL == response) {
		netconn_write(connection->conn, WebSocket_no_mem_error_code, sizeof(WebSocket_no_mem_error_code), NETCONN_COPY);
		return;
	}
	uint8_t* buffer = (uint8_t*)response->buffer;

	buffer[0] = 0x89; // FIN and opcode=9 (pong)
	uint8_t* payload;
	uint16_t package_len = len;
	if (len >= 126) {
		// Use extended length
		buffer[1] = 126;
		buffer[2] = (len & 0xFF00) >> 8;
		buffer[3] = len & 0xFF;
		payload = buffer + 4;
		package_len += 4;
	} else {
		buffer[1] = len & 0x7F;
		payload = buffer + 2;
		package_len += 2;
	}

	memcpy(payload, data, len); // copy data into the payload
	netconn_write(connection->conn, buffer, package_len, NETCONN_COPY);

	pool_free(connection_data_pool, response);
}

/**
 * Send a close frame to the connection.
 */
static void send_close(connection_t* connection) {
	static uint8_t close_frame[] = {
		0x88, // FIN and opcode=8 (close)
		0x00, // No mask, no payload
	};
	netconn_write(connection->conn, close_frame, sizeof(close_frame), NETCONN_COPY);
}

/**
 * Handle a frame from the connection.
 *
 * Reserved buffer and puts the request through ADCP. Writes the ADCP and WS headers and send the response.
 */
static uint8_t handle_frame(connection_t* connection, uint8_t* data, uint16_t len) {
	// Try to get some memory
	connection_data_t* response = (connection_data_t*) pool_alloc(connection_data_pool);
	if (NULL == response) {
		netconn_write(connection->conn, WebSocket_no_mem_error_code, sizeof(WebSocket_no_mem_error_code), NETCONN_COPY);
		return EXIT;
	}
	uint8_t* buffer = (uint8_t*)response->buffer;

	// We are going to write to the tcp connection directly. The format is:
	//   |<-- space (2 or 0 bytes) -->|<-- WS header (2 or 4 bytes) -->|<-- ADCP header (3 bytes) -->|<-- payload -->|
	//  (a)                          (b)                              (c)                           (d)             (e)
	// buffer points to (a). We reserve all 4 bytes for the WS header and point with `package_begin` to the actual
	// beginning, namely (b). `package_length` is the length between (b) and (e).
	// `ws_payload_begin` points to (c) and `ws_payload_length` is the amount of bytes between (c) and (e).
	// `adcp_payload_begin` points to (d) and `adcp_payload_length` is the size between (d) and (e).
	// (c) is fixed at `buffer+4` and (d) at `buffer+7`. (d) will be used to call the ADCP layer.

	uint8_t* package_begin;
	uint16_t package_length;
	uint8_t* ws_payload_begin = buffer + 4;
	uint16_t ws_payload_length;
	uint8_t* adcp_payload_begin = buffer + 7;
	uint16_t adcp_payload_length = 0;

	// Handle ADCP.
	uint8_t exit = adcp_handle_command(connection, data, len, adcp_payload_begin, &adcp_payload_length, CONNECTION_BUFFER_SIZE-7);
	ws_payload_length = adcp_payload_length + 3; // Add the ADCP header.

	// Set ADCP Header
	ws_payload_begin[0] = SEND_TYPE_NONE;
	*(uint16_t*)(ws_payload_begin + 1) = adcp_payload_length;

	// Websocket. The header has either 2 or 4 byte length decided by the ws_payload_length.
	// Do we need extended length?
	package_begin = websocket_write_header(ws_payload_begin, ws_payload_length, &package_length);

	// print_hex(package_begin, package_length);

	// Write message.
	netconn_write(connection->conn, package_begin, package_length, NETCONN_COPY);

	pool_free(connection_data_pool, response);
	return exit;
}

/**
 * Adds the websocket header to the payload. Make sure, you have 4 bytes space before the payload (Also
 * for the payload pointer!). The package begin will be returned and the complete package length
 * written to packge_length.
 *
 * Ex.: uint8_t* buffer = <some memory>
 * uint8_t* payload = buffer+4;
 * <put something into payload>
 * uint16_t package_length;
 * uint8_t package_begin = websocket_write_header(payload, payload_length, &package_length);
 */
uint8_t* websocket_write_header(uint8_t* payload, uint16_t playload_length, uint16_t* package_length) {
	uint8_t* package_begin;
	if (playload_length > 126) { // Yes.
		package_begin = payload - 4;
		package_begin[0] = WEBSOCKET_BINARY_PACKAGE;
		package_begin[1] = 127; // Enable extended length
		// Now follows the 16 bit extended payload length in network byte order...
		package_begin[2] = (playload_length & 0xFF00) >> 8;
		package_begin[3] = playload_length & 0xFF;
		*package_length = playload_length + 4;
	} else {
		// No extended length needed.
		package_begin = payload - 2;
		package_begin[0] = WEBSOCKET_BINARY_PACKAGE;
		package_begin[1] = playload_length;
		*package_length = playload_length + 2;
	}
	return package_begin;
}

/**
 * Unmasks the payload from the client. The masking key must be exactly 4 bytes.
 * The data is altered in place.
 *
 * The masking is interpreting the key as uint32_t, and xor-ing it with the data every
 * 4 Bytes. The remainder is also xor-ed with the beginning of the key. For performance, this
 * is done by blocks 0f 4 byte.
 */
static void unmask_payload(uint8_t* payload, uint8_t* masking_key, uint16_t len) {
	uint16_t blocks = len >> 2; // how much blocks of 32 bit.
	uint32_t* payload_in_blocks = (uint32_t*)payload;
	uint32_t* masking_key_in_blocks = (uint32_t*)masking_key;

	for (uint16_t block = 0; block < blocks; block++) {
		payload_in_blocks[block] = payload_in_blocks[block] ^ *masking_key_in_blocks;
	}

	// Take care of the remainder (max 3 bytes)
	for (uint16_t i = len & (~0x03); i < len; i++) {
		payload[i] = payload[i] ^ masking_key[i];
	}
}

/**
 * Writes the HTTP response to the connection, if the request headers are valid for WebSocket.
 * Returnes 1, if this was successfull.
 */
uint8_t websocket_handshake(connection_t* connection, request_headers_t* headers) {
	if (!verify_headers(headers)) {
		return 0;
	}

	char accept_header[WEBSOCKET_ACCEPT_LENGTH + 1];
	create_accept_header_value(headers->sec_websocket_key, accept_header);

	char response[128 + WEBSOCKET_ACCEPT_LENGTH];
	sprintf(response, "HTTP/1.1 101 Switching Protocols"CRLF"Upgrade: WebSocket"CRLF"Connection: upgrade"CRLF
			"Sec-WebSocket-Accept: %s"CRLF""CRLF, accept_header);
	netconn_write(connection->conn, response, strlen(response), NETCONN_COPY);
	return 1;
}

/**
 * Returns 1, if the given headers are valid to establish a WebSocket connections.
 */
static uint8_t verify_headers(request_headers_t* headers) {
	// Host and origin must be set
	if (NULL == headers->host || NULL == headers->origin) {
		printf("host of origin is null\n");
		return 0;
	}

	// upgrade need to have "websocket" in it:
	if (NULL == headers->upgrade || !is_strcistr(headers->upgrade, "websocket")) {
		printf("not websocket in upgrade\n");
		return 0;
	}

	// connection must include "upgrade":
	if (NULL == headers->connection || !is_strcistr(headers->connection, "upgrade")) {
		printf("not upgrade in connection\n");
		return 0;
	}

	// sec-websocket-key must be valid base 64 with 16 Bytes.
	if (NULL == headers->sec_websocket_key || !is_valid_key(headers->sec_websocket_key)) {
		printf("sec ws key is not valid\n");
		return 0;
	}

	// sec-websocket-version must be "13"
	if (NULL == headers->sec_websocket_version || !strcmp(headers->sec_websocket_version, "\"13\"")) {
		printf("sec ws version is not 13\n");
		return 0;
	}
	return 1;
}

/**
 * Creates the value for the accept header in the HTTP response. The key (sec_websocket_key) is
 * needed for this. The accept header is the base64 of the sha1 of the key + some magic. The magic
 * is just appended to the key (string concatenation, not more).
 *
 * The accept_header must provide at least 29 characters space: 28 chars the resulting key, one for
 * the \0.
 */
static void create_accept_header_value(char* key, char* accept_header) {
	// Some space needed
	char buffer[WEBSOCKET_KEY_LENGTH + WEBSOCKET_MAGIC_LENGTH + 1];
	unsigned char hash[SHA_DIGEST_LENGTH];

	// Copy key and magic into the buffer
	strcpy(buffer, key);
	strcpy(buffer + WEBSOCKET_KEY_LENGTH, WEBSOCKET_MAGIC);
	size_t length = strlen(buffer);

	// calc the hash and format as base 64 into the accept header.
	SHA1((const unsigned char*)buffer, length, hash);
	to_base64(hash, accept_header, SHA_DIGEST_LENGTH);

	// Add 0 terminator
	accept_header[WEBSOCKET_ACCEPT_LENGTH] = 0;
}

/**
 * Returns 1, if the key is a valid websocket key.
 *
 * It must have 22 base64 characters and two '=' at the end. The actual value is
 * not important.
 */
static uint8_t is_valid_key(const char* key) {
    if (strlen(key) != WEBSOCKET_KEY_LENGTH || key[WEBSOCKET_KEY_LENGTH-1] != '='
        || key[WEBSOCKET_KEY_LENGTH-2] != '=') {
        return 0;
    }
    for (int i = 0; i < 22; i++) {
        if (!is_base64_char(key[i])) {
            return 0;
        }
    }
    return 1;
}
