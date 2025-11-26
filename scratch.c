uint16_t TCP_WriteAll(TCP_SOCKET s, const uint8_t *buf, uint16_t len)
{
    uint16_t total = 0;
    while (total < len)
    {
        uint16_t written = TCPIP_TCP_ArrayPut(s, buf + total, len - total);
        if (written == 0)
        {
            // No space right now — must wait
            TCPIP_TCP_Flush(s);
            break;
        }
        total += written;
    }
    return total;
}

/*
	Routines to set up our parallel bus.
*/

#include <xc.h>

// This is where we want to be as fast as possible. With the above complex 
// construction, the stream read will take about 50 clock cycles, or 250 ns,
// plus however long we have to wait for the controller to respond, which 
// will be around 50 ns.
uint8_t bus_read_same_address(void) {
    ds_assert();
    // wait states
    uint8_t v = cd_read_bus();
    ds_deassert();
    return v;
}



/*
	Suppose we re-wire the A3053A so that CA0-CA7 are on RA0-RA7, /CW on RA9,
	/CDS on RA10, and we have CD0-CD7 on RE0-RE7. Here is how ChatGPT says we
	can configure these ports.
*/
// Address bus: RA0–RA7
#define CA_MASK_A      0x00FFu     // bits 0..7 on PORTA

// Data bus: RE0–RE7
#define CD_MASK_E      0x00FFu     // bits 0..7 on PORTE

// Control lines on PORTA
#define CW_BIT_A       (1u << 9)   // RA9  = /CW (active low)
#define CDS_BIT_A      (1u << 10)  // RA10 = /CDS (active low)

void bus_init(void) {
    //--- Make these pins DIGITAL (clear any analog on them)
    ANSELACLR = CA_MASK_A | CW_BIT_A | CDS_BIT_A;  // RA0–7, RA9, RA10
    ANSELECLR = CD_MASK_E;                         // RE0–7

    //--- Address bus & control lines as outputs
    TRISACLR = CA_MASK_A | CW_BIT_A | CDS_BIT_A;   // 0 = output

    //--- Data bus initially as inputs (for safety)
    TRISESET = CD_MASK_E;                          // 1 = input

    //--- De-assert active-low control lines (drive them high)
    LATASET = CW_BIT_A | CDS_BIT_A;
}

/*
TRISx bit = 1 → input, 0 → output
TRISxCLR clears bits = makes them outputs
TRISxSET sets bits = makes them inputs
ANSELxCLR makes the pins digital.
*/

static inline void cd_bus_as_outputs(void) {
    // RE0–7 as outputs
    TRISECLR = CD_MASK_E;
}

static inline void cd_bus_as_inputs(void) {
    // RE0–7 as inputs (tri-stated from your side)
    TRISESET = CD_MASK_E;
}

static inline void bus_set_address(uint8_t addr) {
    // Clear lower 8 bits then OR in the new address
    LATA = (LATA & ~CA_MASK_A) | ((uint32_t)addr & 0xFFu);
}

static inline void cw_assert(void)   { LATACLR = CW_BIT_A; }  // /CW low
static inline void cw_deassert(void) { LATASET = CW_BIT_A; }  // /CW high
static inline void cds_assert(void)   { LATACLR = CDS_BIT_A; } // /CDS low
static inline void cds_deassert(void) { LATASET = CDS_BIT_A; } // /CDS high

// Full bus write byte cycle including setting address, /CDW, and driving data lines.
void bus_write_byte(uint8_t addr, uint8_t data) {
    bus_set_address(addr);
    cw_assert();
    cd_bus_as_outputs();
    LATE = (LATE & ~CD_MASK_E) | ((uint32_t)data & 0xFFu);
    cds_assert();
    // wait states
    cds_deassert();
}

// If we want to write again to the same address, but a different value as in stream
// write, we can do:
void cd_write_bus(uint8_t data) {
    LATE = (LATE & ~CD_MASK_E) | ((uint32_t)data & 0xFFu);
    cds_assert();
    // wait states
    cds_deassert();
}

// Here is a complete read cycle.
uint8_t bus_read_byte(uint8_t addr) {
    uint8_t value;
    bus_set_address(addr);
    cd_bus_as_inputs();
    cw_deassert(); 
    cds_assert();
    // wait states
    value = (uint8_t)(PORTE & CD_MASK_E);
    cds_deassert();
    return value;
}

