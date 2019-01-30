/*
 * fft_memory.c
 *
 *  Created on: Jan 3, 2019
 *      Author: finn
 */
#include "fft_memory.h"

uint8_t fft_memory[FFT_MEMORY_SIZE] __used __section(".extsram");

/**
 * Assignes memory to all N given instances. Returns 1 on success.
 * This might fail if not enough memory is available. If disable_on_overflow is 1, these instances will be disabled and
 * the assignment will be OK.
 */
uint8_t assign_memory_to_fft_instances(FFT_instance** fft_instances, uint8_t N, uint8_t disable_on_overflow) {
	uint32_t mem_index = 0; // current position for the fft_memory.
	for (int i = 0; i < N; i++) {
		FFT_instance* fft = fft_instances[i];
		// clear buffer pointers; this will result in the instance not being ready.
		fft_clear_buffer_pointers(fft);
		if (!fft_instance_enabled(fft)) {
			continue;
		}

		uint32_t needed_buffer_size = fft_needed_buffer_size(fft);
		if ((mem_index + needed_buffer_size) > FFT_MEMORY_SIZE) {
			// overflow!
			if (disable_on_overflow) {
				fft_set_enabled(fft, 0);
				continue;
			} else {
				return 0;
			}
		}

		// No overflow, give the instance the needed buffer space.
		fft_set_raw_buffer(fft, fft_memory+mem_index);
		mem_index += needed_buffer_size;
	}
	return 1;
}
