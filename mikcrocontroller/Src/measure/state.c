/*
 * state.c
 *
 *  Created on: Nov 10, 2018
 *      Author: finn
 */
#include "state.h"
#include "string.h"
#include "ads1262.h"
#include "measure.h"
#include "measurement.h"
#include "send_data.h"
#include "fatfs.h"
#include "cmsis_os.h"
#include "stdio.h"

// THE state of the microcontroller. There are two sub-types of state:
// - general ADC related values
// - per measurement. Includes FFT.
static complete_state_t state;

// Lock access to the state-file on the SD-card.
static osMutexId state_file_mutex;

static uint16_t get_state_size();
static void send_state_to_clients();
static void save_state();
static uint8_t init_last_state_from_file();
static uint8_t set_state(uint8_t* data, uint16_t length);

/**
 * Initialize this module.
 */
void init_state() {
	osMutexDef(stateFileMutex);
	state_file_mutex = osMutexCreate(osMutex(stateFileMutex));
}

inline complete_state_t* get_current_state() {
	return &state;
}

/**
 * Updates the ADC andmeasurements status. If send_and_save_state is given,
 * The change is written to the sd card, and send via network.
 */
void update_complete_state(uint8_t send_and_save_state) {
	update_adc_state(0);
	update_measurement_state(send_and_save_state);
}

/**
 * Updates the ADC state. If send_and_save_state is given,
 * The change is written to the sd card, and send via network.
 *
 * The ADC values are directly read from the ADC.
 */
void update_adc_state(uint8_t send_and_save_state) {
	state.adc.flags.started = measure_get_state();
	if (!is_ADC_reset_flag_set()) {
		state.adc.flags.internal_reference = ADS1262_read_is_internal_reference_used();
		state.adc.sr_filter = ADS1262_read_samplerate() | (ADS1262_read_filter() << 4);
		state.adc.pga = ADS1262_read_gain();
		state.adc.v_ref_tennanovolt = ADS1262_get_reference_voltage();
		state.adc.v_ref_inputs = ADS1262_read_reference_pins();
		state.adc.calibration_offset = ADS1262_read_calibration_offset();
		state.adc.calibration_scale = ADS1262_read_calibration_scale();
	}

	if (send_and_save_state) {
		send_state_to_clients();
		save_state();
	}
}

/**
 * Updates the state of all measurements.  If send_and_save_state is given,
 * The change is written to the sd card, and send via network.
 */
void update_measurement_state(uint8_t send_and_save_state) {
	// Get all measurements.
	measurement_t** measurements = measurement_get_all();
	uint8_t measurements_count = measurement_get_available_count();

	uint8_t state_measurement_index = 0; // The position in the state struct. In this struct,
	// the measurements are placed directly after each other.
	for (uint8_t i = 0; i < measurements_count; i++) {
		measurement_t* m = measurements[i];
		if (NULL != m) { // there is a measurement. Copy the attrivutes to the state.
			state.mesurements[state_measurement_index].id = i;
			state.mesurements[state_measurement_index].input_multiplexer = m->adc_input_multiplexer;
			state.mesurements[state_measurement_index].enabled = m->enabled;
			state.mesurements[state_measurement_index].averaging = m->averaging_count;
			state.mesurements[state_measurement_index].fft_enabled = m->fft.enabled;
			state.mesurements[state_measurement_index].fft_length = m->fft.length;
			state.mesurements[state_measurement_index].fft_window_index = m->fft.window_index;
			state_measurement_index++;
		}
	}
	// Update the amount of measurements.
	state.adc.measurement_count = state_measurement_index;

	if (send_and_save_state) {
		send_state_to_clients();
		save_state();
	}
}

/**
 * Returns the size of the state struct.
 */
static uint16_t get_state_size() {
	// The size is the part of the adc state, and then measurement_state_t as much as there are measurements.
	return sizeof(adc_state_t) + (state.adc.measurement_count * sizeof(measurement_state_t));
}

/**
 * Sends the raw state bytes to all clients.
 */
static void send_state_to_clients() {
	send_data(SEND_TYPE_STATUS, (uint8_t*)(&state), get_state_size());
}

/**
 * Saves the state to the sd card.
 */
