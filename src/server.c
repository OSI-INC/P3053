/*
	server.c -- Implementation of the TCP/IP Server and Communication library.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>                  
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include "configuration.h"
#include "definitions.h"
#include "tcpip/tcpip.h"
#include "tcpip/src/tcpip_private.h"
#include "tcpip/icmp.h"
#include "server.h"
#include "console.h"
#include "utils.h"

/*
	tcp_tick maintains the TCP/IP stack without checking the status of any
	particular socket. It returns no value. It refers to the global sysObj
	structure declared in definitions.h of our Harmony library. 
*/
void tcp_tick(void) {
	DRV_MIIM_OBJECT_BASE_Default.DRV_MIIM_Tasks(sysObj.drvMiim_0);
	TCPIP_STACK_Task(sysObj.tcpip);
}

/*
	tcp_socket_tick maintains the TCP/IP stack and checks to see if a particular
	socket is still open. This routine must be called by any protocol task
	whenever it enters a loop waiting for some other process to complete. We say
	the protocol task is "blocking". When this routine returns a negative value,
	the protocol task must stop blocking and return control to the tcpip_server
	routine that manages the protocol connections. The protocol task must pass
	back a negative value so that the tcpip_server can close the listening
	socket and open a new one. The routine takes as its argument a TCP/IP socket
	handle and returns a negative value when the socket has been closed or has
	been determined by the stack to be disconnected.
*/
int tcp_socket_tick(void* context) {
    TCP_SOCKET sock = *(TCP_SOCKET *) context;
	tcp_tick();
	if (!TCPIP_TCP_IsConnected(sock) || TCPIP_TCP_WasDisconnected(sock)) {
		return -1;
	} else {
		return 0;
	}
}

/*
	tcp_readcount returns the number of bytes that are ready to be read out of
	a TCP socket. It is a wrapper for a Harmony routine. It converts sixteen-bit
	integers to thirty-two bit integers for our thirty-two bit processor. Its
	name is in our opinion more suitable.	
*/ 
int tcp_readcount(void* context) {
    TCP_SOCKET sock = *(TCP_SOCKET *) context;
	return (int) TCPIP_TCP_GetIsReady(sock);
}

/*
	tcp_read attempts to read a specified number of bytes from a TCP socket. It
	returns the number of bytes it actually read. It does not block, meaning it
	does not make the calling process wait until the requested number of bytes
	is available. It takes as arguments a socket handle, a byte buffer pointer,
	and an unsigned integer length.
*/
int tcp_read(void* context, uint8_t* buffer, uint32_t len) {
    TCP_SOCKET sock= *(TCP_SOCKET *) context;
	return (int) TCPIP_TCP_ArrayGet(sock, buffer, len);
}

/*
	tcp_getchar attempts to read a single byte from a TCP socket. It returns a -2
	if the socket is disconnected. It returns -1 if the socket is still connected
	but there is no character to be read. Otherwise it returns a character.
*/
int tcp_getchar(void* context) {
    TCP_SOCKET sock= *(TCP_SOCKET *) context;
    uint8_t c;
	if (sock == INVALID_SOCKET) return -2;
    if (TCPIP_TCP_GetIsReady(sock) < 1) return -1;
    if (TCPIP_TCP_ArrayGet(sock, &c, 1) == 1) return (int) c;
    return -1;
}

/*
	tcp_writecount returns how much space we have available for writing to the
	outgoing TCP buffer of a socket. It takes a socket handle.
*/
int tcp_writecount(void* context) {
    TCP_SOCKET sock= *(TCP_SOCKET *) context;
	return (int) TCPIP_TCP_PutIsReady(sock);
}

/*
	tcp_write writes as many bytes as it can to a TCP socket without overflowing
	the outgoing buffer, stopping only when it has written the specified number
	of bytes. It returns the number of bytes it actually wrote. It takes as
	argument a socket handle, a pointer to the byte buffer containing the data
	that should be written, and an unsigned integer length. The routine does not
	flush the socket. It does not block.
*/
int tcp_write(void* context, const uint8_t* data, uint32_t len) {
    TCP_SOCKET sock= *(TCP_SOCKET *) context;
	return (int) TCPIP_TCP_ArrayPut(sock, data, len);
}

