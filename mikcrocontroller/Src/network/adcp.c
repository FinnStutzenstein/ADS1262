/*
 * protocol.c
 *
 *  Created on: Oct 24, 2018
 *      Author: finn
 */
#include "adcp.h"
#include "ads1262.h"
#include "connection.h"
#include "lwipstats.h"
#include "measurement.h"
#include "measure.h"
#include "state.h"
#include "string.h"
#include "task.h"
#include "fft.h"

#define SET_OK				out_data[0] = RESPONSE_OK; *out_len = 1;
#define SET_RESPONSE(x)		out_data[0] = (x); *out_len = 1;

static uint8_t adcp_handle_connection_command(connection_t* connection, uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len);
static uint8_t adcp_handle_debugging_command(uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len);
static uint8_t adcp_handle_measurement_command(uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len);
static uint8_t adcp_handle_ADC_command(uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len);
static uint8_t adcp_handle_FFT_command(uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len);
static uint8_t adcp_handle_calibration_command(uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len);

static int8_t adcp_check_arg_len(uint16_t len, uint8_t expected, uint8_t* out_data, uint16_t* out_len);
static void adcp_send_wrong_command_response(uint8_t command, uint8_t* out_data, uint16_t* out_len);

/**
 * Handles the incomming message (data) with the length len. The response is written to out_data with the length out_len
 * and respect to max_len.
 * Returns EXIT or NOEXIT.
 */
uint8_t adcp_handle_command(connection_t* connection, uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len) {
	uint8_t exit = NOEXIT;

	// Check the max_len for the output buffer. Yes, 16 is somewhat arbitrary,
	// but we do need a bit of space..
	// This must be ensured by the program, so this cannot be triggered from clients. Error_handler is the
	// right way to go.
	if (max_len < 16) {
		Error_Handler();
	}

	uint8_t* payload = out_data + 3; // Reserve three bytes for the header.
	uint16_t payload_max_len = max_len - 3;
	uint16_t payload_len = 0;

	// We do need at least two bytes.
	if (len < 2) {
		out_data[0] = RESPONSE_MESSAGE_TOO_SHORT;
		*out_len = 1;
		return EXIT;
	}

	// Dispatch the message due to the prefix.
	uint8_t prefix = data[0];
	switch(prefix) {
	case PREFIX_CONNECTION:
		exit = adcp_handle_connection_command(connection, data, len, payload, &payload_len, payload_max_len);
		break;
	case PREFIX_DEBUGGING:
		exit = adcp_handle_debugging_command(data, len, payload, &payload_len, payload_max_len);
		break;
	case PREFIX_MEASUREMENT:
		exit = adcp_handle_measurement_command(data, len, payload, &payload_len, payload_max_len);
		break;
	case PREFIX_ADC:
		exit = adcp_handle_ADC_command(data, len, payload, &payload_len, payload_max_len);
		break;
	case PREFIX_FFT:
		exit = adcp_handle_FFT_command(data, len, payload, &payload_len, payload_max_len);
		break;
	case PREFIX_CALIBRATION:
		exit = adcp_handle_calibration_command(data, len, payload, &payload_len, payload_max_len);
		break;
	default:
		payload[0] = RESPONSE_INVALID_PREFIX;
		payload[1] = prefix;
		payload_len = 2;
		exit = EXIT;
		break;
	}

	adcp_write_header(SEND_TYPE_NONE, payload, payload_len, out_len);
	return exit;
}

/**
 * Adds the ADCP header to the payload. Make sure, you have 3 bytes space before the payload (Also
 * for the payload pointer!). The package begin will be returned and the complete package length
 * written to packge_length.
 *
 * Ex.: uint8_t* buffer = <some memory>
 * uint8_t* payload = buffer+3;
 * <put something into payload>
 * uint16_t package_length;
 * uint8_t package_begin = adcp_write_header(payload, payload_length, &package_length);
 */
uint8_t* adcp_write_header(uint8_t send_type, uint8_t* payload, uint16_t payload_length, uint16_t* package_length) {
	uint8_t* package_begin = payload - 3;

	// Set header
	package_begin[0] = send_type;
	*(uint16_t*)(package_begin + 1) = payload_length;

	*package_length = payload_length + 3;
	return package_begin;
}

/**
 * Handles connection commands.
 */
