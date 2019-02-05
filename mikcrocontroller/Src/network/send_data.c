/*
 * send_data.c
 *
 * Takes care about transmitting data over the network.
 *
 * The `send_task_function` tries to send data from the associated queue over the network.
 * For all four data types (see connection.h) gets an own queue and task to handle
 * the different payloads:
 * The FFT task gives huge amounts of data (>256KB) slowly, but measurements are fast (~4KB) in ~186 times
 * per second.
 *
 * The functions below takes care about putting the data (also from interrupts) into the queues.
 *
 *  Created on: Oct 24, 2018
 *      Author: finn
 */
#include "send_data.h"
#include "pool.h"
#include "error.h"
#include "lwip/api.h"
#include "adc_queue.h"
#include "http.h"
#include "cmsis_os.h"
#include "string.h"
#include "utils.h"
#include "state.h"
#include "measure.h"
#include "websocket.h"

typedef struct {
	Queue* queue;
	uint8_t is_data_task;
} send_task_argument_t;

// 4 arguments for every queue
send_task_argument_t send_task_arguments[4];

uint32_t lock_http_threshold;
uint32_t release_http_threshold;

uint8_t send_queue_flush;
uint8_t initialized = 0;
Queue* queues[4];

// Space for all data descriptors
DEFINE_POOL_IN_SECTION(data_descriptor_pool, DATA_DESCRIPTOR_POOL_SIZE, DataDescriptor, ".extsram");
DEFINE_QUEUE(debug_queue, DEBUG_QUEUE_SIZE);
DEFINE_QUEUE(status_queue, STATUS_QUEUE_SIZE);
DEFINE_QUEUE(data_queue, DATA_QUEUE_SIZE);
DEFINE_QUEUE(fft_queue, FFT_QUEUE_SIZE);

static uint8_t internal_send_data(uint8_t send_type, uint8_t* data, uint32_t len, void (*callback)(void*), void* cb_argument);
void send_task_function(void const *argument);

/**
 * Initializes the data send task.
 */
void send_data_init() {
	pool_init(data_descriptor_pool);
	send_queue_flush = 0;

	lock_http_threshold = (uint32_t)((float)(DATA_QUEUE_SIZE)/4);
	release_http_threshold = (uint32_t)((float)(DATA_QUEUE_SIZE)/8);

	queues[0] = debug_queue;
	queues[1] = status_queue;
	queues[2] = data_queue;
	queues[3] = fft_queue;

	send_task_arguments[0].queue = debug_queue;
	send_task_arguments[0].is_data_task = 0;
	send_task_arguments[1].queue = status_queue;
	send_task_arguments[1].is_data_task = 0;
	send_task_arguments[2].queue = data_queue;
	send_task_arguments[2].is_data_task = 1;
	send_task_arguments[3].queue = fft_queue;
	send_task_arguments[3].is_data_task = 0;

	osThreadDef(send_task, send_task_function, osPriorityNormal, 4, 512);
	for (int i = 0; i < 4; i++) {
		osThreadCreate(osThread(send_task), (void*)(send_task_arguments + i));
	}
	initialized = 1;
}

/**
 * The data send task.
 *
 * It handles sending all data given in the queue. The queue is provided via the argument.
 * Data descriptors contains a pointer to the data, the data length, data type and an optional callback.
 * Also an array for book-keeping, to which connection the data was send.
 *
 * The current data descriptor will be removed, if the data was transmitted to every associated connection.
 */
void send_task_function(void const *_arg) {
	send_task_argument_t* arg = (send_task_argument_t*)_arg;
	Queue* queue = arg->queue;
	while(1) {
		osDelay(1);

		// If we got a queue overflow, we want to inform the client about this.
		// So we need to make space. If we had an overrun, we can clear the complete queue...
		if (arg->is_data_task && send_queue_flush) {
			http_permitted = 1;
			send_queue_flush = 0;
			update_complete_state(1);
		}

		// try at most to empty the queue.
		uint32_t queue_fill = queue_allocated(queue);
		for (uint32_t i = 0; i < queue_fill; i++) {
			DataDescriptor* d = (DataDescriptor*) queue_marker_front(queue);
			if (d == NULL) {
				break;
			}

			// check every connection
			connection_t **connections = (connection_t**)pool_get_entries(connection_pool);
			for (uint32_t j = 0; j < connection_pool->entrycount; j++) {
				if (d->connections[j]) {
					continue;
				}
				connection_t* c = connections[j];

				if (NULL == c || NULL == c->conn || !(d->type & c->send_type)) {
					d->connections[j] = 1; // Connection is handled (because it does not exist, or the send type does not match).
				} else {
					// We need to have a write access..
					if (osMutexWait(c->write_mutex, 1) == osOK) {

						// get the datapointer. It differs, which protocol we need to send the data to.
						uint8_t* dataptr = NULL;
						uint16_t datalen = 0;
						if (c->type == CONNECTION_TYPE_TCP) {
							dataptr = d->adcp_dataptr;
							datalen = d->adcp_len;
						} else if (c->type == CONNECTION_TYPE_WEBSOCKET) {
							dataptr = d->ws_dataptr;
							datalen = d->ws_len;
						} else {
							// This should never happen, or I missed a connection type
							d->connections[j] = 1;
							osMutexRelease(c->write_mutex);
							continue;
						}

						// Write the data
						netconn_write(c->conn, dataptr, datalen, NETCONN_COPY);

						// Connection handeled
						d->connections[j] = 1;
						osMutexRelease(c->write_mutex);
					}
				}
			}


			// check of data is send to all connections
			uint8_t send_to_all = 1;
			for (uint32_t j = 0; j < connection_pool->entrycount; j++) {
				if (!d->connections[j]) {
					send_to_all = 0;
					break;
				}
			}
			if (send_to_all) {
				queue_marker_dequeue(queue);

				// Data is all send, call the callback.
				if (NULL != d->callback) {
					(*d->callback)(d->cb_argument);
				}
			}
		}

		// Maybe release a blocked HTTP service, if this is the data thread.
		if (arg->is_data_task) {
			uint32_t count = pool_get_used_entries_count(data_descriptor_pool);
			if (!http_permitted && count < release_http_threshold) {
				http_permitted = 1;
				print_to_debugger_str("RELEASE\n");
			}
		}
	}
}

