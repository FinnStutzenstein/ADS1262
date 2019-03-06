/*
 * ads1262.c
 *
 * Note: There is a difference between get and read here: get returns the (mirrored)
 * register in the RAM of the microcontroller. "read" reads the register from the ADC.
 *
 * There is the complete register bank copied into the microcontrollers ram and kept in
 * sync during reads and writes.
 *
 *  Created on: Sep 19, 2018
 *      Author: finn
 */


#include "ads1262.h"
#include "stdio.h"
#include "error.h"

static SPI_HandleTypeDef hspi;
static ADS1262_REGISTER_MAP_Type registerMap;
__IO static uint8_t* registerArray = (uint8_t *)&registerMap.ID.reg;

static uint64_t voltage_reference = ADS1262_REF_INTERNAL_TENNANOVOLTS;

static void ADS1262_enable_status_byte_and_checksum();
static void ADS1262_clear_reset_flag();
static void ADS1262_send_command(uint8_t command);
static void ADS1262_write_registers(uint8_t start_reg, uint8_t num);
static void ADS1262_write_register(uint8_t reg);
static void ADS1262_read_registers(uint8_t start_reg, uint8_t num);
static uint8_t ADS1262_read_register(uint8_t reg);

/**
 * Initializes the SPI bus, resets the ADC and read all it's registers.
 */
void ADS1262_Init() {
	hspi.Instance = SPI2;
	hspi.Init.Mode = SPI_MODE_MASTER;
	hspi.Init.Direction = SPI_DIRECTION_2LINES;
	hspi.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi.Init.CLKPhase = SPI_PHASE_2EDGE;
	hspi.Init.NSS = SPI_NSS_SOFT;
	hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; // Original clock: 50MHz -> 6.25MHz
	hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

	if (HAL_SPI_Init(&hspi) != HAL_OK) {
		Error_Handler();
	}
	ADS1262_reset();
}

/**
 * Initializes the underlying required GPIO pins for SPI, SPI itself and the DRDY interrupt.
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef* _hspi) {
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct;

	// SCK
	GPIO_InitStruct.Pin = ADS1262_SCK_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(ADS1262_SCK_PORT, &GPIO_InitStruct);

	// MISO
	GPIO_InitStruct.Pin = ADS1262_MISO_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(ADS1262_MISO_PORT, &GPIO_InitStruct);

	// MOSI
	GPIO_InitStruct.Pin = ADS1262_MOSI_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(ADS1262_MOSI_PORT, &GPIO_InitStruct);

	// DRDY
	GPIO_InitStruct.Pin = ADS1262_DRDY_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(ADS1262_DRDY_PORT, &GPIO_InitStruct);

	// Enable interrupts for DRDY
	HAL_NVIC_SetPriority(ADS1262_DRDY_EXTI_LINE, 6, 0);
	HAL_NVIC_EnableIRQ(ADS1262_DRDY_EXTI_LINE);

	// Reset
	GPIO_InitStruct.Pin = ADS1262_RESET_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(ADS1262_RESET_PORT, &GPIO_InitStruct);
}

/**
 * Deinitializes the hardware.
 */
void HAL_SPI_MspDeInit(SPI_HandleTypeDef* hspi) {
	// Peripheral clock disable
	__HAL_RCC_SPI2_CLK_DISABLE();

	// disable interrupts
	HAL_NVIC_DisableIRQ(ADS1262_DRDY_EXTI_LINE);

	HAL_GPIO_DeInit(ADS1262_SCK_PORT, ADS1262_SCK_PIN);
	HAL_GPIO_DeInit(ADS1262_MISO_PORT, ADS1262_MISO_PIN);
	HAL_GPIO_DeInit(ADS1262_MOSI_PORT, ADS1262_MOSI_PIN);
	HAL_GPIO_DeInit(ADS1262_DRDY_PORT, ADS1262_DRDY_PIN);
	HAL_GPIO_DeInit(ADS1262_RESET_PORT, ADS1262_RESET_PIN);
}

/**
 * Resets the ADS1262
 */