static uint8_t adcp_handle_connection_command(connection_t* connection, uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len) {
	uint8_t command = data[1];
	uint8_t exit = NOEXIT;

	switch (command) {
	case CONNECTION_SET_TYPE:
		if (!adcp_check_arg_len(len, 1, out_data, out_len)) {
			return EXIT;
		}
		connection->send_type = data[2];
		SET_OK;
		break;
	default:
		adcp_send_wrong_command_response(command, out_data, out_len);
		return EXIT;
	}
	return exit;
}

/**
 * handle debugging command
 */
static uint8_t adcp_handle_debugging_command(uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len) {
	uint8_t command = data[1];
	uint8_t exit = NOEXIT;

	switch (command) {
	case DEBUGGING_LWIP_STATS:
		*out_len = format_network_stats(out_data, max_len);
		break;
	case DEBUGGING_TEST_SCHEDULER:
		SET_RESPONSE(RESPONSE_NOT_ENABLED);
		break;
	case DEBUGGING_TEST_MEMORY_BW:
		SET_RESPONSE(RESPONSE_NOT_ENABLED);
		break;
	case DEBUGGING_OS_STATS:
		vTaskGetRunTimeStats((char*)out_data, max_len);
		*out_len = (uint16_t)strlen((char*)out_data);
		if (*out_len == 0) {
			SET_RESPONSE(RESPONSE_NO_MEMORY);
		}
		break;
	case DEBUGGING_CONNECTION_STATS:
		*out_len = format_connections_stats(out_data, max_len);
		break;
	default:
		adcp_send_wrong_command_response(command, out_data, out_len);
		return EXIT;
	}
	return exit;
}

/**
 * Handle measurement command
 */
static uint8_t adcp_handle_measurement_command(uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len) {
	uint8_t command = data[1];
	uint8_t* args = data+2;

	// used in the switch-case:
	ProtocolError err;
	uint16_t averaging;
	uint8_t id;

	if (is_ADC_reset_flag_set()) {
		SET_RESPONSE(RESPONSE_ADC_RESET);
		return NOEXIT;
	}

	switch (command) {
	case MEASUREMENT_START:
		err = measure_start();
		update_adc_state(0);
		SET_RESPONSE(err);
		break;
	case MEASUREMENT_STOP:
		err = measure_stop();
		update_adc_state(0);
		SET_RESPONSE(err);
		break;
	case MEASUREMENT_CREATE: // args: pos, neg, enabled, averaging (uint16_t)
		if (!adcp_check_arg_len(len, 5, out_data, out_len)) {
			return EXIT;
		}
		averaging = *(uint16_t*)(args+3);
		id = 0;
		err = measurement_create(args[0], args[1], args[2], averaging, &id);

		SET_RESPONSE(err); // Add the status code anyway.
		if (err == RESPONSE_OK) { // Add the id on success.
			out_data[1] = id;
			*out_len = 2;
		}
		break;
	case MEASUREMENT_DELETE:
		if (!adcp_check_arg_len(len, 1, out_data, out_len)) {
			return EXIT;
		}
		err = measurement_delete(args[0]);
		SET_RESPONSE(err);
		break;
	case MEASUREMENT_SET_INPUTS:
		if (!adcp_check_arg_len(len, 3, out_data, out_len)) {
			return EXIT;
		}
		err = measurement_set_inputs(args[0], args[1], args[2]);
		SET_RESPONSE(err);
		break;
	case MEASUREMENT_SET_ENABLED:
		if (!adcp_check_arg_len(len, 2, out_data, out_len)) {
			return EXIT;
		}
		err = measurement_set_enabled(args[0], args[1]);
		SET_RESPONSE(err);
		break;
	case MEASUREMENT_SET_AVERAGING:
		if (!adcp_check_arg_len(len, 3, out_data, out_len)) {
			return EXIT;
		}
		averaging = *(uint16_t*)(args+1);
		err = measurement_set_averaging(args[0], averaging);
		SET_RESPONSE(err);
		break;
	case MEASUREMENT_ONE_SHOT: // Args: measurement id
		if (!adcp_check_arg_len(len, 1, out_data, out_len)) {
			return EXIT;
		}
		int32_t value = 0;
		err = measure_oneshot(args[0], &value);
		SET_RESPONSE(err); // Add the status code.
		*((int32_t*)(out_data + 1)) = value;
		*out_len = 5;
		break;
	default:
		adcp_send_wrong_command_response(command, out_data, out_len);
		return EXIT;
	}

	update_measurement_state(1);
	return NOEXIT;
}

/**
 * Handle ADC command
 */
