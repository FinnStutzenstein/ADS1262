/*
 * fft_memory.h
 *
 *  Created on: Jan 3, 2019
 *      Author: finn
 */

#ifndef FFT_MEMORY_H_
#define FFT_MEMORY_H_

#include "sys/cdefs.h"
#include "config.h"
#include "fft.h"

#ifdef __cplusplus
extern "C" {
#endif

// This should be a correct approximation of the needed space for each FFT.
#define FFT_MEMORY_SIZE		((MAX_FFT_SIZE*2*3*sizeof(FFT_DATATYPE) + FFT_HEADER_SIZE) * MAX_MEASUREMENTS)

uint8_t assign_memory_to_fft_instances(FFT_instance** fft_instances, uint8_t N, uint8_t disable_on_overflow);

#ifdef __cplusplus
}
#endif

#endif /* FFT_MEMORY_H_ */
