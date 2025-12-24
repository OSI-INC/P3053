/*
	server.c -- Implementation of the TCP/IP Server and Communication library.
	
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

/*
	The standard C headers we trust the compiler will know how to find. The
	Microchip library headers are configuration.h and definitions.h. The
	compiler must be told where to look for these two files. The remaining
	interfaces are those that go with our EEM implementation files.
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
#include "utils.h"
#include "config.h"
#include "pic.h"
#include "server.h"
#include "console.h"

/*
	The active EEM configuration, a global variable. Intended to reflect the
	current EEM configuration.
*/
eem_config_type eem_config_active;

/*
	eem_config_write writes a EEM configuration record to the flash
	configuration address in flash memory.
*/
int eem_config_write(const eem_config_type* config_ptr) {
	return pic_flash_putbytes(
		EEM_CONFIG_ADDR, 
		(const uint8_t *) config_ptr,
		sizeof(eem_config_type));
}

/*
	eem_config_read reads a EEM configuration record from the flash
	configuration address in flash memory. After reading the record, the routine
	checks that its magic string matches that of a composed string, so as to
	check for a random set of bytes read from uninitialized flash memory. If the
	magic string does not match, we create the factory configuration record and
	use that instead.
*/
int eem_config_read(eem_config_type* config_ptr) {
	int len;
	len = pic_flash_getbytes(
		(uint8_t*) config_ptr,
		EEM_CONFIG_ADDR, 
		sizeof(eem_config_type));
	config_ptr->magic_str[sizeof(config_ptr->magic_str)-1] = '\0';
	if (strcmp(config_ptr->magic_str, CONFIG_FLASH_MAGIC) != 0) {
		*config_ptr = eem_config_factory;
		return 0;
	} else {
		return len;
	}
}

/*
	server_str_from_config takes a EEM configuration record and generates a
	colon-delimited string of parameter names and values, each pair on a
	separate line. The newline is marked by one line-feed character, LF, not by
	a pair of characters CRLF. The routine returns the length of the generated
	string. It does not check to make sure that the destination string is large
	enough.
*/
int server_str_from_config(char* str, const eem_config_type* config_ptr) {
	int len = 0;
	len += sprintf(str+len, "security_level: %u\n", config_ptr->security_level);
	len += sprintf(str+len, "password_str: %s\n", config_ptr->password_str);
	len += sprintf(str+len, "operator_str: %s\n", config_ptr->operator_str);
	len += sprintf(str+len, "time_str: %s\n", config_ptr->time_str);
	len += sprintf(str+len, "ip_str: %s\n", config_ptr->ip_str);
	len += sprintf(str+len, "gw_str: %s\n", config_ptr->gw_str);
	len += sprintf(str+len, "nm_str: %s\n", config_ptr->nm_str);
	len += sprintf(str+len, "device_str: %s\n", config_ptr->device_str);
	len += sprintf(str+len, "lwdaq_port: %u\n", config_ptr->lwdaq_port);
	len += sprintf(str+len, "telnet_port: %u\n", config_ptr->telnet_port);
	len += sprintf(str+len, "lwdaq_timeout: %u\n", config_ptr->lwdaq_timeout);
	len += sprintf(str+len, "telnet_timeout: %u\n", config_ptr->telnet_timeout);
	len += sprintf(str+len, "tcp_tick_ms: %u", config_ptr->tcp_tick_ms);
	return len;
}

