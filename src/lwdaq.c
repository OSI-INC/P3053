/*
	lwdaq.c -- Implementation of the Long-Wire Data Acquisition Relay library.

	Copyright (C) 2025-2026, Kevan Hashemi, Open Source Instruments Inc.

	This program is free software: you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the Free
	Software Foundation, either version 3 of the License, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along with
	this program. If not, see <https://www.gnu.org/licenses/>.
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
#include "configuration.h"
#include "definitions.h"
#include "utils.h"
#include "config.h"
#include "pic.h"
#include "server.h"
#include "console.h"
#include "lwdaq.h"

// Message header and footer sizes.
#define START_CODE 0xA5 
#define END_CODE 0x5A
#define CLOSE_CODE 0x04
#define START_OFFSET 0
#define ID_OFFSET 1
#define CLEN_OFFSET 5
#define CONTENT_OFFSET 9
#define FRAME_SIZE 10
#define FRAME_BUFF_SIZE 15

// Message Identifiers
#define VERSION_READ	0
#define BYTE_READ 		1
#define BYTE_WRITE 		2
#define STREAM_READ		3
#define DATA_RETURN		4
#define BYTE_POLL		5
#define LOGIN			6
#define CONFIG_READ		7
#define CONFIG_WRITE	8
#define MAC_READ		9
#define STREAM_DELETE	10
#define ECHO			11
#define STREAM_WRITE	12
#define REBOOT			13

// Content sizes for particular messages.
#define BYTE_WRITE_LEN		5
#define BYTE_READ_LEN		4
#define BYTE_POLL_LEN		5

// The maximum length of a configuration string.
#define CONFIG_LENGTH 1023

// Turn on additional debugging for the tasks routine and message handler.
#define LWDAQ_DEBUG 0

// Standard LWDAQ controller addresses.
#define FIFO_STROBE_ADDR 62

// Compiler switches based upon host assembly number.
#if defined(EEM_HOST_A3038) \
	|| defined(EEM_HOST_A3042)
#define ENABLE_FIFO_STROBE
#endif

#if defined(EEM_HOST_A2071) \
	|| defined(EEM_HOST_A3050) \
	|| defined(EEM_HOST_A3052)
#define ENABLE_STREAM_WRITE
#endif

#if defined(EEM_HOST_A2071)
#define ENABLE_STREAM_DELETE
#endif

/*
	The LWDAQ server record. We must assign the port and IP string pointers
	before trying to start the LWDAQ server. By default, we disable the timout
	with value zero. We must set the timeout to a number of seconds before
	starting our server if we want the server to close an idle socket.
*/
#define DEFAULT_LWDAQ_PORT 90
tcpip_server_type lwdaq_server = {
	.network_up = false,
	.ip_assigned = false,
	.sock_init = true,
	.logged_in = false,
    .protocol = "LWDAQ",
    .ip_str = "0.0.0.0",
    .port = DEFAULT_LWDAQ_PORT,
    .tcp_timeout = 0,
    .last_tick = 0,
    .socket = INVALID_SOCKET,
    .listening = INVALID_SOCKET,
    .indicator = d4_on
};
	
/*
	lwdaq_header sends a data return message header through a socket.
*/
int lwdaq_header(TCP_SOCKET s, uint32_t id, uint32_t len) {
	uint8_t buff[FRAME_BUFF_SIZE];
	uint32_t *lp;
	buff[START_OFFSET] = START_CODE;
	lp = (uint32_t*) &buff[ID_OFFSET];
	*lp = flip_bytes_u32(id);
	lp = (uint32_t*) &buff[CLEN_OFFSET];
	*lp = flip_bytes_u32(len);
	tcp_writeall(&s, buff, CONTENT_OFFSET);
	return 0;
}

/*
	lwdaq_footer sends a terminating sequence through a socket.
*/
int lwdaq_footer(TCP_SOCKET s) {
	uint8_t buff[FRAME_BUFF_SIZE];
	buff[0] = END_CODE;
	tcp_writeall(&s, buff, 1);
	return 0;
}

