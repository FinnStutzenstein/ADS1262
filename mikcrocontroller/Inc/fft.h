/*
 * fft.h
 *
 *  Created on: Nov 20, 2018
 *      Author: finn
 */

#ifndef FFT_H_
#define FFT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sys/cdefs.h"
#include "stdint.h"
#include "cmsis_os.h"
#include "adcp.h"
#include "send_data.h"

#define MAX_FFT_BITS        		14 /* 16384 */
#define MAX_FFT_SIZE        		(1<<MAX_FFT_BITS)
#define MIN_FFT_BITS        		3 /* 16 */
#define MIN_FFT_SIZE        		(1<<MIN_FFT_BITS)
#define FFT_DEFAULT_LENGTH			128;
#define WINDOW_FUNCTIONS			3
#define WINDOW_FUNCTION_TABLE_BITS	(MAX_FFT_BITS+1)
#define WINDOW_FUNCTION_TABLE_SIZE	(1<<WINDOW_FUNCTION_TABLE_BITS)
#define RECTANGULAR_WINDOW_INDEX	0xFF
#define FFT_DATATYPE				float

typedef struct __packed {
	FFT_DATATYPE re;
	FFT_DATATYPE im;
} complex;

typedef struct __packed {
	uint8_t id;
	uint8_t frame_count; // number of frames to send.
	uint8_t frame_number; // the current frame number from 0 to frame_count-1
	uint16_t length;
	uint64_t timestamp;
	float frequence_resolution;
	float wss;
} fft_packet_metadata;

typedef volatile struct {
	uint8_t id;
	uint8_t enabled;
	uint16_t length;
	uint8_t bits; // The bits needed for the length, so length=2^(bits). Keep this in sync..
	uint16_t fill_step;
	uint8_t window_index;

	// Points to raw_buffer_fill + FFT_HEADER_SIZE to quickly access the data.
	FFT_DATATYPE* buffer_fill;
	FFT_DATATYPE* buffer_window_overlap;
	uint64_t timestamp_first_sample;

	// Both raw buffers holds space for the fft packet headers and the data:
	// [FFT_HEADER_ALIGNMENT][fft_packet_header (3)][fft_packet_metadata (11)][fft_length * sizeof(FFT_DATATYPE)]
	uint8_t* raw_buffer_fill;
	uint8_t* raw_buffer_calc_and_send;
	float frequence_resolution;

	// Amount of data already send; This are the bytes for data WITHOUT the packet headers.
	uint32_t bytes_send;

	// This bit indicates, if the instance is currently calculating and sending.
	// It's set to 1, if the fill_buffer is full and cleared, if the fft is done and the data
	// is transmitted.
	uint8_t dirty;

	uint8_t frame_count; // number of frames to send.
	uint8_t frame_number; // the current frame number from 0 to frame_count-1
	osThreadId thread;
} FFT_instance;

// We need to leave some space in the packet to write some network headers
// This DATA_DESCRIPTOR_BUFFER_RESERVED is the right size.
#define FFT_PACKET_HEADER_SIZE		DATA_DESCRIPTOR_BUFFER_RESERVED

#define FFT_HEADER_SIZE				(sizeof(fft_packet_metadata) + FFT_PACKET_HEADER_SIZE)

// align the header to 32 bytes. So FFT_HEADER_ALIGNMENT+FFT_HEADER_SIZE should be a multiple of 32.
#define _FFT_HEADER_REMAINDER		(FFT_HEADER_SIZE & 0x1F)
#define FFT_HEADER_ALIGNMENT		(_FFT_HEADER_REMAINDER == 0 ? 0 : 32-_FFT_HEADER_REMAINDER)

#define FFT_PACKET_DATA_SPACE		(UINT16_MAX - FFT_HEADER_SIZE)

void fft_instance_init_disabled(FFT_instance* fft, uint8_t id);
uint8_t fft_instance_enabled(FFT_instance* fft);
void fft_instance_init(FFT_instance* fft, uint8_t id);
void fft_instance_deinit(FFT_instance* fft);
void fft_set_enabled(FFT_instance* fft, uint8_t enabled);
ProtocolError fft_set_window(FFT_instance* fft, uint8_t window_index);
uint8_t fft_is_valid_length(uint16_t length);
uint8_t fft_set_length(FFT_instance* fft, uint16_t length);
void fft_set_raw_buffer(FFT_instance* fft, uint8_t* raw_buffer);
void fft_clear_buffer_pointers(FFT_instance* fft);
uint32_t fft_needed_buffer_size(FFT_instance* fft);
void fft_prepare_instances(FFT_instance** fft_instances, uint8_t N);
void fft_instance_new_value(FFT_instance *fft, uint32_t value, uint64_t timestamp);

#ifdef __cplusplus
}
#endif

#endif /* FFT_H_ */
