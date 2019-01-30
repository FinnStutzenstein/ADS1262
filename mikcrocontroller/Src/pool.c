/*
 * pool.c
 *
 *  Created on: Oct 22, 2018
 *      Author: finn
 */


#include "pool.h"

#ifdef POOL_USAGE_HIGH_WATERMARK
#define POOL_UPDATE_USAGE_HIGH_WATERMARK(x)	pool_update_usage_high_watermark(x)
static void pool_update_usage_high_watermark(Pool* pool);
#else
#define POOL_UPDATE_USAGE_HIGH_WATERMARK(x)	(void)
#endif

/**
 * Inits the pool. Do init every pool before using it!
 */
void pool_init(Pool* pool) {
	if (pool->type == PoolTypeDynamic) {
		const osPoolDef_t pool_def = { pool->entrycount, pool->entrysize, NULL };
		pool->pool.dyn = (void*)osPoolCreate(&pool_def);
	}
	for (uint32_t i = 0; i < pool->entrycount; i++) {
		pool->free_management[i] = NULL;
	}
#ifdef POOL_USAGE_HIGH_WATERMARK
	pool->usage_high_watermark = 0;
#endif
}

/**
 * Allocates memory in the pool. Returns NULL if the pool is full.
 */
void* pool_alloc(Pool* pool) {
	if (pool->type == PoolTypeStatic) {
		for (uint32_t i = 0; i < pool->entrycount; i++) {
			if (NULL == pool->free_management[i]) {
				pool->free_management[i] = pool->pool.stat + i*pool->entrysize;
				POOL_UPDATE_USAGE_HIGH_WATERMARK(pool);
				return (void*)pool->free_management[i];
			}
		}
	} else { // dynamic.
		for (uint32_t i = 0; i < pool->entrycount; i++) {
			if (NULL == pool->free_management[i]) {
				void* obj = osPoolAlloc(pool->pool.dyn);
				if (NULL == obj) { // something is async... We think, that we have space, but cannot allocate memory??
					Error_Handler();
				}
				pool->free_management[i] = obj;
				POOL_UPDATE_USAGE_HIGH_WATERMARK(pool);
				return obj;
			}
		}
	}
	return NULL;
}

/**
 * Frees the given memory block in the pool.
 */
uint8_t pool_free(Pool* pool, void* block) {
	int32_t index = 0;

	if (pool->type == PoolTypeStatic) {
		while(index < pool->entrycount && (pool->pool.stat + index*pool->entrysize) != (uint8_t*)block) {
			index++;
		}
		if (index == pool->entrycount) {
			return POOL_ERR;
		} else {
			pool->free_management[index] = NULL;
			return POOL_OK;
		}
	} else {
		// Free the block in the OS. Regardless of success, remove it here also.
		osPoolFree(pool->pool.dyn, block);

		while(index < pool->entrycount && pool->free_management[index] != block) {
			index++;
		}
		if (index == pool->entrycount) {
			return POOL_ERR;
		} else {
			pool->free_management[index] = NULL;
			return POOL_OK;
		}
	}
}

/**
 * Ger an array of pointers to all places in the pool. A NULL pointer
 * means, that this place is not allocated.
 */
inline void** pool_get_entries(Pool* pool) {
	return pool->free_management;
}

/**
 * Count all free spaces in the pool.
 */
inline uint32_t pool_get_free_entries_count(Pool* pool) {
	return pool->entrycount - pool_get_used_entries_count(pool);
}

/**
 * Get the amount of used entries in the pool.
 */
uint32_t pool_get_used_entries_count(Pool* pool) {
	uint32_t count = 0;
	for (uint32_t i = 0; i < pool->entrycount; i++) {
		if (pool->free_management[i] != NULL) {
			count++;
		}
	}
	return count;
}

/**
 * Some statistics. Updates the high watermark. This should be called, if some memor is allocated.
 */
#ifdef POOL_USAGE_HIGH_WATERMARK
static void pool_update_usage_high_watermark(Pool* pool) {
	uint32_t usage = pool_get_used_entries_count(pool);
	if (usage > pool->usage_high_watermark) {
		pool->usage_high_watermark = usage;
	}
}

/**
 * Returns the high watermark of the pool.
 */
inline uint32_t pool_get_usage_high_watermark(Pool* pool) {
	return pool->usage_high_watermark;
}
#endif