/*
	lwdaq_byte_return sends a data return message through a socket with a single-byte
	integer as its content.
*/
int lwdaq_byte_return(TCP_SOCKET s, uint8_t data) {
	uint8_t buff[FRAME_BUFF_SIZE];
	lwdaq_header(s, DATA_RETURN, sizeof(data));
	buff[0] = data;
	tcp_writeall(&s, buff, sizeof(data));
	lwdaq_footer(s);
	return 0;
}

/*
	lwdaq_integer_return sends a data return message through a socket with a four-byte
	integer as its content.
*/
int lwdaq_integer_return(TCP_SOCKET s, uint32_t data) {
	uint8_t buff[FRAME_BUFF_SIZE];
	uint32_t *lp;
	lwdaq_header(s, DATA_RETURN, sizeof(data));
	lp = (uint32_t*) &buff[0];
	*lp = flip_bytes_u32(data);
	tcp_writeall(&s, buff, sizeof(data));
	lwdaq_footer(s);
	return 0;
}

/*
	lwdaq_data_return sends a block of data through a socket.
*/
int lwdaq_data_return(TCP_SOCKET s, uint8_t *block, uint32_t len) {
	lwdaq_header(s, DATA_RETURN, len);
	tcp_writeall(&s, block, len);
	lwdaq_footer(s);
	return 0;
}
	
/*
	lwdaq_str_from_config takes a EEM configuration record and generates a
	colon-delimited LWDAQ configuration string, as expected by the LWDAQ
	Configurator tool. The newline is marked by one line-feed character, LF, not
	by a pair of characters CRLF. The routine returns the length of the
	generated string. It does not check to make sure that the destination string
	is large enough.
*/
int lwdaq_str_from_config(char *str, const eem_config_type *config_ptr) {
	int len = 0;
	len += sprintf(str+len, "lwdaq_relay_configuration:\n");
	len += sprintf(str+len, "configuration_time: %s\n", config_ptr->timestamp);
	len += sprintf(str+len, "driver_id: %s\n", config_ptr->device);
	len += sprintf(str+len, "gateway_addr: %s\n", config_ptr->gw_str);
	len += sprintf(str+len, "ip_addr: %s\n", config_ptr->ip_str);
	len += sprintf(str+len, "ip_port: %u\n", config_ptr->lwdaq_port);
	len += sprintf(str+len, "operator: %s\n", config_ptr->person);
	len += sprintf(str+len, "password: %s\n", config_ptr->password);
	len += sprintf(str+len, "security_level: %u\n", config_ptr->seclevel);
	len += sprintf(str+len, "subnet_mask: %s\n", config_ptr->nm_str);
	len += sprintf(str+len, "tcp_timeout: %u\n", config_ptr->lwdaq_timeout);
	return len;
}

