#include "network.h"
#include "config.h"
#include "cmsis_os.h"
#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "ethernetif.h"
#include "stm32f7xx_hal.h"
#include "connection.h"
#include "send_data.h"
#include "sd_config.h"

static uint8_t use_dhcp;

static ip_addr_t default_ip_addr;
static ip_addr_t default_netmask;
static ip_addr_t default_gateway;

struct netconn *server_connection;
struct netif networkinterface;
extern ETH_HandleTypeDef heth;

osThreadId network_status_task_handle;
osThreadId server_task_handle;
osSemaphoreId connection_semaphore;

DEFINE_POOL_IN_HEAP(connection_pool, MAX_CONNECTIONS, connection_t);

static void start_server_task();
static void network_status_task_function(void const *argument);
static void server_task_function(void const *argument);

/**
 * Network initialization. Sets up LwIP.
 */
void network_init(sd_config_t* config)
{
	// Initialize private variables
	network_status_task_handle = NULL;
	server_task_handle = NULL;

	osSemaphoreDef(connection_semaphore_def);
	connection_semaphore = osSemaphoreCreate(osSemaphore(connection_semaphore_def), MAX_CONNECTIONS);

	pool_init(connection_pool);

	connections_init();
	send_data_init();

	ip4_addr_copy(default_ip_addr, config->ip_addr);
	ip4_addr_copy(default_netmask, config->netmask);
	ip4_addr_copy(default_gateway, config->gateway);

	// Initialize LwIP
	ip4_addr_t ip_addr;
	ip4_addr_t netmask;
	ip4_addr_t gateway;

	// Set values.
	if (config->use_dhcp) {
		printf("DHCP is configured.\n");

		ip_addr.addr = 0;
		netmask.addr = 0;
		gateway.addr = 0;
	} else {
		ip4_addr_copy(ip_addr, config->ip_addr);
		ip4_addr_copy(netmask, config->netmask);
		ip4_addr_copy(gateway, config->gateway);
		printf("No DHCP.\nIP address: %s\nNetmask: %s\nGateway: %s\n",
				ip4addr_ntoa(&ip_addr), ip4addr_ntoa(&netmask), ip4addr_ntoa(&gateway));
	}

	tcpip_init(NULL, NULL);

	// add the network interface with RTOS
	netif_add(&networkinterface, &ip_addr, &netmask, &gateway, NULL, &ethernetif_init, &tcpip_input);

	netif_set_link_callback(&networkinterface, &ethernetif_update_config);

	// Registers the default network interface
	netif_set_default(&networkinterface);

	if (netif_is_link_up(&networkinterface)) {
		netif_set_up(&networkinterface);
	} else {
		netif_set_down(&networkinterface);
	}
}

void network_start() {
	// Start the network status task
	osThreadDef(network_status_task, network_status_task_function, osPriorityBelowNormal, 1, 512);
	network_status_task_handle = osThreadCreate(osThread(network_status_task), NULL);
	if (NULL == network_status_task_handle) {
		Error_Handler();
	}
}

