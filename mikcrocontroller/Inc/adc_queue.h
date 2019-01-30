/*
 * adc_queue.h
 *
 *  Created on: Oct 30, 2018
 *      Author: finn
 */

#ifndef ADC_QUEUE_H_
#define ADC_QUEUE_H_

#include "stdint.h"
#include "stddef.h"

typedef struct {
	void** Q;
	uint32_t length;
	uint32_t head;
	uint32_t marker_head;
	uint32_t count;
	uint8_t marker_updating;
} Queue;


#define QUEUE_OK				1
#define QUEUE_ERR				0

#define __DEFINE_QUEUE_OBJECT(name, length) void* __queue_obj_##name[(length)]

/**
 * Defines a queue with the given name and length.
 */
#define DEFINE_QUEUE(name, length) \
	__DEFINE_QUEUE_OBJECT(name, length);\
	Queue __queue_##name = {__queue_obj_##name, length, 0, 0};\
	Queue* name = &__queue_##name;

uint8_t queue_full(Queue* queue);
uint8_t queue_empty(Queue* queue);
uint32_t queue_free(Queue* queue);
uint32_t queue_allocated(Queue* queue);
uint8_t queue_enqueue(Queue* queue, void* obj);
void* queue_dequeue(Queue* queue);
void* queue_front(Queue* queue);
void queue_reset(Queue* queue);

void* queue_marker_dequeue(Queue* queue);
void* queue_marker_front(Queue* queue);
uint8_t queue_is_marker_updating(Queue* queue);

#endif /* ADC_QUEUE_H_ */
