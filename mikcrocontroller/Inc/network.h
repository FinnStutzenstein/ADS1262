#ifndef __NETWORK_H_
#define __NETWORK_H_

#include "cmsis_os.h"
#include "pool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_STATUS_TASK_DELAY	250

#define EXIT				0x01
#define NOEXIT				0x00

extern Pool* connection_pool;
extern osSemaphoreId connection_semaphore;

void network_init();
void network_start();

#ifdef __cplusplus
}
#endif

#endif
