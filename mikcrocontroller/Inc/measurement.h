/*
 * measurement.h
 *
 *  Created on: Oct 13, 2018
 *      Author: finn
 */

#ifndef MEASUREMENT_H_
#define MEASUREMENT_H_

#include "adcp.h"
#include "config.h"
#include "fft.h"
#include "state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Defines a measurement. Saves the configuration of the input multiplexer,
 * averaging and an optional FFT instance. Holds a reference to the value_buffer.
 */
typedef volatile struct {
	uint8_t adc_input_multiplexer;
	uint8_t enabled;
	uint16_t averaging_count;
	uint16_t averaging_step;
	uint64_t averaging_sum;
	FFT_instance fft;
} measurement_t;

void measurement_init();

void measurement_reset_averaging(measurement_t* m);
measurement_t** measurement_get_all();
uint8_t measurement_get_available_count();
uint8_t measurement_get_active_count();
Pool* measurement_get_pool();

protocol_error_t measurement_create(uint8_t pos, uint8_t neg, uint8_t enabled, uint16_t averaging, uint8_t* id);
protocol_error_t measurement_delete(uint8_t id);
measurement_t* measurement_get_by_id(uint8_t id);

protocol_error_t measurement_set_inputs(uint8_t id, uint8_t pos, uint8_t neg);
protocol_error_t measurement_set_enabled(uint8_t id, uint8_t enabled);
protocol_error_t measurement_set_averaging(uint8_t id, uint16_t averaging);

void measurements_set_to_state(complete_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENT_H_ */
