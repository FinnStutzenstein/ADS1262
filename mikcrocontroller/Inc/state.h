/*
 * state.h
 *
 *  Created on: Nov 10, 2018
 *      Author: finn
 */

#ifndef STATE_H_
#define STATE_H_

#include "sys/cdefs.h"
#include "config.h"

#ifdef __cplusplus
 extern "C" {
#endif

#define STATE_FILENAME	"0:/state"

typedef struct __packed {
	uint8_t id;
	uint8_t input_multiplexer;
	uint8_t enabled;
	uint16_t averaging;
	uint8_t fft_enabled;
	uint16_t fft_length;
	uint8_t fft_window_index;
} measurement_state_t;

typedef struct __packed {
	struct {
		uint8_t started:2;
		uint8_t internal_reference:1;
		uint8_t slow_connection:1;
		uint8_t ADC_reset:1;
	} flags;
	uint8_t sr_filter;
	uint8_t pga;
	uint64_t v_ref_microvolt;
	uint8_t v_ref_inputs;
	int64_t calibration_offset;
	uint64_t calibration_scale;
	uint8_t measurement_count;
} adc_state_t;

typedef struct __packed {
	adc_state_t adc;
	measurement_state_t mesurements[MAX_MEASUREMENTS];
} complete_state_t;

void init_state();
complete_state_t* get_current_state();
void update_complete_state(uint8_t send_and_save_state);
void update_adc_state(uint8_t send_and_save_state);
void update_measurement_state(uint8_t send_and_save_state);
uint16_t state_copy_to_buffer(uint8_t* data, uint16_t max_len);
void init_system_from_last_state();
void state_reset_adc();

void set_slow_connection_flag();
void clear_slow_connection_flag();
void set_ADC_reset_flag();
void clear_ADC_reset_flag();
uint8_t is_ADC_reset_flag_set();

#ifdef __cplusplus
}
#endif

#endif /* STATE_H_ */