/*
	eem_config_from_str looks through a string for pairs "parameter: value"
	on separate lines of a null-terminated string. The first line of the string
	will begin with the first character of the string. The last line of the
	string will end with the last character of the string. All other lines will
	be delimited by line-feed characters. If it finds a parameter with a name
	that matches one of our server parameters, it updates the corresponding
	value in the EEM configuration pointed to by the config_ptr. The routine
	checks only that the beginning of the parameter name matches a server
	parameter name. Thus a given name "ip_str_extra" will match our parameter
	name "ip_str". The routine returns the number of parameters if found, or a
	negative error code. The codes are as follows. For a null pointer in either
	the config_ptr or str, -1. For a line missing a colon, -2. For a line in
	which the colon is not followed by a space, -3.
*/
int eem_config_from_str(eem_config_type *config_ptr, const char *str) {
	const char *p = str;
	int num_copied = 0;
	
	if (!config_ptr || !str) return -1;
	
	while (*p) {
		const char *line_start = p;
		const char *line_end = strchr(p, '\n');
		if (line_end == NULL) {line_end = p + strlen(p);}
	
		const char *colon = memchr(line_start, ':', line_end - line_start);
		if (!colon) return -2;
		if (colon + 1 >= line_end || colon[1] != ' ') return -3;
	
		const char *value = colon + 2;
		size_t name_len = colon - line_start;
		size_t value_len = line_end - value;
	
		if (strncmp(line_start, "security_level", name_len) == 0) {
			config_ptr->security_level = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		} else if (strncmp(line_start, "password_str", name_len) == 0) {
			snprintf(config_ptr->password_str, sizeof(config_ptr->password_str),
				"%.*s", (int)value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "operator_str", name_len) == 0) {
			snprintf(config_ptr->operator_str, sizeof(config_ptr->operator_str),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "time_str", name_len) == 0) {
			snprintf(config_ptr->time_str, sizeof(config_ptr->time_str),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "ip_str", name_len) == 0) {
			snprintf(config_ptr->ip_str, sizeof(config_ptr->ip_str),
				"%.*s", (int)value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "gw_str", name_len) == 0) {
			snprintf(config_ptr->gw_str, sizeof(config_ptr->gw_str),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "nm_str", name_len) == 0) {
			snprintf(config_ptr->nm_str, sizeof(config_ptr->nm_str),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "device_str", name_len) == 0) {
			snprintf(config_ptr->password_str, sizeof(config_ptr->device_str),
				"%.*s", (int)value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "lwdaq_port", name_len) == 0) {
			config_ptr->lwdaq_port = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		} else if (strncmp(line_start, "telnet_port", name_len) == 0) {
			config_ptr->telnet_port = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		} else if (strncmp(line_start, "lwdaq_timeout", name_len) == 0) {
			config_ptr->lwdaq_timeout = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		} else if (strncmp(line_start, "telnet_timeout", name_len) == 0) {
			config_ptr->telnet_timeout = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		} else if (strncmp(line_start, "tcp_tick_ms", name_len) == 0) {
			config_ptr->tcp_tick_ms = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		}
		
		if (*line_end == '\n') {
			p = line_end + 1;
		} else {
			p = line_end;
		}
	}
	return num_copied;
}

/*
	tcp_tick maintains the TCP/IP stack and the Ethernet physical interface
	driver. The TCP/IP stack maintenance TPIP_STACK_Task routine and the MIIM
	driver routines must be called often enough to sustain communication, but
	not so often that repeated calls cause the network to freeze. In one
	iteration of our code, we found that calling the stack maintenance routine
	repeatedly in a polling loop for forty milliseconds froze the entire EEM
	roughly half the time. When we introduced the throttling of the calls with a
	tick-count timer, the freeze never occurred in that same loop nor any other.
	The tcp_tick routine keeps a timer and calls the maintenance routines at
	regular intervals. The period with which it calls the routines is given by
	the tcp_tick_ms field in the active EEM configuration. After calling the
	maintenance routines, tcp_tick returns a 1. If is is waiting, it returns a
	zero. 
*/
int tcp_tick(void) {
	static uint32_t last_tick = 0;
	uint32_t tick_count = SYS_TMR_TickCountGet();
	
	if (tick_count - last_tick >= eem_config_active.tcp_tick_ms) {		
		DRV_MIIM_OBJECT_BASE_Default.DRV_MIIM_Tasks(sysObj.drvMiim_0);
		TCPIP_STACK_Task(sysObj.tcpip);
		last_tick = tick_count;
		return 1;
	} else {
		return 0;
	}
}

/*
	tcp_sock_tick maintains the TCP/IP stack and checks to see if a particular
	socket is still open. It calls tcp_tick and checks its socket only if
	tcp_tick has called the network maintenance routines, which it will do every
	tcp_tick_ms. Any protocol task that enters a loop waiting for some other
	process to complete (a blocking process) must call this procedure. When this
	routine returns a negative value, the protocol task must stop blocking and
	return control to the tcpip_server routine that manages the protocol
	connections. The protocol task must pass back a negative value so that the
	tcpip_server can close the listening socket and open a new one. The routine
	takes as its argument a TCP/IP socket handle and returns a negative value
	when the socket has been closed or has been determined by the stack to be
	disconnected. It returns zero if it did not check anything.
*/
int tcp_sock_tick(void* context) {
	TCP_SOCKET sock = *(TCP_SOCKET *) context;
	if (tcp_tick()) {
		if (!TCPIP_TCP_IsConnected(sock) || TCPIP_TCP_WasDisconnected(sock)) {
			return -1;
		} else {
			return 1;
		}
	} else {
		return 0;
	}
}

