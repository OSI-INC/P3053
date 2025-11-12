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
