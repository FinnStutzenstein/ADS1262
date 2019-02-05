/*
 * config.h
 *
 *  Created on: Oct 11, 2018
 *      Author: finn
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include "stdint.h"
#include "stddef.h"
#include "sys/cdefs.h"

// TCP reated configs
#define MAX_CONNECTIONS 	8
#define MAX_MEASUREMENTS	10

#define LISTEN_PORT		80
/* MAC ADDRESS: MAC_ADDR0:MAC_ADDR1:MAC_ADDR2:MAC_ADDR3:MAC_ADDR4:MAC_ADDR5 */
#define MAC_ADDR0   	0x00
#define MAC_ADDR1   	0x80
#define MAC_ADDR2   	0xE1
#define MAC_ADDR3   	0x00
#define MAC_ADDR4   	0x00
#define MAC_ADDR5   	0x00

//#define SIMULATE_ADC
// scaling: 100khz/scaling is the frequence the drdy interrupt is called.
#define SIMULATE_ADC_SCALING	3

//#define LWIP_DEBUG
#define NETWORK_STATS


// FreeRTOS related configs
// 2^15 (32K)
//#define configTOTAL_HEAP_SIZE                    ((size_t)32768)
// 2^17 (128K)
#define configTOTAL_HEAP_SIZE                    ((size_t)131072)
#define configAPPLICATION_ALLOCATED_HEAP         1
static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ] __section(".sram1") __used;

#define ENABLE_RUNTIMESTATS					 1

// General configs
#define USE_FULL_ASSERT    1U

#endif /* CONFIG_H_ */