void ADS1262_reset() {
	HAL_GPIO_WritePin(ADS1262_RESET_PORT, ADS1262_RESET_PIN, GPIO_PIN_RESET);
	HAL_Delay(10); // 2^16/7.3728MHz (=1/f_clk) ~= 9ms.
	HAL_GPIO_WritePin(ADS1262_RESET_PORT, ADS1262_RESET_PIN, GPIO_PIN_SET);
	HAL_Delay(25);

	// get all registers and enable additional bytes for the data-readout.
	ADS1262_read_registers(0, ADS1262_REG_NUM);
	ADS1262_enable_status_byte_and_checksum();
	ADS1262_clear_reset_flag();
}

/**
 * Enables the optional status- and checksumbyte.
 */
static void ADS1262_enable_status_byte_and_checksum() {
	// setup status byte and Checksum
	registerMap.INTERFACE.bit.STATUS = ADS1262_ENABLE;
	registerMap.INTERFACE.bit.CRC_MODE = ADS1262_CHECKSUM;
	ADS1262_write_register(ADS1262_INTERFACE);
}

/**
 * Clears the internal reset flag
 */
static void ADS1262_clear_reset_flag() {
	registerMap.POWER.bit.RESET = 0;
	ADS1262_write_register(ADS1262_POWER);
}

/**
 * Given the state, configure the ADC to this state. The ADC is also stopped.
 */
void ADS1262_set_to_state(complete_state_t* state) {
	// set ADC. Maybe it was running..
	ADS1262_stop_ADC();

	// set mux to same inputs, to keep both potentials equal.
	ADS1262_set_input_mux_pos_neg(ADS1262_AIN0, ADS1262_AIN0);

	// no chop mode and continuous mode
	ADS1262_set_chop_mode(ADS1262_CHOP_0);
	ADS1262_set_delay(ADS1262_DELAY_0);
	ADS1262_set_continuous_mode();

	// Set calibration
	ADS1262_set_calibration_offset(state->adc.calibration_offset);
	ADS1262_set_calibration_scale(state->adc.calibration_scale);

	// PGA settings
	if (state->adc.pga == 0xFF) {
		ADS1262_bypass_PGA();
	} else {
		ADS1262_set_gain(state->adc.pga);
	}

	if (state->adc.flags.internal_reference) {
		// vref: INTREF on, REF_MUX auf internal (0x00), REFREV=0
		ADS1262_enable_internal_reference();
		ADS1262_set_reference(ADS1262_REF_POS_INT, ADS1262_REF_NEG_INT, ADS1262_REF_INTERNAL_TENNANOVOLTS);
	} else {
		uint8_t pos = state->adc.v_ref_inputs & 0x0F;
		uint8_t neg = (state->adc.v_ref_inputs >> 4) & 0x0F;
		ADS1262_disable_internal_reference();
		ADS1262_set_reference(pos, neg, state->adc.v_ref_tennanovolt);
	}
	ADS1262_set_reference_polarity_normal();

	// no bias
	ADS1262_set_bias(ADS1262_BIAS_ADC1, ADS1262_BIAS_PULLDOWN, ADS1262_BIAS_MAG_0);
	// no IDAC
	ADS1262_set_idac_magnitudes(ADS1262_IDAC_MAG_OFF, ADS1262_IDAC_MAG_OFF);

	uint8_t samplerate = state->adc.sr_filter & 0x0F;
	uint8_t filter = (state->adc.sr_filter >> 4) & 0x0F;
	ADS1262_set_samplerate(samplerate);
	ADS1262_set_filter(filter);
}

inline uint8_t ADS1262_read_ID() {
	return ADS1262_read_register(ADS1262_ID);
}

inline void ADS1262_start_ADC() {
	ADS1262_send_command(ADS1262_START);
}

inline void ADS1262_stop_ADC() {
	ADS1262_send_command(ADS1262_STOP);
}

/**
 * Reads the last conversion result from the ADC and returns the voltage
 * in 10 nanovolts (10^-8). This value is 0 on errors and (if given) the error-flag
 * set to 1. Also if a status byte is given, the read status is saved there.
 */
