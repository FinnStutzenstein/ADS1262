/*
 * fft.c
 *
 *  Created on: Nov 20, 2018
 *      Author: finn
 */
#include "fft.h"
#include "send_data.h"
#include "stdio.h"
#include "config.h"
#include "measure.h"
#include "string.h"

// autogenerated
#include "twiddlefactors.h"
#include "bitrev.h"
#include "windowfunctions.h"

static void fft_task_function(void const *argument);
static void fft_set_package_metadata(FFT_instance* fft, fft_packet_metadata* m);
static void fft_transmitted(void* fft);
static void fft_transmit_frame(FFT_instance* fft);
static void FFT(complex* samples, uint16_t N);
static uint8_t get_bits(uint16_t number);

osThreadDef(fft_task, fft_task_function, osPriorityNormal, MAX_MEASUREMENTS, 512);

/**
 * The buffer needs to fit the size given by fft_needed_buffer_size
 */
void fft_instance_init(FFT_instance* fft, uint8_t id) {
	fft->id = id;
	fft->enabled = 0;
	fft->length = FFT_DEFAULT_LENGTH;
	fft->bits = get_bits(fft->length);
	fft->fill_step = 0;
	fft->dirty = 0;
	fft->bytes_send = 0;
	fft->window_index = RECTANGULAR_WINDOW_INDEX;

	fft->thread = osThreadCreate(osThread(fft_task), (void*)(fft));
}

/**
 * This stops the assigned thread from the fft instance.
 */
void fft_instance_deinit(FFT_instance* fft) {
	osThreadTerminate(fft->thread);
	fft->thread = NULL;
	fft_clear_buffer_pointers(fft);
	fft_set_enabled(fft, 0);
}

/**
 * Check, if the given fft instance is ready to take some data.
 */
inline uint8_t fft_instance_ready(FFT_instance* fft) {
	return !fft->enabled || NULL != fft->raw_buffer_fill;
}

/**
 * Check, if the given fft instance is enabled.
 */
inline uint8_t fft_instance_enabled(FFT_instance* fft) {
	return fft->enabled;
}

/**
 * Enable or disable the given fft instance.
 */
inline void fft_set_enabled(FFT_instance* fft, uint8_t enabled) {
	fft->enabled = !!enabled;
}

/**
 * Sets the window of the fft instance to the given window by name.
 * Tries to find this window.
 */
protocol_error_t fft_set_window(FFT_instance* fft, uint8_t window_index) {
	// Check, if the index is correct
	if (RECTANGULAR_WINDOW_INDEX != window_index && window_index >= WINDOW_FUNCTIONS) {
		return RESPONSE_FFT_INVALID_WINDOW;
	}
	fft->window_index = window_index;
	return RESPONSE_OK;
}

/**
 * Returns 1, if the given length os a valid length for the FFT.
 * It must be a power of 2 and between (MIN_FFT_SIZE*2, MAX_FFT_SIZE*2)
 */
inline uint8_t fft_is_valid_length(uint16_t length) {
	return length <= (MAX_FFT_SIZE*2) && length >= (MIN_FFT_SIZE*2) && ((length&(length-1)) == 0);
	// The *2 comes from the real fft: For N datapoints, an FFT of length N/2 is used.
}

/**
 * 1 on success. Fails, if the length is not a power of two or in [min_fft_size*2, max_fft_size*2].
 */
uint8_t fft_set_length(FFT_instance* fft, uint16_t length) {
	if (!fft_is_valid_length(length)) {
		return 0;
	}
	fft->length = length;
	fft->bits = get_bits(length);
	fft_clear_buffer_pointers(fft);
	return 1;
}

/**
 * Sets the internal data pointers to the given buffer.
 */
void fft_set_raw_buffer(FFT_instance* fft, uint8_t* raw_buffer) {
	uint32_t big_buffer_size = fft->length * sizeof(FFT_DATATYPE) + FFT_HEADER_ALIGNMENT + FFT_HEADER_SIZE;
	fft->raw_buffer_fill = raw_buffer;
	fft->raw_buffer_calc_and_send = raw_buffer + big_buffer_size;
	fft->buffer_fill = (FFT_DATATYPE*)(fft->raw_buffer_fill + FFT_HEADER_ALIGNMENT + FFT_HEADER_SIZE);
	fft->buffer_window_overlap = (FFT_DATATYPE*)(raw_buffer + 2 * big_buffer_size);
}