/*
	tcp_putchar attempts to write a single character to a TCP socket. If the
	socket is disconnected, it returns -1. If the buffer was full, it returns a
	0. Otherwise it returns a 1 to show success.
*/
int tcp_putchar(void* context, char c) {
	TCP_SOCKET sock= *(TCP_SOCKET *) context;
	if (sock == INVALID_SOCKET) return -1;
	while (TCPIP_TCP_PutIsReady(sock) == 0) {
		if (tcp_socket_tick(context) < 0) return -1;
	}
	if (TCPIP_TCP_ArrayPut(sock, (uint8_t *)&c, 1) == 1) return 1;
	return 0;
}

/*
	tcp_flush forces the TCP/IP stack to transmit all bytes in the outgoing
	buffer as soon as it can.
*/
int tcp_flush(void* context) {
    TCP_SOCKET sock= *(TCP_SOCKET *) context;
	TCPIP_TCP_Flush(sock);
	return 0;
}

/*
	tcp_writeall attempts to write all the specified bytes by repeatedly
	writing as many bytes as possible to the outgoing buffer, flushing the
	socket, and writing more bytes to the buffer until all bytes are sent. While
	it is waiting for bytes to be transmitted, it calls tcp_socket_tick, and if this
	routine returns an error, the write routine must abort and itself return an
	error. If it does not return an error, it returns the number of bytes
	written, which must be the number specified.
*/
int tcp_writeall(void* context, const uint8_t *buf, uint16_t len) {
	int total = 0;
	int written = 0;
	while (total < len) {
		written = tcp_write(context, buf+total, len-total);
		total = total + written;
		if (total < len) {
			if (tcp_socket_tick(context) < 0) {return -1;}
		}
	}
	tcp_flush(context);
	return (int)total;
}

/*
	ping_gateway sends a ping echo request to the default gateway address. In
	order to be sure of generating a ping, we fake an ARP table entry for the
	gatewayt, becauyse if there is no such entry in the local network's ARP
	table, the Harmony TCP/IP stack will refuse to generate the ping. We use the
	ping to announce the presence of the EEM on the local network when it boots
	up.
*/
void ping_gateway(void) {
    TCPIP_NET_HANDLE netH;
    IPV4_ADDR gwAddr;
    TCPIP_ICMP_ECHO_REQUEST echoReq;
    TCPIP_ICMP_REQUEST_HANDLE reqHandle;
    ICMP_ECHO_RESULT res;

    netH = TCPIP_STACK_IndexToNet(0);
    gwAddr.Val = TCPIP_STACK_NetAddressGateway(netH);
    TCPIP_MAC_ADDR fakeMac = { .v = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 } };
	TCPIP_ARP_EntrySet(netH, &gwAddr, &fakeMac, true);
    console_print("Pinging %d.%d.%d.%d in %s...\r\n",
        gwAddr.v[0], gwAddr.v[1], gwAddr.v[2], gwAddr.v[3], __func__);
    memset(&echoReq, 0, sizeof(echoReq));
    echoReq.netH            = netH;
    echoReq.targetAddr      = gwAddr;
    echoReq.sequenceNumber  = 1;
    echoReq.identifier      = 0xBEEF;    // arbitrary
    echoReq.pData           = NULL;      // no payload
    echoReq.dataSize        = 0;
    echoReq.callback        = NULL;      // polling mode
    echoReq.param           = NULL;
    res = TCPIP_ICMP_EchoRequest(&echoReq, &reqHandle);
    if (res == ICMP_ECHO_OK) {
    	console_print("Ping succeeded in %s.\r\n", __func__);
	} else {
    	console_print("Ping failed with code %u in %s.\r\n", res, __func__);
    }
}

