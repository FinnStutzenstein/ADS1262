/*
 * measurement.c
 *
 *  Created on: Oct 13, 2018
 *      Author: finn
 */

#include "measurement.h"
#include "ads1262.h"
#include "measure.h"
#include "cmsis_os.h"
#include "setup.h"
#include "pool.h"
#include "utils.h"
#include "state.h"

DEFINE_POOL(measurementPool, MAX_MEASUREMENTS, measurement_t);


/**
 * Initializes the measurement module
 */
void measurement_init() {
	pool_init(measurementPool);
}


/**
 * Returns all measurements. Attention: Some pointers can be NULL, if there are no measurements.
 */
inline measurement_t** measurement_get_all() {
	return (measurement_t**) pool_get_entries(measurementPool);
}

/**
 * Returns the amount of available measurements
 */
inline uint8_t measurement_get_available_count() {
	return measurementPool->entrycount;
}

/**
 * Returns the amount of exisiting measurements
 */
inline uint8_t measurement_get_active_count() {
	return pool_get_used_entries_count(measurementPool);
}

/**
 * Returns the measurement pool.
 */
inline Pool* measurement_get_pool() {
	return measurementPool;
}

/**
 * Resets the averaging of the given measurement
 */
inline void measurement_reset_averaging(measurement_t* m) {
	m->averaging_step = 0;
	m->averaging_sum = 0;
}

/**
 * Creates a new measurement. Return the correct status code for ADCP.
 */
ProtocolError measurement_create(uint8_t pos, uint8_t neg, uint8_t enabled, uint16_t averaging, uint8_t* id) {
	if (is_measure_active()) {
		return RESPONSE_MEASUREMENT_ACTIVE;
	}

	// try to get a measurement
	measurement_t* m = (measurement_t*) pool_alloc(measurementPool);
	if (NULL == m) {
		return RESPONSE_TOO_MUCH_MEASUREMENTS;
	}

	// find the id
	measurement_t** measurements = (measurement_t**) pool_get_entries(measurementPool);
	for (uint8_t i = 0; i < measurementPool->entrycount; i++) {
		if (measurements[i] == m) {
			*id = i;
		}
	}

	// Set all attributes
	m->adc_input_multiplexer = ADS1262_make_input_mux_from_pos_neg(pos, neg);
	m->enabled = enabled;
	m->averaging_count = averaging;
	measurement_reset_averaging(m);
	fft_instance_init(&(m->fft), *id);

	return RESPONSE_OK;
}

/**
 * Returns the measurement object by the giben id. Returns NULL, if the measurement was not found.
 */
measurement_t* measurement_get_by_id(uint8_t id) {
	if (id >= measurementPool->entrycount) {
		return NULL;
	}
	return pool_get_entries(measurementPool)[id];
}

/**
 * Deletes a measurement.
 */
ProtocolError measurement_delete(uint8_t id) {
	if (is_measure_active()) {
		return RESPONSE_MEASUREMENT_ACTIVE;
	}
	measurement_t* m = measurement_get_by_id(id);
	if (NULL == m) { // if not found, it's deleted, too
		return RESPONSE_OK;
	}

	fft_instance_deinit(&(m->fft));
	pool_free(measurementPool, (void*) m);
	return RESPONSE_OK;
}

/**
 * Sets the input of a measurement
 */
ProtocolError measurement_set_inputs(uint8_t id, uint8_t pos, uint8_t neg) {
	if (is_measure_active()) {
		return RESPONSE_MEASUREMENT_ACTIVE;
	}
	measurement_t* m = measurement_get_by_id(id);
	if (NULL == m) {
		return RESPONSE_NO_SUCH_MEASUREMENT;
	}

	m->adc_input_multiplexer = ADS1262_make_input_mux_from_pos_neg(pos, neg);
	return RESPONSE_OK;
}

/**
 * Enables or disables a measurement.
 */
ProtocolError measurement_set_enabled(uint8_t id, uint8_t enabled) {
	if (is_measure_active()) {
		return RESPONSE_MEASUREMENT_ACTIVE;
	}
	measurement_t* m = measurement_get_by_id(id);
	if (NULL == m) {
		return RESPONSE_NO_SUCH_MEASUREMENT;
	}

	m->enabled = enabled;
	return RESPONSE_OK;
}

/**
 * Sets the averaging of a measurement
 */
ProtocolError measurement_set_averaging(uint8_t id, uint16_t averaging) {
	if (is_measure_active()) {
		return RESPONSE_MEASUREMENT_ACTIVE;
	}
	measurement_t* m = measurement_get_by_id(id);
	if (NULL == m) {
		return RESPONSE_NO_SUCH_MEASUREMENT;
	}

	m->averaging_count = averaging;
	return RESPONSE_OK;
}

/**
 * Given a state representation, e.g. from the SD card, setup all measurements as given.
 */
void measurements_set_to_state(complete_state_t* state) {
	if (measurementPool->type != PoolTypeStatic) {
		Error_Handler();
	}

	measurement_t** measurements = measurement_get_all();
	for (uint32_t i = 0; i < measurementPool->entrycount; i++) { // Go through all available spots for measurements
		// find measurement in state.
		measurement_state_t* m = NULL;
		for (uint32_t j = 0; j < state->adc.measurement_count; j++) {
			if (state->mesurements[j].id == i) {
				m = state->mesurements + j;
				break;
			}
		}

		if (NULL == m) { // This measurement does not exist. Set to NULL
			measurements[i] = NULL;
		} else {
			// Read all information from the state.
			((void**)measurements)[i] = measurementPool->pool.stat + i*measurementPool->entrysize;
			measurements[i]->adc_input_multiplexer = m->input_multiplexer;
			measurements[i]->enabled = m->enabled;
			measurements[i]->averaging_count = m->averaging;
			FFT_instance* fft = &(measurements[i]->fft);
			fft_instance_init(fft, i);
			fft_set_enabled(fft, m->fft_enabled);
			fft_set_length(fft, m->fft_length);
		}
	}
}
