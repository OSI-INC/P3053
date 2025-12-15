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
	physical interface driver. The first, tcp_tick, takes no arguments and
	returns no value. This procedure can be called anywhere without specifying a
	socket. The second takes a TCP/IP socket as its argument and returns a
	negative value if and only if the socket is closed or disconnected.
*/
void tcp_tick(void);
int tcp_socket_tick(void* context);

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
 	A tcpip_tasks_type is the type of procedure that will be called by our
 	generic TCP/IP server. It must take as argument a tcpip_server_info
 	structure. If it blocks the server, it must call tcp_socket_tick while it is
 	blocking. If tcp_socket_tick returns a negative value, the task must abort and
 	return a negative value itself.
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
