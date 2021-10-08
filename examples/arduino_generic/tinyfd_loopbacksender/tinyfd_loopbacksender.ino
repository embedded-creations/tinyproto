/*
  This example paired with tinyfd_loopback implements a loopback test with error checking that can be run on two Arduinos

  Packets are filled with known incrementing values here, sent over tinyproto to the tinyfd_loopback, which immediately sends back the packet
  Once received back, the packets are checked here for matching values and any error is printed out to STREAM_TERMINAL

  You can purposely introduce errors on STREAM_TP causing corrupted messages by defining RANDOM_READ_ERRORS and RANDOM_WRITE_ERRORS below

  tinyfd_loopback doesn't implement any buffering of packets beyond the tinyproto queues, so we need to limit the bandwidth by the sender to avoid dropping packets
  DELAY_BETWEEN_SENDS can be used to limit bandwidth
  SLOTS_TO_KEEP_FREE is more effective, as it keeps slots free for resending messages when there's a error on STREAM_TP,
  and also limits the number of outgoing messages sent out to tinyfd_loopback
*/
#include <TinyProtocol.h>
// We need this hack for very small controllers.
#include <proto/fd/tiny_fd_int.h>

// STREAM_TERMINAL is used for printing status and error messages
#define STREAM_TERMINAL Serial

// STREAM_TP communicates with another micro using tinyproto
#define STREAM_TP Serial2

// optional STREAM_DEBUG can print debug messages, and can be set to STREAM_TERMINAL if you prefer, but to get full tinyproto debug messages, follow instructions in the tinyrepo README
//#define STREAM_DEBUG SerialUSB1

/* Creating protocol object is simple. Lets define 64 bytes as maximum. *
 * size for the packet and use IFD_DEFAULT_WINDOW_SIZE (3) packets in outgoing queue. */
tinyproto::Fd<FD_MIN_BUF_SIZE(64, IFD_DEFAULT_WINDOW_SIZE)>  proto;

uint16_t loopbackReceiveCounter = 0;
uint32_t numLoopbackChars = 0;
bool badCharReceived = false;

#define DELAY_BETWEEN_SENDS 0
#define DELAY_AFTER_BADCHAR 5000

#define RANDOM_READ_ERRORS 1000 // smaller number (min size 64, matching bytes per message) introduces more errors, 0 disables
#define RANDOM_WRITE_ERRORS 1000 // smaller number (min size 64, matching bytes per message) introduces more errors, 0 disables
#define SLOTS_TO_KEEP_FREE 2 // 2 slots free ensures there's enough bandwidth for retrying messages even if there are frequent random read/write errors

void onReceive(void *udata, tinyproto::IPacket &pkt)
{
  uint16_t number = pkt.getUint16();
  if(number != loopbackReceiveCounter) {
    STREAM_TERMINAL.print("loopbackReceiveCounter doesn't match: expected ");
    STREAM_TERMINAL.print(loopbackReceiveCounter);
    STREAM_TERMINAL.print(" got ");
    STREAM_TERMINAL.println(number);
    loopbackReceiveCounter = number+1;
    badCharReceived = true;
  } else {
    badCharReceived = false;
    while(pkt.getSize()) {
      char tempchar = pkt.getChar();
      if(tempchar != '0') {
        STREAM_TERMINAL.print("*** payload error ***");
        STREAM_TERMINAL.println(tempchar);
        badCharReceived = true;
        break;
      }
    }
    loopbackReceiveCounter++;
    numLoopbackChars++;
  }
}

void setup()
{
  STREAM_TERMINAL.begin(115200);
  STREAM_TERMINAL.println("\r\n\n\n\nLoopback Test Sender - STREAM_TERMINAL");

  #ifdef STREAM_DEBUG
    // TODO: does it matter if STREAM_TERMINAL.begin() is called twice?
    if((void*)&STREAM_TERMINAL != (void*)&STREAM_DEBUG)
      STREAM_DEBUG.begin(115200);
    STREAM_DEBUG.println("\r\n\n\nLoopback Test Sender - STREAM_DEBUG");
  #endif

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
  /* Redirect all protocol communication to Serial0 UART */
  proto.begin();
}