/*
	server_set_ip reconfigures the network interface with a new IP address, a new
	gateway address, and a new network mask. We pass it three strings that specify
	these three IP addresses, and the routine translates these into IPV4_ADDR types
	and applies them. It also makes sure that DHCP is turned off.
*/
int server_set_ip(const char* ip_str, const char* gw_str, const char* mask_str) {
    IPV4_ADDR ip_addr;
    IPV4_ADDR mask_addr;
    IPV4_ADDR gw_addr;
    TCPIP_NET_HANDLE net_hdl;

    if (!TCPIP_Helper_StringToIPAddress(ip_str, &ip_addr)) {
        console_print("Invalid IP address: %s\r\n", ip_str);
        return -1;
    }

    if (!TCPIP_Helper_StringToIPAddress(gw_str, &gw_addr)) {
        console_print("Invalid Gateway address: %s\r\n", gw_str);
        return -1;
    }

	if (!TCPIP_Helper_StringToIPAddress(mask_str, &mask_addr)) {
        console_print("Invalid netmask: %s\r\n", mask_str);
        return false;
    }
    
    net_hdl = TCPIP_STACK_IndexToNet(0);
    if (net_hdl == 0) {
        console_print("Could not get network interface handle.\r\n");
        return -1;
    }

	console_print("Disabling DHCP...\r\n");
	TCPIP_DHCP_Disable(net_hdl);
	
	console_print("Setting IP and Mask: %s and %s\r\n", ip_str, mask_str);
	if (!TCPIP_STACK_NetAddressSet(net_hdl, &ip_addr, &mask_addr, true)) {
		console_print("NetAddressSet failed.\r\n");
		return -1;
	}
	
	console_print("Setting Gatweay: %s\r\n", gw_str);
	if (!TCPIP_STACK_NetAddressGatewaySet(net_hdl, &gw_addr)) {
        console_print("NetAddressGatewaySet failed.\r\n");
        return -1;
    }
    
    return 0;
}

/*
	server_mac returns the network interface's media access control (MAC) address as
	a six-byte block.
*/
void server_mac(uint8_t* out) {
    TCPIP_NET_HANDLE net_hdl;
    TCPIP_NET_IF* pNetIf;
    uint8_t* mac;

    net_hdl = TCPIP_STACK_IndexToNet(0);
    pNetIf = _TCPIPStackHandleToNet(net_hdl);
    mac = pNetIf->netMACAddr.v;
	out[0] = mac[0];
	out[1] = mac[1];
	out[2] = mac[2];
	out[3] = mac[3];
	out[4] = mac[4];
	out[5] = mac[5];
}

