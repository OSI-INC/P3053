/*
	server.c -- Implementation of the TCP/IP Server and Communication library.
	
	Copyright (C) 2025-2026, Kevan Hashemi, Open Source Instruments Inc.

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
	eem_config_write writes a EEM configuration record to the flash memory at a
	location given by the configuration address.
*/
int eem_config_write(const eem_config_type *config_ptr) {
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
int eem_config_read(eem_config_type *config_ptr) {
	int len;
	len = pic_flash_getbytes(
		(uint8_t*) config_ptr,
		EEM_CONFIG_ADDR, 
		sizeof(eem_config_type));
	config_ptr->flash_magic[sizeof(config_ptr->flash_magic)-1] = '\0';
	if (strcmp(config_ptr->flash_magic, CONFIG_FLASH_MAGIC) != 0) {
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
int server_str_from_config(char *str, const eem_config_type *config_ptr) {
	int len = 0;
	len += sprintf(str+len, "seclevel: %u\n", config_ptr->seclevel);
	len += sprintf(str+len, "password: %s\n", config_ptr->password);
	len += sprintf(str+len, "person: %s\n", config_ptr->person);
	len += sprintf(str+len, "timestamp: %s\n", config_ptr->timestamp);
	len += sprintf(str+len, "ip_str: %s\n", config_ptr->ip_str);
	len += sprintf(str+len, "gw_str: %s\n", config_ptr->gw_str);
	len += sprintf(str+len, "nm_str: %s\n", config_ptr->nm_str);
	len += sprintf(str+len, "device: %s\n", config_ptr->device);
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
	
		if (strncmp(line_start, "seclevel", name_len) == 0) {
			config_ptr->seclevel = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		} else if (strncmp(line_start, "password", name_len) == 0) {
			snprintf(config_ptr->password, sizeof(config_ptr->password),
				"%.*s", (int)value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "person", name_len) == 0) {
			snprintf(config_ptr->person, sizeof(config_ptr->person),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "timestamp", name_len) == 0) {
			snprintf(config_ptr->timestamp, sizeof(config_ptr->timestamp),
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
		} else if (strncmp(line_start, "device", name_len) == 0) {
			snprintf(config_ptr->device, sizeof(config_ptr->device),
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
int tcp_sock_tick(void *context) {
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
int tcp_readcount(void *context) {
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
int tcp_read(void *context, uint8_t *buffer, uint32_t len) {
	TCP_SOCKET sock= *(TCP_SOCKET *) context;
	return (int) TCPIP_TCP_ArrayGet(sock, buffer, len);
}

/*
	tcp_getchar attempts to read a single byte from a TCP socket. It returns a
	-2 if the socket is disconnected. It returns -1 if the socket is still
	connected but there is no character to be read. Otherwise it returns a
	character. This routine can be deployed in a CLI channel descriptor.
*/
int tcp_getchar(void *context) {
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
int tcp_writecount(void *context) {
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
int tcp_write(void *context, const uint8_t *data, uint32_t len) {
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
int tcp_putchar(void *context, char c) {
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
int tcp_flush(void *context) {
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
int tcp_writeall(void *context, const uint8_t *buf, uint16_t len) {
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
	up. We provide no routine to determine if the ping succeeded. The ping
	identifier we pass is arbitrary. The routine returns a negative value if it
	encounters an error.
*/
int ping_gateway(void) {
	TCPIP_NET_HANDLE net_hdl;
	IPV4_ADDR gwAddr;
	TCPIP_ICMP_ECHO_REQUEST echoReq;
	TCPIP_ICMP_REQUEST_HANDLE reqHandle;
	ICMP_ECHO_RESULT res;

	net_hdl = TCPIP_STACK_IndexToNet(0);
	gwAddr.Val = TCPIP_STACK_NetAddressGateway(net_hdl);
	TCPIP_MAC_ADDR fakeMac = { .v = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 } };
	TCPIP_ARP_EntrySet(net_hdl, &gwAddr, &fakeMac, true);
	if (debug) console_print("Pinging %d.%d.%d.%d...",
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
	if (res != ICMP_ECHO_OK) {
		if (debug) console_print("Ping failed with error code %u.\n", res);
		return -1;
	}
	if (debug) console_print("Ping succeeded.\n");
	return 0;
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
int server_set_ip(const char *ip_str, const char *gw_str, const char *nm_str) {
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
int server_mac(uint8_t *out) {
	TCPIP_NET_HANDLE net_hdl;
	TCPIP_NET_IF *pNetIf;
	uint8_t *mac;

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
int server_mac_str(char *out) {
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
int server_ip_str(char *out) {
	TCPIP_NET_HANDLE net_hdl;
	TCPIP_NET_IF *pNetIf;
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
int server_nm_str(char *out) {
	TCPIP_NET_HANDLE net_hdl;
	TCPIP_NET_IF *pNetIf;
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
int server_gw_str(char *out) {
	TCPIP_NET_HANDLE net_hdl;
	TCPIP_NET_IF *pNetIf;
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
int server_interface_str(char *out) {
	TCPIP_NET_HANDLE net_hdl;
	const char *name;

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
int server_info(char *out) {
	int len = 0;
	len += sprintf(out+len,   "Interface       ");
	len += server_interface_str(out+len);
	len += sprintf(out+len, "\nIP              ");
	len += server_ip_str(out+len);
	len += sprintf(out+len, "\nMask            ");
	len += server_nm_str(out+len);
	len += sprintf(out+len, "\nGateway         ");
	len += server_gw_str(out+len);
	len += sprintf(out+len, "\nMAC             ");
	len += server_mac_str(out+len);
	len += sprintf(out+len, "\nLink            ");
	if (server_link_up()) {
		len += sprintf(out+len, "UP");
	} else {
		len += sprintf(out+len, "DOWN");
	}
	len += sprintf(out+len, "\nNetwork         ");
	if (server_network_up()) {
		len += sprintf(out+len, "UP");
	} else {
		len += sprintf(out+len, "DOWN");
	}
	return len;
}

/*
	tcpip_server manages connections to a TCP/IP server and deploys a messaging
	protocol to them using a messaging task function we provide. To service a
	socket, the server routine calls this task function, passing to the task
	function the complete server record, which includes a handle to the socket.

	The tcpip_server is a sequence of conditional clauses that implement the
	server logic with the help of a few flags, an active socket, a listening
	socket, and a socket queue. The routine does not block unless its protocol
	task blocks. The protocol task is permitted to block, but it must call
	tcp_sock_tick while it is blocking, so as to maintain the TCP/IP stack, and
	to detect closure of the socket by the client. If the client closes the
	socket, the protocol task must abort and return a negative value.

	The server begins its activities by opening a listening socket on the port
	named in its server record. When it receives a connection, it puts the
	connection at the back of its socket queue and opens a new listening socket.
	If the queue is full, the server closes the new socket immediately. In the
	server record, the listening socket is the one called "listening". The one
	called "socket" is the one that the protocol call-back function will operate
	upon. We call it the "active" socket.

	If the active socket is open, the server calls the protocol task function to
	service the socket. But if the active socket is no longer open, and there is
	an open socket at the front of the socket queue, the server makes this
	socket the active socket and moves all the sockets in the queue one step
	forwards. Once a socket is made active, the server begins a timeout
	countdown. The countdown resets every time the server sees that the socket
	input buffer is not empty. The connection ends when the client closes the
	socket or when the timeout expires. Only then will the next socket in the
	queue be serviced.

	The protocol task call-back function takes as a parameter the server status
	record, which includes an initialization flag and a socket handle. The
	server sets the initialization flag before the first call to the tasks
	routine on a new socket and clears it after. The task function can use the
	initialization flag to initialize itself. It uses the socket handle to
	communicate with the client. The server will close the socket whenever it
	sees a negative return value. The task call-back function should not close
	the socket itself. A return value of EEM_SOCK_ERR indicates that the client
	has closed the socket. A return value of EEM_SOCK_END indicates that the
	client has requested that the socket be closed, ending the connection. A
	return value of EEM_SOCK_SIN indicates that the protocol has detected
	misbehavior on the part of the client and is closing the socket
	unilaterally. 
*/
void tcpip_server(tcpip_server_type *server, tcpip_tasks_type tasks) {

	SYS_STATUS tcpip_status;
	TCP_SOCKET_INFO sock_info;
	TCPIP_NET_HANDLE net_hdl;

	const char *interface_name, *host_name;
	int status;
	
	// We disable the server by setting the port to zero. This way, we can 
	// disable or enable servers using the EEM configuration record rather
	// than by setting flags in our source code.
	if (server->port == 0) return;
	
	// The network_up flag must be cleared to false. So long as it is cleared,
	// we will do nothing more than check this flag and see if the network is up
	// and running. When the networks is ready to negotiate or assert its
	// address, we will set the flag. We have another local static flag that
	// controls our console announcements about the state of the network
	// interface.
	if (!server->network_up) {
		static bool flag = true;
		server->ip_assigned = false;
		tcpip_status = TCPIP_STACK_Status(sysObj.tcpip);
		if (tcpip_status < 0) {
			if (flag) {
				console_print("ERROR: Stack initialization failed for %s server\n",
					server->protocol);
				flag = false;
			}
			return;
		} else if (tcpip_status == SYS_STATUS_READY) {
			net_hdl = TCPIP_STACK_IndexToNet(0);
			interface_name = TCPIP_STACK_NetNameGet(net_hdl);
			host_name = TCPIP_STACK_NetBIOSName(net_hdl);
			console_print(
				"%s server waiting for IP address on interface %s of %s.\n",
				server->protocol,
				interface_name,
				string_trim(host_name));
			server->network_up = true;
		} else if (flag) {
			console_print("%s server waiting for stack initialization.\n",
				server->protocol);
			flag = false;
		}
		return;
	}

	// We get here only if the network is up. Now we are waiting for an address.
	// The ip_assigned flag has been cleared while we waited for the network to
	// come up. Now we check to see if we have a network address, and if so, we
	// set the flag and initialize all the sockets in our queue, our listening
	// socket, and our active socket, to the invalid socket value. At the end of
	// this clause, we always return. We want to allow the main loop to call the
	// tcpip_tick routine and do other things.
	if (!server->ip_assigned) {
		net_hdl = TCPIP_STACK_IndexToNet(0);
		if (TCPIP_STACK_NetIsReady(net_hdl)) {
			server_ip_str(server->ip_str);
			console_print("%s server assigned IP address %s.\n", 
				server->protocol, server->ip_str);
			if (ping_gateway() >= 0) {
				console_print("%s server ping of gateway %s complete.\n",
					server->protocol, eem_config_active.gw_str);
			} else {
				console_print("%s server ping of gateway %s failed.\n",
					server->protocol, eem_config_active.gw_str);
			}
			server->ip_assigned = true;
			server->socket = INVALID_SOCKET;
			server->listening = INVALID_SOCKET;
			for (int i = 0; i < EEM_TCB_QUEUE_SIZE; i++) {
				server->queue[i] = INVALID_SOCKET;
			}
		}
		return;
	}
	
	// If our active socket is open, which is to say: it is not marked as
	// invalid, we service the socket. If we find the socket has been closed by
	// the client, or it has timed out, or the protocol task routine returns an
	// error code, we close the socket at the server end and mark it as invalid,
	// which is to say: we mark it as closed. We use the socket handle itself as
	// an identifier, which we can do because the handle is just a sixteen-bit
	// index into a table of socket records. So we will have small positive
	// integer values for the socket idendifier, and these will be re-used as
	// sockets close and are opened again. We will see multiple identifiers when
	// the queue is used to save connections until we are ready to service them.
	// The invalid socket constant is just 0xFFFF, or sixteen-bit negative one,
	// so if we ever see "-1" as our socket identifier, we are printing out the
	// identifier of an invalid socket. At the end of this clause we do not
	// return: we must allow the listening socket and queue to be maintained, so
	// we cannot return after servicing the active socket.
	if (server->socket != INVALID_SOCKET) {
		bool close_socket = false;
		if (!TCPIP_TCP_IsConnected(server->socket) ||
				TCPIP_TCP_WasDisconnected(server->socket)) {
			if (debug) console_print(
				"%s socket %d closed by client.\n", 
				server->protocol, (int) server->socket);
			close_socket = true;
		} else {
			if (server->tcp_timeout > 0) {
				if (TCPIP_TCP_GetIsReady(server->socket) > 0) {
					server->last_tick = SYS_TMR_TickCountGet();
				} else if (SYS_TMR_TickCountGet() - server->last_tick 
						>  (server->tcp_timeout * 1000u)) {
					if (debug) console_print(
						"%s socket %d timeout, server closing socket.\n",
						server->protocol, (int) server->socket);	
					close_socket = true;		
				}
			}
			if (server->socket != INVALID_SOCKET) { 
				status = tasks(server);
				server->sock_init = false;
				if (status < 0) {
					close_socket = true;
					switch (status) {
						case EEM_SOCK_ERR: {
							if (debug) console_print(
								"%s socket %d closed by server after socket error.\n",
								server->protocol, (int) server->socket);
						}
						break;
						
						case EEM_SOCK_END: {
							if (debug) console_print(
								"%s socket %d closed by server at client request.\n",
								server->protocol, (int) server->socket);
						}
						break; 
						
						case EEM_SOCK_SIN: {
							if (debug) console_print(
								"%s socket %d closed by server after client error.\n",
								server->protocol, (int) server->socket);
						}
						break;
					}	
				}
			}
		}
		if (close_socket) {
			TCPIP_TCP_Close(server->socket);
			server->socket = INVALID_SOCKET;
		}
	}
	
	// If our active socket is invalid, but we have a valid socket at the front
	// of the queue, we move this valid socket to the active position and shift
	// all the sockets in the queue one step forwards, filling in the last one
	// with the invalid socket value. No servicing of manipulating of the
	// communication channels takes place here, and we do not return after
	// completing the shift. We still have to attend to the listening socket.
	if (server->socket == INVALID_SOCKET && server->queue[0] != INVALID_SOCKET) {
		server->socket = server->queue[0];
		server->sock_init = true;
		server->last_tick = SYS_TMR_TickCountGet();
		for (int i = 0; i < EEM_TCB_QUEUE_SIZE-1; i++) {
			server->queue[i] = server->queue[i+1];
		}
		server->queue[EEM_TCB_QUEUE_SIZE-1] = INVALID_SOCKET;
		if (debug) console_print("%s socket %d activated.\n",
			server->protocol, (int) server->socket);
	}
	
	// If our listening socket is open, we check to see if it has a connection.
	// If it has a connection, it is no longer a listening socket, but a
	// connected socket. We move the connected socket to the back of the queue.
	// If there is no space in the queue, we close the socket. Having moved or
	// closed the formerly listening socket we set the current listening socket
	// to the invalid code.
	if (server->listening != INVALID_SOCKET) {
		if (TCPIP_TCP_IsConnected(server->listening)) {
			if (TCPIP_TCP_SocketInfoGet(server->listening, &sock_info)) {
				IPV4_ADDR ip = sock_info.remoteIPaddress.v4Add;
				if (debug) console_print(
					"%s connection from %u.%u.%u.%u to socket %d.\n",
					server->protocol, ip.v[0], ip.v[1], ip.v[2], ip.v[3], 
					(int) server->listening);
			} else {
				if (debug) console_print(
					"%s connection from unknown peer to socket %d.\n",
					server->protocol, (int) server->listening);
			}
			int i = 0;
			bool queued = false;
			for (i = 0; i < EEM_TCB_QUEUE_SIZE; i++) {
				if (server->queue[i] == INVALID_SOCKET) {
					server->queue[i] = server->listening;
					queued = true;
					break;
				}
			}
			if (queued) {
				if (server->socket != INVALID_SOCKET) 
					if (debug) console_print(
						"%s socket %d waiting in queue position %d.\n",
						server->protocol, (int) server->listening, i);
			} else {
				TCPIP_TCP_Close(server->listening);	
				if (debug) console_print(
					"%s socket %d closed: queue is full.\n",
					server->protocol, (int) server->listening);
			}
			server->listening = INVALID_SOCKET;
		}
	}

	// After we first receive our address, this is the only clause in the server
	// routine that will be executed. All other sockets are invalid, including
	// the listening socket itself. We try to open a listening sockeet. If we
	// fail, we make an announcement and set a flag so that we don't keep making
	// the same announcement. But we will keep trying to open a listening
	// socket, and if we succeed, we will announce our success.
	if (server->listening == INVALID_SOCKET) {
		static bool flag = true;
		server->listening = TCPIP_TCP_ServerOpen(IP_ADDRESS_TYPE_IPV4, server->port, 0);
		if (server->listening == INVALID_SOCKET) {
			if (flag) console_print("%s failed to open listening socket on %s:%d.\n",
				server->protocol, server->ip_str, server->port);
			flag = false;
		} else {
			if (debug) console_print("%s socket %d listening for connection on %s:%d.\n",
				server->protocol, (int) server->listening, server->ip_str, server->port);
			flag = true;
		}
		return;
	}
	
	return;
}

/*
	cli_config is a CLI command procedure that displays Embedded Ethernet Module
	(EEM) configuration records and allows us to modify the fields of the flash
	memory configuration record. When we call the procedure from the CLI, we
	specify "show" or "set". For "show", we name a target "active", "flash", or
	"factory". For "set", the target is always the flash memory configuration.
	We can pass options that contain the name of one of the records in the EEM
	configuration, as well as a value for that record. We might pass "--ip_str",
	for example, followed by a string like "192.168.1.10" for the IP address we
	want the EEM to use when it next starts up. We can also pass the "--replace"
	option and specify either "active" or "factory". The routine will overwrite
	the entire flash memory configuration with the active or factory
	configurations respectively.

	The factory EEM configuration is hard-coded. The active configuration is the
	configuration that was applied after the most recent system reset. We are
	not permitted to modify the active configuration. To change the
	configuration of the EEM, we write to the configuration record stored in
	flash memory. We might want to write the factory configuration to flash so
	as to perform a factory reset when we do not have access to the motherboard
	configuration switch. We might want to write the active configuration to
	flash in order to undo changes to the flash configuration we made earlier
	with this routines.

	The next time the EEM boots up, it will load the flash configuration as its
	active configuration. The flash configuration we write now will be loaded
	after reset later, so long as the configuration switch on the motherboard
	is not depressed during the reset. If this switch is depressed, the EEM will
	first over-write the flash configuration with the factory configuration and
	the factory configuration is the one that will be applied after reset.
	
	The cli_config command respects the security level field of the active EEM
	configuration. If the security level is 1 or higher, cli_config will insist
	that the client be logged in before performing any function other than
	showing help or info.	
*/
void cli_config(cli_chan_type *ch, char *args) {
	char *source = NULL;
    bool update = false;
    bool verb_received = false;
    bool print_info = false;
    bool print_help = false;
	IPV4_ADDR addr;
	
	eem_config_type config;
	eem_config_read(&config);
	const eem_config_type *config_ptr = NULL;
	
	char *tok = strtok(args, " \t");
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
		} else if (strcmp(tok, "--ip-str") == 0 
				|| strcmp(tok, "--gw-str") == 0
				|| strcmp(tok, "--nm-str") == 0)  {
			char *str = strtok(NULL, " \t\"\'");
			if (!TCPIP_Helper_StringToIPAddress(str, &addr)) {
				cli_print(ch, "ERROR: Invalid IP address '%s' in %s.\n", str, __func__);
				return;
			}
			if (strcmp(tok, "--ip-str") == 0) {
				strcpy(config.ip_str, str);
			} else if (strcmp(tok, "--gw-str") == 0) {
				strcpy(config.gw_str, str);
			} else {
				strcpy(config.nm_str, str);
			}
		} else if (strcmp(tok, "--telnet-port") == 0 
				|| strcmp(tok, "--lwdaq-port") == 0) {
			char *val = strtok(NULL, " \t");
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
			if (p > 65535) {
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
			char *val = strtok(NULL, " \t");
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
		} else if (strcmp(tok, "--password") == 0) {
			char *str = strtok(NULL, " \t\"\'");
			if (!str) {
    			cli_print(ch, 
    				"ERROR: Option '%s' requires value in %s.\n", tok, __func__);
        		return;
			}			
			strcpy(config.password, str);
		} else if (strcmp(tok, "--person") == 0) {
			char *str = strtok(NULL, " \t\"\'");
			if (!str) {
    			cli_print(ch, 
    				"ERROR: Option '%s' requires value in %s.\n", tok, __func__);
        		return;
			}			
			strcpy(config.person, str);
		} else if (strcmp(tok, "--timestamp\"\'") == 0) {
			char *str = strtok(NULL, " \t");
			if (!str) {
    			cli_print(ch, 
    				"ERROR: Option '%s' requires value in %s.\n", tok, __func__);
        		return;
			}			
			strcpy(config.timestamp, str);
		} else if (strcmp(tok, "--device") == 0) {
			char *str = strtok(NULL, " \t\"\'");
			if (!str) {
    			cli_print(ch, 
    				"ERROR: Option '%s' requires value in %s.\n", tok, __func__);
        		return;
			}			
			strcpy(config.device, str);
		} else if (strcmp(tok, "--tcp_tick_ms") == 0) {
			char *val = strtok(NULL, " \t");
			if (!val) {
    			cli_print(ch, 
    				"ERROR: Option '%s' requires value in %s.\n", tok, __func__);
        		return;
			}
			char *end;
    		unsigned long p = strtoul(val, &end, 10);
			if (*end != '\0' || p < 1) {
				cli_print(ch,
					"ERROR: Value '%s' invalid for %s in %s.\n", val, tok, __func__);
				return;
			}
			config.telnet_timeout = (uint32_t) p;
		} else if (strcmp(tok, "--seclevel") == 0) {
			char *val = strtok(NULL, " \t");
			if (!val) {
    			cli_print(ch, 
    				"ERROR: Option '%s' requires value in %s.\n", tok, __func__);
        		return;
			}
			char *end;
    		unsigned long p = strtoul(val, &end, 10);
			if (*end != '\0' || p < 0 || p > 2) {
				cli_print(ch,
					"ERROR: Value '%s' invalid for %s in %s.\n", val, tok, __func__);
				return;
			}
			config.seclevel = (uint32_t) p;
		} else {
			cli_print(ch, "ERROR: Unrecognized option '%s' in %s.\n",
				tok, __func__);
			return;
		}
		tok = strtok(NULL, " \t");
	}
	
	if (print_info) {
		cli_message(ch, "Display or modify configuration records.\n");
		return;
	}

	if (print_help) {
		cli_message(ch,
"Usage:\n"
"  config [--info|--help]\n"
"  config show <active|flash|factory>\n"
"  config set --option <value> [OPTIONS]\n"
"\n"
"Description:\n"
"  Display or modify Embedded Ethernet Module (EEM) configuration records. The\n"
"  'show' operation allows us to select one of the three EEM configuration records:\n"
"  'active', 'flash', or 'factory' for display. The 'set' operation allows us to\n"
"  modify or over-write the configuration stored in flash memory. With '--replace'\n"
"  we name 'active' or 'factory' to overwrite the flash configuration. Other 'set'\n"
"  options allow us to modify individual fields in the flash configuration. Several\n"
"  fields take the form of character strings. These must contain no spaces or tabs.\n"
"  Any quotation marks, either single or double, will be interpreted as delimiters\n"
"  rather than members of the string. This command respects the security level\n"
"  declared by the active configuration. If the security level is > 0, the command\n"
"  will prohibit both 'show' and 'set'. operations unless the channel status is\n"
"  LOGIN_PASS. For the Telnet and LWDAQ port numbers, we enter a value 0-65535, for\n"
"  which 0 will disable the server.\n"
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
"  --seclevel <0|1|2>    Set security level to value 0-2.\n"
"  --password <str>      Set password to string containing no whitespace.\n"
"  --person <str>        Set operator name to string containing no whitespace.\n"
"  --ip-str <str>        Set flash IP address to x.x.x.x.\n"
"  --gw-str <str>        Set flash gateway address to new string x.x.x.x.\n"
"  --nm-str <str>        Set flash network mask to new string x.x.x.x.\n"
"  --timestamp <str>     Set timestamp to string containing no white space.\n"
"  --device <str>        Set device name to string containing no white space.\n"
"  --telnet-port <port>  Set flash Telnet server port, 0 to disable.\n"
"  --lwdaq-port <port>   Set flash LWDAQ server port, 0 to disable.\n"
"  --telnet-timeout <s>  Set flash Telnet server timeout in seconds.\n"
"  --lwdaq-timeout <s>   Set flash LWDAQ server timeout in seconds.\n"
"  --tcp_tick_ms <ms>    Set tcp tick execution period in milliseconds.\n"
"  --info                Summary of this command, no modifications made.\n"
"  --help                Detailed help for this command, no modifications made.\n"
		);
		return;
	}
	
	if (!verb_received) {
		cli_print(ch, "ERROR: No verb or option specified in %s.\n", __func__);
		return;
	}

	if (eem_config_active.seclevel > 0 && ch->status != CLI_PASS) {
		cli_print(ch, "ERROR: Access denied, login required in %s.\n", __func__);
		ch->status = CLI_FAULT;
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