void loop()
{
  static tinyproto::Packet<64> out_packet;
  static uint16_t loopbackSendCounter = 0;
  static uint32_t lastTransmitTime = 0;
  static uint32_t lastStatusUpdate = 0;

  bool readyToSend = (!badCharReceived && (millis() - lastTransmitTime > DELAY_BETWEEN_SENDS)) ||
                      (badCharReceived && (millis() - lastTransmitTime > DELAY_AFTER_BADCHAR));

  if(readyToSend) {
    static const uint8_t seq_bits_mask = 0x07;
    tiny_fd_handle_t handle = proto.getHandle();
    uint8_t busy_slots = ((uint8_t)(handle->frames.last_ns - handle->frames.confirm_ns) & seq_bits_mask);
    // Check if space is actually available
    if(busy_slots) {
      #ifdef STREAM_DEBUG
        STREAM_DEBUG.print("busy_slots ");
        STREAM_DEBUG.print(busy_slots);
      #endif

      #if (SLOTS_TO_KEEP_FREE > 0)
        if ( (busy_slots+1) + SLOTS_TO_KEEP_FREE > handle->frames.max_i_frames ) {
          #ifdef STREAM_DEBUG
            STREAM_DEBUG.print(" waiting");
          #endif
          readyToSend = false;
        }
      #endif
      #ifdef STREAM_DEBUG
        STREAM_DEBUG.println();
      #endif
    }
  }

  // load packet with a sequence number followed by random numbers
  if( !out_packet.size()) {
    out_packet.put(loopbackSendCounter);
    loopbackSendCounter++;

    while(out_packet.size() < 64)
      out_packet.put('0');
  }

  // try sending packet if it's already loaded
  if(out_packet.size() && readyToSend) {
    int result = proto.write(out_packet);
    if(result == 0) {
      #ifdef STREAM_DEBUG
        STREAM_DEBUG.print("write: ");
        STREAM_DEBUG.println(out_packet.size());
      #endif
      out_packet.clear();
      lastTransmitTime = millis();
      readyToSend = false;
    } else {
      // keep data in packet, so we can try again

      // TINY_ERR_TIMEOUT is common as we're using 0ms timeout for write, otherwise print
      if(result != TINY_ERR_TIMEOUT)
      {
        #ifdef STREAM_DEBUG
          STREAM_DEBUG.println("readyToSend result: ");
          STREAM_DEBUG.println(result);
        #endif
      }
    }  
  }

  if (STREAM_TP.available())
  {
    proto.run_rx([](void *p, void *b, int s)->int {
      #if (!RANDOM_READ_ERRORS)
        return STREAM_TP.readBytes((uint8_t *)b, s); 
      #else
        int retval = STREAM_TP.readBytes((uint8_t *)b, s);

        unsigned long randomnum = (unsigned long)random(10000);
        if(randomnum < s) {
          #ifdef STREAM_DEBUG
            STREAM_DEBUG.println("*** RANDOM READ ERROR ***");
          #endif
          ((uint8_t*)b)[randomnum] = '\0';
        }

        return retval;
      #endif
    });
  }
  proto.run_tx([](void *p, const void *b, int s)->int {
    #if (!RANDOM_WRITE_ERRORS)
      return STREAM_TP.write((const uint8_t *)b, s);
    #else
      unsigned long randomnum = (unsigned long)random(10000);
      if(randomnum < s) {
        #ifdef STREAM_DEBUG
          STREAM_DEBUG.println("*** RANDOM WRITE ERROR ***");
        #endif
        ((uint8_t*)b)[randomnum] = '\0';
      }

      return STREAM_TP.write((const uint8_t *)b, s);
    #endif
  });

  if(millis() - lastStatusUpdate > 1000) {
    STREAM_TERMINAL.print("numLoopbackChars: ");
    STREAM_TERMINAL.println(numLoopbackChars);
    lastStatusUpdate = millis();
  }
}
