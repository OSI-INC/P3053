/*
	server.c is a library of communication routines, including network and
	console interfaces.
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
	tcp_get_ready returns the number of bytes that are ready to be read out of a
	TCP socket. It is the first of four routines that allow use to manage
	reading and writing from a socket. These routines are wrappers for Harmony
	TCP routines. The wrappers stop us from dealing with sixteen-bit unsigned
	integers in our own code, which are a waste of time on a thirty-two bit
	machine. And they allow us to adapt our TCP/IP socket reading and writing
	without changing our protocol handling routines.	
*/ 
uint32_t tcp_get_ready(TCP_SOCKET s) {
	return TCPIP_TCP_GetIsReady(s);
}

/*
	tcp_get attempts to read a specified number of bytes from a TCP socket. It
	returns the number of bytes it actually read. It takes as arguments a socket
	handle, a byte buffer pointer, and an unsigned integer length.
*/
uint32_t tcp_get(TCP_SOCKET s, uint8_t* buffer, uint32_t len) {
	return TCPIP_TCP_ArrayGet(s,buffer,len);
}

/*
	tcp_put_ready returns how much space we have available for writing to the
	outgoing TCP buffer of a socket. It takes a socket handle.
*/
uint32_t tcp_put_ready(TCP_SOCKET s) {
	return TCPIP_TCP_PutIsReady(s);
}

/*
	tcp_put attempts to write a specified number of bytes to a TCP socket. It
	returns the number of bytes it actually wrote. It takes as argument a socket
	handle, a pointer to the byte buffer containing the data that should be
	written, and an unsigned integer length.
*/
uint32_t tcp_put(TCP_SOCKET s, const uint8_t* data, uint32_t len) {
	return TCPIP_TCP_ArrayPut(s,data,len);
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
        gwAddr.v[0],gwAddr.v[1],gwAddr.v[2],gwAddr.v[3],__func__);
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
    	console_print("Ping succeeded in %s.\r\n",__func__);
	} else {
    	console_print("Ping failed with code %u in %s.\r\n",res,__func__);
    }
}

