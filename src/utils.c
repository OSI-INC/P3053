/*
	utils.c is a library of utility routines that communicate with the PIC32MZ
	registers, read and write throught he MPCIE parallel port, and perform
	various mundane tasks with strings and buffers.
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
#include "console.h"
#include "tcpip/tcpip.h"
#include "tcpip/src/tcpip_private.h"
#include "tcpip/icmp.h"

/*
	string_trim takes a pointer to a null-terminated string and returns a pointer
	to another null-terminated string that is the same as the original but with all
	whitespace removed from the beginning and the end.
*/
const char* string_trim(const char *s) {
    static char buf[256];
    const char *start = s;
    const char *end;
    size_t len;

    if (s == NULL) return "";
    while (*start && isspace((unsigned char)*start)) start++;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    len = end - start;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, start, len);
    buf[len] = '\0';
    return buf;
}

/*
	eem_reset resets the Embedded Etherent Module. It does so by unlocking the
	PIC32MZ configuration registers and writing to the reset configuration bit.
	We make three writes to the thirty-two bit SYSKEY register with the correct
	combination to unlock the configuration bits. Then we set the RSWRSTSET
	register equal to a value defined in the device pack: "_RSWRST_SWRST_MASK".
	We read the reset register after that, which completes the initiation of
	reset. The routine does not return, it puts itself in an infinite loop,
	trusting that the reset will take place and reboot the system.
*/
void eem_reset(void) {
    SYSKEY = 0x00000000;
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;
    RSWRSTSET = _RSWRST_SWRST_MASK;
    (void) RSWRST;
    while(1);
}

void force_gateway_arp(void)
{
    TCPIP_NET_HANDLE netH = TCPIP_STACK_IndexToNet(0);
    IPV4_ADDR gwAddr;

    netH = TCPIP_STACK_IndexToNet(0);
    gwAddr.Val = TCPIP_STACK_NetAddressGateway(netH);
	console_print("Manufacturing ARP for gateway %d.%d.%d.%d... ",
		gwAddr.v[0], gwAddr.v[1], gwAddr.v[2], gwAddr.v[3]);
    TCPIP_MAC_ADDR fakeMac = { .v = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 } };
	TCPIP_ARP_RESULT res = TCPIP_ARP_EntrySet(netH, &gwAddr, &fakeMac, true);
    if (res >= 0) {
    	console_message("Succeeded.\r\n");
    } else {
    	console_print(" Failed with error code %u\r\n", res);
    }
}

void ping_gateway(void)
{
    TCPIP_NET_HANDLE netH;
    IPV4_ADDR gwAddr;
    TCPIP_ICMP_ECHO_REQUEST echoReq;
    TCPIP_ICMP_REQUEST_HANDLE reqHandle;
    ICMP_ECHO_RESULT res;

    netH = TCPIP_STACK_IndexToNet(0);
    gwAddr.Val = TCPIP_STACK_NetAddressGateway(netH);
    console_print("Pinging gateway at %d.%d.%d.%d...",
        gwAddr.v[0], gwAddr.v[1], gwAddr.v[2], gwAddr.v[3]);
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
    	console_message(" Succeeded.\r\n");
	} else {
    	console_print(" Failed with error code %u\r\n", res);
    }
}