int32_t ADS1262_read_ADC(ADS1262_STATUS_Type* status, uint8_t* error) {
	union { // create a structure to hold all the data
		struct {
			uint32_t DATA4:8; // bits 0.. 7
			uint32_t DATA3:8; // bits 8.. 15
			uint32_t DATA2:8; // bits 16.. 23
			uint32_t DATA1:8; // bits 24.. 31
		} bit;
		int32_t reg;
	} ADC_BYTES;

	static uint8_t txBuffer[7] = {0};
	static uint8_t rxBuffer[7] = {0};

	for (int i = 0; i < 7; i++) {
		rxBuffer[i] = 0;
	}

	// set read command
	txBuffer[0] = ADS1262_RDATA;

	HAL_StatusTypeDef hal_status = HAL_SPI_TransmitReceive(&hspi, txBuffer, rxBuffer, 7, 1000);
	if (hal_status != HAL_OK) {
		if (NULL != error) {
			*error = 1;
		}
		return 0;
	}

	if (NULL != status) {
		status->reg = rxBuffer[1]; // this are the status from the ADC
	}

	// save the data bytes
	ADC_BYTES.bit.DATA1 = rxBuffer[2];
	ADC_BYTES.bit.DATA2 = rxBuffer[3];
	ADC_BYTES.bit.DATA3 = rxBuffer[4];
	ADC_BYTES.bit.DATA4 = rxBuffer[5];

	uint8_t checksum = rxBuffer[6];
	uint8_t calculated_checksum = rxBuffer[2] + rxBuffer[3] + rxBuffer[4] + rxBuffer[5] + ADS1262_CHECKSUM_BYTE;
	if (checksum != calculated_checksum) {
		if (NULL != error) {
			*error = 1;
		}
		return 0;
	}

	int64_t _tenNanoVolt = ADC_BYTES.reg * voltage_reference;
	int32_t tenNanoVolt = (int32_t)(_tenNanoVolt/(1LL<<31)); // 2^31
	return tenNanoVolt;
}

inline void ADS1262_set_continuous_mode() {
	registerMap.MODE0.bit.RUNMODE = ADS1262_CONV_CONT;
	ADS1262_write_register(ADS1262_MODE0);
}

inline void ADS1262_set_pulse_mode() {
	registerMap.MODE0.bit.RUNMODE = ADS1262_CONV_PULSE;
	ADS1262_write_register(ADS1262_MODE0);
}

inline void ADS1262_set_input_mux_pos_neg(uint8_t pos_pin, uint8_t neg_pin) {
	registerMap.INPMUX.bit.MUXN = neg_pin;
	registerMap.INPMUX.bit.MUXP = pos_pin;
    ADS1262_write_register(ADS1262_INPMUX);
}

inline void ADS1262_set_input_mux(uint8_t reg) {
	registerMap.INPMUX.reg = reg;
	ADS1262_write_register(ADS1262_INPMUX);
}

inline uint8_t ADS1262_make_input_mux_from_pos_neg(uint8_t pos_pin, uint8_t neg_pin) {
	ADS1262_INPMUX_Type tmp;
	tmp.bit.MUXN = neg_pin;
	tmp.bit.MUXP = pos_pin;
	return tmp.reg;
}

inline void ADS1262_set_samplerate(uint8_t samplerate) {
	registerMap.MODE2.bit.DR = samplerate;
	ADS1262_write_register(ADS1262_MODE2);
}

inline uint8_t ADS1262_read_samplerate() {
	ADS1262_read_register(ADS1262_MODE2);
	return registerMap.MODE2.bit.DR;
}

inline uint8_t ADS1262_get_samplerate() {
	return registerMap.MODE2.bit.DR;
}

inline void ADS1262_set_filter(uint8_t filter) {
	registerMap.MODE1.bit.FILTER = filter;
	ADS1262_write_register(ADS1262_MODE1);
}

inline uint8_t ADS1262_read_filter() {
	ADS1262_read_register(ADS1262_MODE1);
	return registerMap.MODE1.bit.FILTER;
}

inline void ADS1262_set_gain(uint8_t gain) {
	// also enable the PGA
	registerMap.MODE2.bit.BYPASS = ADS1262_PGA_ENABLE;

	registerMap.MODE2.bit.GAIN = gain;
	ADS1262_write_register(ADS1262_MODE2);
}