static void save_state() {
	if (osMutexWait(state_file_mutex, 0) != osOK) {
		return; // someone else is writing the state, so it's fine..
	}

	FIL file;
	FRESULT fres;

	// override an existing file.
	if ((fres = f_open(&file, STATE_FILENAME, FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK) {
		osMutexRelease(state_file_mutex);
		return;
	}

	uint8_t buffer[sizeof(complete_state_t)];
	uint16_t len = state_copy_to_buffer(buffer, sizeof(complete_state_t));
	unsigned int bytes_written;

	// Write file
	fres = f_write(&file, buffer, len, &bytes_written);
	if (fres != FR_OK || bytes_written < len) {
		osMutexRelease(state_file_mutex);
		return;
	}

	// Close it.
	if ((fres = f_close(&file)) != FR_OK) {
		printf("error closing file: %d\n", fres);
		osMutexRelease(state_file_mutex);
		return;
	}

	osMutexRelease(state_file_mutex);
}

/**
 * Copies the state to the given buffer with respect to max_len of the buffer.
 * Returns the amount of copied bytes. If max_len is too small, nothing will be copied.
 */
uint16_t state_copy_to_buffer(uint8_t* data, uint16_t max_len) {
	uint16_t len = get_state_size();
	if (len > max_len) {
		return 0;
	}
	memcpy(data, (uint8_t*)(&state), len);
	return len;
}

/**
 * Sets the slow_connection-flag. Does not send any updates.
 */
inline void set_slow_connection_flag() {
	state.adc.flags.slow_connection = 1;
}

/**
 * Clears the slow_connection-flag. Does not send any updates.
 */
inline void clear_slow_connection_flag() {
	state.adc.flags.slow_connection = 0;
}

/**
 * Sets the slow_connection-flag. Does not send any updates.
 */
inline void set_ADC_reset_flag() {
	state.adc.flags.ADC_reset = 1;
}

/**
 * Clears the slow_connection-flag. Does not send any updates.
 */
inline void clear_ADC_reset_flag() {
	state.adc.flags.ADC_reset = 0;
}

inline uint8_t is_ADC_reset_flag_set() {
	return !!state.adc.flags.ADC_reset;
}

/**
 * Inits the microcontroller from the last state on the sd card. If there is no
 * last state, or the file is corrupted, the current state is saved.
 */
void init_system_from_last_state() {
	if (init_last_state_from_file()) {
		printf("loaded state from SD-card!\n");
		// state is loaded. Now set the ADC and measurements to this state.
		ADS1262_set_to_state(&state);
		measurements_set_to_state(&state);
	} else {
		printf("could not load state from SD-card!\n");
		update_complete_state(0);
		save_state();
	}
}

/* Tries to load the state-file from the sd-card. Init the state and all relates modules
 * to this state. 1 on successfull initialization.
 */
static uint8_t init_last_state_from_file() {
	FIL file;
	FRESULT fres;

	// Try to open the file
	if ((fres = f_open(&file, STATE_FILENAME, FA_READ)) != FR_OK) {
		return 0;
	}

	uint8_t buffer[sizeof(complete_state_t)];
	uint8_t* buffer_ptr = buffer;
	unsigned int bytes_read;
	uint16_t bytes_read_sum = 0;
	unsigned int bytes_left = sizeof(complete_state_t);

	// Read file.
	do {
		fres = f_read(&file, buffer_ptr, bytes_left, &bytes_read);
		bytes_left -= bytes_read;
		bytes_read_sum += bytes_read;
		buffer_ptr += bytes_read;
	} while(fres == FR_OK && bytes_read > 0);

	f_close(&file);

	// OK. Try to set the state.
	return set_state(buffer, bytes_read_sum);
}

/**
 * tries to set the state by the given data. This data should be a state representation
 * as complete_state_t, but not all measurements may be given. Initialize `state`. Does the reverse
 * as state_format_fo_response and verification.
 * Returns 1 on success
 */
static uint8_t set_state(uint8_t* data, uint16_t length) {
	if (length < sizeof(adc_state_t)) {
		return 0;
	}

	adc_state_t* s = (adc_state_t*) data;
	s->flags.started = 0;
	s->flags.slow_connection = 0;
	// s->flags.internal_reference;
	s->flags.ADC_reset = 0;

	uint8_t filter = (s->sr_filter >> 4) & 0x0F;
	if (filter > 4) { // This filter does not exist.
		return 0;
	}

	if (s->pga != 0xFF && s->pga > 5) { // not bypass and not existing gain
		return 0;
	}

	if (s->flags.internal_reference && s->v_ref_tennanovolt != ADS1262_REF_INTERNAL_TENNANOVOLTS) {
		return 0;
	}

	if (s->calibration_offset > ADS1262_CALIBRATION_OFFSET_MAX ||
		s->calibration_offset < ADS1262_CALIBRATION_OFFSET_MIN) {
		printf("cal offset wrong\n");
		return 0;
	}

	if (s->measurement_count > MAX_MEASUREMENTS) {
		return 0;
	}

	// Check for size.
	if (length != (sizeof(adc_state_t) + s->measurement_count*sizeof(measurement_state_t))) {
		return 0;
	}

	for (uint8_t i = 0; i < s->measurement_count; i++) {
		measurement_state_t* m = (measurement_state_t*)(data + sizeof(adc_state_t) + i*sizeof(measurement_state_t));
		if (m->id >= MAX_MEASUREMENTS) {
			return 0;
		}
		if (m->enabled > 1) {
			return 0;
		}
		if (m->fft_enabled > 1) {
			return 0;
		}
		if (!fft_is_valid_length(m->fft_length)) {
			return 0;
		}
		if (RECTANGULAR_WINDOW_INDEX != m->fft_window_index && m->fft_window_index >= WINDOW_FUNCTIONS) {
			return 0;
		}
	}

	// OK! Copy data into status:
	memcpy((uint8_t*)(&state), data, length);
	state.adc.flags.started = 0;
	state.adc.flags.slow_connection = 0;
	return 1;
}

