/*
 * error.h
 *
 *  Created on: Oct 11, 2018
 *      Author: finn
 */

#ifndef ERROR_H_
#define ERROR_H_

#include "stdint.h"


#ifdef __cplusplus
 extern "C" {
#endif

 void _Error_Handler(char *, int);
 void assert_failed(uint8_t* file, uint32_t line);

 #define Error_Handler() _Error_Handler(__FILE__, __LINE__)


#ifdef __cplusplus
}
#endif


#endif /* ERROR_H_ */
