/*
	lwdaq.c defines functions used by our LWDAQ Relay to provide a LWDAQ Server.
	It does not include the routines used by LWDAQ Relay to communicate with the
	LWDAQ Controller. Those routines are in our pic.c file.

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
#define CONFIG_LENGTH 1023
#define LWDAQ_DEBUG 0

// The LWDAQ server structure.
tcpip_server_type lwdaq_server = 
	{INVALID_SOCKET, S_WAIT_STACK, LWDAQ_PORT, "LWDAQ"};
	
/*
	lwdaq_header sends a data return message header through a socket.
*/
int lwdaq_header(tcpip_server_type* s, uint32_t id, uint32_t len) {
	uint8_t buff[15];
	uint32_t* lp;
	
	buff[START_OFFSET] = START_CODE;
	lp = (uint32_t*)&buff[ID_OFFSET];
	*lp = flip_bytes_u32(id);
	lp = (uint32_t*)&buff[CLEN_OFFSET];
	*lp = flip_bytes_u32(len);
	tcp_write_all((*s).socket, buff, CONTENT_OFFSET);
	return 0;
}

/*
	lwdaq_footer sends a terminating sequence through a socket.
*/
int lwdaq_footer(tcpip_server_type* s) {
	uint8_t buff[15];
	buff[0] = END_CODE;
	tcp_write_all((*s).socket, buff, 1);
	return 0;
}

/*
	lwdaq_byte_return sends a data return message through a socket with a single-byte
	integer as its content.
*/
int lwdaq_byte_return(tcpip_server_type* s, uint8_t data) {
	uint8_t buff[15];
	lwdaq_header(s, DATA_RETURN, sizeof(data));
	buff[0] = data;
	tcp_write_all((*s).socket, buff, sizeof(data));
	lwdaq_footer(s);
	return 0;
}

/*
	lwdaq_integer_return sends a data return message through a socket with a four-byte
	integer as its content.
*/
int lwdaq_integer_return(tcpip_server_type* s, uint32_t data) {
	uint8_t buff[15];
	uint32_t* lp;
	lwdaq_header(s, DATA_RETURN, sizeof(data));
	lp = (uint32_t*)&buff[0];
	*lp = flip_bytes_u32(data);
	tcp_write_all((*s).socket, buff, sizeof(data));
	lwdaq_footer(s);
	return 0;
}

/*
	lwdaq_data_return sends a block of data through a socket.
*/
int lwdaq_data_return(tcpip_server_type* s, uint8_t* block, uint32_t len) {
	lwdaq_header(s, DATA_RETURN, len);
	tcp_write_all((*s).socket, block, len);
	lwdaq_footer(s);
	return 0;
}

/*
	lwdaq_process_message handles an incoming LWDAQ message.
*/
int lwdaq_process_message (tcpip_server_type* s, 
		uint32_t id, uint32_t len, uint8_t* content) {
	uint8_t register_addr = 0;
	uint8_t value = 0;
	uint32_t tx_len = 0;
	static uint8_t tx_buffer[TCP_TX_BUFF_SIZE];
	uint32_t i = 0;
	static int logged_in = 0;
	static int security_level = 0;
	static char password[32] = "LWDAQ";
	static char configuration[CONFIG_LENGTH];

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
				if (socket_tick((*s).socket)<0) return -1;
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
						if (socket_tick((*s).socket)<0) return -1;
					}
					tx_buffer[i] = lwdaq_byte_read(register_addr);
					i++;
					if ((i == sizeof(tx_buffer)) || (j == tx_len - 1)) {
						tcp_write_all((*s).socket, &tx_buffer[0], i);
						if (socket_tick((*s).socket) < 0) {
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
				pic_config_read(configuration, sizeof(configuration));
			  	lwdaq_data_return(s, (uint8_t*) configuration, strlen(configuration));
				if (debug) console_print(
					"Transmitted configuration of %d characters.\r\n",
					strlen(configuration));
			} else {
				if (debug) console_print("Rejected: not logged in.\r\n");
				return -1;
			}
		}
		break;

		case CONFIG_WRITE:{
			if (debug) console_print("CONFIG_WRITE in %s.\r\n",__func__);
			if ((logged_in==1) || (security_level==0)) {
				if (len<CONFIG_LENGTH) {
					if (debug) console_print("Accepted: config %d characters.\r\n",len);
					content[len]=0x00;
					console_print("%s",(char*) content);
					pic_config_write((char*) content);
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
	lwdaq_tasks services a LWDAQ connection.
*/
int lwdaq_tasks(tcpip_server_type* s) {
	uint32_t id, len;
	static uint8_t rx_buffer[TCP_RX_BUFF_SIZE];
	static uint32_t rx_available = 0;
	uint32_t rx_ready = 0;
	int status;

	if ((*s).state == S_LISTENING) {
		rx_available = 0;
		if (debug) console_print("Initialized %s connection in %s.\r\n",
			(*s).protocol, __func__);
		return 0;
	};

	rx_ready = tcp_available((*s).socket);
	if (rx_ready>0) {
		tcp_read((*s).socket, &rx_buffer[rx_available], rx_ready);	
		rx_available = rx_available+rx_ready;
	}
	if (rx_available == 0) return 0;
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
	if (rx_available<9) return rx_available;		
	id = reverse_load_u32(&rx_buffer[1]);
	len = reverse_load_u32(&rx_buffer[5]);
	if (debug && LWDAQ_DEBUG) console_print(
		"id=%u len=%u in %s.\r\n", id, len, __func__);
	if (rx_available<len+10) return rx_available;
	if (rx_buffer[len+9] != END_CODE) {
		if (debug) console_print("Invalid end code in %s.\r\n", __func__);
		return -1;
	}
	status = lwdaq_process_message(s, id, len, &rx_buffer[9]);
	if (status<0) return status;
	if (rx_available>len+10) {
		if (debug && LWDAQ_DEBUG) console_print(
			"Copying %u to %u from %u in %s.\r\n",
			rx_available-len-10, 0, len+10, __func__);
		memmove(&rx_buffer[0], &rx_buffer[len+10], rx_available-len-10);
		rx_available = rx_available-len-10;
	} else {
		rx_available = 0;
	}
	if (debug && LWDAQ_DEBUG) console_print(
		"rx_available=%u in %s.\r\n", rx_available, __func__);
	return rx_available;
}