/**
 * Clears the internal data pointers.
 */
void fft_clear_buffer_pointers(FFT_instance* fft) {
	fft->raw_buffer_calc_and_send = fft->raw_buffer_fill = NULL;
	fft->buffer_fill = fft->buffer_window_overlap = NULL;
}

/**
 * Space for two buffers are needed with space for all samples and the fft header each.
 * One small buffer for window overlapping is needed.
 */
uint32_t fft_needed_buffer_size(FFT_instance* fft) {
	uint32_t big_buffer = fft->length * sizeof(FFT_DATATYPE) + FFT_HEADER_ALIGNMENT + FFT_HEADER_SIZE;
	uint32_t small_buffer = (fft->length >> 1) * sizeof(FFT_DATATYPE);
	return 2 * big_buffer + small_buffer;
}

/**
 * Prepares all given instances (array of length N) for a new measurement.
 */
void fft_prepare_instances(FFT_instance** fft_instances, uint8_t N) {
	for (int i = 0; i < N; i++) {
		FFT_instance* fft = fft_instances[i];

		if (RECTANGULAR_WINDOW_INDEX == fft->window_index) {
			fft->fill_step = 0;
		} else {
			fft->fill_step = fft->length >> 1;
		}

		uint16_t N_half = N >> 1;
		for (uint16_t j = 0; j < N_half; j++) {
			fft->buffer_window_overlap[j] = 0.0f;
		}
	}
}

/**
 * Make sure, the window_index is not the rectangular one!
 */
FFT_DATATYPE fft_window_value(FFT_DATATYPE value, uint8_t window_index, uint8_t bits, uint16_t step) {
	const FFT_DATATYPE* window = windows[window_index];
	uint16_t window_entry_index = step; // Ranges from 0..N-1

	// Table is symmetrical, so swap the window_index, if necessary.
	uint16_t half_length = (1 << (bits - 1));
	if (window_entry_index >= half_length) { // Table is symmetrical to N/2, so 0..N/2-1 == N-1..N/2
		window_entry_index = (1 << bits) - window_entry_index - 1;
	}

	// There are entries with a resolution of 2^WINDOW_FUNCTION_TABLE_BITS.
	// E.g. this is 14, the actual table size is 13 (half width), because of the symmetry.
	// If the fft bits is 10, we need to multiply the index by 2^((14-1)-(10-1))=2^(14-10), because
	// Just N/2 (or 9 bits) are used to for the full range. This is equal to shifting the index by 4 bits.
	window_entry_index = window_entry_index << (WINDOW_FUNCTION_TABLE_BITS - bits);
	return value * window[window_entry_index];
}

inline uint16_t fft_bitrev_index(uint16_t index, uint8_t bits) {
	uint16_t i = index >> 1;
	uint16_t j = index & 1;
	return bit_rev_table[bits-1][i]*2 + j;
}