inline uint8_t ADS1262_read_gain() {
	ADS1262_read_register(ADS1262_MODE2);
	if (registerMap.MODE2.bit.BYPASS) {
		return 0xFF;
	} else {
		return registerMap.MODE2.bit.GAIN;
	}
}

/**
 * Enable with set_gain
 */
inline void ADS1262_bypass_PGA() {
	registerMap.MODE2.bit.BYPASS = ADS1262_PGA_BYPASS;
	ADS1262_write_register(ADS1262_MODE2);
}

inline void ADS1262_set_reference(uint8_t pos, uint8_t neg, uint64_t v_ref) {
	registerMap.REFMUX.bit.RMUXP = pos;
	registerMap.REFMUX.bit.RMUXN = neg;
	ADS1262_write_register(ADS1262_REFMUX);
	voltage_reference = v_ref;
}

inline uint64_t ADS1262_get_reference_voltage() {
	return voltage_reference;
}

inline void ADS1262_enable_internal_reference() {
	registerMap.POWER.bit.INTREF = ADS1262_ENABLE;
	ADS1262_write_register(ADS1262_POWER);
}

inline void ADS1262_disable_internal_reference() {
	registerMap.POWER.bit.INTREF = ADS1262_DISABLE;
	ADS1262_write_register(ADS1262_POWER);
}

inline uint8_t ADS1262_read_is_internal_reference_used() {
	ADS1262_read_register(ADS1262_POWER);
	ADS1262_read_register(ADS1262_REFMUX);
	return registerMap.POWER.bit.INTREF && registerMap.REFMUX.bit.RMUXN == 0 && registerMap.REFMUX.bit.RMUXP == 0;
}

inline uint8_t ADS1262_read_reference_pins() {
	ADS1262_read_register(ADS1262_REFMUX);
	return (registerMap.REFMUX.bit.RMUXN & 0x0F) | ((registerMap.REFMUX.bit.RMUXP & 0x0F) << 4);
}

inline void ADS1262_set_reference_polarity_normal() {
	registerMap.MODE0.bit.REFREV = ADS1262_DISABLE;
	ADS1262_write_register(ADS1262_MODE0);
}

inline void ADS1262_set_reference_polarity_reverse() {
	registerMap.MODE0.bit.REFREV = ADS1262_ENABLE;
	ADS1262_write_register(ADS1262_MODE0);
}

// Calibration
int32_t ADS1262_read_calibration_offset() {
	ADS1262_read_registers(ADS1262_OFCAL0, 3);

	int32_t offset = (registerMap.OFCAL2.reg << 16) | (registerMap.OFCAL1.reg << 8) | registerMap.OFCAL0.reg;

	// number is neg do sign extension:
	if ((registerMap.OFCAL2.reg & 0x80)) {
		offset |= 0xFF << 24;
	}
	return offset;
}

/**
 * Sets the offset.
 */
void ADS1262_set_calibration_offset(int32_t offset) {
	if (offset > 0x7fffff) {
		offset = 0x7fffff;
	} else if (offset < (-0x800000)) {
		offset = 0x800000;
	}
	registerMap.OFCAL0.reg = offset & 0xFF;
	registerMap.OFCAL1.reg = (offset >> 8) & 0xFF;
	registerMap.OFCAL2.reg = (offset >> 16) & 0xFF;
	ADS1262_write_registers(ADS1262_OFCAL0, 3);
}


inline void ADS1262_send_offset_calibration_command() {
	ADS1262_send_command(ADS1262_SYOCAL);
}

inline uint32_t ADS1262_read_calibration_scale() {
	ADS1262_read_registers(ADS1262_FSCAL0, 3);
	return (registerMap.FSCAL2.reg << 16) | (registerMap.FSCAL1.reg << 8) | registerMap.FSCAL0.reg;
}

/**
 * Sets the scale.
 */