/*
	tcp_readcount returns the number of bytes that are ready to be read out of a
	TCP socket. It is a wrapper for a Harmony routine. It converts sixteen-bit
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
	tcp_getchar attempts to read a single byte from a TCP socket. It returns a
	-2 if the socket is disconnected. It returns -1 if the socket is still
	connected but there is no character to be read. Otherwise it returns a
	character. This routine can be deployed in a CLI channel descriptor.
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
	socket is disconnected, it returns -2. If the buffer was full, it keeps
	waiting. It never returns the buffer full error code "-1". When it writes
	the character, it returns a 1. This routine can be deployed in a CLI channel
	descriptor.
*/
int tcp_putchar(void* context, char c) {
	TCP_SOCKET sock= *(TCP_SOCKET *) context;
	if (sock == INVALID_SOCKET) return -2;
	while (TCPIP_TCP_PutIsReady(sock) == 0) {
		if (tcp_sock_tick(context) < 0) return -2;
	}
	if (TCPIP_TCP_ArrayPut(sock, (uint8_t *)&c, 1) == 1) return 1;
	return 0;
}

/*
	tcp_flush forces the TCP/IP stack to transmit all bytes in the outgoing
	buffer as soon as it can. This routine can be deployed in a CLI channel
	descriptor.
*/
int tcp_flush(void* context) {
	TCP_SOCKET sock= *(TCP_SOCKET *) context;
	TCPIP_TCP_Flush(sock);
	return 0;
}

/*
	tcp_writeall attempts to write all requested bytes by repeatedly writing as
	many bytes as possible to the outgoing buffer, flushing the socket, and
	writing more bytes to the buffer until all bytes are sent. While it is
	waiting for bytes to be transmitted, it calls tcp_sock_tick, and if this
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
			if (tcp_sock_tick(context) < 0) {return -1;}
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
	up. We provide no routine currently to determine if the ping succeeded. The
	ping identifier we pass is arbitrary.
*/
void ping_gateway(void) {
	TCPIP_NET_HANDLE net_hdl;
	IPV4_ADDR gwAddr;
	TCPIP_ICMP_ECHO_REQUEST echoReq;
	TCPIP_ICMP_REQUEST_HANDLE reqHandle;
	ICMP_ECHO_RESULT res;

	net_hdl = TCPIP_STACK_IndexToNet(0);
	gwAddr.Val = TCPIP_STACK_NetAddressGateway(net_hdl);
	TCPIP_MAC_ADDR fakeMac = { .v = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 } };
	TCPIP_ARP_EntrySet(net_hdl, &gwAddr, &fakeMac, true);
	console_print("Pinging %d.%d.%d.%d...",
		gwAddr.v[0], gwAddr.v[1], gwAddr.v[2], gwAddr.v[3], __func__);
	memset(&echoReq, 0, sizeof(echoReq));
	echoReq.netH = net_hdl;
	echoReq.targetAddr = gwAddr;
	echoReq.sequenceNumber = 1;
	echoReq.identifier = 0xBEEF;
	echoReq.pData = NULL;
	echoReq.dataSize = 0;
	echoReq.callback = NULL;
	echoReq.param = NULL;
	res = TCPIP_ICMP_EchoRequest(&echoReq, &reqHandle);
	if (res == ICMP_ECHO_OK) {
		console_print(" Succeeded in %s.\n", __func__);
	} else {
		console_print(" Failed with code %u in %s.\n", res, __func__);
	}
}