/*
	lwdaq_config_from_str skips the first line in a string, which it assumes to
	be occupied by the word "lwdaq_relay_configuration:", and in the lines
	following it looks for pairs "parameter: value". The last line of the string
	will end with the last character of the string. All other lines will be
	delimited by line-feed characters. If it finds a parameter with a name that
	matches one of our lwdaq parameters, it updates the corresponding value in
	the EEM configuration pointed to by the config_ptr. The routine checks only
	that the beginning of the parameter name matches a server parameter name.
	Thus a given name "ip_address" will match our parameter name "ip_addr". The
	routine returns the number of parameters if found, or a negative error code.
	The codes are as follows. For a null pointer in either the config_ptr or
	str, -1. For a line missing a colon, -2. For a line in which the colon is
	not followed by a space, -3.
*/
int lwdaq_config_from_str(eem_config_type *config_ptr, const char *str) {
	const char *p = str;
	int num_copied = 0;
	
	if (!config_ptr || !str) return -1;

	while (*p && *p != '\n') p++;
	if (*p == '\n') p++;
	
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
	
		if (strncmp(line_start, "ip_addr", name_len) == 0) {
			snprintf(config_ptr->ip_str, sizeof(config_ptr->ip_str),
				"%.*s", (int)value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "gateway_addr", name_len) == 0) {
			snprintf(config_ptr->gw_str, sizeof(config_ptr->gw_str),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "subnet_mask", name_len) == 0) {
			snprintf(config_ptr->nm_str, sizeof(config_ptr->nm_str),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "operator", name_len) == 0) {
			snprintf(config_ptr->person, sizeof(config_ptr->person),
				"%.*s", (int) value_len, value);
			num_copied++;
		}  else if (strncmp(line_start, "driver_id", name_len) == 0) {
			snprintf(config_ptr->device, sizeof(config_ptr->device),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "configuration_time", name_len) == 0) {
			snprintf(config_ptr->timestamp, sizeof(config_ptr->timestamp),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "password", name_len) == 0) {
			snprintf(config_ptr->password, sizeof(config_ptr->password),
				"%.*s", (int)value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "ip_port", name_len) == 0) {
			config_ptr->lwdaq_port = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		} else if (strncmp(line_start, "security_level", name_len) == 0) {
			config_ptr->seclevel = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		} else if (strncmp(line_start, "tcp_timeout", name_len) == 0) {
			config_ptr->lwdaq_timeout = (uint32_t) strtoul(value, NULL, 10);
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
	lwdaq_handle_message executes a LWDAQ message a LWDAQ server has received
	from a client. It does not read from the server socket. Instead, it accepts
	a complete and accurate message identifier, message length, and content byte
	array as inputs. The routine can and will write to the server socket in
	response to the instruction contained in the message. We pass into the
	routine a server record, which contains a TCP socket handle and a logged-in
	flag that the message handler uses to determine whether or not it should
	permit access to the LWDAQ instructions. 
*/
int lwdaq_handle_message(tcpip_server_type *server,
		uint32_t id, uint32_t len, uint8_t *content) {
	uint8_t register_addr = 0;
	uint8_t value = 0;
	static uint8_t tx_buff[TCP_TX_BUFF_SIZE];
	static char config_str[CONFIG_LENGTH];
	static eem_config_type config;

	if (eem_config_active.seclevel >= 2 && !server->logged_in && id != LOGIN) {
		console_print(
			"REJECTED: Immediate login required by security level %d in %s.\n",
			eem_config_active.seclevel, __func__);
		return EEM_SOCK_SIN;
	}

	switch (id) {
	
		// The byte-write operation writes a single byte to a specified location
		// in controller address space. The LWDAQ message allows four bytes to
		// specify this address, in big-endian order, but the EEM communicates
		// with the controller through an eight-bit address bus, so we must
		// ignore all but one of the bytes of the address, and so obtain the
		// single-byte "register address" to which we will write. The byte value
		// occupies the fifth byte of the content.
		case BYTE_WRITE: {
			if (len != BYTE_WRITE_LEN) {
				console_print("ERROR: Length '%d' <> BYTE_WRITE_LEN in %s.\n", 
					len, __func__);
				return EEM_SOCK_SIN;			
			}
			register_addr = content[3];
			value = content[4];
			if (debug) console_print("BYTE_WRITE to %d of %d in %s.\n",
				register_addr, value, __func__);
			mpcie_byte_write(register_addr, value);
		}
		break;

		// The byte-read instruction reads the value of a register and uses a data 
		// return message to send this value back to the client. The register address
		// we obtain in the same way as for byte-write: we use the fourth byte of
		// the content.
		case BYTE_READ: {
			if (len != BYTE_READ_LEN) {
				console_print("ERROR: Length '%d' <> BYTE_READ_LEN in %s.\n", 
					len, __func__);
				return EEM_SOCK_SIN;			
			}
			register_addr = content[3];
			value = mpcie_byte_read(register_addr);
			lwdaq_byte_return(server->socket, value);
			if (debug) console_print("BYTE_READ from %d of %d in %s.\n",
				register_addr, value, __func__);
		}
		break;

		// The byte-poll causes the controller to wait for a particular location
		// to assume a particular value. We use byte-poll to wait for data
		// acquisition tasks to complete. As soon as the byte-poll completes, we
		// process the next LWDAQ operation, which will be waiting in our LWDAQ
		// server's input buffer. The delay between the byte poll completion and
		// the next operation commencing is microseconds. If we were to poll a
		// controller byte from our LWDAQ client, this delay will be tens of
		// milliseconds, during which time our time-sensitive data acquisition,
		// such as acquiring an image from a radiation-damaged sensor, will be
		// ruined.
		case BYTE_POLL: {
			if (len != BYTE_POLL_LEN) {
				console_print("ERROR: Length '%d' <> BYTE_POLL_LEN in %s.\n", 
					len, __func__);
				return EEM_SOCK_SIN;			
			}
			register_addr = content[3];
			value = content[4];
			if (debug) console_print("BYTE_POLL of %d for %d in %s.\n",
				register_addr, value, __func__);
			while (mpcie_byte_read(register_addr) != value) {
				if (tcp_sock_tick(&server->socket) < 0) return EEM_SOCK_ERR;
			}
		}
		break;

		// The version-read retrieves the EEM's software version number, which will
		// hope will be some sensible function of the GitHub repository version, and
		// also let us deduce that this server is an EEM, not an RCM6700.
		case VERSION_READ: {
			if (debug) console_print("VERSION_READ in %s.\n", __func__);
			lwdaq_integer_return(server->socket, EEM_SOFTWARE_VERSION);
		}
		break;
		
		// The stream-read operation reads repeatedly from the same location.
		// The operation is useful when the location acts like a First-In,
		// First-Out (FIFO) buffer, as is the case for the RAM Portal location
		// in all standard LWDAQ Controllers. The RAM Portal in these
		// controllers is at address 0x3F. As we are reading data from the
		// portal, we will accumulated it in a transmit buffer. When that buffer
		// is full, we write the entire buffer to the TCP socket that has
		// requested the data. The transmit buffer will be a few kilobytes long,
		// but the requested data might be several megabytes, so we will be
		// filling and writing the buffer hundreds of times during the course of
		// the stream-read operation.
		case STREAM_READ: {
			register_addr = content[3];
			uint32_t tx_len = reverse_load_u32(&content[4]);
			if (debug) console_print("STREAM_READ from %d of %d bytes in %s.\n",
				register_addr, tx_len, __func__);
			lwdaq_header(server->socket, DATA_RETURN, tx_len);
			
		// Here we are setting up a stream read, where we assert the address and
		// unassert /CW only once. Subsequent reads are faster because the
		// controller logic is incrementing the data address. But these three
		// instructions don't do anything for hosts that have to check the FIFO
		// strobe register.
#ifndef ENABLE_FIFO_STROBE		
			mpcie_addr_set(register_addr);
			mpcie_data_input();
    		mpcie_wr_unassert(); 
#endif

		// Start the stream read loop. We will be reading until we have read
		// 'len' bytes, but we will be stopping to flush our transmit buffer
		// along the way.
			if (tx_len > 0) {
				int i = 0;
				for (int j = 0; j < tx_len; j++) {
		
		// In the case of telemetry receivers like the Animal Location Tracker
		// (A3038) or Telemetry Control Box (A3042), the RAM Portal cannot
		// provide immediately the data we want to read. Sometimes, we have to
		// wait to pop a message from the FIFO output while the receiver pushes
		// another message into the FIFO input. In these receivers, we must
		// write to the FIFO data strobe register to request a byte from the
		// FIFO, and poll the same register until it returns a non-zero value.
		// Now we can read the byte. While polling the strobe register, we call
		// our TCP maintenance routine to keep the stack and network drivers
		// going.
#ifdef ENABLE_FIFO_STROBE
					mpcie_byte_write(FIFO_STROBE_ADDR, 0);
					while (mpcie_byte_read(FIFO_STROBE_ADDR) == 0) {
						if (tcp_sock_tick(&server->socket) < 0) return EEM_SOCK_ERR;
					}
					tx_buff[i] = mpcie_byte_read(register_addr);
#else
		// If we don't have to check the FIFO strobe register, all we need
		// to do is reapeat-read the same FIFO address.
					tx_buff[i] = mpcie_byte_read_repeat();
#endif

					i++;
					if ((i == sizeof(tx_buff)) || (j == tx_len - 1)) {
						tcp_writeall(&server->socket, tx_buff, i);
						if (tcp_sock_tick(&server->socket) < 0) {
							console_print(
								"ERROR: Failed to write %d bytes in %s.\n", i, __func__);
							return EEM_SOCK_ERR;
						}
						if (debug && LWDAQ_DEBUG) console_print(
							"Wrote %u bytes in %s.\n", i, __func__);
						i = 0;
					}
				}
			}
			lwdaq_footer(server->socket);
			if (debug) console_print("STREAM_READ complete in %s.\n", __func__);
		}
		break;

#ifdef ENABLE_STREAM_WRITE
		// The stream-write operation uploads a block of data to the
		// controller's memory by writing repeatedly to its RAM Portal. Each
		// time we write to the RAM Portal, the data address associated with the
		// portal increments by one, so that the next byte written to the portal
		// is written to the next address in controller memory. We use this
		// operation to test the memory on LWDAQ Drivers, and to upload
		// waveforms to Analog Signal Generators (A3052).
		case STREAM_WRITE: {
			register_addr = content[3];
			if (debug) console_print("STREAM_WRITE to %d of %d bytes in %s.\n",
				register_addr, len - 4, __func__);
			for (int i = 4; i < len; i++) {
				mpcie_byte_write(register_addr, content[i]);
			}
			if (debug) console_print("STREAM_WRITE complete in %s.\n", __func__);
		}
		break;
#endif

#ifdef ENABLE_STREAM_DELETE
		// The stream-delete operation sets a block of controller RAM locations
		// to a particular value by writing this value repeatedly to the
		// controller's RAM Portal. We use this operation to test the memory on
		// LWDAQ Drivers. We delete the rows of a simulated vertical gray-scale
		// image and then read it back with stream-read to see if it is perfect.
		case STREAM_DELETE: {
			register_addr = content[3];
			uint32_t wr_len = reverse_load_u32(&content[4]);
			value = content[8];
			if (debug) console_print("STREAM_DELETE %d to %d of %d bytes in %s.\n",
				value, register_addr, len - 4, __func__);
			for (int i = 0; i < wr_len; i++) {
				mpcie_byte_write(register_addr, value);
			}
			if (debug) console_print("STREAM_DELETE complete in %s.\n", __func__);
		}
		break;
#endif

		// The login operation compares the password in the message content to
		// the EEM active configuration password. If they are the same, the
		// operation returns byte value 0x01 to the client. If they are
		// different, the operation returns byte value 0x00.
		case LOGIN: {
			content[len] = '\0';
			if (debug) console_print("LOGIN in %s.\n", __func__);
			if (!strcmp(eem_config_active.password, (char*) content)) {
				server->logged_in = true;
				if (debug) console_print("Logged in with password '%s'.\n", content);
			} else {
				server->logged_in = false;
				console_print("REJECTED: Login attempt with password '%s'.\n", content);
			}
			lwdaq_byte_return(server->socket, server->logged_in);
		}
		break;

		// The config-read will look up the active EEM configuration, extract
		// the values that the LWDAQ Configurator Tool expects to see, format
		// them into the string the tool expects to receive, and tranmit the
		// configuration string to the client. If the client has not logged in,
		// and security level is one or higher, the operation returns an error.
		case CONFIG_READ:{
			if (debug) console_print("CONFIG_READ in %s.\n", __func__);
			if (server->logged_in || eem_config_active.seclevel == 0) {
				lwdaq_str_from_config(config_str, &eem_config_active);
			  	lwdaq_data_return(server->socket,
			  		(uint8_t*) config_str, strlen(config_str));
				if (debug) console_print(
					"Transmitted configuration of %d characters.\n",
					strlen(config_str));
			} else {
				console_print("REJECTED: Not logged in.\n");
				return EEM_SOCK_SIN;
			}
		}
		break;

		// The config-write receives a string in the format provided by the
		// LWDAQ Configurator Tool and extracts from it EEM configuration
		// fields, which it applies to the flash configuration. If the client
		// has not logged in, and security level is one or higher, the operation
		// returns an error.
		case CONFIG_WRITE:{
			if (debug) console_print("CONFIG_WRITE in %s.\n",__func__);
			if (server->logged_in || eem_config_active.seclevel == 0) {
				if (len<CONFIG_LENGTH) {
					if (debug) console_print("Accepted config %d characters:\n",len);
					content[len]=0x00;
					if (debug) console_print("%s",(char*) content);
					config = eem_config_active;
					lwdaq_config_from_str(&config, (char*) content);
					eem_config_write(&config);
					if (debug) console_print("Wrote new configuration to flash.\n",len);
				} else {
					console_print("REJECTED: Configuration string too long.\n",len);
					return EEM_SOCK_SIN;
				}
			} else {
				console_print("REJECTED: Not logged in.\n");
				return EEM_SOCK_SIN;
			}
		}
		break;

		// The mac-read operation returns the Media Access Control (MAC) address
		// of the EEM.
		case MAC_READ:{
			if (debug) console_print("MAC_READ in %s.\n", __func__);
			int size;
			size = server_mac(tx_buff);
			lwdaq_data_return(server->socket, tx_buff, size);
		}
		break;

		// The echo operation returns what was sent to it. We use this operation
		// for diagnostics.
		case ECHO: {
			if (debug) console_print("ECHO of %u bytes in %s.\n", len, __func__);
			lwdaq_data_return(server->socket, content, len);
			content[len] = '\0';
			if (debug) console_print("%s\n", content);
		}
		break;

		// The reboot command initiates a software reset. If the security level
		// requires a login, and the client is not logged in, the operation
		// generates an error.
		case REBOOT: {
			if (debug) console_print("REBOOT in %s.\n", __func__);
			if (server->logged_in || eem_config_active.seclevel == 0) {
				tx_buff[0] = CLOSE_CODE;
				tcp_writeall(&server->socket, tx_buff, 1);
				int counter = 1e6;
				while (counter > 0) {
					tcp_tick();
					counter--;
				}
				pic_reset();
			} else {
				if (debug) console_print("REJECTED: not logged in.\n");
				return EEM_SOCK_SIN;
			}
		}
		break;

		default: {
			if (debug) console_print("ERROR: Unrecognised message in %s.\n", __func__);
			return EEM_SOCK_SIN;
		}
		break;
	}
	return 0;
}

/*
	lwdaq_tasks services a LWDAQ connection. When the LWDAQ server starts
	listening for a socket, the tasks routine resets its received byte counter,
	and is ready to receive a fresh LWDAQ message stream. While serving an open
	socket, it reads bytes from the socket and stores them in its own receiving
	buffer. When it has accumulated a complete and correct LWDAQ message in its
	buffer, it passes the message to the LWDAQ message-handling routine.
	
	The LWDAQ task manager is not supposed to block the LWDAQ server that calls
	it, but it will block for however long it takes to handle one LWDAQ message.
	If ten messages arrive at once, they will be handled once per call to the
	task manager. If one of the tasks is to wait for a delay, or to read out a
	large image sensor, the message handler could be blocking for one or two
	seconds. During this time the message hanlder must be calling the routines
	that maintain the TCP/IP stack, the Ethernet PHY driver, and the console.

	A LWDAQ message begins with a start code, which is a single byte, followed
	by a four-byte, big-endian message identifier, which will be something like
	0x00000001, which stands for "byte read", or "read a single byte form the
	eight-bit parallel interface that connects the LWDAQ relay to the LWDAQ
	controller". After that we have a four-byte, big-endian unsigned integer for
	the length of the message content. Once the task routine has the length, it
	waits until it accumulates the required number of content bytes. The LWDAQ
	message must then be terminated with a close code. The byte read instruction
	has for its content the address that should be read, and this address we
	specify with another four-byte, big-endian unsigned integer. The content
	length is therefore "4". On a "byte write", the content length is "5"
	because we follow the address with the byte value we want to write to the
	eight-bit bus.

	If the incoming byte stream deviates in any way from the LWDAQ message
	protocol, the tasks routine will return a negative value. If the
	message-handler returns a negative value, the task routine aborts itself
	with a negative value. Our assumption is that when the task returns a
	negative value, the LWDAQ server will close the LWDAQ client's socket.
	Neither the task procedure nor the message-handling procedure are allowed to
	close the socket themselves.
	
	The routine takes as input a tcpip server structure, which contains not
	only the socket handle but also the server state, which allows the tasks
	manager to determine when to reset its buffer.
*/
int lwdaq_tasks(tcpip_server_type *server) {
	static uint8_t rx_buffer[TCP_RX_BUFF_SIZE];
	static uint32_t rx_available = 0;
	uint32_t id, len, rx_ready, rx_space, rx_received;
	int status;
	
	// When the server has just begun to service a socket, the sock_init flag
	// will be set.
	if (server->sock_init) {
		server->logged_in = false;
		rx_available = 0;
		if (debug) console_print("Initialized %s connection in %s.\n",
			server->protocol, __func__);
		return 0;
	};

	// The server is handling an existing, open socket. Our input buffer counter
	// is persistent from one call to the next, so we know how many bytes we
	// have in our buffer. We start by finding out how many bytes are available
	// for us to read from the socket and add to our buffer.
	rx_ready = tcp_readcount(&server->socket);
	rx_space = TCP_RX_BUFF_SIZE - rx_available;
	if (rx_ready > rx_space) rx_ready = rx_space;
	if (rx_ready>0) {
		rx_received = tcp_read(&server->socket, &rx_buffer[rx_available], rx_ready);	
		rx_available = rx_available + rx_received;
	}
	
	// If we have nothing available, we are going to return.
	if (rx_available == 0) return 0;
	
	// We have at least one byte, so we will check to see if we have a complete
	// message. We need at least nine bytes: the start code, the four-byte
	// message identifier, and the four-byte length.
	if (LWDAQ_DEBUG) console_print("rx_ready=%u rx_available=%u in %s.\n",
		rx_ready, rx_available, __func__);
	if (rx_buffer[0] != START_CODE) {
		if (rx_buffer[0] == CLOSE_CODE) {
			if (debug) console_print("Close code received in %s.\n", __func__);
			return EEM_SOCK_END;
		} else {
			if (debug) console_print("Invalid start code in %s.\n", __func__);
			return EEM_SOCK_SIN;
		}
	}
	if (rx_available<CONTENT_OFFSET) return rx_available;
	
	// We have a complete header, so extract the message identifier and length.
	// Once we have these, we can see if we have the content bytes in our buffer
	// already. If not, we return the number of byts available and do nothing
	// more.
	id = reverse_load_u32(&rx_buffer[ID_OFFSET]);
	len = reverse_load_u32(&rx_buffer[CLEN_OFFSET]);
	if (LWDAQ_DEBUG) console_print("id=%u len=%u in %s.\n", id, len, __func__);
	
	// We have the header, but we don't have all the content yet, so return.
	// The LWDAQ client is responsible for making sure that any incoming data
	// is broken into chunks smaller than our receive buffer. In the LWDAQ
	// client code, we assume the rx_buffer has size is equal to the
	// LWDAQ_Driver parameter server_buffer_size = 1400 bytes. In this server,
	// the buffer is can accommodate TCP_RX_BUFF_SIZE = 4096 bytes.
	if (rx_available < len + FRAME_SIZE) return rx_available;
	
	// We have all the content bytes we need, and a byte afterwards for the end
	// code. Check to see if the end code is valie. If not, we return a
	// negative value to indicate an error.
	if (rx_buffer[len+CONTENT_OFFSET] != END_CODE) {
		if (debug) console_print("Invalid end code in %s.\n", __func__);
		return EEM_SOCK_SIN;
	}
	
	// We appear to have a complete LWDAQ message, so pass it to the message
	// handler. We point the message handler to the tenth byte in our buffer,
	// which is byte nine, and corresponds to the first byte of the message
	// content. If the message handler returns an error, we exit with the same
	// error code.
	status = lwdaq_handle_message(server, id, len, &rx_buffer[CONTENT_OFFSET]);
	if (status<0) return status;
	
	// We always read from our socket as many bytes as it has to give. If we
	// have received bytes following the message we just handled, copy all the
	// bytes to the front of the buffer, so we can begin our message
	// accumulation process again.
	if (rx_available > len + FRAME_SIZE) {
		if (LWDAQ_DEBUG) console_print("Copying %u to %u from %u in %s.\n",
			rx_available - len - FRAME_SIZE, 0, len + FRAME_SIZE, __func__);
		memmove(&rx_buffer[0], 
			&rx_buffer[len+FRAME_SIZE], 
			rx_available - len - FRAME_SIZE);
		rx_available = rx_available - len - FRAME_SIZE;
	} else {
		rx_available = 0;
	}
	if (LWDAQ_DEBUG) console_print("rx_available=%u in %s.\n", rx_available, __func__);
		
	// Return the number of bytes available. This will certainly be zero or greater.
	return rx_available;
}