/*
	net_info composes a string that presents the status and configuration of the
	network interface. It writes the string to a buffer specified by the calling
	process. The calling process also provides a maximum size for the stirng.
	The function returns the number of characters it wrote, and it also terminates
	the string with null character.
*/
int net_info(char* out, uint32_t max_len) {
    TCPIP_NET_HANDLE netH;
    TCPIP_NET_IF* pNetIf;
    IPV4_ADDR ip, mask, gw;
    uint8_t* mac;
    uint32_t i = 0;
    uint32_t n = 0;

    netH = TCPIP_STACK_IndexToNet(0);
    pNetIf = _TCPIPStackHandleToNet(netH);

    ip.Val  = pNetIf->netIPAddr.Val;
    mask.Val = pNetIf->netMask.Val;
    gw.Val   = pNetIf->netGateway.Val;
    mac = pNetIf->netMACAddr.v;
	
	n = snprintf(&out[i],max_len,"\r\nNetwork Info:\r\n");
	i = i + n;
	n = snprintf(&out[i],max_len-i,"IP      : %u.%u.%u.%u\r\n",
        ip.v[0],ip.v[1],ip.v[2],ip.v[3]);
	i = i + n;
	n = snprintf(&out[i],max_len-i,"Mask    : %u.%u.%u.%u\r\n",
        mask.v[0],mask.v[1],mask.v[2],mask.v[3]);
	i = i + n;
    n = snprintf(&out[i],max_len-i,"Gateway : %u.%u.%u.%u\r\n",
        gw.v[0],gw.v[1],gw.v[2],gw.v[3]);
	i = i + n;
    n = snprintf(&out[i],max_len-i,"MAC     : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	i = i + n;
    n = snprintf(&out[i],max_len-i,"Link    : %s\r\n",
        (TCPIP_STACK_NetIsLinked(netH) ? "UP" : "DOWN"));
	i = i + n;
    n = snprintf(&out[i],max_len-i,"\r\n");
	i = i + n;
	
	return i;
}

/*
	tcpip_tick maintains the TCP/IP stack running.
*/
void tcpip_tick(void) {
	DRV_MIIM_OBJECT_BASE_Default.DRV_MIIM_Tasks(sysObj.drvMiim_0);
	TCPIP_STACK_Task(sysObj.tcpip);
}

/*
	server_tick maintains the TCP/IP stack running, and also checks to see if a
	particular server socket is still open. We use this in server task loops
	that block all other socket connection tasks. The client can break out of
	the loop by closing the socket. The routine takes as its argument a server
	state structure and returns a negative value only when the socket has
	been closed.
*/
int server_tick(SERVER* s) {
	DRV_MIIM_OBJECT_BASE_Default.DRV_MIIM_Tasks(sysObj.drvMiim_0);
	TCPIP_STACK_Task(sysObj.tcpip);
	if (!TCPIP_TCP_IsConnected((*s).socket) ||
			TCPIP_TCP_WasDisconnected((*s).socket)) {
		return -1;
	} else {
		return 0;
	}
}


/*
	tcpip_server provides management of connection, service, and closure of a
	TCP/IP protocol. The core actions of waiting for the TCP/IP stack to start
	up, waiting for an IP address to be assigned, listening on the port assigned
	to the protocol, accepting a socket connection on that port, maintaining
	service of the protocol on that socket by repeatedly calling a task
	management routine, and closing the socket when the management routine
	returns an error, are all handled by tcpip_server. 
	
	In order to support a particular protocol, we pass into the routine a
	call-back function. This call-back function takes as a parameter the
	server's status record, which includes the server state and a handle to the
	socket. The server procedure calls the task procedure first when the socket
	is accepted, and then repeatedly every time the server procedure is called
	by the main event loop. The task procedure can tell whether it should
	initialize the protocol interaction or continue an existing interaction
	because it has access to the server state, which will be S_LISTENING for a
	newly-opened socket and S_SERVING for a pre-existing socket. The task
	procedure can read and write from the socket using the tcp_get and tcp_put
	routines. If it encounters an error, or detects that the socket has closed,
	it should return a negative value. When the server receives this negative
	value, it closes the socket and starts listening again for the next
	connection. 
	
	The server starts in the S_WAIT_STACK state, where it checks the status of
	the stack. If the stack initialization failed, the server will move to its
	error state. The server announces success or failure provided that it is the
	first server to perform the stack check. Otherwise it proceeds quietly. 
	
	In the S_WAIT_IP state, the server waits until the network interface has its
	IP address. When the IP address is established, the first server to detect the
	address will attempt to ping the gateway, so as to announce the presence of the
	ethernet module.
	
	In the S_OPEN_SERVER state, the server opens a listening socket. When it receives
	a connection, it calls its tasks routine, and moves to the next state after that.
	
	In S_SERVING, the server checks the connection is still active and calls the
	tasks routine. If it receives a negative return from the tasks routine, the
	server moves to its socket close state.
	
	In S_Close, the server closes the socket, and in S_ERROR the server sits and 
	makes an occasional heartbeat announcement about its state.
	
	At least one server will announce the initialization of the stack and the
	assigning of an IP address. If the debug flag is cleared, all other
	announcements will be suppressed.
*/
void tcpip_server(SERVER* s, tcpip_tasks_type tasks) {
	#define HEARTBEAT_PERIOD 5000000

	SYS_STATUS tcpip_status;
	IPV4_ADDR ip_addr;
	TCP_SOCKET_INFO sock_info;
	TCPIP_NET_HANDLE net_hdl;

	const char* interface_name, *host_name;
	int status;
	static uint32_t wait_stack_done = 0;
	static uint32_t wait_ip_done = 0;

	switch ((*s).state) {
		case S_WAIT_STACK: {
			tcpip_status = TCPIP_STACK_Status(sysObj.tcpip);
			if (tcpip_status < 0) {   
				if (!wait_stack_done) {
					console_print(
						"TCP/IP stack initialization failed in %s.\r\n",
						__func__);
				}
				(*s).state = S_ERROR;
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
				(*s).state = S_WAIT_IP;
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
						ip_addr.v[0],ip_addr.v[1],ip_addr.v[2],ip_addr.v[3],
						__func__);
					ping_gateway();
				}
				(*s).state = S_OPEN_SERVER;
				wait_ip_done = 1;
			}
		}
		break;

		case S_OPEN_SERVER: {
			(*s).socket = TCPIP_TCP_ServerOpen(IP_ADDRESS_TYPE_IPV4,(*s).port,0);
			if ((*s).socket == INVALID_SOCKET) {
				if (debug) console_print(
					"Could not open %s server on port %d in %s.\r\n",
					(*s).protocol,(*s).port,__func__);
				(*s).state = S_ERROR;
			} else {
				if (debug) console_print(
					"Listening for %s connection on port %d in %s.\r\n",
					(*s).protocol,(*s).port,__func__);
				(*s).state = S_LISTENING;
			}
		}
		break;

		case S_LISTENING: {
			if (TCPIP_TCP_IsConnected((*s).socket)) {
				if (TCPIP_TCP_SocketInfoGet((*s).socket,&sock_info)) {
					IPV4_ADDR ip = sock_info.remoteIPaddress.v4Add;
					if (debug) console_print(
						"%s connection from %u.%u.%u.%u in %s.\r\n",
						(*s).protocol,ip.v[0],ip.v[1],ip.v[2],ip.v[3],__func__);
				} else {
					if (debug) console_print(
						"%s connection from unknown peer in %s.\r\n",
						(*s).protocol,__func__);
				}
				status = tasks(s);
				if (status >= 0) {
					(*s).state = S_SERVING;
				} else {
					if (debug) console_print(
						"%s socket closed pre-emptively by server in %s.\r\n",
						(*s).protocol,__func__);			
					(*s).state = S_CLOSE;
				}
			}
		}
		break;

		case S_SERVING: {
			if (!TCPIP_TCP_IsConnected((*s).socket) ||
					TCPIP_TCP_WasDisconnected((*s).socket)) {
				if (debug) console_print("%s socket closed by client in %s.\r\n",
					(*s).protocol,__func__);
				(*s).state = S_CLOSE;
			} else {
				status = tasks(s);
				if (status < 0) {
					if (debug) console_print("%s socket closed by server in %s.\r\n",
						(*s).protocol,__func__);			
					(*s).state = S_CLOSE;
				}
			}
		}
		break;
		
		case S_CLOSE: {
			TCPIP_TCP_Close((*s).socket);
			(*s).socket = INVALID_SOCKET;
			(*s).state = S_OPEN_SERVER;
		}
		break;
		
		case S_ERROR: {
			if (rand() % HEARTBEAT_PERIOD == 0) {
				if (debug) console_print("Failed to start %s server in %s.\r\n",
					(*s).protocol,__func__);
			}		
		}
		break;
		
		default: {
			if (rand() % HEARTBEAT_PERIOD == 0) {
				if (debug) console_print("Unknown state %u for % server in %s.\r\n",
					(*s).protocol,(*s).state,__func__);
			}		
		}
		break;
	}
}