static uint8_t adcp_handle_ADC_command(uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len) {
	uint8_t command = data[1];
	uint8_t* args = data+2;

	if (is_measure_active() && command != ADC_GET_STATUS) {
		SET_RESPONSE(RESPONSE_MEASUREMENT_ACTIVE);
		return NOEXIT;
	}

	if (is_ADC_reset_flag_set() && command != ADC_RESET && command != ADC_GET_STATUS) {
		SET_RESPONSE(RESPONSE_ADC_RESET);
		return NOEXIT;
	}

	switch (command) {
	case ADC_RESET:
		ADS1262_reset();
		clear_ADC_reset_flag();
		ADS1262_set_to_state(get_current_state());
		SET_OK;
		break;
	case ADC_SET_SR:
		if (!adcp_check_arg_len(len, 1, out_data, out_len)) {
			return EXIT;
		}
		if (args[0] > 15) {
			SET_RESPONSE(RESPONSE_WRONG_ARGUMENT);
			return EXIT;
		}
		ADS1262_set_samplerate(args[0]);
		SET_OK;
		break;
	case ADC_SET_FILTER:
		if (!adcp_check_arg_len(len, 1, out_data, out_len)) {
			return EXIT;
		}
		if (args[0] > 4) {
			SET_RESPONSE(RESPONSE_WRONG_ARGUMENT);
			return EXIT;
		}
		ADS1262_set_filter(args[0]);
		SET_OK;
		break;
	case ADC_PGA_SET_GAIN:
		if (!adcp_check_arg_len(len, 1, out_data, out_len)) {
			return EXIT;
		}
		if (args[0] > 5) {
			SET_RESPONSE(RESPONSE_WRONG_ARGUMENT);
			return EXIT;
		}
		ADS1262_set_gain(args[0]);
		SET_OK;
		break;
	case ADC_PGA_BYPASS:
		ADS1262_bypass_PGA();
		SET_OK;
		break;
	case ADC_REF_SET_INTERNAL:
		ADS1262_enable_internal_reference();
		// 0 and 0 are the pins for the internal voltage reference
		ADS1262_set_reference(0, 0, ADS1262_REF_INTERNAL_TENNANOVOLTS);
		ADS1262_set_reference_polarity_normal();
		SET_OK;
		break;
	case ADC_REF_SET_EXTERNAL: // uint64_t v_ref, pos pin, neg pin as uin8_t
		if (!adcp_check_arg_len(len, 6, out_data, out_len)) {
			return EXIT;
		}
		uint64_t v_ref = *(uint64_t*)args;
		uint8_t pos = args[4];
		uint8_t neg = args[5];
		// Both arguments must be between 1 and 4. This are the available inputs.
		if (pos < 1 || pos > 5 || neg < 1 || neg > 5) {
			SET_RESPONSE(RESPONSE_WRONG_REFERENCE_PINS);
			return NOEXIT;
		}
		ADS1262_set_reference(pos, neg, v_ref);
		ADS1262_set_reference_polarity_normal();
		ADS1262_disable_internal_reference();
		SET_OK;
		break;
	case ADC_GET_STATUS:
		*out_len = state_copy_to_buffer(out_data+1, max_len);
		if (*out_len == 0) {
			SET_RESPONSE(RESPONSE_NO_MEMORY);
		} else {
			out_data[0] = RESPONSE_OK;
			(*out_len)++;
		}
		break;
	default:
		adcp_send_wrong_command_response(command, out_data, out_len);
		return EXIT;
	}

	// Do update, if not just the state was requested. All other commands did change something.
	if (command != ADC_GET_STATUS || !is_measure_active()) {
		update_adc_state(1);
	}

	return NOEXIT;
}

/**
 * Handle FFT command
 */