// The stream read takes about 10 clock cycles with the above, simpler register
// allocations.
uint8_t cd_read_bus(void) {
    cds_assert();
    // wait states
    value = (uint8_t)(PORTE & CD_MASK_E);
    cds_deassert();
    return value;
}

// Harmony Code Warning:
// ---------------------
//
// Below you will find the original TCP/IP echo server from the Harmony example
// project. It contains a serious bug. Its management of the receive buffer
// violates the range of the receive buffer. Because the receive buffer resides
// in the stack, writing past the end of the buffer overwrites a location in the
// stack. With the receive buffer declaration where it is in the code, it just
// so happens that the the location above the buffer in the stack is not vital
// to the operation of the program. But if we move the buffer declaration to the
// top of the routine, the compiler places the buffer below a vital location,
// the stack gets trashed by APP_Tasks, and the entire program crashes. So far
// as we can tell, the authors placed the buffer declaration where it is now
// during development. They probably tried to move the buffer declaration to the
// top of the procedure when they had the code working, found that the code
// crashed, could not figure out why in the time they had available, and decided
// to restore the declaration to its development location and move on to their
// next programming project.
//
// Our warning is this: assume that all of the Harmony code is written in the
// same spirit. The code works within the bounds that it was tested, but any
// change that you make to it, however trivial, even in the ordering or location
// of variable declarations, can break the code.
//
void APP_Tasks ( void )
{
    SYS_STATUS          tcpipStat;
    const char          *netName, *netBiosName;
    static IPV4_ADDR    dwLastIP[2] = { {-1}, {-1} };
    IPV4_ADDR           ipAddr;
    int                 i, nNets;
    TCPIP_NET_HANDLE    netH;

    switch(appData.state)
    {
        case APP_TCPIP_WAIT_INIT:
            tcpipStat = TCPIP_STACK_Status(sysObj.tcpip);
            if(tcpipStat < 0)
            {   // some error occurred
                SYS_CONSOLE_MESSAGE(" APP: TCP/IP stack initialization failed!\r\n");
                appData.state = APP_TCPIP_ERROR;
            }
            else if(tcpipStat == SYS_STATUS_READY)
            {
                // now that the stack is ready we can check the
                // available interfaces
                nNets = TCPIP_STACK_NumberOfNetworksGet();
                for(i = 0; i < nNets; i++)
                {

                    netH = TCPIP_STACK_IndexToNet(i);
                    netName = TCPIP_STACK_NetNameGet(netH);
                    netBiosName = TCPIP_STACK_NetBIOSName(netH);

#if defined(TCPIP_STACK_USE_NBNS)
                    SYS_CONSOLE_PRINT("    Interface %s on host %s - NBNS enabled\r\n", netName, netBiosName);
#else
                    SYS_CONSOLE_PRINT("    Interface %s on host %s - NBNS disabled\r\n", netName, netBiosName);
#endif  // defined(TCPIP_STACK_USE_NBNS)
                    (void)netName;          // avoid compiler warning 
                    (void)netBiosName;      // if SYS_CONSOLE_PRINT is null macro

                }
                appData.state = APP_TCPIP_WAIT_FOR_IP;

            }
            break;

        case APP_TCPIP_WAIT_FOR_IP:

            // if the IP address of an interface has changed
            // display the new value on the system console
            nNets = TCPIP_STACK_NumberOfNetworksGet();

            for (i = 0; i < nNets; i++)
            {
                netH = TCPIP_STACK_IndexToNet(i);
                if(!TCPIP_STACK_NetIsReady(netH))
                {
                    return;    // interface not ready yet!
                }
                ipAddr.Val = TCPIP_STACK_NetAddress(netH);
                if(dwLastIP[i].Val != ipAddr.Val)
                {
                    dwLastIP[i].Val = ipAddr.Val;

                    SYS_CONSOLE_MESSAGE(TCPIP_STACK_NetNameGet(netH));
                    SYS_CONSOLE_MESSAGE(" IP Address: ");
                    SYS_CONSOLE_PRINT("%d.%d.%d.%d \r\n", 
                    	ipAddr.v[0], ipAddr.v[1], ipAddr.v[2], ipAddr.v[3]);
                }
                appData.state = APP_TCPIP_OPENING_SERVER;
            }
            break;
            
        case APP_TCPIP_OPENING_SERVER:
        {
            SYS_CONSOLE_PRINT("Waiting for Client Connection on port: %d\r\n", SERVER_PORT);
            appData.socket = TCPIP_TCP_ServerOpen(IP_ADDRESS_TYPE_IPV4, SERVER_PORT, 0);
            if (appData.socket == INVALID_SOCKET)
            {
                SYS_CONSOLE_MESSAGE("Couldn't open server socket\r\n");
                break;
            }
            appData.state = APP_TCPIP_WAIT_FOR_CONNECTION;
        }
        break;

        case APP_TCPIP_WAIT_FOR_CONNECTION:
        {
            if (!TCPIP_TCP_IsConnected(appData.socket))
            {
                return;
            }
            else
            {
                // We got a connection
                appData.state = APP_TCPIP_SERVING_CONNECTION;
                SYS_CONSOLE_MESSAGE("Received a connection\r\n");
            }
        }
        break;

        case APP_TCPIP_SERVING_CONNECTION:
        {
            if (!TCPIP_TCP_IsConnected(appData.socket) || TCPIP_TCP_WasDisconnected(appData.socket))
            {
                appData.state = APP_TCPIP_CLOSING_CONNECTION;
                SYS_CONSOLE_MESSAGE("Connection was closed\r\n");
                break;
            }
            int16_t wMaxGet, wMaxPut, wCurrentChunk;
            uint16_t w, w2;
            uint8_t AppBuffer[32 + 1];
            // Figure out how many bytes have been received and how many we can
            // transmit.
            wMaxGet = TCPIP_TCP_GetIsReady(appData.socket);	// Get TCP RX FIFO byte count
            wMaxPut = TCPIP_TCP_PutIsReady(appData.socket);	// Get TCP TX FIFO free space

            // Make sure we don't take more bytes out of the RX FIFO than we can
           // put into the TX FIFO
            if(wMaxPut < wMaxGet)
                    wMaxGet = wMaxPut;

            // Process all bytes that we can This is implemented as a loop,
            // processing up to sizeof(AppBuffer) bytes at a time. This limits
            // memory usage while maximizing performance.  Single byte Gets and
            // Puts are a lot slower than multibyte GetArrays and PutArrays.
            wCurrentChunk = sizeof(AppBuffer) -1;
            for(w = 0; w < wMaxGet; w += sizeof(AppBuffer) - 1)
            {
                // Make sure the last chunk, which will likely be smaller than
                // sizeof(AppBuffer), is treated correctly.
                if(w + sizeof(AppBuffer) - 1 > wMaxGet)
                    wCurrentChunk = wMaxGet - w;

                // Transfer the data out of the TCP RX FIFO and into our local
                // processing buffer.
                TCPIP_TCP_ArrayGet(appData.socket, AppBuffer, wCurrentChunk);

                // Perform the "ToUpper" operation on each data byte
                for(w2 = 0; w2 < wCurrentChunk; w2++)
                {
                    i = AppBuffer[w2];
                    if(i == '\x1b')   // escape
                    {
                        appData.state = APP_TCPIP_CLOSING_CONNECTION;
                        SYS_CONSOLE_MESSAGE("Connection was closed\r\n");
                    }
                }
                AppBuffer[w2] = 0;  // end the console string properly

                // Transfer the data out of our local processing buffer and into
                // the TCP TX FIFO.
                SYS_CONSOLE_PRINT("Server Sending %s\r\n", AppBuffer);
                TCPIP_TCP_ArrayPut(appData.socket, AppBuffer, wCurrentChunk);

                // No need to perform any flush.  TCP data in TX FIFO will
                // automatically transmit itself after it accumulates for a
                // while.  If you want to decrease latency (at the expense of
                // wasting network bandwidth on TCP overhead), perform and
                // explicit flush via the TCPFlush() API.
            }
        }
        break;
        case APP_TCPIP_CLOSING_CONNECTION:
        {
            // Close the socket connection.
            TCPIP_TCP_Close(appData.socket);
            appData.socket = INVALID_SOCKET;
            appData.state = APP_TCPIP_WAIT_FOR_IP;

        }
        break;
        default:
            break;
    }
}

