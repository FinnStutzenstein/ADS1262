#ifndef __STM32F7xx_IT_H
#define __STM32F7xx_IT_H

#ifdef __cplusplus
 extern "C" {
#endif 

#include "stm32f7xx_hal.h"
#include "error.h"

void SysTick_Handler(void);
void TIM6_DAC_IRQHandler(void);
void ETH_IRQHandler(void);
void OTG_FS_IRQHandler(void);
void LTDC_IRQHandler(void);
void DMA2D_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif
