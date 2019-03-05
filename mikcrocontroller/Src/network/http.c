/*
 * http.c
 *
 *  Created on: Oct 13, 2018
 *      Author: finn
 */
#include "http.h"
#include "lwip/api.h"
#include "string.h"
#include "fatfs.h"
#include "connection.h"
#include "websocket.h"
#include "utils.h"

static void parse_header(char* header_begin, char* header_end, request_headers_t* headers);
static void get_mime_type(char* filename, char* mime_type);
static uint8_t deliver_resource(connection_t* connection, char* resource);
static void send_error(connection_t* connection, char* message, uint16_t statuscode, char* file, int line);

#define _SEND_ERROR(conn, msg, code)			send_error((conn), (msg), (code), __FILE__, __LINE__)
#define SERVER_ERROR(conn, msg)					_SEND_ERROR((conn), (msg), 500)
#define BAD_REQUEST_ERROR(conn, msg)			_SEND_ERROR((conn), (msg), 400)
#define METHOD_NOT_ALLOWED_ERROR(conn, msg)		_SEND_ERROR((conn), (msg), 405)
#define REQUEST_URL_TOO_LONG_ERROR(conn, msg)	_SEND_ERROR((conn), (msg), 414)
#define IM_A_TEAPOT_ERROR(conn, msg)			_SEND_ERROR((conn), (msg), 418)

volatile uint8_t http_permitted = 1;

// HTTP request:
// GET <resource> <protocol>CRLF
// header1: value1 CRLF
// ...
// headerN: valueN CRLF
// CRLF
// <payload>

uint8_t handle_HTTP(connection_t* connection, uint8_t* _data, uint16_t len) {
	char* data = (char*) _data;

	/*if (data[len-1] != 0) {
		data[len-1] = 0;
	}*/
	if (data[len] != 0) {
		data[len] = 0;
	}

	if (strncmp(data, "GET", 3) != 0) {
		METHOD_NOT_ALLOWED_ERROR(connection, "Just GET requests are valid.");
		return EXIT;
	}

	// get the resource by replacing the space between the resource and method with 0.
	char* resource = data+4;
	char* space = strstr(resource, " ");
	if (NULL == space){
		BAD_REQUEST_ERROR(connection, "The request line is malformed.");
		return EXIT;
	}
	*space = 0;

	// the resource should be max. 255 characters long.
	if (strlen(resource) > 255) {
		REQUEST_URL_TOO_LONG_ERROR(connection, "The max length is 255");
		return EXIT;
	}

	// check for path traversals
	if (strstr(resource, "..") != NULL) {
		BAD_REQUEST_ERROR(connection, "The URL should not contain '..'.");
		return EXIT;
	}

	// the protocol
	char* protocol = space+1;
	char* crlf = strstr(protocol, CRLF);
	if (NULL == crlf) {
		BAD_REQUEST_ERROR(connection, "The request line is malformed.");
		return EXIT;
	}
	*crlf = 0;

	// check for http/1.1 protocol
	if (strcicmp(protocol, "http/1.1") != 0) {
		BAD_REQUEST_ERROR(connection, "The protocol is not supported. Use HTTP/1.1.");
		return EXIT;
	}

	printf("\nGet request. Resource: %s, protocol: %s\n", resource, protocol);

	// Headers, we are searching values for. If some headers are not given, the pointers will be NULL.
	request_headers_t headers = {0};
	char* header_begin = crlf+2; // Skip CRLF
	// Iterate through all headers
	do {
		char* header_end = strstr(header_begin, CRLF);
		if (NULL == header_end) {
			BAD_REQUEST_ERROR(connection, "The headers are malformed");
		}

		if (header_end - header_begin <= 0) {
			// finished headers. this is a double crlf
			// the data lay at header_end + 2;
			break;
		}
		*header_end = 0;
		parse_header(header_begin, header_end, &headers);
		header_begin = header_end + 2; // Skip CRLF
	} while (1);

	// Note: Here may be content, but we do not need it..

	// Catch special routes. Deliver the requested resource per default.
	// Always exit, to keep browser from blocking connections..
	uint8_t exit = EXIT;
	if (strcmp(resource, "/ws") == 0) {
		if (websocket_handshake(connection, &headers)) {
			connection->type = CONNECTION_TYPE_WEBSOCKET;
			exit = NOEXIT;
		} else {
			BAD_REQUEST_ERROR(connection, "The given headers are malformed.");
		}
	} else if (strcmp(resource, "/brew-coffee") == 0) {
		IM_A_TEAPOT_ERROR(connection, "I'm not able to brew coffee, tea is preferred. Can you bring me some?");
	} else {
		deliver_resource(connection, resource);
	}

	return exit;
}

