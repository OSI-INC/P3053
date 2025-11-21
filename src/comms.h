/*
	comms.h declares the variables, types, and functions used in our
	communications routines.
*/

#ifndef COMMS_H
#define COMMS_H

// A structure that provides names for the states of a TCP/IP server. 
typedef enum {
    S_WAIT_STACK,
    S_WAIT_IP,
    S_OPEN_SERVER,
    S_LISTENING,
    S_CONNECTED,
    S_SERVING,
    S_CLOSE,
    S_ERROR
} SERVER_STATE;

// A structure that provides a complete description of the state, nature, and
// configurationof a TCP/IP server.
typedef struct {
    TCP_SOCKET socket;
    SERVER_STATE state;
    int port;
    const char* protocol;
} SERVER;

// Declare a connection-serving procedure type that we will use in our server
// procedure. Any library that calls our server routine must pass to it a
// connection-serving procedure that takes a SERVER structure as an argument.
typedef int (*tcpip_tasks_type)(SERVER* s);

// TCP/IP procedures.
uint32_t tcp_get_ready(TCP_SOCKET s);
uint32_t tcp_get(TCP_SOCKET s, uint8_t* buffer, uint32_t len);
uint32_t tcp_put_ready(TCP_SOCKET s);
uint32_t tcp_put(TCP_SOCKET s, const uint8_t* data, uint32_t len);
void ping_gateway(void);
void net_info(void);
void tcpip_server(SERVER* s, tcpip_tasks_type tasks);


#endif
