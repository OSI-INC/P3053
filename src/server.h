/*
	server.h -- Interface of the TCP/IP Server and Communication library.

	Provides a TCP/IP server state machine that does not block, maintains a
	listening socket and a single connection to that socket, and can be called
	maintained by calling a single routine in the application's main loop. The
	server code is re-entrant, so that we can launch as many of them as we like,
	provided each one has its own dedicated listening socket. Each server calls
	a task routine that maintains whatever function the server is supposed to
	provide on the socket. Thus the particular tasks of a particular server
	function are separated from the shared tasks of listening, accepting, and
	closing a socket. These shared tasks are implemented by our tcpip_server
	routine.	
	
	Provides routines to maintain, read, and write from TCP sockets that we find
	more convenient than the Harmony3 routines, and which in addition take a
	pointer to a TCP_SOCKET type rather than a TCP_SOCKET type itself. Because
	the routines accept a pointer rather than a TCP-specific type, they can be
	used with the generic communication channel descriptor structure we define
	in our Command-Line Interface (CLI) library, so that we can provide a CLI on
	a TCP/IP socket, as well as on a UART interface, or an artificial channel
	communicating with memory, or any other channel that writes and reads bytes,
	without changing the CLI code to accommodate the peculiarities of the
	physical channel. The CLI is just one example of a piece of code that is
	channel-agnostic. Any other channel-agnostic project would also require TCP
	routines that conformed to a generic format. 
	
	(C) 2025, Kevan Hashemi, Open Source Instruments Inc.

	This program is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation, either version 3 of the License, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along with
	this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef SERVER_H
#define SERVER_H

#include "config.h"

/*
	We store the configuration of our server in a EEM configuration record.
*/
#define EEM_CONFIG_STR_SIZE 31
typedef char eem_config_str[EEM_CONFIG_STR_SIZE];
typedef struct {
	eem_config_str magic_str;
    uint32_t security_level;
    eem_config_str password_str;
    eem_config_str operator_str;
	eem_config_str ip_str;
	eem_config_str gw_str;
	eem_config_str nm_str;
    eem_config_str time_str;
    eem_config_str device_str;
    uint16_t lwdaq_port;
	uint16_t telnet_port;
    uint32_t lwdaq_timeout;
    uint32_t telnet_timeout;
    uint32_t tcp_tick_ms;
} eem_config_type;

/*
	Here is a string that is unlikely to appear in flash memory at random. We
	use it to determine if the flash memory EEM configuration has been written
	by our code deliberately some time in the past. We assign it to the
	configuration's magic string, and we look for it in the magic string when we
	read a configuration from flash memory. If we see anything other than the
	correct magic string, we assume the configuration is invalid and substitute
	the default configuration.
*/
#define CONFIG_FLASH_MAGIC "The Pelagic Argosy Sites Land"

/*
	The factory EEM configuration is the one we use when we have not yet written
	a configuration to flash memory. 
*/
static const eem_config_type eem_config_factory = {
	.magic_str = CONFIG_FLASH_MAGIC,
	.security_level = 0,
	.password_str = "LWDAQ",
	.operator_str = "unassigned",
	.ip_str = "10.0.0.37",
	.gw_str = "10.0.0.1",
	.nm_str = "255.255.255.0",
	.time_str = "00000000000000",
	.device_str = PLATFORM_VARIANT,
	.lwdaq_port = 90,
	.telnet_port = 23,
	.lwdaq_timeout = 10,
	.telnet_timeout = 300,
	.tcp_tick_ms = 2
};

/*
	The active Embedded Ethernet Module (EEM) configuration is the current
	configuration of the server. No procedure is permitted to modify the active
	configuration, even though it is possible with our library routines to
	update the IP address, the EEM rules prohibit live modification of the
	network interface after start-up. On start-up, the EEM reads the flash
	configuration record into the active configuration record and applies all
	its fields to the newly-started server.
*/
extern eem_config_type eem_config_active;

/*
	We want to be able to modify the Embedded Ethernet Module (EEM) network
	interface, and various other configuration parameters, and have these
	parameters persist from one power cycle to the next. To achieve this end, we
	store the configuration in flash memory. On start-up, we load the
	configuration into our active configuration record and we use the active
	configuration record to configure the server. If the configuration switch on
	the host board is pressed, however, the EEM performs a factory reset by
	writing its factory configuration record to flash memory before it reads the
	flash memory configuration into the active configuration record. By this
	means, the EEM provides both persistent configuration and restorable
	configuration. The EEM configuration rules are simple: we are not permitted
	to modify the EEM network settings while the EEM is running. Instead, if we
	want to change the network settings, we write the values we want to flash
	memory, then reboot the EEM to apply these values. This rule simplifies our
	configuration implementation and debugging. We apply network settings only
	once: during start-up. We have a record of settings we can examine to see
	what is going on, but we do not have to worry about keeping this record up
	to date with the current EEM settings.

	The PIC32MZ2048EFH's 2 MByte of flash memory appears twice in the CPU's
	virtual address space. Once in the range 0x9D000000 to 0x9D0FFFFF, in which
	range the flash memory is accessed by the CPU indirectly through a cache
	memory, and again in the range 0xBD000000 to 0xBD0FFFFF, in which range the
	CPU accesses the flash memory directly, without a cache. We want to put our
	string in the un-cached copy so that we can write with the direct memory
	access (DMA) hardware and read back immediately from the physical flash
	memory rather than getting a stale copy of the flash memory from cache. So
	we place our string in the 0xBD range. In that range, we must make sure we
	are above the program itself. In our case, our program is less than 256
	KByte, so anywhere above 0xBD040000 will be fine. The flash memory pages are
	16 Kbyte in the PIC32MZ, and flash memory rows are 2 KByte. So we must pick
	a location that is on a 16-KByte boundary. Reading repeatedly from un-cached
	flash memory is far slower than reading repeatedly from a cached copy of an
	flash memory page, but we do not plan to read repeatedly from our
	configuration string, so we will suffer no loss of performance from reading
	direction from flash memory.
*/
#define EEM_CONFIG_ADDR 0xBD100000
int eem_config_write(const eem_config_type* config);
int eem_config_read(eem_config_type* config);
int server_str_from_config(char* str, const eem_config_type* config_ptr);
int eem_config_from_str(eem_config_type* config_ptr, const char* str);