void fft_instance_new_value(FFT_instance *fft, uint32_t nanovolt, uint64_t timestamp) {
	if (!fft->enabled || !fft_instance_ready(fft)) {
		Error_Handler();
	}

	// Save the timestamp from the first sample
	if (fft->fill_step == 0) {
		fft->timestamp_first_sample = timestamp;
	}

	FFT_DATATYPE original_value = (FFT_DATATYPE)nanovolt/1000000000.f; // Convert from nanovolt to volt
	FFT_DATATYPE windowed_value = original_value;
	if (RECTANGULAR_WINDOW_INDEX != fft->window_index) {
		windowed_value = fft_window_value(original_value, fft->window_index, fft->bits, fft->fill_step);
	}

	// Put the sample in the bitreversed order, when interpreting buffer_fill as a complex array
	// of size N/2.
	uint16_t bitrev_index = fft_bitrev_index(fft->fill_step, fft->bits);
	fft->buffer_fill[bitrev_index] = windowed_value;

	// If a window is active, we overlap by 50%; meaning, we are currently filling N/2..N-1.
	// The lower part 0..N/2-1 must be filled from the buffer_window_overlap, where the values
	// from last fill cycle are saved.
	if (RECTANGULAR_WINDOW_INDEX != fft->window_index) {
		// Copy the corresponding lower-half value from the window overlap buffer to the fill buffer
		uint16_t copy_index = fft->fill_step - (fft->length >> 1);
		uint16_t bitrev_copy_index = fft_bitrev_index(copy_index, fft->bits);
		FFT_DATATYPE copy_value = fft->buffer_window_overlap[copy_index];

		// window the value and put in the fill buffer
		windowed_value = fft_window_value(copy_value, fft->window_index, fft->bits, copy_index);
		fft->buffer_fill[bitrev_copy_index] = windowed_value;

		// Save original (non windowed) value for next cycle
		fft->buffer_window_overlap[copy_index] = original_value;
	}

	// Done with this one.
	fft->fill_step++;

	// Check, if the fill buffer is full.
	if (fft->fill_step >= fft->length) {
		// Frequency resolution is: (N-1)/N * 100.000/timediff. Timediff is in 10e-5 secs, so the 100000 will bring this to secs.
		uint64_t timediff = timestamp - fft->timestamp_first_sample;
		fft->frequence_resolution = ((fft->length-1) * 100000.0f)/(((float)fft->length)*timediff);

		uint16_t reset_fill_step = 0;
		if (RECTANGULAR_WINDOW_INDEX != fft->window_index) {
			reset_fill_step = fft->length >> 1;
		}

		if (fft->dirty) {
			fft->fill_step = reset_fill_step;
			return;
		}

		// Swap the buffers
		uint8_t* tmp = fft->raw_buffer_fill;
		fft->raw_buffer_fill = fft->raw_buffer_calc_and_send;
		fft->raw_buffer_calc_and_send = tmp;
		fft->buffer_fill = (FFT_DATATYPE*)(fft->raw_buffer_fill + FFT_HEADER_ALIGNMENT + FFT_HEADER_SIZE);
		fft->fill_step = reset_fill_step;

		// Notify the thread to calculate the FFT.
		osSignalSet(fft->thread, 1);
	}
}

static void fft_task_function(void const *argument) {
	FFT_instance* fft = (FFT_instance*) argument;
	while(1) {
		osEvent evt = osSignalWait(0, osWaitForever); // Wait for any signal
		if (evt.status == osEventSignal) {
			fft->dirty = 1;

			if (!is_measure_active()) {
				fft->dirty = 0;
				continue; // skip, if the measurement is stopped.
			}

			// Calculate FFT
			FFT_DATATYPE* samples = (FFT_DATATYPE*)(fft->raw_buffer_calc_and_send + FFT_HEADER_ALIGNMENT + FFT_HEADER_SIZE);
			REALFFT(samples, fft->length);

			if (!is_measure_active()) {
				fft->dirty = 0;
				continue; // skip, if the measurement is stopped.
			}

			// Use DataDeskriptors, if possible: Max 4K of data, so get the max length of the fft by dividing
			// 4K with the datatype size.
			uint16_t max_length_for_4K_data = 4096/sizeof(FFT_DATATYPE);
			if (fft->length <= max_length_for_4K_data) {
				uint32_t packet_len = fft->length * sizeof(FFT_DATATYPE) + sizeof(fft_packet_metadata);
				uint8_t* data = fft->raw_buffer_calc_and_send + FFT_HEADER_ALIGNMENT + FFT_PACKET_HEADER_SIZE; // Skip packet headers. Will be added by send_data

				// Set the packet's metadata
				fft->frame_number = 0; // Sending the first of 1 packet...
				fft->frame_count = 1;
				fft_packet_metadata* m = (fft_packet_metadata*)data;
				fft_set_package_metadata(fft, m);

				// Send it (there, the data is copied) and finished.
				send_data(SEND_TYPE_FFT, data, packet_len);
				fft->dirty = 0;
			} else {
				// The data is to big. Do not use DataDescriptors. Use the send_data task to send raw bytes.
				// So we must calculate how much frames we are going to send.
				fft->bytes_send = 0;

				// framecount: Per frame, 0xFFFF-FFT_HEADER_WITH_ALIGNMENT_SIZE space for data
				uint32_t data_to_send = fft->length * sizeof(FFT_DATATYPE);
				uint8_t frames = data_to_send / FFT_PACKET_DATA_SPACE;
				if (data_to_send % FFT_PACKET_DATA_SPACE != 0) {
					frames++;
				}
				fft->frame_count = frames;
				fft->frame_number = 0;

				fft_transmit_frame(fft);
			}
		}
	}
}