/*
	server_set_ip configures the network interface with a new IP address, a
	new gateway address, and a new network mask. We pass it three strings that
	specify these three IP addresses. The routine checks all three strings to
	make sure they are formatted correctly. It makes sure DHCP is turned off, so
	as to ensure that the TCP/IP stack will accept a static IP address. It
	applies all three addresses. It does not need to, nor does it attempt to,
	re-start the stack. Once it is done, the routine records the new IP values
	in the active EEM configuration. Any servers running on the EEM within the
	server_task procedure will detect a change in the active EEM configuration
	IP address and restart automatically. Note that the EEM configuration policy
	does not allow us to call this routine at any time other than when the EEM
	is starting up.
*/
int server_set_ip(const char* ip_str, const char* gw_str, const char* nm_str) {
	IPV4_ADDR ip_addr;
	IPV4_ADDR mask_addr;
	IPV4_ADDR gw_addr;
	TCPIP_NET_HANDLE net_hdl;

	if (!TCPIP_Helper_StringToIPAddress(ip_str, &ip_addr)) {
		console_print("ERROR: Invalid IP address '%s' in %s.\n", ip_str, __func__);
		return -1;
	}
	if (!TCPIP_Helper_StringToIPAddress(gw_str, &gw_addr)) {
		console_print("ERROR: Invalid gateway '%s' in %s.\n", gw_str, __func__);
		return -1;
	}
	if (!TCPIP_Helper_StringToIPAddress(nm_str, &mask_addr)) {
		console_print("ERROR: Invalid mask '%s' in %s.\n", nm_str, __func__);
		return -1;
	}
	
	net_hdl = TCPIP_STACK_IndexToNet(0);
	if (net_hdl == 0) {
		console_print("ERROR: Failed to obtain interface handle in %s.\n", __func__);
		return -1;
	}
	TCPIP_DHCP_Disable(net_hdl);
	
	if (!TCPIP_STACK_NetAddressSet(net_hdl, &ip_addr, &mask_addr, true)) {
		console_print("ERROR: Failed to set IP address in %s.\n", __func__);
		return -1;
	}
	if (!TCPIP_STACK_NetAddressGatewaySet(net_hdl, &gw_addr)) {
		console_print("ERROR: Failed to set gateway in %s.\n", __func__);
		return -1;
	}
	
	strcpy(eem_config_active.ip_str, ip_str);
	strcpy(eem_config_active.gw_str, gw_str);
	strcpy(eem_config_active.nm_str, nm_str);
	
	return 0;
}

/*
	server_mac returns the network interface's media access control (MAC)
	address as a six-byte block. The routine does not check the size of the
	destination byte buffer, but it does return the number of bytes it wrote.
*/
int server_mac(uint8_t* out) {
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
	
	return 6;
}

/*
	server_mac_str writes the network interface's media access control (MAC)
	address to a string as six, two-digit hexadecimal characters separeted by
	colons, making 18 charcters in all. The routine does not check the size of
	the destination string buffer, but it does return the number of characters
	it wrote.
*/
int server_mac_str(char* out) {
	uint8_t m[6];
	server_mac(m);
	return sprintf(out, 
		"%02X:%02X:%02X:%02X:%02X:%02X", 
		m[0], m[1], m[2], m[3], m[4], m[5]);
}

/*
	server_ip_str provides the IP address of the network interface, as read from
	the TCP/IP stack. It writes the IP address to a buffer as four printable
	decimal values separated by period. The routine does not check the size of
	the destination string buffer, but it does return the number of characters
	it wrote.
*/
int server_ip_str(char* out) {
	TCPIP_NET_HANDLE net_hdl;
	TCPIP_NET_IF* pNetIf;
	IPV4_ADDR ip;

	net_hdl = TCPIP_STACK_IndexToNet(0);
	pNetIf = _TCPIPStackHandleToNet(net_hdl);
	ip.Val  = pNetIf->netIPAddr.Val;
	return sprintf(out, 
		"%u.%u.%u.%u",
		ip.v[0], ip.v[1], ip.v[2], ip.v[3]);
}

/*
	server_nm_str writes the server network mask to a buffer as four printable
	decimal values separated by periods. The routine does not check the size of
	the destination string buffer, but it does return the number of characters
	it wrote.
*/
int server_nm_str(char* out) {
	TCPIP_NET_HANDLE net_hdl;
	TCPIP_NET_IF* pNetIf;
	IPV4_ADDR nm;

	net_hdl = TCPIP_STACK_IndexToNet(0);
	pNetIf = _TCPIPStackHandleToNet(net_hdl);
	nm.Val = pNetIf->netMask.Val;
	return sprintf(out, 
		"%u.%u.%u.%u",
		nm.v[0], nm.v[1], nm.v[2], nm.v[3]);
}

