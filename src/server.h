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

/*
	Buffer sizes we recommend for server tasks.
*/
#define TCP_TX_BUFF_SIZE 4096
#define TCP_RX_BUFF_SIZE 4096

/*
	Two procedures that maintain the TCP/IP stack, including the Ethernet
	physical interface driver. The first, tcpip_tick, takes no arguments and
	returns no value. This procedure can be called anywhere without specifying a
	socket. The second takes a TCP/IP socket as its argument and returns a
	negative value if and only if the socket is closed or disconnected.
*/
void tcpip_tick(void);
int tcpip_socket_tick(void* context);

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
	our TCP/IP servers. The structure includes the server state, a handle to the
	socket, which is either invalid, listening or connected, the port number the
	server should listen on, and the name of the protocol it serves. The IP
	address to which the socket is bound we save in its own field, which we
	check every time we call the server maintenance routine, tcpip_server, to
	see if it is the same as the current IP address. When we chanage the IP
	address, we must close the server sockets and re-open them again so they
	will be bound to the new IP address. The only feature of the server that is
	not encoded in this structure is the protocol tast procedure, which we pass
	into the server routine as a separate argument.
*/
typedef struct {
	TCP_SOCKET socket;
 	tcpip_server_state_type state;
	int port;
	const char* protocol;
	IPV4_ADDR ip_addr;
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
int server_name_str(char* out);
bool server_linked(void);
int server_info(char* out);

/*
	We store the configuration of our server in a server configuration record.
*/
#define SERVER_CONFIG_STR_SIZE 31
static const char server_config_magic_str[] = "The Pelagic Argosy Sites Land";
typedef char server_config_str[SERVER_CONFIG_STR_SIZE];
typedef struct {
	const char* magic_str;
	server_config_str ip_str;
	server_config_str gw_str;
	server_config_str nm_str;
    server_config_str operator_str;
    server_config_str time_str;
    server_config_str password_str;
    server_config_str lwdaq_port_str;
    server_config_str telnet_port_str;
    server_config_str security_level_str;
    server_config_str tcp_timeout_str;
} server_config_type;

/*
	The default server configuration is the one we use when we have not yet written
	a configuration to non-volatile memory.
*/
const server_config_type server_config_default = {
	.magic_str = server_config_magic_str,
	.ip_str = "10.0.0.37",
	.gw_str = "10.0.0.1",
	.nm_str = "255.255.255.0",
	.operator_str = "unassigned",
	.time_str = "00000000000000",
	.password_str = "LWDAQ",
	.lwdaq_port_str = "90",
	.telnet_port_str = "23",
	.security_level_str = "0",
	.tcp_timeout_str = "10"
};

/*
	The active server configuration is the one that is intended to reflect the current
	state of the server. The accuracy of this record does, however, depend upon the
	cooperation of the procedures that modify the state of the server. If we modify the
	IP address, for example, we must record our modification in the server configuration.
*/
server_config_type server_config_active;

/*
	We want to be able to update the server configuration and have the server
	remember the configuration through reset and power cycles. We can place all
	necessary configuration information in a server configuration record, and
	these records have fixed length, even though they consist of strings. We
	store the configuration record in non-volatile memory as a table of
	null-terminated strings. We read them in the same way, by copying directly
	into a server configuration record.

	The PIC32MZ2048EFH's 2 MByte of NVM appears twice in the CPU's virtual
	address space. Once in the range 0x9D000000 to 0x9D0FFFFF, in which range
	the NVM is accessed by the CPU indirectly through a cache memory, and again
	in the range 0xBD000000 to 0xBD0FFFFF, in which range the CPU accesses the
	NVM directly, without a cache. We want to put our string in the un-cached
	copy so that we can write with the direct memory access (DMA) hardware and
	read back immediately from the physical NVM rather than getting a stale copy
	of the NVM from cache. So we place our string in the 0xBD range. In that
	range, we must make sure we are above the program itself. In our case, our
	program is less than 256 KByte, so anywhere above 0xBD040000 will be fine.
	The NVM pages are 16 Kbyte in the PIC32MZ, and NVM rows are 2 KByte. So we
	must pick a location that is on a 16-KByte boundary. Reading repeatedly from
	un-cached NVM is far slower than reading repeatedly from a cached copy of an
	NVM page, but we do not plan to read repeatedly from our configuration
	string, so we will suffer no loss of performance from reading direction from
	NVM.
*/
#define FLASH_CONFIG_ADDR 0xBD100000
int server_config_write(const server_config_type* config);
int server_config_read(server_config_type* config);

/*
 	A tcpip_tasks_type is the type of procedure that will be called by our
 	generic TCP/IP server. It must take as argument a tcpip_server_info
 	structure. If it blocks the server, it must call tcpip_socket_tick while it
 	is blocking. If tcpip_socket_tick returns a negative value, the task must
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

#endif
