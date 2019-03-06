/*
 * measure.c
 *
 *  Created on: Oct 13, 2018
 *      Author: finn
 */

#include "measure.h"
#include "ads1262.h"
#include "cmsis_os.h"
#include "error.h"
#include "setup.h"
#include "send_data.h"
#include "pool.h"
#include "utils.h"
#include "fft_memory.h"
#include "watchdog.h"
#include "state.h"
#include "measurement.h"

static measure_state_t measure_state = MEASURE_STATE_IDLE;
static osThreadId thread_to_notify = NULL;
static int32_t oneshotMeasurementValue;

static uint8_t current_measurement_index;

typedef struct __packed {
	uint64_t time_reference;
	value_t buffer[VALUE_BUFFER_SIZE];
} ValueBuffer;

ValueBuffer _vb;
ValueBuffer* value_buffer = &_vb;
uint16_t value_buffer_index;

static inline uint8_t send_buffer();
static inline void setup_valuebuffer();
static protocol_error_t measure_do_calibration(uint8_t pos_input, uint8_t neg_input, calibration_type_t type, void* cal_value);

/**
 * Initializes the measure module
 */
void measure_init() {
	current_measurement_index = 0;
	measurement_init();
}

/**
 * Called, if the DRDY-Signal goes low on the ADC signaling
 * that new data is ready.
 */