/**
 * Transmit an frame of fft data. in bytes_to_send is saved, how much bytes are already send, so
 * this is used to get the position in the data array for the next frame.
 */
static void fft_transmit_frame(FFT_instance* fft) {
	// Calculate, how many actual data bytes (no headers) must be send:
	uint32_t bytes_left = fft->length * sizeof(FFT_DATATYPE) - fft->bytes_send;

	// limit the bytes by the max payload space
	uint16_t bytes_to_send = FFT_PACKET_DATA_SPACE;
	if (bytes_left < bytes_to_send) {
		bytes_to_send = bytes_left;
	}

	uint8_t* data = fft->raw_buffer_calc_and_send + fft->bytes_send + FFT_HEADER_ALIGNMENT + FFT_PACKET_HEADER_SIZE;

	// Set header information
	//fft_packet_header* h = (fft_packet_header*)data; // Set header information
	//h->send_type = SEND_TYPE_FFT;
	//h->len = bytes_to_send + sizeof(fft_packet_metadata);

	// Set metadata
	fft_packet_metadata* m = (fft_packet_metadata*)(data);
	fft_set_package_metadata(fft, m);
	fft->frame_number++;

	fft->bytes_send += bytes_to_send;
	send_data_non_copy(SEND_TYPE_FFT, data, bytes_to_send + sizeof(fft_packet_metadata), &fft_transmitted, (void*) fft);
}

/**
 * Sets all the metadata in m according to the given fft instance.
 */
static void fft_set_package_metadata(FFT_instance* fft, fft_packet_metadata* m) {
	m->id = fft->id;
	m->timestamp = measure_reference_timer_ticks;
	m->frame_count = fft->frame_count;
	m->frame_number = fft->frame_number;
	m->length = fft->length;
	m->frequence_resolution = fft->frequence_resolution;
	if (fft->window_index == RECTANGULAR_WINDOW_INDEX) {
		m->wss = (float)fft->length;
	} else {
		m->wss = windows_ss[fft->window_index][fft->bits];
	}
}

/**
 * Callback function, if a frame with fft data was transmitted. Transmit the next frame, if there are more.
 */
static void fft_transmitted(void* argument) {
	FFT_instance* fft = (FFT_instance*) argument;

	// If the measurement was stopped, do not send any data.
	if (!is_measure_active()) {
		fft->dirty = 0;
		return;
	}

	uint32_t bytes_left = fft->length * sizeof(FFT_DATATYPE) - fft->bytes_send;
	// Are all values send?
	if (bytes_left == 0) {
		fft->dirty = 0;
	} else {
		fft_transmit_frame(fft);
	}
}

/**
 * Takes an array of N complex samples and calculates the FFT.
 * The samples need to be bit reversed!
 */
static void FFT(complex* samples, uint16_t N) {
    uint16_t stage_size_half = 1, stage_size = 2; // merge two stage_size/2 DFTs two one DFT with size stage_size together

    while(stage_size <= N) {
        uint16_t tw_delta = TWIDDLE_FACTOR_TABLE_SIZE/stage_size;

        for (int j = 0; j < stage_size_half; j++) { // j-th entry in DFT
            complex tw = twiddleFactors[tw_delta*j];

            for (int i = 0; i < N; i+=stage_size) { // i-th DFT
                uint16_t a = j + i;
                uint16_t b = a + stage_size_half; // a and b are indices of the even and odd values.

                // Multiply the twiddle factor with B:
                FFT_DATATYPE B_re = samples[b].re*tw.re - samples[b].im*tw.im;
                FFT_DATATYPE B_im = samples[b].re*tw.im + samples[b].im*tw.re;

                FFT_DATATYPE A_re = samples[a].re;
                FFT_DATATYPE A_im = samples[a].im;

                samples[a].re = A_re + B_re;
                samples[a].im = A_im + B_im;
                samples[b].re = A_re - B_re;
                samples[b].im = A_im - B_im;
            }
        }

        stage_size_half = stage_size;
        stage_size *= 2;
    }
}

