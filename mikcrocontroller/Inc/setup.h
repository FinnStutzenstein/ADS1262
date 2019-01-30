#ifndef __SETUP_H__
#define __SETUP_H__

#include "error.h"

#define RMII_TXD0_Pin					GPIO_PIN_13
#define RMII_TXD0_GPIO_Port				GPIOG
#define RMII_TXD1_Pin					GPIO_PIN_14
#define RMII_TXD1_GPIO_Port				GPIOG
#define RMII_TX_EN_Pin					GPIO_PIN_11
#define RMII_TX_EN_GPIO_Port			GPIOG
#define RMII_MDC_Pin					GPIO_PIN_1
#define RMII_MDC_GPIO_Port				GPIOC
#define RMII_RXER_Pin					GPIO_PIN_2
#define RMII_RXER_GPIO_Port				GPIOG
#define RMII_REF_CLK_Pin				GPIO_PIN_1
#define RMII_REF_CLK_GPIO_Port			GPIOA
#define RMII_RXD0_Pin					GPIO_PIN_4
#define RMII_RXD0_GPIO_Port				GPIOC
#define RMII_MDIO_Pin					GPIO_PIN_2
#define RMII_MDIO_GPIO_Port				GPIOA
#define RMII_RXD1_Pin					GPIO_PIN_5
#define RMII_RXD1_GPIO_Port				GPIOC
#define RMII_CRS_DV_Pin					GPIO_PIN_7
#define RMII_CRS_DV_GPIO_Port			GPIOA
#define SDMMC_CK_Pin					GPIO_PIN_12
#define SDMMC_CK_GPIO_Port				GPIOC
#define SDMMC_D3_Pin					GPIO_PIN_11
#define SDMMC_D3_GPIO_Port				GPIOC
#define SDMMC_D2_Pin					GPIO_PIN_10
#define SDMMC_D2_GPIO_Port				GPIOC
#define SDMMC_CMD_Pin					GPIO_PIN_2
#define SDMMC_CMD_GPIO_Port				GPIOD
#define uSD_Detect_Pin					GPIO_PIN_13
#define uSD_Detect_GPIO_Port			GPIOC
#define FMC_NBL1_Pin					GPIO_PIN_1
#define FMC_NBL1_GPIO_Port				GPIOE
#define FMC_NBL0_Pin					GPIO_PIN_0
#define FMC_NBL0_GPIO_Port				GPIOE
#define FMC_SDCLK_Pin					GPIO_PIN_8
#define FMC_SDCLK_GPIO_Port				GPIOG
#define FMC_SDNME_Pin					GPIO_PIN_5
#define FMC_SDNME_GPIO_Port				GPIOH
#define FMC_SDNCAS_Pin					GPIO_PIN_15
#define FMC_SDNCAS_GPIO_Port			GPIOG
#define FMC_SDNE0_Pin					GPIO_PIN_3
#define FMC_SDNE0_GPIO_Port				GPIOH
#define FMC_SDCKE0_Pin					GPIO_PIN_3
#define FMC_SDCKE0_GPIO_Port			GPIOC
#define FMC_BA1_Pin						GPIO_PIN_5
#define FMC_BA1_GPIO_Port				GPIOG
#define FMC_BA0_Pin						GPIO_PIN_4
#define FMC_BA0_GPIO_Port				GPIOG
#define FMC_SDNRAS_Pin					GPIO_PIN_11
#define FMC_SDNRAS_GPIO_Port			GPIOF
#define FMC_D0_Pin						GPIO_PIN_14
#define FMC_D0_GPIO_Port				GPIOD
#define FMC_D1_Pin						GPIO_PIN_15
#define FMC_D1_GPIO_Port				GPIOD
#define FMC_D2_Pin						GPIO_PIN_0
#define FMC_D2_GPIO_Port				GPIOD
#define FMC_D3_Pin						GPIO_PIN_1
#define FMC_D3_GPIO_Port				GPIOD
#define FMC_D4_Pin						GPIO_PIN_7
#define FMC_D4_GPIO_Port				GPIOE
#define FMC_D5_Pin						GPIO_PIN_8
#define FMC_D5_GPIO_Port				GPIOE
#define FMC_D6_Pin						GPIO_PIN_9
#define FMC_D6_GPIO_Port				GPIOE
#define FMC_D7_Pin						GPIO_PIN_10
#define FMC_D7_GPIO_Port				GPIOE
#define FMC_D8_Pin						GPIO_PIN_11
#define FMC_D8_GPIO_Port				GPIOE
#define FMC_D9_Pin						GPIO_PIN_12
#define FMC_D9_GPIO_Port				GPIOE
#define FMC_D10_Pin						GPIO_PIN_13
#define FMC_D10_GPIO_Port				GPIOE
#define FMC_D11_Pin						GPIO_PIN_14
#define FMC_D11_GPIO_Port				GPIOE
#define FMC_D12_Pin						GPIO_PIN_15
#define FMC_D12_GPIO_Port				GPIOE
#define FMC_D13_Pin						GPIO_PIN_8
#define FMC_D13_GPIO_Port				GPIOD
#define FMC_D14_Pin						GPIO_PIN_9
#define FMC_D14_GPIO_Port				GPIOD
#define FMC_D15_Pin						GPIO_PIN_10
#define FMC_D15_GPIO_Port				GPIOD
#define FMC_A0_Pin						GPIO_PIN_0
#define FMC_A0_GPIO_Port				GPIOF
#define FMC_A1_Pin						GPIO_PIN_1
#define FMC_A1_GPIO_Port				GPIOF
#define FMC_A2_Pin						GPIO_PIN_2
#define FMC_A2_GPIO_Port				GPIOF
#define FMC_A3_Pin						GPIO_PIN_3
#define FMC_A3_GPIO_Port				GPIOF
#define FMC_A4_Pin						GPIO_PIN_4
#define FMC_A4_GPIO_Port				GPIOF
#define FMC_A5_Pin						GPIO_PIN_5
#define FMC_A5_GPIO_Port				GPIOF
#define FMC_A6_Pin						GPIO_PIN_12
#define FMC_A6_GPIO_Port				GPIOF
#define FMC_A7_Pin						GPIO_PIN_13
#define FMC_A7_GPIO_Port				GPIOF
#define FMC_A8_Pin						GPIO_PIN_14
#define FMC_A8_GPIO_Port				GPIOF
#define FMC_A9_Pin						GPIO_PIN_15
#define FMC_A9_GPIO_Port				GPIOF
#define FMC_A10_Pin						GPIO_PIN_0
#define FMC_A10_GPIO_Port				GPIOG
#define FMC_A11_Pin						GPIO_PIN_1
#define FMC_A11_GPIO_Port				GPIOG

/* #define SDRAM_MEMORY_WIDTH            FMC_SDRAM_MEM_BUS_WIDTH_8  */
#define SDRAM_MEMORY_WIDTH               FMC_SDRAM_MEM_BUS_WIDTH_16

#define SDCLOCK_PERIOD                   FMC_SDRAM_CLOCK_PERIOD_2
/* #define SDCLOCK_PERIOD                FMC_SDRAM_CLOCK_PERIOD_3 */

#define REFRESH_COUNT                    ((uint32_t)0x0603)   /* SDRAM refresh counter (100Mhz SD clock) */

#define SDRAM_TIMEOUT                    ((uint32_t)0xFFFF)


/**
  * @brief  FMC SDRAM Mode definition register defines
  */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)


#ifdef __cplusplus
 extern "C" {
#endif

volatile uint64_t measure_reference_timer_ticks;

void setup();
void TIM2_Init();

#ifdef __cplusplus
}
#endif

#endif
