/*
 * This example sends back every buffer received over UART.
 *
 * Note the default error check is set to CRC16 not Checksum,
 * make sure this setting matches your loopback sender program
 *
 * !README!
 * The sketch is developed to perform UART tests between Arduino
 * and PC, or Arduino and Arduino using tinyfd_loopbacksender
 *
 * To test with a PC:
 * 1. Burn this program to Arduino
 * 2. Compile tiny_loopback tool (see tools folder) for your system
 * 3. Connect Arduino TX and RX lines to your PC com port
 * 4. Run tiny_loopback on the PC (use correct port name on your system)
 * 5. tiny_loopback will print the test speed results
 *
 * Also, this example demonstrates how to pass data between 2 systems
 * By default the sketch and tiny_loopback works as 115200 speed.
 */
#include <TinyProtocol.h>
// We need this hack for very small controllers.
#include <proto/fd/tiny_fd_int.h>

// STREAM_TERMINAL is used for printing status and error messages
#define STREAM_TERMINAL Serial

// STREAM_TP communicates with another micro using tinyproto
#define STREAM_TP Serial2

#define DEBUG_PRINT(...) SerialUSB1.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) SerialUSB1.println(__VA_ARGS__)

/* Creating protocol object is simple. Lets define 64 bytes as maximum. *
 * size for the packet and use IFD_DEFAULT_WINDOW_SIZE (3) packets in outgoing queue. */
tinyproto::Fd<FD_MIN_BUF_SIZE(64, IFD_DEFAULT_WINDOW_SIZE)>  proto;

uint32_t numLoopbackPackets = 0;

void onReceive(void *udata, tinyproto::IPacket &pkt)
{
    numLoopbackPackets++;

    if ( proto.write(pkt) == TINY_ERR_TIMEOUT )
    {
        // Do what you need to do if there is no place to put new frame to.
        // But never use blocking operations inside callback

        // if we got here, we had to drop a received packet as we couldn't immediately add it to the outgoing queue
        STREAM_TERMINAL.println("TINY_ERR_TIMEOUT");
    }
}

void setup()
{
    STREAM_TERMINAL.begin(115200);
    STREAM_TERMINAL.println("\r\n\n\n\nLoopback Test Receiver - STREAM_TERMINAL");
    /* No timeout, since we want non-blocking UART operations. */
    STREAM_TP.setTimeout(0);
    /* Initialize serial protocol for test purposes */
    STREAM_TP.begin(115200);
    /* We could use the 8-bit checksum, available on all platforms */
    //proto.enableCheckSum();
    /* We instead use the CRC16 check which detects more errors */
    proto.enableCrc16();
    /* Lets process all incoming frames */
    proto.setReceiveCallback( onReceive );
    /* Redirect all protocol communication to STREAM_TP */
    proto.begin();
}

void loop()
{
    static uint32_t lastStatusUpdate = 0;

    if (STREAM_TP.available())
    {
        proto.run_rx([](void *p, void *b, int s)->int { return STREAM_TP.readBytes((char *)b, s); });
    }
    proto.run_tx([](void *p, const void *b, int s)->int { return STREAM_TP.write((const char *)b, s); });

    if(millis() - lastStatusUpdate > 1000) {
      Serial.print("numLoopbackPackets: ");
      Serial.println(numLoopbackPackets);
      lastStatusUpdate = millis();
    }
}
