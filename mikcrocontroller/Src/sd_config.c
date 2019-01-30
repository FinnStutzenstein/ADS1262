/*
 * config.c
 *
 *  Created on: Jan 29, 2019
 *      Author: finn
 */

#include "sd_config.h"
#include "fatfs.h"
#include "string.h"

static sd_config_t sd_config;
static char read_buffer[256];

static void set_to_defaults();
static uint8_t read_from_sd_card();
static void parse_read_buffer();
static void parse_line(char* line);
static void process_ip_address(char* value, ip_addr_t* dest);

/**
 * Reads the config file from the sd card.
 * returns the config struct with the loaded config.
 * If the reading fails, the defaults will be loaded.
 * All wrong or non existing keys will be set to the defaults. For the
 * defaults see `set_to_defaults()`
 */
sd_config_t* read_sd_config() {
	set_to_defaults();
	if (read_from_sd_card()) {
		parse_read_buffer();
	}

	return &sd_config;
}

/**
 * Sets all config values to defaults
 */
static void set_to_defaults() {
	// The defaults
	sd_config.use_dhcp = 0;
	IP4_ADDR(&(sd_config.ip_addr), 192, 168, 1, 20);
	IP4_ADDR(&(sd_config.netmask), 255, 255, 255, 0);
	IP4_ADDR(&(sd_config.gateway), 192, 168, 1, 1);
}

/*
 * Reads the state file from the sd card into the read_buffer.
 * Returns 1 on success.
 */
static uint8_t read_from_sd_card(uint16_t* read_len) {
	//static sd_read_config_t read_config = {0};

	FIL file;
	FRESULT fres;

	if ((fres = f_open(&file, CONFIG_FILENAME, FA_READ)) != FR_OK) {
		return 0;
	}

	unsigned int bytes_left = sizeof(read_buffer) - 1; // spce for the null terminator
	char* buffer_ptr = read_buffer;
	unsigned int bytes_read;
	uint16_t bytes_read_sum = 0;

	// Read file
	do {
		fres = f_read(&file, buffer_ptr, bytes_left, &bytes_read);
		bytes_left -= bytes_read;
		bytes_read_sum += bytes_read;
		buffer_ptr += bytes_read;
	} while(fres == FR_OK && bytes_read > 0);

	f_close(&file);

	// set \0 terminator (just to be safe)
	read_buffer[bytes_read_sum] = 0;

	return 1;
}

/**
 * Parses the read buffer. All valid key-value-pairs will be
 * parsed into `sd_config`
 */
static void parse_read_buffer() {
	// Go there line by line
	size_t buffer_len = strlen(read_buffer);
	char* buffer_end = read_buffer + buffer_len; // points to the \0

	char* line_start = read_buffer;
	char* line_end;

	uint8_t end = 0;
	while (!end) {
		line_end = strstr(line_start, "\n");
		if (NULL == line_end) {
			// There may be no line ending, but a complete line.
			line_end = buffer_end;
			end = 1;
		}
		*line_end = 0;

		parse_line(line_start);
		line_start = line_end + 1;
	}
}

/**
 * Parses one line. A line is <key>=<value>.
 */
void parse_line(char* line) {
	char* key = line;
	char* equal = strstr(line, "=");
	if (NULL == equal) {
		// not found, skip this line.
		return;
	}
	*equal = 0;

	char* value = equal+1;
	if (0 == value) { // Did we hit the end of the line?
		return;
	}

	if (strcmp(key, "dhcp") == 0 && '1' == *value) {
		sd_config.use_dhcp = 1;
	} else if (strcmp(key, "ip") == 0) {
		process_ip_address(value, &(sd_config.ip_addr));
	} else if (strcmp(key, "netmask") == 0) {
		process_ip_address(value, &(sd_config.netmask));
	} else if (strcmp(key, "gateway") == 0) {
		process_ip_address(value, &(sd_config.gateway));
	}
}

/**
 * Stores the ip address given as a string into `dest`, if it is valid.
 */
static void process_ip_address(char* value, ip_addr_t* dest) {
	ip_addr_t ip_addr;

	if (ip4addr_aton(value, &ip_addr)) {
		ip4_addr_copy(*dest, ip_addr);
	}
}