/**
 * Sends debug data.
 */
void send_debug_data(char* buffer, uint16_t len) {
	if (!initialized) {
		return;
	}
	internal_send_data(SEND_TYPE_DEBUG, (uint8_t*) buffer, len, NULL, NULL);
}

/**
 * Sends the given data via the data send service. The data will be copied into one datadescriptor,
 * if the data size is below DATA_DESCRIPTOR_USER_SPACE. You will get an error, if the data is bigger,
 * so make sure it will fit. For larger chunks of data see send_data_non_copy.
 * The data must be RAW data! Do not add any headers, this will be done for you.
 * Returns 1 on success. Failures can be an out of memory, not initialized, or send_type is NONE.
 * Note: On failure because of memory, the current measurement is stopped, and the queues are flushed!
 */
uint8_t send_data(uint8_t send_type, uint8_t* data, uint16_t len) {
	if (!initialized || send_type == SEND_TYPE_NONE) {
		return 0;
	}

	uint8_t ret = internal_send_data(send_type, data, len, NULL, NULL);
	if (!ret) {
		if (is_measure_active()) {
			measure_stop();
		}
		set_slow_connection_flag();
		send_queue_flush_and_update_status();
	}
	return ret;
}

/**
 * Adds the data to the queue. The provided callback will be called, if the data is transmitted.
 * Make sure your data have at least 7 Bytes of space before the given pointer, that can be accessed.
 * This will be used to add a WebSocket and ADCP header.
 */
inline uint8_t send_data_non_copy(uint8_t send_type, uint8_t* data, uint32_t len, void (*callback)(void*), void* cb_argument) {
	return internal_send_data(send_type, data, len, callback, cb_argument);
}

/**
 * Allocates a datadescriptor, fill it appropriately, set headers to the given data and put in in a queue.
 * Returns 1 on success. Failures can be no memory.
 *
 * if callback is NULL, the data will be copied and must not be bigger then 4K!
 */
static uint8_t internal_send_data(uint8_t send_type, uint8_t* data, uint32_t len, void (*callback)(void*), void* cb_argument) {
	DataDescriptor* dd = pool_alloc(data_descriptor_pool);
	if (NULL == dd) {
		return 0;
	}

	dd->callback = callback;
	dd->cb_argument = cb_argument;

	uint8_t* dataptr;

	// If no callback is given, use the internal space
	if (NULL == callback) {
		// Check data length and copy data.
		if (len > DATA_DESCRIPTOR_USER_SPACE) {
			Error_Handler();
		}
		dataptr = dd->data + DATA_DESCRIPTOR_BUFFER_RESERVED;
		memcpy(dataptr, data, len);
	} else {
		// Just use the external data.
		dataptr = data;
	}

	// Add ADCP header (offset -3 from payload, length 3 bytes)
	dd->adcp_dataptr = adcp_write_header(send_type, dataptr, len, &(dd->adcp_len));

	// Add WS header.
	dd->ws_dataptr = websocket_write_header(dd->adcp_dataptr, dd->adcp_len, &(dd->ws_len));

	dd->type = send_type;
	for (uint32_t i = 0; i < connection_pool->entrycount; i++) {
		dd->connections[i] = 0;
	}

	// Find right queue..
	Queue* queue = NULL;
	switch(send_type) {
	case SEND_TYPE_DEBUG:
		queue = debug_queue;
		break;
	case SEND_TYPE_STATUS:
		queue = status_queue;
		break;
	case SEND_TYPE_DATA:
		queue = data_queue;
		break;
	case SEND_TYPE_FFT:
		queue = fft_queue;
		break;
	default:
		Error_Handler();
	}
	if (queue_enqueue(queue, dd) == QUEUE_ERR) {
		pool_free(data_descriptor_pool, dd);
		return 0;
	}

	for (int i = 0; i < 4; i++) {
		Queue* q = queues[i];
		if (!queue_is_marker_updating(q)) {
			while(q->head != q->marker_head) {
				void* _dd = queue_dequeue(q);
				pool_free(data_descriptor_pool, _dd);
			}
		}
	}

	// Some debug info for a full data queue
	uint32_t pool_count = pool_get_used_entries_count(data_descriptor_pool);
	uint32_t data_queue_count = queue_allocated(data_queue);
	if (pool_count > 8) {
		char b[128];
		int l = sprintf(b, "pool: %lu, dqueue: %lu\n", pool_count, data_queue_count);
		print_to_debugger(b,l);
	}

	// Check for HTTP
	if (queue == data_queue) {
		if (http_permitted && data_queue_count > lock_http_threshold) {
			http_permitted = 0;
			print_to_debugger_str("AQUIRE\n");
		}
	}

	return 1;
}

/**
 * Flushed the data queue. If it was flushed, a status update will be raised.
 */
void send_queue_flush_and_update_status() {
	send_queue_flush = 1;

	for (int i = 0; i < 4; i++) {
		Queue* q = queues[i];
		while(!queue_empty(q)) {
			DataDescriptor* dd = queue_dequeue(q);
			pool_free(data_descriptor_pool, (void*) dd);
		}
	}
}
