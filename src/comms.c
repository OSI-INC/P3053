/*
	comms.c is a library of communication routines, including network and
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
#include "comms.h"
#include "console.h"
#include "utils.h"

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
    console_print("Pinging %d.%d.%d.%d...",
        gwAddr.v[0],gwAddr.v[1],gwAddr.v[2],gwAddr.v[3]);
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
    	console_print(" succeeded in %s.\r\n",__func__);
	} else {
    	console_print(" failed with code %u in %s.\r\n",res,__func__);
    }
}

/*
	net_info prints network information to the console.
*/
void net_info(void) {
    TCPIP_NET_HANDLE netH;
    TCPIP_NET_IF* pNetIf;
    IPV4_ADDR ip, mask, gw;
    uint8_t* mac;

    netH = TCPIP_STACK_IndexToNet(0);
    pNetIf = _TCPIPStackHandleToNet(netH);

    ip.Val  = pNetIf->netIPAddr.Val;
    mask.Val = pNetIf->netMask.Val;
    gw.Val   = pNetIf->netGateway.Val;
    mac = pNetIf->netMACAddr.v;

    console_message("\r\nNetwork Info:\r\n");
    console_print("IP      : %u.%u.%u.%u\r\n",
        ip.v[0],ip.v[1],ip.v[2],ip.v[3]);
    console_print("Mask    : %u.%u.%u.%u\r\n",
        mask.v[0],mask.v[1],mask.v[2],mask.v[3]);
    console_print("Gateway : %u.%u.%u.%u\r\n",
        gw.v[0],gw.v[1],gw.v[2],gw.v[3]);
    console_print("MAC     : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    console_print("Link    : %s\r\n",
        (TCPIP_STACK_NetIsLinked(netH) ? "UP" : "DOWN"));
    console_message("\r\n");
}

/*
	tcpip_server handles TCP/IP connections.
*/
void tcpip_server(SERVER* s, tcpip_tasks_type tasks) {
	const uint32_t heartbeat_period = 5000000;
	
	SYS_STATUS tcpip_status;
	IPV4_ADDR ip_addr;
	TCP_SOCKET_INFO sock_info;
	TCPIP_NET_HANDLE net_hdl;

	const char* interface_name, *host_name;
	int status;

	switch ((*s).state) {
		case S_WAIT_STACK: {
			tcpip_status = TCPIP_STACK_Status(sysObj.tcpip);
			if (tcpip_status < 0) {   
				console_print("TCP/IP stack initialization failed in %s.\r\n",__func__);
				(*s).state = S_ERROR;
			} else if (tcpip_status == SYS_STATUS_READY) {
				net_hdl = TCPIP_STACK_IndexToNet(0);
				interface_name = TCPIP_STACK_NetNameGet(net_hdl);
				host_name = TCPIP_STACK_NetBIOSName(net_hdl);
				console_print(
					"Interface %s on host %s awaiting initialization in %s.\r\n",
					interface_name,
					string_trim(host_name),
					__func__);
				(*s).state = S_WAIT_IP;
			}
		}
		break;



		case S_WAIT_IP: {
			net_hdl = TCPIP_STACK_IndexToNet(0);
			if (TCPIP_STACK_NetIsReady(net_hdl)) {
				ip_addr.Val = TCPIP_STACK_NetAddress(net_hdl);
				interface_name = TCPIP_STACK_NetNameGet(net_hdl);
				console_print(
					"Interface %s assigned IP address %d.%d.%d.%d in %s.\r\n", 
					interface_name,
					ip_addr.v[0],ip_addr.v[1],ip_addr.v[2],ip_addr.v[3],
					__func__);
				ping_gateway();
				(*s).state = S_OPEN_SERVER;
			}
		}
		break;

		case S_OPEN_SERVER: {
			(*s).socket = TCPIP_TCP_ServerOpen(IP_ADDRESS_TYPE_IPV4,(*s).port,0);
			if ((*s).socket == INVALID_SOCKET) {
				console_print("Could not open %s server on port %d in %s.\r\n",
					(*s).protocol,(*s).port,__func__);
				(*s).state = S_ERROR;
			} else {
				console_print("Listening for %s connection on port %d in %s.\r\n",
					(*s).protocol,(*s).port,__func__);
				(*s).state = S_LISTENING;
			}
		}
		break;

		case S_LISTENING: {
			if (TCPIP_TCP_IsConnected((*s).socket)) {
				if (TCPIP_TCP_SocketInfoGet((*s).socket,&sock_info)) {
					IPV4_ADDR ip = sock_info.remoteIPaddress.v4Add;
					console_print("%s connection from %u.%u.%u.%u in %s.\r\n",
						(*s).protocol,ip.v[0],ip.v[1],ip.v[2],ip.v[3],__func__);
				} else {
					console_print("%s connection from unknown peer in %s.\r\n",
						(*s).protocol,__func__);
				}
				(*s).state = S_SERVING;
			}
		}
		break;

		case S_SERVING: {
			if (!TCPIP_TCP_IsConnected((*s).socket) ||
					TCPIP_TCP_WasDisconnected((*s).socket)) {
				console_print("%s socket closed by client in %s.\r\n",
					(*s).protocol,__func__);
				(*s).state = S_CLOSE;
			} else {
				status = tasks(s);
				if (status < 0) {
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
			if (rand() % heartbeat_period == 0) {
				console_print("Failed to start TCP/IP server in %s.\r\n",
					__func__);
			}		
		}
		break;
		
		default:
		break;
	}
}