// checks the status of the link. Handles the DHCP and manages the server task.
static void network_status_task_function(void const *argument) {
	uint8_t link_status = 0;
	uint8_t wait_for_dhcp = 0; // Not used, if use_dhcp == 0
	uint8_t dhcp_timeout_counter = 0;

	while(1) {
		// check for changed link status. Query the PHY chip.
		uint32_t phyreg = 0;
		HAL_ETH_ReadPHYRegister(&heth, PHY_BSR, &phyreg);
		phyreg &= PHY_LINKED_STATUS;

		if (phyreg != link_status) {
			// Link status changed. Set link and interface status
			if (phyreg & PHY_LINKED_STATUS) {
				netif_set_link_up(&networkinterface);
			} else {
				netif_set_link_down(&networkinterface);
			}
			link_status = phyreg;

			// update interface
			if (netif_is_link_up(&networkinterface)) {
				// When the netif is fully configured this function must be called
				netif_set_up(&networkinterface);

				if (use_dhcp) {
					printf("Get an ip from the network (timeout: %ds)...\n", DHCP_TIMEOUT/1000);
					// Start DHCP negotiation
					dhcp_start(&networkinterface);

					dhcp_timeout_counter = 0;
					wait_for_dhcp = 1;
				} else {
					// directly start the servertask, because we have an ip assigned per default.
					start_server_task();
				}
			} else {
				// When the netif link is down this function must be called
				netif_set_down(&networkinterface);

				// stop server thread
				if (NULL != server_task_handle) {
					if (osThreadTerminate(server_task_handle) == osOK) {
						if (NULL != server_connection) {
							netconn_close(server_connection);
							netconn_delete(server_connection);
							server_connection = NULL;
						}
					}
					server_task_handle = NULL;
				}
			}
		}

		// update, if the DHCP has assigned us an IP
		if (wait_for_dhcp) {
			dhcp_timeout_counter++;
			if (dhcp_supplied_address(&networkinterface)) {
				printf("Got IP address: %s\nNetmask: %s\nGateway: %s\n",
						ip4addr_ntoa(&(networkinterface.ip_addr)),
						ip4addr_ntoa(&(networkinterface.netmask)),
						ip4addr_ntoa(&(networkinterface.gw)));

				wait_for_dhcp = 0;

				// start echo thread
				start_server_task();
			} else if ((dhcp_timeout_counter * NETWORK_STATUS_TASK_DELAY) > DHCP_TIMEOUT) {
				dhcp_stop(&networkinterface);
				netif_set_addr(&networkinterface, &default_ip_addr, &default_netmask, &default_gateway);
				printf("DHCP timeout. Using default addresses.\nIP address: %s\nNetmask: %s\nGateway: %s\n",
						ip4addr_ntoa(&default_ip_addr),
						ip4addr_ntoa(&default_netmask),
						ip4addr_ntoa(&default_gateway));

				wait_for_dhcp = 0;
				start_server_task();
			}
		}

		osDelay(NETWORK_STATUS_TASK_DELAY);
	}
}

/**
 * Starts the server task...
 */
static void start_server_task() {
	osThreadDef(server_task, server_task_function, osPriorityNormal, 1, 1024);
	if (NULL == server_task_handle) {
		server_task_handle = osThreadCreate(osThread(server_task), NULL);
		if (NULL == server_task_handle) {
			Error_Handler();
		}
	}
}

/**
 * The server task. Binds to the the listening port and accepts new connections.
 */
static void server_task_function(void const *argument) {
	osThreadDef(connection_task, connection_task_function, osPriorityNormal, MAX_CONNECTIONS, 1024);

	err_t err, accept_err;
	struct netconn* accepted_connection;

	server_connection = netconn_new(NETCONN_TCP);
	if (NULL == server_connection) {
		Error_Handler();
	}

	ip_set_option(server_connection->pcb.tcp, SOF_REUSEADDR);

	// Bind to our port.
	err = netconn_bind(server_connection, NULL, LISTEN_PORT);
	if (err == ERR_OK) {
		// Tell connection to go into listening mode.
		netconn_listen(server_connection);
		while (1) {
			osThreadYield();
			osSemaphoreWait(connection_semaphore, osWaitForever);
			// Grab new connection.
			accept_err = netconn_accept(server_connection, &accepted_connection);

			// Process the new connection.
			if (accept_err == ERR_OK) {
				// start new thread. pass acceptedConnection as arg.
				connection_t *connection = pool_alloc(connection_pool);
				if (NULL == connection) { // this should not happen, in fact this is secured by the connectionSemaphore.
					printf("No free task id was found! Aborting..\n");
					if (NULL != accepted_connection) {
						netconn_close(accepted_connection);
						netconn_delete(accepted_connection);
					}
					continue;
				}
				connection->conn = accepted_connection;
				connection->type = CONNECTION_TYPE_UNKNOWN;
				connection->send_type = SEND_TYPE_NONE;
				const osMutexDef_t mutex = {0};
				connection->write_mutex = osMutexCreate(&mutex);
				// the thread is automatically started. Make sure it is the last thing to initialize
				connection->thread = osThreadCreate(osThread(connection_task), (void*)connection);
				if (NULL == connection->thread) {
					Error_Handler();
				}
			} else {
				printf("cannot accept the connection. Code: %d\n", accept_err);
				osSemaphoreRelease(connection_semaphore);
			}
		}
	} else {
		printf("cannot bind. Errorcode: %d\n", err);
		netconn_delete(server_connection);
		server_connection = NULL;
	}
}
