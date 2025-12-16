/*
	lwdaq.c -- Implementation of the Long-Wire Data Acquisition Relay library.

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

// Include files.
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>				  
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include "configuration.h"
#include "definitions.h"
#include "utils.h"
#include "pic.h"
#include "server.h"
#include "console.h"
#include "lwdaq.h"

// Software version number.
#define RELAY_VERSION 32

// LWDAQ messages
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

// Other constants.
#define LWDAQ_CONFIG_LENGTH 1023
#define LWDAQ_DEBUG 0

// The LWDAQ server record.
tcpip_server_type lwdaq_server = {
    .socket   = INVALID_SOCKET,
    .state    = S_WAIT_STACK,
    .port     = LWDAQ_PORT,
    .protocol = "LWDAQ",
    .ip_addr = { .Val = 0 }
};
	
/*
	lwdaq_header sends a data return message header through a socket.
*/
int lwdaq_header(TCP_SOCKET s, uint32_t id, uint32_t len) {
	uint8_t buff[FRAME_BUFF_SIZE];
	uint32_t* lp;
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
	uint32_t* lp;
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
int lwdaq_data_return(TCP_SOCKET s, uint8_t* block, uint32_t len) {
	lwdaq_header(s, DATA_RETURN, len);
	tcp_writeall(&s, block, len);
	lwdaq_footer(s);
	return 0;
}
	
/*
	lwdaq_str_from_config takes a server configuration record and generates a
	colon-delimited LWDAQ configuration string, as expected by the LWDAQ
	Configurator tool. The newline is marked by one line-feed character, LF, not by
	a pair of characters CRLF. The routine returns the length of the generated
	string. It does not check to make sure that the destination string is large
	enough.
*/
int lwdaq_str_from_config(char* str, const server_config_type* config_ptr) {
	int len = 0;
	len += sprintf(str+len, "lwdaq_relay_configuration:\n");
	len += sprintf(str+len, "configuration_time: %s\n", config_ptr->time_str);
	len += sprintf(str+len, "driver_id: %s\n", config_ptr->device_str);
	len += sprintf(str+len, "gateway_addr: %s\n", config_ptr->gw_str);
	len += sprintf(str+len, "ip_addr: %s\n", config_ptr->ip_str);
	len += sprintf(str+len, "ip_port: %u\n", config_ptr->lwdaq_port);
	len += sprintf(str+len, "operator: %s\n", config_ptr->operator_str);
	len += sprintf(str+len, "password: %s\n", config_ptr->password_str);
	len += sprintf(str+len, "security_level: %u\n", config_ptr->security_level);
	len += sprintf(str+len, "subnet_mask: %s\n", config_ptr->nm_str);
	len += sprintf(str+len, "tcp_timeout: %u\n", config_ptr->tcp_timeout);
	return len;
}

/*
	lwdaq_config_from_str skips the first line in a string, which it assumes to
	be occupied by the word "lwdaq_relay_configuration:", and in the lines
	following it looks for pairs "parameter: value". The last line of the string
	will end with the last character of the string. All other lines will be
	delimited by line-feed characters. If it finds a parameter with a name that
	matches one of our lwdaq parameters, it updates the corresponding value in
	the server configuration pointed to by the config_ptr. The routine checks
	only that the beginning of the parameter name matches a server parameter
	name. Thus a given name "ip_address" will match our parameter name
	"ip_addr". The routine returns the number of parameters if found, or a
	negative error code. The codes are as follows. For a null pointer in either
	the config_ptr or str, -1. For a line missing a colon, -2. For a line in
	which the colon is not followed by a space, -3.
*/
int lwdaq_config_from_str(server_config_type *config_ptr, const char *str) {
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
			snprintf(config_ptr->operator_str, sizeof(config_ptr->operator_str),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "configuration_time", name_len) == 0) {
			snprintf(config_ptr->time_str, sizeof(config_ptr->time_str),
				"%.*s", (int) value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "password", name_len) == 0) {
			snprintf(config_ptr->password_str, sizeof(config_ptr->password_str),
				"%.*s", (int)value_len, value);
			num_copied++;
		} else if (strncmp(line_start, "ip_port", name_len) == 0) {
			config_ptr->lwdaq_port = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		} else if (strncmp(line_start, "security_level", name_len) == 0) {
			config_ptr->security_level = (uint32_t) strtoul(value, NULL, 10);
			num_copied++;
		} else if (strncmp(line_start, "tcp_timeout", name_len) == 0) {
			config_ptr->tcp_timeout = (uint32_t) strtoul(value, NULL, 10);
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
	lwdaq_handle_message handles an incoming LWDAQ message. We pass the routine
	a socket handle, which allows it to respond to the command when a response
	is required. We pass the LWDAQ message identifier, the length of the message
	content, and a pointer to the content buffer.
*/
int lwdaq_handle_message(TCP_SOCKET s, uint32_t id, uint32_t len, uint8_t* content) {
	uint8_t register_addr = 0;
	uint8_t value = 0;
	uint32_t tx_len = 0;
	static uint8_t tx_buffer[TCP_TX_BUFF_SIZE];
	uint32_t i = 0;
	static int logged_in = 0;
	static int security_level = 0;
	static char password[32] = "LWDAQ";
	static char config_str[LWDAQ_CONFIG_LENGTH];
	static server_config_type config;

	switch (id) {
		case BYTE_WRITE: {
			register_addr = content[3];
			value = content[4];
			if (debug) console_print("BYTE_WRITE to %d of %d in %s.\r\n",
				register_addr, value, __func__);
			lwdaq_byte_write(register_addr, value);
		}
		break;

		case BYTE_READ: {
			register_addr = content[3];
			value = lwdaq_byte_read(register_addr);
			lwdaq_byte_return(s, value);
			if (debug) console_print("BYTE_READ from %d of %d in %s.\r\n",
				register_addr, value, __func__);
		}
		break;

		case BYTE_POLL: {
			register_addr = content[3];
			value = content[4];
			if (debug) console_print("BYTE_POLL of %d for %d in %s.\r\n",
				register_addr, value, __func__);
			while (lwdaq_byte_read(register_addr) != value) {
				if (tcpip_socket_tick(&s)<0) return -1;
			}
		}
		break;

		case VERSION_READ: {
			if (debug) console_print("VERSION_READ in %s.\r\n", __func__);
			lwdaq_integer_return(s, RELAY_VERSION);
		}
		break;
		
		case STREAM_READ: {
			register_addr = content[3];
			tx_len = reverse_load_u32(&content[4]);
			if (debug) console_print("STREAM_READ from %d of %d bytes in %s.\r\n",
				register_addr, tx_len, __func__);
			lwdaq_header(s, DATA_RETURN, tx_len);
			if (tx_len > 0) {
				i = 0;
				for (int j = 0; j < tx_len; j++) {
					lwdaq_byte_write(62, 0);
					while (lwdaq_byte_read(62) == 0) {
						if (tcpip_socket_tick(&s)<0) return -1;
					}
					tx_buffer[i] = lwdaq_byte_read(register_addr);
					i++;
					if ((i == sizeof(tx_buffer)) || (j == tx_len - 1)) {
						tcp_writeall(&s, &tx_buffer[0], i);
						if (tcpip_socket_tick(&s) < 0) {
							if (debug) console_print(
								"Failed to write %d bytes in %s.\r\n", i, __func__);
							return -1;
						}
						if (debug && LWDAQ_DEBUG) console_print(
							"Wrote %u bytes in %s.\r\n", i, __func__);
						i = 0;
					}
				}
			}
			lwdaq_footer(s);
			if (debug) console_print("STREAM_READ complete in %s.\r\n", __func__);
		}
		break;

		case LOGIN:{
			if (debug) console_print("LOGIN in %s.\r\n", __func__);
			if (len<1) {
			  logged_in=0;
				if (debug) console_print("Failed with empty password in %s.\r\n");
			}
			if (!strcmp(password,(char*) content)) {
				logged_in=1;
				if (debug) console_print("Logged in with password: %s.\r\n", content);
			} else {
			  logged_in=0;
				if (debug) console_print("Failed with password: %s.\r\n", content);
			}
			lwdaq_byte_return(s, logged_in);
		}
		break;

		case CONFIG_READ:{
			if (debug) console_print("CONFIG_READ in %s.\r\n", __func__);
			if ((logged_in==1) || (security_level==0)) {
				lwdaq_str_from_config(config_str, &server_config_active);
			  	lwdaq_data_return(s, (uint8_t*) config_str, strlen(config_str));
				if (debug) console_print(
					"Transmitted configuration of %d characters.\r\n",
					strlen(config_str));
			} else {
				if (debug) console_print("Rejected: not logged in.\r\n");
				return -1;
			}
		}
		break;

		case CONFIG_WRITE:{
			if (debug) console_print("CONFIG_WRITE in %s.\r\n",__func__);
			if ((logged_in==1) || (security_level==0)) {
				if (len<LWDAQ_CONFIG_LENGTH) {
					if (debug) console_print("Accepted: config %d characters.\r\n",len);
					content[len]=0x00;
					console_print("%s",(char*) content);
					config = server_config_active;
					lwdaq_config_from_str(&config, (char*) content);
					server_config_write(&config);
					if (debug) console_print("Wrote new configuration to NVM.\r\n",len);
				} else {
					if (debug) console_print("Rejected: %d characters too long.\r\n",len);
					return -1;
				}
			} else {
				if (debug) console_print("Rejected: not logged in.\r\n");
				return -1;
			}
		}
		break;

		case MAC_READ:{
			if (debug) console_print("MAC_READ in %s.\r\n", __func__);
			server_mac(tx_buffer);
			lwdaq_data_return(s, tx_buffer, strlen((char*) tx_buffer));
		}
		break;

		case ECHO: {
			if (debug) console_print("ECHO of %u bytes in %s.\r\n", len, __func__);
			lwdaq_data_return(s, content, len);
			content[len] = 0x00;
			if (debug) console_print("%s\r\n", content);
		}
		break;

		default: {
			if (debug) console_print("Unrecognised message in %s.\r\n", __func__);
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
int lwdaq_tasks(tcpip_server_type* server) {
	static uint8_t rx_buffer[TCP_RX_BUFF_SIZE];
	static uint32_t rx_available = 0;
	uint32_t id, len, rx_ready;
	int status;

	/*
		When the server is listening for a new connection, and it receives a
		connection, it calls the task routine. So when we check for the server
		being in its listening state, we are checking to see if a new socket has
		just been opened. We reset our input buffer. As some point, we would
		like to introduce an idle timer her as well, so we can close the sockeet
		when it is idle for too long. Here we would start the timer.
	*/
	if (server->state == S_LISTENING) {
		rx_available = 0;
		if (debug) console_print("Initialized %s connection in %s.\r\n",
			server->protocol, __func__);
		return 0;
	};

	/*
		If we get here, the server is handling an existing, open socket. Our input
		buffer counter is persistent from one call to the next, so we know how many
		bytes we have in our buffer. We start by finding out how many bytes are
		available for us to read from the socket and add to our buffer.
	*/
	rx_ready = tcp_readcount(&server->socket);
	if (rx_ready>0) {
		tcp_read(&server->socket, &rx_buffer[rx_available], rx_ready);	
		rx_available = rx_available+rx_ready;
	}
	if (rx_available == 0) return 0;
	
	/*
		We have at least one byte, so we will check to see if we have a complete
		message. We need at least nine bytes: the start code, the four-byte
		message identifier, and the four-byte length.
	*/
	if (debug && LWDAQ_DEBUG) console_print("rx_ready=%u rx_available=%u in %s.\r\n",
		rx_ready, rx_available, __func__);
	if (rx_buffer[0] != START_CODE) {
		if (rx_buffer[0] == CLOSE_CODE) {
			if (debug) console_print("Close code received in %s.\r\n", __func__);
		} else {
			if (debug) console_print("Invalid start code in %s.\r\n", __func__);
		}
		return -1;
	}
	if (rx_available<CONTENT_OFFSET) return rx_available;
	
	/*
		We have a complete header, so extract the message identifier and length. Once
		we have these, we can see if we have the content bytes in our buffer already. 
		If not, we return the number of byts available and do nothing more.
	*/
	id = reverse_load_u32(&rx_buffer[1]);
	len = reverse_load_u32(&rx_buffer[5]);
	if (debug && LWDAQ_DEBUG) console_print(
		"id=%u len=%u in %s.\r\n", id, len, __func__);
	if (rx_available<len+FRAME_SIZE) return rx_available;
	
	/*
		We have all the content bytes we need, and a byte afterwards for the end
		code. Check to see if the end code is valie. If not, we return a
		negative value to indicate an error.
	*/
	if (rx_buffer[len+CONTENT_OFFSET] != END_CODE) {
		if (debug) console_print("Invalid end code in %s.\r\n", __func__);
		return -1;
	}
	
	/*
		We appear to have a valid and complete LWDAQ message, so pass it to the
		message handler. We point the message handler to the tenth byte in our
		buffer, which is byte nine, and corresponds to the first byte of the
		message content. If the message handler returns an error, we exit with
		the same error code.
	*/
	status = lwdaq_handle_message(server->socket, id, len, &rx_buffer[9]);
	if (status<0) return status;
	
	/*
		Because we always read from our socket as many bytes as it has to give,
		we can read more than one message into our receive buffer at a time. If
		we have received bytes following the message we just handled, copy all
		the bytes to the front of the buffer, so we can begin our message
		accumulation process again.
	*/
	if (rx_available>len+FRAME_SIZE) {
		if (debug && LWDAQ_DEBUG) console_print(
			"Copying %u to %u from %u in %s.\r\n",
			rx_available-len-FRAME_SIZE, 0, len+FRAME_SIZE, __func__);
		memmove(&rx_buffer[0], 
			&rx_buffer[len+FRAME_SIZE], 
			rx_available-len-FRAME_SIZE);
		rx_available = rx_available-len-FRAME_SIZE;
	} else {
		rx_available = 0;
	}
	if (debug && LWDAQ_DEBUG) console_print(
		"rx_available=%u in %s.\r\n", rx_available, __func__);
		
	/*
		Return the number of bytes available. This will certainly be zero or greater,
		so not an error.
	*/
	return rx_available;
}



