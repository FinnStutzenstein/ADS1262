/*
 * lwipstats.c
 *
 *  Created on: Oct 15, 2018
 *      Author: finn
 */

#include "lwipstats.h"

#ifdef NETWORK_STATS
#include "string.h"

extern struct stats_ lwip_stats;

#define ALIGNED_U32		"5lu"

/**
 * Formats the lwip stats into the given buffer with respect to max_length.
 * Returns the length of the written string without the null terminator.
 */
uint16_t format_network_stats(uint8_t* buffer, uint16_t max_length) {
	char* pos = (char*)buffer;

	// System
	struct stats_sys* sys = &lwip_stats.sys;
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "\nSYS   | used | max | err |\n");
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "sem   %"ALIGNED_U32"%"ALIGNED_U32"%"ALIGNED_U32"\n", (u32_t)sys->sem.used, (u32_t)sys->sem.max, (u32_t)sys->sem.err);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "mutex %"ALIGNED_U32"%"ALIGNED_U32"%"ALIGNED_U32"\n", (u32_t)sys->mutex.used, (u32_t)sys->mutex.max, (u32_t)sys->mutex.err);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "mbox  %"ALIGNED_U32"%"ALIGNED_U32"%"ALIGNED_U32"\n", (u32_t)sys->mbox.used, (u32_t)sys->mbox.max, (u32_t)sys->mbox.err);

	struct stats_proto *proto = &lwip_stats.tcp;
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "\nTCP\t");
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "xmit: %"STAT_COUNTER_F"\n\t", proto->xmit);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "recv: %"STAT_COUNTER_F"\n\t", proto->recv);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "fw: %"STAT_COUNTER_F"\n\t", proto->fw);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "drop: %"STAT_COUNTER_F"\n\t", proto->drop);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "chkerr: %"STAT_COUNTER_F"\n\t", proto->chkerr);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "lenerr: %"STAT_COUNTER_F"\n\t", proto->lenerr);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "memerr: %"STAT_COUNTER_F"\n\t", proto->memerr);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "rterr: %"STAT_COUNTER_F"\n\t", proto->rterr);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "proterr: %"STAT_COUNTER_F"\n\t", proto->proterr);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "opterr: %"STAT_COUNTER_F"\n\t", proto->opterr);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "err: %"STAT_COUNTER_F"\n\t", proto->err);
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "cachehit: %"STAT_COUNTER_F"\n", proto->cachehit);

	struct stats_mem *mem = &lwip_stats.mem;
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "\nMEM HEAP | avail | used |  max | err\n");
	pos += snprintf(pos, max_length - ((char*)buffer - pos), "         | %"ALIGNED_U32" |%"ALIGNED_U32" |%"ALIGNED_U32" |%"ALIGNED_U32"\n",
		(u32_t)mem->avail, (u32_t)mem->used, (u32_t)mem->max, (u32_t)mem->err);

	char whitespaces[15+1];
	whitespaces[0] = '\0';

	for (int i = 0; i < MEMP_MAX; i++) {
		// Max name length is 15.
		mem = lwip_stats.memp[i];

		int name_len = strlen(mem->name);
		if (name_len <= 15) {
			for (int j = 0; j < (15-name_len); j++) {
				whitespaces[j] = ' ';
			}
			whitespaces[15 - name_len] = '\0';
		}

		pos += snprintf(pos, max_length - ((char*)buffer - pos), "\nMEM %s%s | avail | used |  max | err\n", mem->name, whitespaces);
		pos += snprintf(pos, max_length - ((char*)buffer - pos), "                    | %"ALIGNED_U32" |%"ALIGNED_U32" |%"ALIGNED_U32" |%"ALIGNED_U32"\n",
			(u32_t)mem->avail, (u32_t)mem->used, (u32_t)mem->max, (u32_t)mem->err);
	}

	return strlen((char*)buffer);
}

#else

uint16_t send_network_stats(uint8_t* buffer) {
	buffer[0] = REPSONSE_NOT_ENABLED;
	return 1;
}

#endif // LWIP_STATS