/*
	server_info composes a string that presents the status and configuration of the
	network interface. It writes the string to a buffer specified by the calling
	process. The calling process also provides a maximum size for the stirng.
	The function returns the number of characters it wrote, and it also terminates
	the string with null character.
*/
void server_info(char* out, uint32_t max_len) {
    TCPIP_NET_HANDLE net_hdl;
    TCPIP_NET_IF* pNetIf;
    IPV4_ADDR ip, mask, gw;
    uint8_t* mac;
    uint32_t i = 0;
    uint32_t n = 0;
    const char* name;

    net_hdl = TCPIP_STACK_IndexToNet(0);
    pNetIf = _TCPIPStackHandleToNet(net_hdl);

    ip.Val  = pNetIf->netIPAddr.Val;
    mask.Val = pNetIf->netMask.Val;
    gw.Val   = pNetIf->netGateway.Val;
    mac = pNetIf->netMACAddr.v;
    name = TCPIP_STACK_NetNameGet(net_hdl);
	
	n = snprintf(&out[i], max_len, "\r\nNetwork Info:\r\n");
	i = i + n;
	n = snprintf(&out[i], max_len-i, "Interface : %s\r\n", name);
	i = i + n;
	n = snprintf(&out[i], max_len-i, "IP        : %u.%u.%u.%u\r\n",
        ip.v[0], ip.v[1], ip.v[2], ip.v[3]);
	i = i + n;
	n = snprintf(&out[i], max_len-i, "Mask      : %u.%u.%u.%u\r\n",
        mask.v[0], mask.v[1], mask.v[2], mask.v[3]);
	i = i + n;
    n = snprintf(&out[i], max_len-i, "Gateway   : %u.%u.%u.%u\r\n",
        gw.v[0], gw.v[1], gw.v[2], gw.v[3]);
	i = i + n;
    n = snprintf(&out[i], max_len-i, "MAC       : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	i = i + n;
    n = snprintf(&out[i], max_len-i, "Link      : %s\r\n",
        (TCPIP_STACK_NetIsLinked(net_hdl) ? "UP" : "DOWN"));
	i = i + n;
}

/*
	tcpip_server provides management of connection, service, and closure of a
	TCP/IP protocol.  The core actions of waiting for the TCP/IP stack to start
	up, waiting for an IP address to be assigned, listening on the port assigned
	to the protocol, accepting a socket connection on that port, maintaining
	service of the protocol on that socket by repeatedly calling a task
	management routine, and closing the socket when the management routine
	returns an error, are all handled by tcpip_server.

	The tcpip_server is a state machine that does not block its calling
	procedure, excepting when the protocol task provided to it causes a block.
	The task procedure is permitted to block, but it must call tcp_socket_tick while
	it is blocking, so as to maintain the TCP/IP stack, and to detect closure of
	the socket by the client. If the client closes the socket, the protocol task
	must abort and return a negative value. The tcpip_server will close the
	listening socket and open a new one. By this arrangement, the protocol task
	can block the servicing of other protocols by other tcpip_server processes
	indefinitely, but it can block its own server only so long as its client
	does not close its socket.

	In order to support a particular protocol, we pass into the routine a
	call-back function. This call-back function takes as a parameter the
	server's status record, which includes the server state and a handle to the
	socket. The server procedure calls the task procedure first when the socket
	is accepted, and then repeatedly every time the server procedure is called
	by the main event loop.

	The task procedure can tell whether it should initialize the protocol
	interaction or continue an existing interaction because it has access to the
	server state, which will be S_LISTENING for a newly-opened socket and
	S_SERVING for a pre-existing socket. The task procedure can read and write
	from the socket using the tcp_get and tcp_put routines. If it encounters an
	error, or detects that the socket has closed, it should return a negative
	value. When the server receives this negative value, it closes the socket
	and starts listening again for the next connection.

	In S_WAIT_STACK, the server checks the status of the TCP/IP stack. If the
	stack initialization failed, the server will move to its error state. The
	first server to check for success or failure reports its finding to the
	console.

	In the S_WAIT_IP state, the server waits until the network interface has its
	IP address. When the IP address is established, the first server to detect
	the establishment pings the gateway, so as to announce the presence of the
	ethernet module. This same server will report its ping to the console.

	In the S_OPEN_SERVER state, the server opens a listening socket. When it
	receives a connection, it calls its tasks routine, and moves to the next
	state after that.

	In S_SERVING, the server checks the connection is still active and calls the
	tasks routine. If it receives a negative return from the tasks routine, the
	server moves to its socket close state.

	In S_CLOSE, the server closes the socket, and in S_ERROR the server sits and
	makes an occasional heartbeat announcement about its state.

	In S_ERROR, the server will be stuck in this state until some external agent
	changes its state. In debug mode, the server will issue a heartbeat report
	of its error state to the console.

	In the default state, the server remains stuck, and issues a debug heartbeat
	report of its sitting in an unknown state.
*/
void tcpip_server(tcpip_server_type* server, tcpip_tasks_type tasks) {
	#define HEARTBEAT_PERIOD 50000000

	SYS_STATUS tcpip_status;
	IPV4_ADDR ip_addr;
	TCP_SOCKET_INFO sock_info;
	TCPIP_NET_HANDLE net_hdl;

	const char* interface_name, *host_name;
	int status;
	static uint32_t wait_stack_done = 0;
	static uint32_t wait_ip_done = 0;

	switch (server->state) {
		case S_WAIT_STACK: {
			tcpip_status = TCPIP_STACK_Status(sysObj.tcpip);
			if (tcpip_status < 0) {   
				if (!wait_stack_done) {
					console_print(
						"TCP/IP stack initialization failed in %s.\r\n",
						__func__);
				}
				server->state = S_ERROR;
			} else if (tcpip_status == SYS_STATUS_READY) {
				net_hdl = TCPIP_STACK_IndexToNet(0);
				interface_name = TCPIP_STACK_NetNameGet(net_hdl);
				host_name = TCPIP_STACK_NetBIOSName(net_hdl);
				if (!wait_stack_done) {
					console_print(
						"Interface %s on host %s awaiting initialization in %s.\r\n",
						interface_name,
						string_trim(host_name),
						__func__);
				}
				server->state = S_WAIT_IP;
			}
			wait_stack_done = 1;
		}
		break;

		case S_WAIT_IP: {
			net_hdl = TCPIP_STACK_IndexToNet(0);
			if (TCPIP_STACK_NetIsReady(net_hdl)) {
				ip_addr.Val = TCPIP_STACK_NetAddress(net_hdl);
				interface_name = TCPIP_STACK_NetNameGet(net_hdl);
				if (!wait_ip_done) {
					console_print(
						"Interface %s assigned IP address %d.%d.%d.%d in %s.\r\n", 
						interface_name,
						ip_addr.v[0], ip_addr.v[1], ip_addr.v[2], ip_addr.v[3],
						__func__);
					ping_gateway();
				}
				server->state = S_OPEN_SERVER;
				wait_ip_done = 1;
			}
		}
		break;

		case S_OPEN_SERVER: {
			server->socket = TCPIP_TCP_ServerOpen(IP_ADDRESS_TYPE_IPV4, server->port, 0);
			if (server->socket == INVALID_SOCKET) {
				if (debug) console_print(
					"Failed to open %s server on port %d in %s.\r\n",
					server->protocol, server->port, __func__);
				server->state = S_ERROR;
			} else {
				if (debug) console_print(
					"Listening for %s connection on port %d in %s.\r\n",
					server->protocol, server->port, __func__);
				server->state = S_LISTENING;
			}
		}
		break;

		case S_LISTENING: {
			if (TCPIP_TCP_IsConnected(server->socket)) {
				if (TCPIP_TCP_SocketInfoGet(server->socket, &sock_info)) {
					IPV4_ADDR ip = sock_info.remoteIPaddress.v4Add;
					if (debug) console_print(
						"%s connection from %u.%u.%u.%u in %s.\r\n",
						server->protocol, ip.v[0], ip.v[1], ip.v[2], ip.v[3], __func__);
				} else {
					if (debug) console_print(
						"%s connection from unknown peer in %s.\r\n",
						server->protocol, __func__);
				}
				status = tasks(server);
				if (status >= 0) {
					server->state = S_SERVING;
				} else {
					if (debug) console_print(
						"%s socket closed pre-emptively by server in %s.\r\n",
						server->protocol, __func__);			
					server->state = S_CLOSE;
				}
			}
		}
		break;

		case S_SERVING: {
			if (!TCPIP_TCP_IsConnected(server->socket) ||
					TCPIP_TCP_WasDisconnected(server->socket)) {
				if (debug) console_print("%s socket closed by client in %s.\r\n",
					server->protocol, __func__);
				server->state = S_CLOSE;
			} else {
				status = tasks(server);
				if (status < 0) {
					if (debug) console_print("%s socket closed by server in %s.\r\n",
						server->protocol, __func__);			
					server->state = S_CLOSE;
				}
			}
		}
		break;
		
		case S_CLOSE: {
			TCPIP_TCP_Close(server->socket);
			server->socket = INVALID_SOCKET;
			server->state = S_OPEN_SERVER;
		}
		break;
		
		case S_ERROR: {
		}
		break;
		
		default: {
			if (rand() % HEARTBEAT_PERIOD == 0) {
				if (debug) console_print("Unknown state %u for % server in %s.\r\n",
					server->protocol, server->state, __func__);
			}		
		}
		break;
	}
}




