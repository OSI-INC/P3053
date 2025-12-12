/*
	server.c -- Interface of the TCP/IP Server and Communication library.
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
	server should listen on, and the name of the protocol it serves. The only
	feature of the server that is not encoded in this structure is the protocol
	tast procedure, which we pass into the server routine as a separate
	argument.
*/
typedef struct {
	TCP_SOCKET socket;
 	tcpip_server_state_type state;
	int port;
	const char* protocol;
} tcpip_server_type;

/*
	Here are some utility routines for TCP/IP management and reporting.
*/
void ping_gateway(void);
int server_set_ip(const char* ip_str, const char* gw_str, const char* mk_str);
void server_mac(uint8_t* out);
void server_info(char* out, uint32_t max_len);

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
