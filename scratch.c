void APP_Tasks(void)
{
    static IPV4_ADDR lastIP = { {-1} };
    IPV4_ADDR ipAddr;
    TCPIP_NET_HANDLE netH;
    SYS_STATUS tcpipStat;
    int16_t wMaxGet, wMaxPut, wCurrentChunk;
    uint16_t w, w2;
    uint8_t AppBuffer[32 + 1];
    int i;

    switch (appData.state)
    {
        case APP_TCPIP_WAIT_INIT:
            tcpipStat = TCPIP_STACK_Status(sysObj.tcpip);
            if (tcpipStat < 0)
            {
                SYS_CONSOLE_MESSAGE("APP: TCP/IP stack initialization failed!\r\n");
                appData.state = APP_TCPIP_ERROR;
            }
            else if (tcpipStat == SYS_STATUS_READY)
            {
                netH = TCPIP_STACK_IndexToNet(0);
                SYS_CONSOLE_PRINT("Interface %s on host %s ready\r\n",
                    TCPIP_STACK_NetNameGet(netH),
                    TCPIP_STACK_NetBIOSName(netH));
                appData.state = APP_TCPIP_WAIT_FOR_IP;
            }
            break;

        case APP_TCPIP_WAIT_FOR_IP:
            netH = TCPIP_STACK_IndexToNet(0);
            if (!TCPIP_STACK_NetIsReady(netH))
                return;

            ipAddr.Val = TCPIP_STACK_NetAddress(netH);
            if (lastIP.Val != ipAddr.Val)
            {
                lastIP.Val = ipAddr.Val;
                SYS_CONSOLE_PRINT("IP Address: %d.%d.%d.%d\r\n",
                    ipAddr.v[0], ipAddr.v[1], ipAddr.v[2], ipAddr.v[3]);
            }

            appData.state = APP_TCPIP_OPENING_SERVER;
            break;

        case APP_TCPIP_OPENING_SERVER:
            SYS_CONSOLE_PRINT("Waiting for client connection on port %d\r\n",
                TCPIP_SERVER_PORT);
            appData.socket = TCPIP_TCP_ServerOpen(IP_ADDRESS_TYPE_IPV4,
                TCPIP_SERVER_PORT, 0);
            if (appData.socket == INVALID_SOCKET)
            {
                SYS_CONSOLE_MESSAGE("Couldn't open server socket\r\n");
                break;
            }
            appData.state = APP_TCPIP_WAIT_FOR_CONNECTION;
            break;

        case APP_TCPIP_WAIT_FOR_CONNECTION:
            if (!TCPIP_TCP_IsConnected(appData.socket))
                return;

            SYS_CONSOLE_MESSAGE("Client connected\r\n");
            appData.state = APP_TCPIP_SERVING_CONNECTION;
            break;

        case APP_TCPIP_SERVING_CONNECTION:
            if (!TCPIP_TCP_IsConnected(appData.socket) ||
                TCPIP_TCP_WasDisconnected(appData.socket))
            {
                SYS_CONSOLE_MESSAGE("Connection closed\r\n");
                appData.state = APP_TCPIP_CLOSING_CONNECTION;
                break;
            }

            // RX/TX buffer handling
            wMaxGet = TCPIP_TCP_GetIsReady(appData.socket);
            wMaxPut = TCPIP_TCP_PutIsReady(appData.socket);
            if (wMaxPut < wMaxGet)
                wMaxGet = wMaxPut;

            wCurrentChunk = sizeof(AppBuffer) - 1;
            for (w = 0; w < wMaxGet; w += sizeof(AppBuffer) - 1)
            {
                if (w + sizeof(AppBuffer) - 1 > wMaxGet)
                    wCurrentChunk = wMaxGet - w;

                TCPIP_TCP_ArrayGet(appData.socket, AppBuffer, wCurrentChunk);

                for (w2 = 0; w2 < wCurrentChunk; w2++)
                {
                    i = AppBuffer[w2];
                    if (i == '\x1b')   // ESC closes connection
                    {
                        SYS_CONSOLE_MESSAGE("ESC received, closing connection\r\n");
                        appData.state = APP_TCPIP_CLOSING_CONNECTION;
                    }
                }

                AppBuffer[w2] = 0;
                SYS_CONSOLE_PRINT("Server echo: %s\r\n", AppBuffer);
                TCPIP_TCP_ArrayPut(appData.socket, AppBuffer, wCurrentChunk);
            }
            break;

        case APP_TCPIP_CLOSING_CONNECTION:
            TCPIP_TCP_Close(appData.socket);
            appData.socket = INVALID_SOCKET;
            appData.state = APP_TCPIP_WAIT_FOR_IP;
            break;

        default:
            break;
    }
}


#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

#include <stdarg.h>
#include <stdio.h>
#include "plib_uart2.h"

// Blocking write of one character
static inline void uart_putc(char c)
{
    // UART2_Write returns 0 if TX buffer is full
    while (UART2_Write((uint8_t*)&c, 1) == 0)
        ;
}

// Write a zero-terminated string
static inline void uart_puts(const char* s)
{
    while (*s)
        uart_putc(*s++);
}

// printf-style formatted output
static inline void uart_printf(const char* fmt, ...)
{
    char buf[256];      // Adjust size as needed
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    uart_puts(buf);
}

#endif // UART_CONSOLE_H


static const char* trim_spaces(const char* s)
{
    static char buf[17];  // NetBIOS names are 16 chars max
    int i;

    // Copy string (up to 16 chars)
    for (i = 0; i < 16 && s[i]; i++)
        buf[i] = s[i];
    buf[i] = '\0';

    // Trim trailing spaces
    for (i = strlen(buf) - 1; i >= 0 && buf[i] == ' '; i--)
        buf[i] = '\0';

    return buf;
}


// ----- SYS_CMD Adapter for UART2 ------

// Return # of characters available to read
static int cmd_uart_isRdy(const void* unused)
{
    return UART2_ReadCountGet();
}

// Read one char (required by SYS_CMD)
static char cmd_uart_getc(const void* unused)
{
    uint8_t c = 0;

    // UART2_Read returns number of bytes read
    if (UART2_Read(&c, 1) == 1)
        return (char)c;

    return 0;   // SYS_CMD treats "0" as "no new character"
}

// Write one character
static void cmd_uart_putc(const void* unused, char c)
{
    while (UART2_Write((uint8_t*)&c, 1) == 0)
        ;
}

// Simple message function
static void cmd_uart_msg(const void* unused, const char* str)
{
    console_puts(str);
}

// Formatted printf function
static void cmd_uart_print(const void* unused, const char* fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    console_puts(buf);
}

static const SYS_CMD_API cmdUartApi =
{
    .msg    = cmd_uart_msg,
    .print  = cmd_uart_print,
    .putc_t = cmd_uart_putc,
    .isRdy  = cmd_uart_isRdy,
    .getc_t = cmd_uart_getc
};

SYS_CMDIO_ADD(&cmdUartApi, NULL, 0);