#ifdef SIMULATE_ADC
void DRDY_Interrupt(int32_t test_value) {
#else
void DRDY_Interrupt() {
#endif

	// Save the timer reference on top, where it is not so much runtime-dependend.
	uint64_t measure_reference = measure_reference_timer_ticks;

	// This is a false alarm, if there is no active measurements, or there are measurements, before or after the
	// calibration/oneshot command was send.
	if (MEASURE_STATE_IDLE == measure_state ||
			((MEASURE_STATE_CALIBRATING == measure_state || MEASURE_STATE_ONESHOT == measure_state) && NULL == thread_to_notify)) {
		return;
	}

	if (MEASURE_STATE_CALIBRATING == measure_state) {
		osThreadId _thread = thread_to_notify;
		osSignalSet(_thread, 0x01); // notify the task when calibrating.
		thread_to_notify = NULL; // Prevent from notifying the task a second time.
		return; // We are not interested in the current measurement..
	}

	ADS1262_STATUS_Type status;

#ifdef SIMULATE_ADC
	int32_t tennanovolt = test_value;
#else
	// Read the value
	uint8_t error = 0;
	int32_t tennanovolt = ADS1262_read_ADC(&status, &error);
	if (error && ADS1262_get_samplerate() != 0x0F) { // Try again on error.
		// There is not enough time to read a second time, if the max samplerate is choosen...
		tennanovolt = ADS1262_read_ADC(&status, &error);
	}
#endif

	// Reset the watchdog
	measurement_watchdog_reset();

	// We've got a reset!
	// stop the measurement and notify the user.
	if (status.bit.RESET) {
		set_ADC_reset_flag();
		measure_stop();
		update_adc_state(1);
	}

	// Get current measurement
	measurement_t* current_measurement = measurement_get_all()[current_measurement_index];
	if (NULL == current_measurement) {
		Error_Handler();
	}

	uint8_t save_value = 1; // if we still collect data for averaging, the value should not be saved, so this
	// flag is set to 0.

	// Do averaging.
	if (current_measurement->averaging_count > 0) {
		current_measurement->averaging_sum += tennanovolt;
		current_measurement->averaging_step++;
		if (current_measurement->averaging_step < current_measurement->averaging_count) {
			save_value = 0;
		} else {
			// Do the averaging. Round "normal".
			tennanovolt = ((float)current_measurement->averaging_sum + 0.5f)/current_measurement->averaging_count;
			measurement_reset_averaging(current_measurement);
		}
	}

	// Until here, we can have a continuous or oneshot measurement.
	if (MEASURE_STATE_ONESHOT == measure_state) {
		if (save_value && NULL != thread_to_notify) {
			// we have finished the oneshot measurement
			oneshotMeasurementValue = tennanovolt;
			osThreadId _thread = thread_to_notify;
			osSignalSet(_thread, 0x01); // ready
			thread_to_notify = NULL;
		}
		return;
	}

	// Continuous measurement

	if (save_value) {
		// set the reference, if it's the first value in the buffer.
		if (value_buffer->time_reference == 0) {
			value_buffer->time_reference = measure_reference;
		}

		uint64_t ref = value_buffer->time_reference;

		uint64_t timestamp_delta = measure_reference - ref;
		// 0.64 secs. 2^16 is the max value for the timedelta. switch buffers now.
		if (timestamp_delta > 64000) {
			if (!send_buffer()) {
				return;
			}

			// If we have send the value buffer, we need to set a new timereference.
			value_buffer->time_reference = measure_reference;
			// Also update the delta (which is 0, becuase the it's the same timestamp as the reference)
			timestamp_delta = 0;
		}

		value_buffer->buffer[value_buffer_index].value = tennanovolt;
		value_buffer->buffer[value_buffer_index].timestamp_delta = timestamp_delta;
		uint8_t status_bits = (status.reg << 2) & 0xF8; // All PGA alarms and extclk are important (bits 1-5). Shift them
		// into the upper 5 bits. The lower 3 bits are the measurement id.
		value_buffer->buffer[value_buffer_index++].id_and_status = (current_measurement_index & 0x07) | status_bits;

		if (value_buffer_index >= VALUE_BUFFER_SIZE) {
			if (!send_buffer()) {
				return;
			}
		}

		// Put the value into the fft...
		if (fft_instance_enabled(&(current_measurement->fft))) {
			fft_instance_new_value(&(current_measurement->fft), tennanovolt, measure_reference);
		}
	}

	// get next measurement.
	// This is the first operation to quickly set the input mux, if the measurement changes.
	measurement_t** measurements = measurement_get_all();
	measurement_t* next_measurement = NULL;
	uint8_t next_measurement_index = 0;

	// In the worst case, we do have just one measurement and find the current measurement as the next one.
	for (int i = 1; i <= measurement_get_available_count(); i++) {
		next_measurement_index = (current_measurement_index+i)%MAX_MEASUREMENTS;
		next_measurement = measurements[next_measurement_index];
		if (NULL != next_measurement && next_measurement->enabled) {
			// found one
			break;
		}
	}

	// if next and current index are the same, do not change the input mux, so the ADC won't reload.
	if (next_measurement_index != current_measurement_index) {
		ADS1262_set_input_mux(next_measurement->adc_input_multiplexer);
	}

	// set next masurement.
	current_measurement_index = next_measurement_index;
}

/**
 * Transmits a full buffer. This might fail, if the send_data task cannot take more data.
 */
static inline uint8_t send_buffer() {
	// Buffer is full. Switch to the other one and send this one away
	// The size is the amount of data in the buffer * the buffer size plus
	// the space for the timestamp.
	uint16_t size = sizeof(uint64_t) + value_buffer_index*sizeof(value_t);
	uint8_t ret = send_data(SEND_TYPE_DATA, (uint8_t*)value_buffer, size);
	setup_valuebuffer();

	return ret;
}

/**
 * Resets the valuebuffer to take new values.
 */
static inline void setup_valuebuffer() {
	value_buffer_index = 0;
	value_buffer->time_reference = 0; // will be set in the interrupt
}

/**
 * Returns, if some mesurements are started.
 */
inline uint8_t is_measure_active() {
	return measure_state != MEASURE_STATE_IDLE;
}

inline measure_state_t measure_get_state() {
	return measure_state;
}

/**
 * Start the measurements. May raise errors, if there are no measurements etc.
 * Sets up the buffers for enabled fft instances, finds the first measurement,...
 */
protocol_error_t measure_start() {
	if (measurement_get_active_count() == 0) {
		return RESPONSE_NO_MEASUREMENTS;
	}

	FFT_instance *fft_instances[MAX_MEASUREMENTS];
	for (int i = 0; i < MAX_MEASUREMENTS; i++) {
		fft_instances[i] = NULL;
	}
	uint8_t fft_instance_index = 0; // Will also be use after collecting all instances as the length
	// of the resulting array

	// Find one entry to be the first one and reset all measurements.
	// Accumulate all fft instances.
	uint8_t enabled_measurement_count = 0;
	measurement_t** measurements = measurement_get_all();
	for (uint8_t i = 0; i < measurement_get_available_count(); i++) {
		if (NULL != measurements[i]) {
			measurement_reset_averaging(measurements[i]);
			if (measurements[i]->enabled) {
				current_measurement_index = i;
				enabled_measurement_count++;
			}

			fft_instances[fft_instance_index++] = &(measurements[i]->fft);
		}
	}

	if (enabled_measurement_count == 0) {
		return RESPONSE_NO_ENABLED_MEASUREMENT;
	}

	// Assign buffer space to each fft instance.
	if (!assign_memory_to_fft_instances(fft_instances, fft_instance_index, 0)) {
		return RESPONSE_FFT_NO_MEMORY;
	}

	// Prepare the first measurement and start the ADC
	clear_slow_connection_flag();
	fft_prepare_instances(fft_instances, fft_instance_index);
	measurement_watchdog_start();
	ADS1262_set_input_mux(measurements[current_measurement_index]->adc_input_multiplexer);
	setup_valuebuffer();
	measure_state = MEASURE_STATE_RUNNING;
	ADS1262_set_continuous_mode();
	ADS1262_start_ADC();

	return RESPONSE_OK;
}

/**
 * Stops running measurements. Sends the value buffer, if some samples were not send yet.
 */
protocol_error_t measure_stop() {
	uint8_t was_started = is_measure_active();
	measure_state = MEASURE_STATE_IDLE;
	ADS1262_stop_ADC();
	measurement_watchdog_stop();

	if (value_buffer_index > 1 && was_started) {
		value_buffer_index--; // The last value might be corrupt, if the interrupt was not finished.
		send_buffer();
	}

	return RESPONSE_OK;
}

protocol_error_t measure_oneshot(uint8_t measurement_id, int32_t* value) {
	if (MEASURE_STATE_IDLE != measure_state) {
		return RESPONSE_MEASUREMENT_ACTIVE;
	}

	measurement_t* m = measurement_get_by_id(measurement_id);
	if (NULL == m) {
		return RESPONSE_NO_SUCH_MEASUREMENT;
	}
	measurement_reset_averaging(m);

	current_measurement_index = measurement_id;
	clear_slow_connection_flag();
	//measurement_watchdog_start(1); // one active measurement
	ADS1262_set_input_mux(m->adc_input_multiplexer);
	measure_state = MEASURE_STATE_ONESHOT;

	// Do a update right here, before the ADC is started, because then, this task will sleep..
	update_adc_state(1);

	// How long to we want to have a timeout?
	uint16_t delay_seconds = m->averaging_count;
	if (delay_seconds < 3) {
		delay_seconds = 3; // Wait at least three seconds
	}

	ADS1262_set_continuous_mode();
	thread_to_notify = osThreadGetId();
	ADS1262_start_ADC();

	osEvent evt = osSignalWait(0, 1000*delay_seconds); // Wait for any signal, max. 10 seconds.
	ADS1262_stop_ADC();
	thread_to_notify = NULL;
	measure_state = MEASURE_STATE_IDLE;

	if (evt.status == osEventSignal) {
		// get value.
		*value = oneshotMeasurementValue;
		return RESPONSE_OK;
	} else if(evt.status == osEventTimeout) {
		*value = 0;
		return RESPONSE_CALIBRATION_TIMEOUT;
	} else {
		Error_Handler(); // Should not happen..
		return RESPONSE_SOMETHING_IS_NOT_GOOD; // shut up compiler!
	}
}

/**
 * Does offset calibration. Needs to be called from a thread, because it will be paused until
 * we got a response from the ADC. Provides the offset value through `offset`
 */
inline protocol_error_t measure_do_offset_calibration(uint8_t pos_input, uint8_t neg_input, int32_t* offset) {
	return measure_do_calibration(pos_input, neg_input, CALIBRATION_TYPE_OFFSET, (void*) offset);
}

/**
 * Does scale calibration. Needs to be called from a thread, because it will be paused until
 * we got a response from the ADC. Provides the scale value through `scale`
 */
inline protocol_error_t measure_do_scale_calibration(uint8_t pos_input, uint8_t neg_input, uint32_t* scale) {
	return measure_do_calibration(pos_input, neg_input, CALIBRATION_TYPE_SCALE, (void*) scale);
}

/**
 * Does one calibration
 */
static protocol_error_t measure_do_calibration(uint8_t pos_input, uint8_t neg_input, calibration_type_t type, void* cal_value) {
	if (MEASURE_STATE_IDLE != measure_state) {
		return RESPONSE_MEASUREMENT_ACTIVE;
	}

	ADS1262_set_input_mux_pos_neg(pos_input, neg_input);
	clear_slow_connection_flag();
	measure_state = MEASURE_STATE_CALIBRATING;
	// Do a update right here, before the ADC is started, because then, this task will sleep..
	update_adc_state(1);

	// Start the ADC and send the offset calibration command.
	ADS1262_set_continuous_mode();
	ADS1262_start_ADC();
	if (type == CALIBRATION_TYPE_OFFSET) {
		ADS1262_send_offset_calibration_command();
	} else {
		ADS1262_send_scale_calibration_command();
	}
	// set this value _after_ sending the calibration command: The samplerate might be so high, that
	// we got an DRDY interrupt before sending the calibration command. So we would not wait for the
	// command to finish.
	thread_to_notify = osThreadGetId();

	// Wait for the signal..
	osEvent evt = osSignalWait(0, 1000*10); // Wait for any signal, max. 10 seconds.
	ADS1262_stop_ADC();
	// Read offset/scale
	if (type == CALIBRATION_TYPE_OFFSET) {
		*((int32_t*)cal_value) = ADS1262_read_calibration_offset();
	} else {
		*((uint32_t*)cal_value) = ADS1262_read_calibration_scale();
	}
	thread_to_notify = NULL;
	measure_state = MEASURE_STATE_IDLE;

	if (evt.status == osEventSignal) {
		return RESPONSE_OK;
	} else if(evt.status == osEventTimeout) {
		return RESPONSE_CALIBRATION_TIMEOUT;
	} else {
		Error_Handler(); // Should not happen..
		return RESPONSE_SOMETHING_IS_NOT_GOOD; // shut up compiler!
	}
}