/*
	Buffer sizes we recommend for server tasks.
*/
#define TCP_TX_BUFF_SIZE 4096
#define TCP_RX_BUFF_SIZE 4096

/*
	Two procedures that maintain the TCP/IP stack and Ethernet
	physical interface driver.
*/
int tcp_tick(void);
int tcp_sock_tick(void* context);

/*
	Routines that read and write from sockets. These are wrappers for Harmony
	routines. They are compatible with our command-line interface (CLI) through
	their use of a generic pointer rather than a TCP socket index. Instead of a
	TCP_SOCKET type, we pass to these routines a pointer to a TCP_SOCKET. The
	routines de-reference the pointer and so fetch the socket index, which they
	then pass to the lower-level Harmony library routines. It is this use of a
	generic pointer that makes the routines compatible with the CLI and other
	channel-agnostic communication processes we might devise.
*/
int tcp_readcount(void* context);
int tcp_read(void* context, uint8_t* buffer, uint32_t len);
int tcp_getchar(void* context);
int tcp_writecount(void* context);
int tcp_write(void* context, const uint8_t* data, uint32_t len);
int tcp_putchar(void* context, char c);
int tcp_flush(void* context);
int tcp_writeall(void* context, const uint8_t *buf, uint16_t len);

/*
 	The tcpip_server_state structure defines the states of our generic TCP/IP server process.
*/
typedef enum {
    S_WAIT_STACK,
    S_WAIT_IP,
    S_OPEN_SERVER,
    S_LISTENING,
    S_CONNECTED,
    S_SERVING,
    S_CLOSE,
    S_ERROR
} tcpip_server_state_type;

/*
	The tcpip_server_type structure provides a complete description of one of
	our TCP/IP servers. The protocol string holds the name of the protocol. The
	socket field holds a TCP socket index. The server state is an enumerated
	type defined above. The port pointer should point to a field in a
	configuration record that holds the port that should be used for this
	server. We recommend creating dedicated port fields for each server in the
	active EEM configuration record, and setting the port pointer to point to
	the corresponding field in the configuration record. The bound port and
	bound IP address fields hold the port and IP address to which the server is
	currently bound. The server checks to see if the bound IP address and port
	number differ from the current IP address asserted by the TCP/IP stack or
	the port number pointed to by the port pointer, and if either differ, the
	server will restart. When we chanage the IP address or port number at which
	we want the server to listen, we must close the server sockets and re-open
	them again so they will be bound to the new IP address and port number. The
	last_tick field records the time when a byte was last received on the
	socket. We use this in the server state machine to implement a timeout. The
	only feature of the server that is not encoded in this structure is the
	protocol task procedure, which we pass into the server routine as a separate
	argument.
*/
typedef struct {
	const char* protocol;
	TCP_SOCKET socket;
 	tcpip_server_state_type state;
	uint16_t port;
	char ip_str[EEM_CONFIG_STR_SIZE];
	uint32_t tcp_timeout;
	uint32_t last_tick;
	bool logged_in;
} tcpip_server_type;

/*
	Routines for TCP/IP management and reporting.
*/
void ping_gateway(void);
int server_set_ip(const char* ip_str, const char* gw_str, const char* nm_str);
int server_mac(uint8_t* out);
int server_mac_str(char* out);
int server_ip_str(char* out);
int server_nm_str(char* out);
int server_gw_str(char* out);
int server_interface_str(char* out);
bool server_linked(void);
int server_info(char* out);

/*
 	A tcpip_tasks_type is the type of procedure that will be called by our
 	generic TCP/IP server. It must take as argument a tcpip_server_info
 	structure. If it blocks the server, it must call tcp_sock_tick while it
 	is blocking. If tcp_sock_tick returns a negative value, the task must
 	abort and return a negative value itself.
*/
typedef int (*tcpip_tasks_type)(tcpip_server_type* s);

/*
	Our generic TCP/IP server procedure. The tcpip_server is a generic
	connection management routine that we can use for a variety of protocols, so
	long as the protocol tasks can be implemented in a non-blocking state
	machine of its own.
*/
void tcpip_server(tcpip_server_type* s, tcpip_tasks_type tasks);

/*
	Command-Line Interpreter (CLI) procedures that can be registered with our CLI
	to add server functions.
*/
#include "cli.h"
void cli_eem_config(cli_chan_type *ch, char *args);

#endif
