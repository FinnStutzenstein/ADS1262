/*
 * watchdog.c
 *
 *  Created on: Jan 22, 2019
 *      Author: finn
 */
#include "watchdog.h"
#include "stm32f7xx_hal.h"
#include "measure.h"
#include "state.h"
#include "ads1262.h"

TIM_HandleTypeDef htim5;

// Flag for the first tick after a watchdog start.
static uint8_t wd_start_flag = 0;

// Signals, if the watchdog is started
static uint8_t wd_started = 0;

// Counts up per tick.
static uint8_t wd_counter = 0;

// The max value for wd_counter. If the counter is greater
// then this value, the ADC has reseted.
static uint8_t wd_max_counter = 1;

/**
 * Sets up the measurement watchdog. This timer fires an interrupt every second
 * and is the same priority as the TIM2, but a lower subpriority.
 * Uses the tiM5 (32 Bit timer)
 */
void measurement_watchdog_init() {
	__HAL_RCC_TIM5_CLK_ENABLE();

	// Runs at 100mhz /4 /25000 = 1000 Hz.
	htim5.Instance = TIM5;
	htim5.Init.Prescaler = 25000;
	htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim5.Init.Period = 1000-1; // 1khz / 1000 -> 1 Hz
	htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV4; // runs with 100mhz / 4 = 25mhz
	htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
	{
		Error_Handler();
	}

	TIM_ClockConfigTypeDef sClockSourceConfig;
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}

	HAL_NVIC_SetPriority(TIM5_IRQn, 5, 1);
	HAL_NVIC_EnableIRQ(TIM5_IRQn);

	HAL_TIM_Base_Start_IT(&htim5);
}

/**
 * Skips, if the wd is stopped, or it's the first tick since the start.
 * Counts `wd_counter` up. If `wd_counter_max` is reached, the watchdog
 * fires, the measurement is stopped and the state updated
 */
void measurement_watchdog_tick() {
	if (!wd_started) {
		return;
	}
	// Skip the first tick, when the watchdog is started. We do not know, how fast
	// this tick is..
	if (wd_start_flag) {
		wd_start_flag = 0;
		return;
	}

	wd_counter++;
	if (wd_counter > wd_max_counter) {
		printf("Watchdog!\n");
		wd_started = 0;
		set_ADC_reset_flag();
		measure_stop();
		update_adc_state(1);
		return;
	}

	wd_counter = 1;
}

/**
 * Resets the watchdog.
 */
inline void measurement_watchdog_reset() {
	wd_counter = 0;
}

/**
 * Starts the watchdog. `enabled_measurements` should give the
 * amount of current measurements. Sets up all needed variables.
 */
void measurement_watchdog_start() {
	wd_max_counter = 1;
	// All samplerates (incl. their conversion latency with enabled multiplexing)
	// are fast enough to produce samples within 1 second. Just with 5 and 2.5 SPS
	// we want to give the watchdog more time.
	uint8_t samplerate = ADS1262_get_samplerate();
	if (ADS1262_RATE_5 == samplerate) {
		wd_max_counter = 2;
	} else if (ADS1262_RATE_2_5 == samplerate) {
		wd_max_counter = 3;
	}
	wd_counter = 0;
	wd_start_flag = 1;
	wd_started = 1;
}

/**
 * Stops the watchdog.
 */
inline void measurement_watchdog_stop() {
	wd_started = 0;
}