/**
 * Parses one header line. The structure should be <key: "value">, with optional whitespaces
 * (maybe multiple) after the colon. Assign the raw value (with trimmed whitespaces to the header
 * struct).
 * If the heaerline is malfomed, ignore it.
 */
static void parse_header(char* header_begin, char* header_end, request_headers_t* headers) {
	// get the resource by replacing the space between the resource and method with 0.
	char* colon = strstr(header_begin, ":");
	if (NULL == colon){
		return; // skip this line
	}
	*colon = 0;
	// Now, the key is in header_begin.

	char* value = colon + 1;
	// Trimm all whitespaces at the end or beginning
	while(' ' == *value) {
		value++;
	}
	// is something left?
	if (value != header_end) {
		char* last_character = header_end - 1;
		while (' ' == *last_character) {
			last_character--;
		}
		*(last_character + 1) = 0;
	}

	if (strcicmp(header_begin, "host") == 0) {
		headers->host = value;
	} else if (strcicmp(header_begin, "upgrade") == 0) {
		headers->upgrade = value;
	} else if (strcicmp(header_begin, "connection") == 0) {
		headers->connection = value;
	} else if (strcicmp(header_begin, "sec-websocket-key") == 0) {
		headers->sec_websocket_key = value;
	} else if (strcicmp(header_begin, "sec-websocket-version") == 0) {
		headers->sec_websocket_version = value;
	} else if (strcicmp(header_begin, "origin") == 0) {
		headers->origin = value;
	}
}

/**
 * Delivers the requested resource to the connection.
 * Tries to open the existing file. If it is a path, or a non existing file, the index.html
 * is deliverd. If any other error occurs, or the index.html is not found, an error will be send.
 * Returns EXIT on failure, NOEXIT on success.
 */
static uint8_t deliver_resource(connection_t* connection, char* resource) {
	// Try to get some memory
	connection_data_t* data_buffer = (connection_data_t*) pool_alloc(connection_data_pool);
	if (NULL == data_buffer) {
		SERVER_ERROR(connection, "No memory for data_buffer.");
		return EXIT;
	}

	// Format the filename
	if (strlen(resource) == 1 && resource[0] == '/') {
		strcpy(data_buffer->filename, "0:/www/index.html");
	} else if (resource[0] == '/') {
		sprintf(data_buffer->filename, "0:/www%s", resource);
	} else {
		sprintf(data_buffer->filename, "0:/www/%s", resource);
	}

	uint8_t exit = NOEXIT;
	FIL file;
	FRESULT fres;

	// Try to open the file
	if ((fres = f_open(&file, data_buffer->filename, FA_READ)) != FR_OK) {
		char error_buffer[32];
		if (fres == FR_NO_FILE || fres == FR_NO_PATH || fres == FR_INVALID_NAME) {
			// file was not found or is a directory. Send index.html instead.
			strcpy(data_buffer->filename, "0:/www/index.html");
			if ((fres = f_open(&file, data_buffer->filename, FA_READ)) != FR_OK) {
				snprintf(error_buffer, sizeof(error_buffer), "File open error: %d", fres);
				SERVER_ERROR(connection, error_buffer);
				exit = EXIT;
			}
		} else {
			snprintf(error_buffer, sizeof(error_buffer), "File open error: %d", fres);
			SERVER_ERROR(connection, error_buffer);
			exit = EXIT;
		}
	}

	// Continue, if the file was opened.
	if (NOEXIT == exit) {
		printf("openend %s\n", data_buffer->filename);
		uint32_t filesize = f_size(&file);

		// Get the MIME type.
		char mime_type[25]; // application/octet-stream is the longest with 24+1 chars.
		get_mime_type(data_buffer->filename, mime_type);

		// Write response header.
		char response[128]; // The sing below are about 80 chars. With mime-type max 104, so about 20 chars for the
		// filesize should be enough.
		int response_len = snprintf(response, sizeof(response),
				"HTTP/1.1 200 OK"CRLF"Connection: Close"CRLF"Content-Type: %s"CRLF"Content-Length: %lu"CRLF""CRLF,
				mime_type, filesize);
		netconn_write(connection->conn, response, response_len, NETCONN_COPY);

		// Read file.
		unsigned int bytes_read;
		// Time the reading and sending
		uint64_t start = measure_reference_timer_ticks;
		while (1) {
			// Wait to read.
			while (!http_permitted) {
				osDelay(10);
			}
			fres = f_read(&file, data_buffer->buffer, sizeof data_buffer->buffer, &bytes_read);
			if (fres != FR_OK || bytes_read == 0) {
				break; // error or eof
			}
			printf("bytes read: %u\n", bytes_read);

			// Send the 64K in 16K chunks..
			unsigned int bytes_to_send = bytes_read;
			char *bytes_to_send_position = data_buffer->buffer;
			const unsigned int blocksize = 16384;

			while (bytes_to_send > blocksize) {
				// Wait between sucessive transmissions, if http is blocked.
				while (!http_permitted) {
					osDelay(10);
				}
				netconn_write(connection->conn, bytes_to_send_position, blocksize, NETCONN_COPY);
				printf("bytes send: %u\n", blocksize);
				bytes_to_send -= blocksize;
				bytes_to_send_position += blocksize;
				osDelay(1); // general delay
			}
			// Write the rest.
			if (bytes_to_send > 0) {
				while (!http_permitted) {
					osDelay(10);
				}
				netconn_write(connection->conn, bytes_to_send_position, bytes_to_send, NETCONN_COPY);
				printf("bytes send: %u\n", bytes_to_send);
			}
			printf("bytes send (sum): %u\n", bytes_read);
		}
		uint64_t end = measure_reference_timer_ticks;
		printf("done reading\ntook %lums\n", (uint32_t)(end-start)/100);

		if ((fres = f_close(&file)) != FR_OK) {
			printf("error closing file: %d\n", fres);
			exit = EXIT;
		}
	}

	pool_free(connection_data_pool, data_buffer);
	return exit;
}

