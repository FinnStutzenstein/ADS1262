/*
 * watchdog.h
 *
 *  Created on: Jan 22, 2019
 *      Author: finn
 */

#ifndef WATCHDOG_H_
#define WATCHDOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

void measurement_watchdog_init();
void measurement_watchdog_tick();
void measurement_watchdog_reset();
void measurement_watchdog_start(uint8_t enabled_measurements);
void measurement_watchdog_stop();

#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_H_ */
