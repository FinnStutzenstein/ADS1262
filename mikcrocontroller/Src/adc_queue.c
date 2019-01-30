/*
 * queue.c
 *
 * Used to synchronize datadescriptors for all send_data tasks.
 *
 * Mostly from interrupts (the DRDy one) data must be inserted into the "threaded-world"
 * where we can interact with the network. This queue implementation is the connection
 * between those.
 *
 * Main concern: An interrupt cannot wait. If an interrupt must put data in the queue,
 * we cannot lock it, so this queue must be safe to always put data in, even if some thread is currently
 * accessing it.
 *
 * This is done by having a "marker-head" in the queue. This is just a pointer to the current read-position
 * of the thread. If a thread reads an entry, the head advances, but the queue is not changed.
 * If an interrupt adds new data, it cleans up: It removes all entries, the marker has passed, so the thread
 * already read the data. One gotcha: This action is not allowed, if a thread is modifying the marker head.
 *
 * All marker-operations have the prefix `marker_`
 *
 *  Created on: Oct 30, 2018
 *      Author: finn
 */


#include "adc_queue.h"

/**
 * Returns 1, if the queue is full.
 */
inline uint8_t queue_full(Queue* queue) {
	return queue->count == queue->length;
}

/**
 * Returns 1, if the queue is empty.
 */
inline uint8_t queue_empty(Queue* queue) {
	return queue->count == 0;
}

/**
 * Returns 1, if the marker head is at the front. This means, that a thread has emptied the queue.
 */
inline uint8_t queue_marker_empty(Queue* queue) {
	return (queue->head + queue->count) % queue->length == queue->marker_head;
}

/**
 * Returns the amount of free space in a queue
 */
inline uint32_t queue_free(Queue* queue) {
	return queue->length - queue->count;
}

/**
 * Returns the amount of items in the queue.
 */
inline uint32_t queue_allocated(Queue* queue) {
	return queue->count;
}

/**
 * Enqueues a new object. Returns QUEUE_ERR, if the queue is full.
 * On success QUEUE_OK is returned.
 */
uint8_t queue_enqueue(Queue* queue, void* obj) {
	if (queue_full(queue)) {
		return QUEUE_ERR;
	}

	queue->Q[(queue->head + queue->count) % queue->length] = obj;
	queue->count++;
	return QUEUE_OK;
}

/**
 * Returns the pointer to the object at the front. Returns NULL, if the
 * queue is empty.
 */
void* queue_dequeue(Queue* queue) {
	if (queue_empty(queue)) {
		return NULL;
	}
	void* obj = queue->Q[queue->head];
	// If the maker head is at the normal heads position,
	// We need to also update it.
	if (queue->head == queue->marker_head) {
		queue->head = (queue->head+1) % queue->length;
		queue->marker_head = queue->head;
	} else {
		queue->head = (queue->head+1) % queue->length;
	}
	// One element less.
	queue->count--;
	return obj;
}

/**
 * Returns the position at the marker position. Moves the marker one position back.
 * Returns NULL, if the queue is empty. This operation modifies the queue, so
 * set `marker_updating` to 1.
 */
void* queue_marker_dequeue(Queue* queue) {
	queue->marker_updating = 1;
	if (queue_marker_empty(queue)) {
		queue->marker_updating = 0;
		return NULL;
	}
	void* obj = queue->Q[queue->marker_head];
	queue->marker_head = (queue->marker_head+1) % queue->length;
	queue->marker_updating = 0;
	return obj;
}

/**
 * Returns the element at the front. NULL if empty.
 */
inline void* queue_front(Queue* queue) {
	if (queue_empty(queue)) {
		return NULL;
	}
	return queue->Q[queue->head];
}

/**
 * Returns the ement at the markers front. Null if empty.
 */
inline void* queue_marker_front(Queue* queue) {
	if (queue_marker_empty(queue)) {
		return NULL;
	}
	return queue->Q[queue->marker_head];
}

/**
 * Resets the queue.
 */
inline void queue_reset(Queue* queue) {
	queue->head = queue->count = 0;
}

/**
 * Returns 1, if the merker is currently updating.
 */
inline uint8_t queue_is_marker_updating(Queue* queue) {
	return queue->marker_updating;
}
