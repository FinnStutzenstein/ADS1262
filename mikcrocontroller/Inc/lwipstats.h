/*
 * lwipstats.h
 *
 *  Created on: Oct 15, 2018
 *      Author: finn
 */

#ifndef LWIPSTATS_H_
#define LWIPSTATS_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "config.h"

#ifdef NETWORK_STATS
#include "lwip/opt.h"
#include "lwip/stats.h"

#if (SYS_STATS == 0) || (MEM_STATS == 0) || (MEMP_STATS == 0) || (TCP_STATS == 0) || (IP_STATS == 0)
	#error "You have to enable these stats to enable NETWORK_STATS"
#endif
#endif // NETWORK_STATS

uint16_t format_network_stats(uint8_t* buffer, uint16_t max_length);

#ifdef __cplusplus
}
#endif

#endif // LWIPSTATS_H_
