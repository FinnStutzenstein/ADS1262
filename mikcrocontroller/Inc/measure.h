/*
 * measure.h
 *
 *  Created on: Oct 13, 2018
 *      Author: finn
 */

#ifndef MEASURE_H_
#define MEASURE_H_

#include "adcp.h"
#include "config.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VALUE_BUFFER_SIZE		206 /* 206 for exactly (8+206*7=) 1450 bytes of payload. +3+4 for ADCP and WS
									   results in max. 1457 bytes to send (MTU is 1460).*/

typedef enum {
	CALIBRATION_TYPE_OFFSET,
	CALIBRATION_TYPE_SCALE,
} calibration_type_t;

typedef volatile struct __packed {
	uint8_t id_and_status;
	int32_t value;
	uint16_t timestamp_delta;
} value_t;

typedef enum {
	MEASURE_STATE_IDLE 			= 0,
	MEASURE_STATE_RUNNING 		= 1,
	MEASURE_STATE_ONESHOT 		= 2,
	MEASURE_STATE_CALIBRATING	= 3,
} measure_state_t;

void measure_init();

#ifdef SIMULATE_ADC
void DRDY_Interrupt(int32_t test_value);
#else
void DRDY_Interrupt();
#endif

uint8_t is_measure_active();
measure_state_t measure_get_state();

ProtocolError measure_start();
ProtocolError measure_stop();

ProtocolError measure_oneshot(uint8_t measurement_id, int32_t* value);

ProtocolError measure_do_offset_calibration(uint8_t pos_input, uint8_t neg_input, int32_t* offset);
ProtocolError measure_do_scale_calibration(uint8_t pos_input, uint8_t neg_input, uint32_t* scale);

#ifdef __cplusplus
}
#endif

#endif /* MEASURE_H_ */