/**
 * Guesses the mime type of the given filename. Puts the mimetype into
 * the given buffer. The buffer needs at least 24+1 space, because
 * "application/octet-stream" is the longest string with 24 chars.
 */
static void get_mime_type(char* filename, char* mime_type) {
	char *ext = NULL;
	size_t filename_len = strlen(filename);
	// search the last dot. dot+1 is the file extension.
	for (int i = filename_len - 1; i >= 0; i--) {
		if (filename[i] == '.') {
			ext = filename + i;
		}
	}
	if (NULL == ext) {
		strcpy(mime_type, "application/octet-stream");
	} else {
		ext++;
		if (strcmp(ext, "html") == 0) {
			strcpy(mime_type, "text/html");
		} else if (strcmp(ext, "js") == 0) {
			strcpy(mime_type, "application/javascript");
		} else if (strcmp(ext, "css") == 0) {
			strcpy(mime_type, "text/css");
		} else if (strcmp(ext, "ico") == 0) {
			strcpy(mime_type, "imge/x-icon");
		} else if (strcmp(ext, "png") == 0) {
			strcpy(mime_type, "imge/png");
		} else if (strcmp(ext, "map") == 0) {
			strcpy(mime_type, "application/octet-stream");
		} else {
			printf("Unkown MIME type: %s", ext);
			strcpy(mime_type, "application/octet-stream");
		}
	}
}

/**
 * Writes the given message as a http error (with the given statuscode) to the connection.
 * If the status code is 500 (server error) the file and line are included. The maximum message
 * length is 511 and will be cut, if the given message is longer.
 */
static void send_error(connection_t* connection, char* message, uint16_t statuscode, char* file, int line) {
	static char error_response[1024+128]; // There may be race conditions if multiple threads try to use this; But this should be a minor case.
	// Cut message to 512 bytes.
	static char error_message[512];
	int message_len = snprintf(error_message, 512, "%s", message);
	static char error_message_with_line[2*512];

	switch(statuscode) {
	case 500:
		message_len = sprintf(error_message_with_line, "%s (file: %s, line: %d)", error_message, file, line);
		message_len = sprintf(error_response, "HTTP/1.1 500 Internal Server Error"CRLF"Connection: close"CRLF"Content-Length: %d"CRLF""CRLF"%s",
					message_len, error_message_with_line);
		break;
	case 400: // Bad request
		message_len = sprintf(error_response, "HTTP/1.1 400 Bad Request"CRLF"Connection: close"CRLF"Content-Length: %d"CRLF""CRLF"%s",
					message_len, error_message);
		break;
	case 405: // Method Not Allowed
		message_len = sprintf(error_response, "HTTP/1.1 405 Method Not Allowed"CRLF"Connection: close"CRLF"Content-Length: %d"CRLF""CRLF"%s",
					message_len, error_message);
		break;
	case 414: // Request-URL Too Long
		message_len = sprintf(error_response, "HTTP/1.1 414 Request-URL Too Long"CRLF"Connection: close"CRLF"Content-Length: %d"CRLF""CRLF"%s",
					message_len, error_message);
		break;
	case 418: // I'm a teapot
		message_len = sprintf(error_response, "HTTP/1.1 418 I'm a teapot"CRLF"Connection: close"CRLF"Content-Length: %d"CRLF""CRLF"%s",
					message_len, error_message);
		break;
	default:
		Error_Handler();
	}

	netconn_write(connection->conn, error_response, message_len, NETCONN_COPY);
}
