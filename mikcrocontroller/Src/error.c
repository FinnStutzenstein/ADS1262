/*
 * error.c
 *
 *  Created on: Oct 11, 2018
 *      Author: finn
 */
#include "config.h"
#include "error.h"
#include "stdio.h"

/**
  * This function is executed in case of error occurrence.
  */
void _Error_Handler(char *file, int line) {
	printf("Error handler: File %s on line %d\r\n", file, line);
	while(1) {}
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line) {
	printf("Wrong parameters value: file %s on line %lu\r\n", file, line);
}
#endif /* USE_FULL_ASSERT */
