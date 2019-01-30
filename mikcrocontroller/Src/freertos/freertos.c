#include "FreeRTOS.h"
#include "task.h"
#include "stdio.h"
#include "error.h"
#include "string.h"

// Hook prototypes
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook();

#if ( configUSE_IDLE_HOOK == 1)
void vApplicationIdleHook() {}
#endif

void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName) {
	printf("Stackoverflow in Task %s. Terminating...\n", pcTaskName);
	Error_Handler();
}

void vApplicationMallocFailedHook() {
	printf("Malloc failed!\n");
}

/**
 * See https://www.freertos.org/uxTaskGetSystemState.html
 */
void vTaskGetRunTimeStats(char *writeBuffer, uint16_t max_len) {
	TaskStatus_t *taskStatusArray;
	volatile UBaseType_t arraySize;
	unsigned long totalRunTime;

	// Clear the write buffer* Make sure the write buffer does not contain a string.
	*writeBuffer = 0x00;

	// Take a snapshot of the number of tasks in case it changes while this function is executing.
	arraySize = uxTaskGetNumberOfTasks();

	// Finn: Estimate the size needed for the table to avoid write-out-of-bounds by
	// 128 Bytes per task + 128 bytes for the table.
	uint16_t len_estimate = (arraySize + 1) * 128;
	if (max_len < len_estimate) {
		return;
	}

	// Allocate a TaskStatus_t structure for each task.  An array could be allocated statically at compile time.
	taskStatusArray = pvPortMalloc(arraySize * sizeof(TaskStatus_t));
	if(taskStatusArray == NULL) {
		return;
	}

	// Generate raw status information about each task.
	if ((arraySize = uxTaskGetSystemState(taskStatusArray, arraySize, &totalRunTime)) == 0) {
		vPortFree(taskStatusArray);
		return;
	}

	// For percentage calculations.
	totalRunTime /= 100UL;

	// Avoid divide by zero errors.
	if(totalRunTime <= 0) {
		vPortFree(taskStatusArray);
		return;
	}

	// For each populated position in the pxTaskStatusArray array,
    // format the raw data as human readable ASCII data.
	for(unsigned long i = 0; i < arraySize; i++) {
		// Percentage of the total run time that has the task used
		unsigned long ulStatsAsPercentage =
				taskStatusArray[i].ulRunTimeCounter / totalRunTime;

		strcpy(writeBuffer, taskStatusArray[i].pcTaskName);
		size_t len = strlen(taskStatusArray[i].pcTaskName);
		writeBuffer += len;

		// fill with whitespaces to 20 characters
		for (size_t i = 0; i < (15-len); i++) {
			*writeBuffer = ' ';
			writeBuffer++;
		}

		writeBuffer += sprintf(writeBuffer, "\t%lu\t\t", taskStatusArray[i].ulRunTimeCounter);
		if(ulStatsAsPercentage > 0UL) {
			writeBuffer += sprintf(writeBuffer, "%lu%%\t", ulStatsAsPercentage);
		} else {
			writeBuffer += sprintf(writeBuffer, "<1%%\t");
		}
		switch (taskStatusArray[i].eCurrentState) {
		case eReady:
			writeBuffer += sprintf(writeBuffer, "(Ready)\t\t\t");
			break;
		case eRunning:
			writeBuffer += sprintf(writeBuffer, "(Running)\t\t");
			break;
		case eBlocked:
			writeBuffer += sprintf(writeBuffer, "(Blocked)\t\t");
			break;
		case eSuspended:
			writeBuffer += sprintf(writeBuffer, "(Suspended)\t\t");
			break;
		case eDeleted:
			writeBuffer += sprintf(writeBuffer, "(Deleted)\t\t");
			break;
		case eInvalid:
			writeBuffer += sprintf(writeBuffer, "(Invalid)\t\t");
			break;
		}
		writeBuffer += sprintf(writeBuffer, "%u\n", taskStatusArray[i].usStackHighWaterMark);
	}

	vPortFree(taskStatusArray);
}