/*
	server_gw_str writes the server gateway address to a buffer as four printable
	decimal values separated by periods. The routine does not check the size of
	the destination string buffer, but it does return the number of characters
	it wrote.
*/
int server_gw_str(char* out) {
	TCPIP_NET_HANDLE net_hdl;
	TCPIP_NET_IF* pNetIf;
	IPV4_ADDR gw;

	net_hdl = TCPIP_STACK_IndexToNet(0);
	pNetIf = _TCPIPStackHandleToNet(net_hdl);
	gw.Val   = pNetIf->netGateway.Val;
	return sprintf(out, 
		"%u.%u.%u.%u",
		gw.v[0], gw.v[1], gw.v[2], gw.v[3]);
}

/*
	server_interface_str writes the server interface name to a buffer as a
	printable string. The routine does not check the size of the destination
	string buffer, but it does return the number of characters it wrote.
*/
int server_interface_str(char* out) {
	TCPIP_NET_HANDLE net_hdl;
	const char* name;

	net_hdl = TCPIP_STACK_IndexToNet(0);
	name = TCPIP_STACK_NetNameGet(net_hdl);
	return sprintf(out, "%s", name);
}

/*
	server_link_up returns true iff the Ethernet interface has an active link
	to a peer, hub, or switch.
*/
bool server_link_up(void) {
	TCPIP_NET_HANDLE net_hdl;
	net_hdl = TCPIP_STACK_IndexToNet(0);
	return TCPIP_STACK_NetIsLinked(net_hdl);
}

/*
	server_network_up returns true iff the TCP/IP interface is up and running.
*/
bool server_network_up(void) {
	TCPIP_NET_HANDLE net_hdl;
	net_hdl = TCPIP_STACK_IndexToNet(0);
	return TCPIP_STACK_NetIsUp(net_hdl);
}