void ADS1262_set_calibration_scale(uint32_t scale) {
	if (scale > 0xFFFFFF) {
		scale = 0xFFFFFF;
	}
	registerMap.FSCAL0.reg = scale & 0xFF;
	registerMap.FSCAL1.reg = (scale >> 8) & 0xFF;
	registerMap.FSCAL2.reg = (scale >> 16) & 0xFF;
	ADS1262_write_registers(ADS1262_FSCAL0, 3);
}

inline void ADS1262_send_scale_calibration_command() {
	ADS1262_send_command(ADS1262_SYGCAL);
}

// The following are not configurable, but are used during reset
inline void ADS1262_set_chop_mode(uint8_t mode) {
	registerMap.MODE0.bit.CHOP = mode;
	ADS1262_write_register(ADS1262_MODE0);
}

inline void ADS1262_set_delay(uint8_t del) {
	registerMap.MODE0.bit.DELAY = del;
	ADS1262_write_register(ADS1262_MODE0);
}

inline void ADS1262_set_bias(uint8_t adc_choice, uint8_t polarity, uint8_t mag) {
	registerMap.MODE1.bit.SBADC = adc_choice;
	registerMap.MODE1.bit.SBPOL = polarity;
	registerMap.MODE1.bit.SBMAG = mag;
	ADS1262_write_register(ADS1262_MODE1);
}


inline void ADS1262_set_idac_pins(uint8_t idac1, uint8_t idac2) {
	registerMap.IDACMUX.bit.MUX1 = idac1;
	registerMap.IDACMUX.bit.MUX2 = idac2;
	ADS1262_write_register(ADS1262_IDACMUX);
}

inline void ADS1262_set_idac_magnitudes(uint8_t mag1, uint8_t mag2) {
	registerMap.IDACMAG.bit.MAG1 = mag1;
	registerMap.IDACMAG.bit.MAG2 = mag2;
	ADS1262_write_register(ADS1262_IDACMAG);
}

// ###### Internal communication with the ADS1262

static void ADS1262_send_command(uint8_t command) {
	HAL_SPI_Transmit(&hspi, &command, 1, 1000);
}

static void ADS1262_write_registers(uint8_t start_reg, uint8_t num) { // page 87
	assert_param(num <= ADS1262_REG_NUM);
	assert_param(start_reg < ADS1262_REG_NUM);
	assert_param((num + start_reg) <= ADS1262_REG_NUM);
	uint8_t txBuffer[ADS1262_TXRX_MAX_LENGTH] = {0}; // plenty of room, all zeros

	txBuffer[0] = start_reg | ADS1262_WREG; // first byte is starting register with write command
	txBuffer[1] = num-1; // tell how many registers to write, see datasheet

	// put the desired register values in buffer
	for(uint8_t i = 0; i < num; i++) {
		txBuffer[i + 2] = registerArray[i + start_reg];
	}

	// have the microcontroller send the amounts, plus the commands
	HAL_SPI_Transmit(&hspi, txBuffer, num + 2, 1000);
}

static void ADS1262_write_register(uint8_t reg) {
	ADS1262_write_registers(reg,1);
}

static void ADS1262_read_registers(uint8_t start_reg, uint8_t num) { // page 86
	assert_param(num <= ADS1262_REG_NUM);
	assert_param(start_reg < ADS1262_REG_NUM);
	assert_param((num + start_reg) <= ADS1262_REG_NUM);
	static uint8_t txBuffer[ADS1262_TXRX_MAX_LENGTH] = {0}; // plenty of room, all zeros
	static uint8_t rxBuffer[ADS1262_TXRX_MAX_LENGTH] = {0};

	for (int i = 0; i < ADS1262_TXRX_MAX_LENGTH; i++) {
		rxBuffer[i] = 0;
	}

	txBuffer[0] = start_reg | ADS1262_RREG; // first byte is starting register with read command
	txBuffer[1] = num-1; // tell how many registers to read, see datasheet

	HAL_SPI_TransmitReceive(&hspi, txBuffer, rxBuffer, num + 2, 1000);

	// save the commands to the register
	for(uint8_t i = 0; i < num; i++) {
		registerArray[i + start_reg] = rxBuffer[i + 2];
	}
}

static uint8_t ADS1262_read_register(uint8_t reg) {
	ADS1262_read_registers(reg,1);
	return registerArray[reg];
}
