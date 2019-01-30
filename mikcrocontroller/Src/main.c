#include "state.h"
#include "network.h"
#include "main.h"
#include "error.h"
#include "setup.h"
#include "cmsis_os.h"
#include "fatfs.h"
#include "ads1262.h"
#include "stdio.h"
#include "utils.h"
#include "measure.h"
#include "sd_config.h"

void defaultTaskFunction(void const *argument);

int main() {
	// Setup hardware and the ADS1262.
	setup();
	ADS1262_Init();

    // Create the main thread
    osThreadDef(defaultTask, defaultTaskFunction, osPriorityNormal, 0, 1024);
    osThreadCreate(osThread(defaultTask), NULL);

    // Start scheduler
    osKernelStart();
	while (1) {}
}

void defaultTaskFunction(void const *argument) {
	// init code for FATFS
	FATFS_init();

	// Read the config from the SD card. Needed to initialize the network.
	sd_config_t* config = read_sd_config();

	// init code for LWIP
	network_init(config);

	// init the measure system.
	measure_init();

	// init the current state and try to get it from the SD card.
	init_state();
	init_system_from_last_state();

	uint8_t id = ADS1262_read_ID();
	printf("ads1262 id: %u\n", id);

	// Setup the network. Starts the server task
	network_start();

	// Infinite loop
	while (1) {
		osDelay(1000);
	}
}