/*
	server_info composes a string that presents the current state of the network
	interface. It writes the string to a buffer specified by the calling
	process. The calling process must provide a buffer that is large enough for
	the entire string. The routine does not check the size of the destination
	string buffer, but it does return the number of characters it wrote.
*/
int server_info(char* out) {
	int len = 0;
	len += sprintf(out+len,   "Interface    ");
	len += server_interface_str(out+len);
	len += sprintf(out+len, "\nIP           ");
	len += server_ip_str(out+len);
	len += sprintf(out+len, "\nMask         ");
	len += server_nm_str(out+len);
	len += sprintf(out+len, "\nGateway      ");
	len += server_gw_str(out+len);
	len += sprintf(out+len, "\nMAC          ");
	len += server_mac_str(out+len);
	len += sprintf(out+len, "\nLink         ");
	if (server_link_up()) {
		len += sprintf(out+len, "UP");
	} else {
		len += sprintf(out+len, "DOWN");
	}
	len += sprintf(out+len, "\nNetwork      ");
	if (server_network_up()) {
		len += sprintf(out+len, "UP");
	} else {
		len += sprintf(out+len, "DOWN");
	}
	return len;
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
	The task procedure is permitted to block, but it must call tcp_sock_tick
	while it is blocking, so as to maintain the TCP/IP stack, and to detect
	closure of the socket by the client. If the client closes the socket, the
	protocol task must abort and return a negative value. The tcpip_server will
	close the listening socket and open a new one. By this arrangement, the
	protocol task can block the servicing of other protocols by other
	tcpip_server processes indefinitely, but it can block its own server only so
	long as its client does not close its socket.

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
	TCP_SOCKET_INFO sock_info;
	TCPIP_NET_HANDLE net_hdl;

	const char* interface_name, *host_name;
	int status;
	static bool print_init_wait = true;
	static bool ping_sent = false;
	
	switch (server->state) {
		case S_WAIT_STACK: {
			tcpip_status = TCPIP_STACK_Status(sysObj.tcpip);
			if (tcpip_status < 0) {   
				if (print_init_wait) {
					console_print(
						"ERROR: TCP/IP stack initialization failed in %s.\n", __func__);
				}
				server->state = S_ERROR;
			} else if (tcpip_status == SYS_STATUS_READY) {
				net_hdl = TCPIP_STACK_IndexToNet(0);
				interface_name = TCPIP_STACK_NetNameGet(net_hdl);
				host_name = TCPIP_STACK_NetBIOSName(net_hdl);
				console_print(
					"%s server waiting for IP address, interface %s, host %s.\n",
					server->protocol,
					interface_name,
					string_trim(host_name));
				server->state = S_WAIT_IP;
			} else if (print_init_wait) {
				console_print("Waiting for TCP/IP stack in %s.\n", __func__);
				print_init_wait = false;
			}
		}
		break;

		case S_WAIT_IP: {
			net_hdl = TCPIP_STACK_IndexToNet(0);
			if (TCPIP_STACK_NetIsReady(net_hdl)) {
				server_ip_str(server->ip_str);
				console_print("%s server assigned IP address %s.\n", 
					server->protocol, server->ip_str);
				if (!ping_sent) {
					ping_gateway();
					ping_sent = true;
				}
				server->state = S_OPEN_SERVER;
			}
		}
		break;

		case S_OPEN_SERVER: {
			server->socket = TCPIP_TCP_ServerOpen(
				IP_ADDRESS_TYPE_IPV4, server->port, 0);
			if (server->socket == INVALID_SOCKET) {
				console_print("%s server failed to open socket on %s:%d.\n",
					server->protocol, server->ip_str,
					server->port);
				server->state = S_ERROR;
			} else {
				console_print("%s server listening for connection on %s:%d.\n",
					server->protocol, server->ip_str,
					server->port);
				server->state = S_LISTENING;
			}
		}
		break;

		case S_LISTENING: {
			if (TCPIP_TCP_IsConnected(server->socket)) {
				if (TCPIP_TCP_SocketInfoGet(server->socket, &sock_info)) {
					IPV4_ADDR ip = sock_info.remoteIPaddress.v4Add;
					server->last_tick = SYS_TMR_TickCountGet();
					console_print("%s server connection from %u.%u.%u.%u.\n",
						server->protocol, ip.v[0], ip.v[1], ip.v[2], ip.v[3]);
				} else {
					console_print("%s server connection from unknown peer.\n",
						server->protocol);
				}
				status = tasks(server);
				if (status >= 0) {
					server->state = S_SERVING;
				} else {
					console_print("%s server socket closed.\n",
						server->protocol);			
					server->state = S_CLOSE;
				}
			}
		}
		break;

		case S_SERVING: {
			if (!TCPIP_TCP_IsConnected(server->socket) ||
					TCPIP_TCP_WasDisconnected(server->socket)) {
				console_print("%s server socket closed.\n",
					server->protocol);
				server->state = S_CLOSE;
			} else {
				if (server->tcp_timeout > 0) {
					if (TCPIP_TCP_GetIsReady(server->socket) > 0) {
						server->last_tick = SYS_TMR_TickCountGet();
					} else if (SYS_TMR_TickCountGet() - server->last_tick 
							>  (server->tcp_timeout * 1000u)) {
						console_print("%s server timeout, closing socket.\n",
							server->protocol);			
						server->state = S_CLOSE;
					}
				}
				status = tasks(server);
				if (status < 0) {
					console_print("%s server socket closed.\n",
						server->protocol);			
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
			if (server_network_up()) {
				server->state = S_WAIT_STACK;
			}
		}
		break;
		
		default: {
			if (rand() % HEARTBEAT_PERIOD == 0) {
				if (debug) console_print("Unknown state %u for % server in %s.\n",
					server->protocol, server->state, __func__);
			}		
		}
		break;
	}
}

/*
	cli_eem_config is a CLI command procedure that displays Embedded Ethernet
	Module (EEM) configuration records and allows us to modify the fields of the
	flash memory configuration record. When we call the procedure from the CLI,
	we specify "show" or "set". For "show", we name a target "active", "flash",
	or "factory". For "set", the target is always the flash memory
	configuration. We can pass options that contain the name of one of the
	records in the EEM configuration, as well as a value for that record. We
	might pass "--ip_str", for example, followed by a string like "192.168.1.10"
	for the IP address we want the EEM to use when it next starts up. We can
	also pass the "--replace" option and specify either "active" or "factory".
	The routine will overwrite the entire flash memory configuration with the
	active or factory configurations respectively.

	The factory configuration is hard-coded. The active configuration is the
	configuration that was applied after the most recent system reset. We are
	not permitted to modify the active configuration. To change the
	configuration of the EEM, we write to the configuration record stored in
	flash memory. We might want to write the factory configuration to flash so
	as to perform a factory reset when we do not have access to the host board
	configuration switch. We might want to write the active configuration to
	flash in order to undo changes to the flash configuration we made earlier
	with this routines.

	The next time the EEM boots up, it will load the flash configuration as its
	active configuration. The flash configuration we write now will be loaded
	after reset later, so long as the configuration switch on the host board is
	not depressed during the reset. If this switch is depressed, the EEM will
	first over-write the flash configuration with the factory configuration and
	the factory configuration is the one that will be applied after reset.
*/
void cli_eem_config(cli_chan_type *ch, char *args) {
	char* source = NULL;
    bool update = false;
    bool verb_received = false;
    bool print_info = false;
    bool print_help = false;
	IPV4_ADDR addr;
	char* str;
	
	eem_config_type config;
	eem_config_read(&config);
	const eem_config_type* config_ptr = NULL;
	
	char* tok = strtok(args, " \t");
	while (tok != NULL) {
		if (strcmp(tok, "--info") == 0) {
			print_info = true;
			break;
		} 
		
		if (strcmp(tok, "--help") == 0) {
			print_help = true;
			break;
		} 
		
		if (strcmp(tok, "show") == 0 || (strcmp(tok, "set") == 0) ) {
			if (verb_received) {
				cli_print(ch,"ERROR: Only one command verb permitted in %s.\n",
					__func__);
				return;			
			}
			verb_received = true;
			if (strcmp(tok, "show") == 0) {
				source = strtok(NULL, " \t");
				if (source == NULL) {
					config_ptr = &config;		
				} else if (strcmp(source, "active") == 0) {
					config_ptr = &eem_config_active;
				} else if (strcmp(source, "flash") == 0) {
					config_ptr = &config;
				} else if (strcmp(source, "factory") == 0) {
					config_ptr = &eem_config_factory;
				} else {
					cli_print(ch,"ERROR: Unknown '%s' configuration '%s' in %s.\n",
						tok, source, __func__);
					return;			
				}
				update = false;
				break;
			} else {
				config_ptr = &config;
				update = true;
				tok = strtok(NULL, " \t");
				continue;
			}
		} 
		
		if (!verb_received) {
			cli_print(ch, "ERROR: Must specify 'show' or 'set' in %s.\n", __func__);
			return;
		}
				
		if (strcmp(tok, "--replace") == 0) {
			source = strtok(NULL, " \t");
			if (source == NULL) {
				cli_print(ch,"ERROR: Must specify soure for '%s' in %s.\n",
					tok,  __func__);
				return;			
			}
			if (strcmp(source, "active") == 0) {
				config_ptr = &eem_config_active;
			} 
			else if (strcmp(source, "factory") == 0) {
				config_ptr = &eem_config_factory;
			} 
			else {
				cli_print(ch,"ERROR: Invalid source '%s' for '%s' in %s.\n",
					tok, source, __func__);
				return;			
			}
		} else if (strcmp(tok, "--ip") == 0 
				|| strcmp(tok, "--gateway") == 0
				|| strcmp(tok, "--mask") == 0)  {
			str = strtok(NULL, " \t");
			if (!TCPIP_Helper_StringToIPAddress(str, &addr)) {
				cli_print(ch, "ERROR: Invalid IP address '%s' in %s.\n", str, __func__);
				return;
			}
			if (strcmp(tok, "--ip") == 0) {strcpy(config.ip_str, str);} 
			else if (strcmp(tok, "--gateway") == 0) {
				strcpy(config.gw_str, str);
			} else {
				strcpy(config.nm_str, str);
			}
		} else if (strcmp(tok, "--telnet-port") == 0 
				|| strcmp(tok, "--lwdaq-port") == 0) {
			char* val = strtok(NULL, " \t");
			if (!val) {
    			cli_print(ch, 
    				"ERROR: Option '%s' requires value in %s.\n", tok, __func__);
        		return;
			}
			char *end;
    		unsigned long p = strtoul(val, &end, 10);
			if (*end != '\0') {
				cli_print(ch,
					"ERROR: Value '%s' invalid for %s in %s.\n", val, tok, __func__);
				return;
			}
			if (p == 0 || p > 65535) {
				cli_print(ch,
					"ERROR: Value '%lu' out of range (1-65535) for %s in %s.\n", 
					p, tok, __func__);
				return;
			}
			if (strcmp(tok, "--telnet-port") == 0) {
				config.telnet_port = (uint32_t) p;
			} else {
				config.lwdaq_port = (uint32_t) p;
			}
		} else if (strcmp(tok, "--telnet-timeout") == 0 
				|| strcmp(tok, "--lwdaq-timeout") == 0) {
			char* val = strtok(NULL, " \t");
			if (!val) {
    			cli_print(ch, 
    				"ERROR: Option '%s' requires value in %s.\n", tok, __func__);
        		return;
			}
			char *end;
    		unsigned long p = strtoul(val, &end, 10);
			if (*end != '\0') {
				cli_print(ch,
					"ERROR: Value '%s' invalid for %s in %s.\n", val, tok, __func__);
				return;
			}
			if (strcmp(tok, "--telnet-timeout") == 0) {
				config.telnet_timeout = (uint32_t) p;
			} else {
				config.lwdaq_timeout = (uint32_t) p;
			}
		} else {
			cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n",
				tok, __func__);
			return;
		}
		tok = strtok(NULL, " \t");
	}
	
	if (print_info) {
		cli_message(ch, "Display or modify EEM configuration records.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  eem-config [--info|--help]\n"
"  eem-config show <active|flash|factory>\n"
"  eem-config set --option <value> [OPTIONS]\n"
"\n"
"Description:\n"
"  Display or modify Embedded Ethernet Module (EEM) configuration records. The\n"
"  'show' operation allows us to select one of the three EEM configuration records:\n"
"  'active', 'flash', or 'factory'. The one we select will be displayed on the CLI\n"
"  console. The 'set' operation allows us to modify or over-write the EEM\n"
"  configuration stored in flash memory. With the '--replace' option we name one of\n"
"  the two possible sources of a replacement configuration: the 'active' or\n"
"  'factory' configurations. The other options select a single field in the EEM\n"
"  flash configuration record, and apply a new value to that field. We can specify\n"
"  as many fields as we like in a single command line. During a normal start-up, in\n"
"  wich the configuration switch on the host is not depressed, the EEM loads the\n"
"  flash configuration directly into the active configuration and implements the\n"
"  active configuraiton. By this means we provide both persistent EEM configuration\n"
"  through reset and power cycles, and also the means to restore the EEM to a known\n"
"  state.\n"
"\n"
"Verbs:\n"
"  show                  Display active, flash, or factory configuration.\n"
"  set                   Over-write the flash configuration record.\n"
"\n"
"Selectors:\n"
"  active                Currently active configuration.\n"
"  flash                 Configuration stored in flash memory.\n"
"  factory               Factory-default configuration.\n"
"\n"
"Options:\n"
"  --replace <config>    Over-write flash with active or factory configuration.\n"
"  --ip <addr>           Set flash IP address to 'x.x.x.x'.\n"
"  --gateway <addr>      Set flash gateway address to new string 'x.x.x.x'.\n"
"  --mask <addr>         Set flash network mask to new string 'x.x.x.x'.\n"
"  --telnet-port <port>  Set flash Telnet sesrver port.\n"
"  --lwdaq-port <port>   Set flash LWDAQ server port.\n"
"  --telnet-timeout <s>  Set flash Telnet server timeout in seconds.\n"
"  --lwdaq-timeout <s>   Set flash LWDAQ server timeout in seconds.\n"
"  --info                Summary of this command, no changes made.\n"
"  --help                Detailed help for this command, no changes made.\n"
		);
		return;
	}
	
	if (!verb_received) {
		cli_print(ch, "ERROR: No verb or option specified in %s.\n", __func__);
		return;
	}

	if (!update) {
		server_str_from_config(ch->tx_buff, config_ptr);
		cli_print(ch, "%s\n", ch->tx_buff);
	} else {
		eem_config_write(config_ptr);
	}
	
    return;
}


