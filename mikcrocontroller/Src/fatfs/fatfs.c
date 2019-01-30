#include "fatfs.h"
#include "error.h"
#include "string.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"

static FATFS _fs;
FATFS* FS = &_fs;

void FATFS_init() {
	char device_prefix[4];

	// Link the SD driver. The driver is defined in the sd_discio.h
	if (!FATFS_LinkDriver(&SD_Driver, device_prefix)) {
		Error_Handler();
	}

	// The device should always get this prefix.
	if (strncmp(device_prefix, "0:/", 3) != 0) {
		Error_Handler();
	}

	// Check for the SD card.
	if (BSP_SD_IsDetected() != SD_PRESENT) {
		printf("No sd card detected!\n");
		Error_Handler();
	}
	printf("sd card detected...\n");

	FRESULT fres;
	if ((fres = f_mount(FS, "0:/", 1)) != FR_OK) {
		printf("mount error: %d\n", fres);
		Error_Handler();
	}
	printf("...mounted!\n");
}

/*	Unmounting..
	if (f_mount(NULL, "0:/", 1) != FR_OK) {
		Error_Handler();
	}
 */

DWORD get_fattime() {
	return 0;
}

WCHAR ff_convert (WCHAR wch, UINT dir) {
	if (wch < 0x80) {
		/* ASCII Char */
		return wch;
	}

	// I don't support unicode; it is too big!
	return 0;
}

WCHAR ff_wtoupper (WCHAR wch) {
	if (wch < 0x80) {
		/* ASCII Char */
		if (wch >= 'a' && wch <= 'z') {
			wch &= ~0x20;
		}
		return wch;
	}

	// I don't support unicode; it is too big!
	return 0;
}
