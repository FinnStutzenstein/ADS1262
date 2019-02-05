/*
 * sd_config.h
 *
 *  Created on: Jan 29, 2019
 *      Author: finn
 */

#ifndef SD_CONFIG_H_
#define SD_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/tcpip.h"
#include "stdint.h"

#define CONFIG_FILENAME	"0:/config"

typedef struct {
	uint8_t use_dhcp;
	uint8_t dhcp_timeout; // in seconds
	ip_addr_t ip_addr;
	ip_addr_t netmask;
	ip_addr_t gateway;
} sd_config_t;

sd_config_t* read_sd_config();

#ifdef __cplusplus
}
#endif

#endif /* SD_CONFIG_H_ */