/**
 * Calculates the real FFT of N given samples. The result can be interpreted as the positive half
 * of the FFT, with samples[0] as F_0 and samples[1] = F_N/2, both real.
 * The samples needs to be bitreversed!
 */
void REALFFT(FFT_DATATYPE* samples, uint16_t N) {
	uint16_t N_half = N/2;

	// Interpret samples as complex
	complex* s = (complex*)samples;

	FFT(s, N_half);

	// Transform the result of two "independent" FFTs back into one.
	uint16_t tw_delta = TWIDDLE_FACTOR_TABLE_SIZE/N;
	complex H1, H2;
	for (uint16_t n = 1; n < N_half/2; n++) { // Case n=0 done later
		complex tw = twiddleFactors[tw_delta*n];
		uint16_t n2 = N_half-n;
		H1.re = 0.5f * (s[n].re + s[n2].re);
		H1.im = 0.5f * (s[n].im - s[n2].im);
		H2.re = 0.5f * (s[n].im + s[n2].im);
		H2.im = -0.5f * (s[n].re - s[n2].re);

		s[n].re = H1.re + (tw.re*H2.re - tw.im*H2.im);
		s[n].im = H1.im + (tw.re*H2.im + tw.im*H2.re);
		s[n2].re = H1.re + (-tw.re*H2.re + tw.im*H2.im);
		s[n2].im = -H1.im + (tw.re*H2.im + tw.im*H2.re);
	}
	FFT_DATATYPE tmp = s[0].re;
	s[0].re += s[0].im;
	s[0].im = tmp - s[0].im;
}

/**
 * Returns the number of bits needed to represent the number.
 * This is equal to returning the position of the highes one set in the binary representation.
 */
static uint8_t get_bits(uint16_t number) {
    uint8_t bits = 1;
    while((number>>bits) > 0) {
        bits++;
    }
    return bits-1;
}

/**
 * If COMPARE_FFT is defined, we have some helper functions
 * to generate samples.
 */
#ifdef COMPARE_FFTS
#include "arm_math.h"
#include "math.h"

float __section(".extsram") samples[COMPARE_FFTS_N];
float __section(".extsram") out_samples[COMPARE_FFTS_N];

float f(FFT_DATATYPE x) {
    return 3.0 + cos(2 * (FFT_DATATYPE)M_PI * x * 500) + cos(2 * (FFT_DATATYPE)M_PI * x * 1000); // 500hz sine wave
}

void gen_samples_real(FFT_DATATYPE* samples, uint16_t N, uint32_t samplerate) {
	FFT_DATATYPE sr = (FFT_DATATYPE)samplerate;
    for (int i = 0; i < N; i++) {
    	FFT_DATATYPE x = (FFT_DATATYPE)i/sr;
    	samples[i] = f(x);
    }
}

void compare_fft_algorithms(uint32_t* own, uint32_t* dsp_lib) {
	printf("First own implementation, then DSP-Lib. N=%d\n", COMPARE_FFTS_N);
	gen_samples_real(samples, COMPARE_FFTS_N, COMPARE_FFTS_SR);
	uint64_t start = measure_reference_timer_ticks;
	REALFFT(samples, COMPARE_FFTS_N);
	uint64_t stop = measure_reference_timer_ticks;
	*own = stop - start;
	printf("own implementation: %lu\n", *own);

	// DSP lib implementation
	gen_samples_real(samples, COMPARE_FFTS_N, COMPARE_FFTS_SR);
	start = measure_reference_timer_ticks;

	// Some initializations
	arm_rfft_instance_f32 S;
	arm_cfft_radix4_instance_f32 S_CFFT;
	arm_status s = arm_rfft_init_f32(&S, &S_CFFT, COMPARE_FFTS_N, 0, 1);
	printf("status1: %d\n", s);
	// Do the fft.
	arm_rfft_f32(&S, samples, out_samples);

	stop = measure_reference_timer_ticks;
	*dsp_lib = stop - start;
	printf("DSP lib implementation: %lu\n", *dsp_lib);
}
#endif