static uint8_t adcp_handle_FFT_command(uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len) {
	uint8_t command = data[1];
	uint8_t* args = data+2;

	if (is_measure_active()) {
		SET_RESPONSE(RESPONSE_MEASUREMENT_ACTIVE);
		return NOEXIT;
	}

	measurement_t* m;
	switch (command) {
	case FFT_SET_ENABLED: // id and enabled as uint8_t's
		if (!adcp_check_arg_len(len, 2, out_data, out_len)) {
			return EXIT;
		}
		m = measurement_get_by_id(args[0]);
		if (NULL == m) {
			SET_RESPONSE(RESPONSE_NO_SUCH_MEASUREMENT);
		} else {
			fft_set_enabled(&(m->fft), args[1]);
			SET_OK;
		}
		break;
	case FFT_SET_LENGTH: // id (uint8_t) and length (uint16_t)
		if (!adcp_check_arg_len(len, 3, out_data, out_len)) {
			return EXIT;
		}
		m = measurement_get_by_id(args[0]);
		if (NULL == m) {
			SET_RESPONSE(RESPONSE_NO_SUCH_MEASUREMENT);
		} else {
			if (fft_set_length(&(m->fft), *(uint16_t*)(args+1))) {
				SET_OK;
			} else {
				SET_RESPONSE(RESPONSE_FFT_INVALID_LENGTH)
			}
		}
		break;
	case FFT_SET_WINDOW: // id and window_index both as uint8_t.
		if (!adcp_check_arg_len(len, 2, out_data, out_len)) {
			return EXIT;
		}
		m = measurement_get_by_id(args[0]);
		if (NULL == m) {
			SET_RESPONSE(RESPONSE_NO_SUCH_MEASUREMENT);
		} else {
			SET_RESPONSE(fft_set_window(&(m->fft), args[1]));
		}
		break;
	default:
		adcp_send_wrong_command_response(command, out_data, out_len);
		return EXIT;
	}

	update_measurement_state(1);
	return NOEXIT;
}

/**
 * Handle calibration commands
 */
static uint8_t adcp_handle_calibration_command(uint8_t* data, uint16_t len, uint8_t* out_data, uint16_t* out_len, uint16_t max_len) {
	uint8_t command = data[1];
	uint8_t* args = data+2;

	if (is_measure_active()) {
		SET_RESPONSE(RESPONSE_MEASUREMENT_ACTIVE);
		return NOEXIT;
	}

	if (is_ADC_reset_flag_set()) {
		SET_RESPONSE(RESPONSE_ADC_RESET);
		return NOEXIT;
	}

	int32_t offset;
	uint32_t scale;
	ProtocolError err;

	switch (command) {
	case CALIBRATION_SET_OFFSET: // We need a int32_t, but limited by 24 bits. If the number send is too big
		// of too small, it will be limited.
		if (!adcp_check_arg_len(len, 4, out_data, out_len)) {
			return EXIT;
		}
		ADS1262_set_calibration_offset(*((int32_t*)args));
		SET_OK;
		break;
	case CALIBRATION_SET_SCALE: // We need a uin32_t, but limited to 2 bits. If the number is too big, it
		// will be limited.
		if (!adcp_check_arg_len(len, 4, out_data, out_len)) {
			return EXIT;
		}
		ADS1262_set_calibration_scale(*((uint32_t*)args));
		SET_OK;
		break;
	case CALIBRATION_DO_OFFSET: // pos, neg pins
		if (!adcp_check_arg_len(len, 2, out_data, out_len)) {
			return EXIT;
		}
		err = measure_do_offset_calibration(args[0], args[1], &offset);
		SET_RESPONSE(err); // Add the status code.
		*((int32_t*)(out_data + 1)) = offset;
		*out_len = 5;
		break;
	case CALIBRATION_DO_SCALE:
		if (!adcp_check_arg_len(len, 2, out_data, out_len)) {
			return EXIT;
		}
		err = measure_do_scale_calibration(args[0], args[1], &scale);
		SET_RESPONSE(err); // Add the status code.
		*((uint32_t*)(out_data + 1)) = scale;
		*out_len = 5;
		break;
	default:
		adcp_send_wrong_command_response(command, out_data, out_len);
		return EXIT;
	}

	update_adc_state(1);

	return NOEXIT;
}

/**
 * Checks, if the length matches the expects length for all args. This is tested for bytes, so if an uint16_t is
 * required, the length is 2. If the length is too low, an error will be prepared in out_data and out_len
 * to be send to the client and this function returns 0. Returns 1 on success.
 */
static int8_t adcp_check_arg_len(uint16_t len, uint8_t expected, uint8_t* out_data, uint16_t* out_len) {
	// the length includes the prefix and command.
	if (len < (expected+2)) {
		// too few arguments..
		out_data[0] = RESPONSE_TOO_FEW_ARGUMENTS;
		out_data[1] = expected;
		*out_len = 2;
		return 0;
	}
	return 1;
}

/**
 * Sends the wrong command error.
 */
static void adcp_send_wrong_command_response(uint8_t command, uint8_t* out_data, uint16_t* out_len) {
	out_data[0] = RESPONSE_INVALID_COMMAND;
	out_data[1] = command;
	*out_len = 2;
}
