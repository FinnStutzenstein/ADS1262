/*
 * pool.h
 *
 *  Created on: Oct 22, 2018
 *      Author: finn
 */

#ifndef POOL_H_
#define POOL_H_

#include "stdint.h"
#include "stdlib.h"
#include "cmsis_os.h"
#include "sys/cdefs.h"

#ifdef __cplusplus
 extern "C" {
#endif

#define POOL_USAGE_HIGH_WATERMARK

#define POOL_FREE				1
#define POOL_USED				0

#define POOL_OK					1
#define POOL_ERR				0

/**
 * Pool types: Static uses our implementation. You have to provide the memory for it.
 * Dynamic uses FreeRTOS memory implementation, so the memory is provided by the heap.
 */
enum PoolType {
	PoolTypeStatic,
	PoolTypeDynamic,
};

typedef struct {
	enum PoolType type;
	uint32_t entrycount;
	uint32_t entrysize;
	union {
		uint8_t* stat;
		osPoolId dyn;
	} pool;
	void** free_management;
#ifdef POOL_USAGE_HIGH_WATERMARK
	uint32_t usage_high_watermark;
#endif
} Pool;

#define __DEFINE_POOL_MEMORY(name, entrycount, entrytype)\
	uint8_t __aligned(4) __pool_##name[(entrycount) * sizeof(entrytype)]

#define __DEFINE_POOL_FREE_MANAGEMENT(name, entrycount)\
	void* __aligned(4) __pool_free_management_##name[(entrycount)]

#define __DEFINE_POOL_OBJECT_FOR_STATIC(name, entrycount, entrytype)\
	Pool __pool_object_##name = {PoolTypeStatic, (entrycount), sizeof(entrytype), {(uint8_t*)(__pool_##name)},\
			(void**)(__pool_free_management_##name)}

#define __DEFINE_POOL_OBJECT_FOR_DYNAMIC(name, entrycount, entrytype)\
	Pool __pool_object_##name = {PoolTypeDynamic, (entrycount), sizeof(entrytype), {NULL},\
			(void**)(__pool_free_management_##name)}

#define __DEFINE_POOL(name) Pool* name = &__pool_object_##name

/**
 * Defines a pool. The memory for it is declarated at this position.
 * name: The name of the pool
 * entrycount: The max amount of objects.
 * entrytype: The type of the objects.
 */
#define DEFINE_POOL(name, entrycount, entrytype)\
	__DEFINE_POOL_MEMORY(name, entrycount, entrytype);\
	__DEFINE_POOL_FREE_MANAGEMENT(name, entrycount);\
	__DEFINE_POOL_OBJECT_FOR_STATIC(name, entrycount, entrytype);\
	__DEFINE_POOL(name);

/**
 * Defines a pool. The memory for it is reserved in the given section.
 * name: The name of the pool
 * entrycount: The max amount of objects.
 * entrytype: The type of the objects.
 * section: The section where the memory is reserved
 */
#define DEFINE_POOL_IN_SECTION(name, entrycount, entrytype, section)\
	__DEFINE_POOL_MEMORY(name, entrycount, entrytype) __section((section));\
	__DEFINE_POOL_FREE_MANAGEMENT(name, entrycount) __section((section));\
	__DEFINE_POOL_OBJECT_FOR_STATIC(name, entrycount, entrytype);\
	__DEFINE_POOL(name);

/**
 * Defines a pool. It uses the heap for memory.
 * name: The name of the pool
 * entrycount: The max amount of objects.
 * entrytype: The type of the objects.
 */
#define DEFINE_POOL_IN_HEAP(name, entrycount, entrytype)\
	__DEFINE_POOL_FREE_MANAGEMENT(name, entrycount);\
	__DEFINE_POOL_OBJECT_FOR_DYNAMIC(name, entrycount, entrytype);\
	__DEFINE_POOL(name);

void pool_init(Pool* pool);
void* pool_alloc(Pool* pool);
uint8_t pool_free(Pool* pool, void* block);
void** pool_get_entries(Pool* pool);
uint32_t pool_get_free_entries_count(Pool* pool);
uint32_t pool_get_used_entries_count(Pool* pool);

#ifdef POOL_USAGE_HIGH_WATERMARK
uint32_t pool_get_usage_high_watermark(Pool* pool);
#endif

#ifdef __cplusplus
}
#endif

#endif /* POOL_H_ */
