/*
  This example paired with TinyProtocolStreamLoopbackReceiver implements a loopback test between two Arduino devices
    looking for identical Stream data to be returned from the remote loopback over TinyProtocolStream

  If there are errors in the stream, they will be printed out here.  The most likely error is if the TinyProtocolStreamLoopbackReceiver
    rx buffer size is set too low and it can't keep up with the amount of data it needs to loop back to this sketch.
    If DEBUG_PRINT is defined in TinyProtocolStream.h, you'll see "onReceive(): rx_stream.write() fail!" on the receiver side,
    while the mismatched data is detected here on the sender side.

  Follow the instructions in the main README to set RANDOM_READ_ERRORS and RANDOM_WRITE_ERRORS to artificially create errors in STREAM_TP,
    which should be handled by the error checking and retransmissions in tinyproto
*/

#include <TinyProtocolStream.h>

// STREAM_TERMINAL is forwarded to another micro under tinyproto, data received from tinyproto is printed here
#define STREAM_TERMINAL Serial

// STREAM_TP communicates with another micro using tinyproto
#define STREAM_TP Serial2

// optional STREAM_DEBUG can print debug messages, and can be set to STREAM_TERMINAL if you prefer, but to get full tinyproto debug messages, follow instructions in the tinyrepo README
//#define STREAM_DEBUG SerialUSB1

FdStream<64> proto(STREAM_TP);

// The LoopbackReceiver needs to have a receive buffer capable of storing all the characters received from tinyproto while it's waiting for free space for sending messages via tinyproto
// Set this value to roughly match the remote receive buffer's size or lower, so that not too many bytes are queued up on the receiver side
#define NUM_CHARS_UNCONFIRMED (64*3)

void setup() {
  STREAM_TERMINAL.begin(115200);

  STREAM_TERMINAL.println("\r\n\n\nLoopback Sender STREAM_TERMINAL");

  #ifdef STREAM_DEBUG
    // TODO: does it matter if STREAM_TERMINAL.begin() is called twice?
    if((void*)&STREAM_TERMINAL != (void*)&STREAM_DEBUG)
      STREAM_DEBUG.begin(115200);
    STREAM_DEBUG.println("\r\n\n\nLoopback Sender STREAM_DEBUG");
  #endif

  // manually init stream so it's ready for use by tinyproto
  STREAM_TP.begin(115200);
  proto.begin();
}

void loop() {
  static uint16_t loopbackSendCounter = 0;
  static uint16_t loopbackReceiveCounter = 0;
  static uint32_t numLoopbackDigits = 0;
  static uint32_t lastTransmitTime = 0;
  static uint32_t lastStatusUpdate = 0;
  static bool badCharReceived = false;

  uint32_t loopStartTime = millis();

  if(!badCharReceived || millis() - lastTransmitTime > 1000) {
    while(proto.availableForWrite() >= 6 && ((loopbackSendCounter - loopbackReceiveCounter) < NUM_CHARS_UNCONFIRMED/6)) {
      char printbuf[7];
      sprintf(printbuf, "%05d,", loopbackSendCounter);
      proto.print(printbuf);

#if 0 && defined(STREAM_DEBUG)
      STREAM_DEBUG.print("printbuf: ");
      STREAM_DEBUG.println(printbuf);
#endif

      loopbackSendCounter++;
    }

    //badCharReceived = false;
    lastTransmitTime = millis();
  }

  if(millis() - lastStatusUpdate > 1000) {
    STREAM_TERMINAL.print("numLoopbackDigits: ");
    STREAM_TERMINAL.println(numLoopbackDigits);
    lastStatusUpdate = millis();
  }

  while (proto.available()) {
    static int32_t readNumber = -1;
    int result = proto.read();

    if(result < 0) {
      STREAM_TERMINAL.println("proto.read() < 0");
      badCharReceived = true;
    }

    if(result >= '0' && result <= '9') {
      if(readNumber < 0)
        readNumber = 0;
      readNumber *= 10;
      readNumber += result-'0';
      if(readNumber > 65535) {
        STREAM_TERMINAL.println("number larger than expected");
        badCharReceived = true;
        readNumber = -1;
      }
    }
    if(result == ',') {
      if(readNumber >= 0) {
        if(readNumber != loopbackReceiveCounter) {
          //while(STREAM_TERMINAL.availableForWrite() < 20) { }
          STREAM_TERMINAL.print("loopbackReceiveCounter doesn't match: expected ");
          STREAM_TERMINAL.print(loopbackReceiveCounter);
          STREAM_TERMINAL.print(" got ");
          STREAM_TERMINAL.println(readNumber);
          loopbackReceiveCounter = readNumber+1;
          badCharReceived = true;
        } else {
          loopbackReceiveCounter++;
          numLoopbackDigits++;
          badCharReceived = false;
        }
      } else {
        if(!badCharReceived) {
          STREAM_TERMINAL.println("got unexpected comma");
          badCharReceived = true;          
        } else {
          // synced up 
          badCharReceived = false;
        }
      }

      readNumber = -1;
    }
  }

  proto.loop();

  #ifdef STREAM_DEBUG
    uint32_t loopEndTime = millis();
    if(loopEndTime - loopStartTime > 100) {
      STREAM_DEBUG.print("loop time: ");
      STREAM_DEBUG.println(loopEndTime - loopStartTime);
    } else {
      //STREAM_DEBUG.print("*");
    }
  #endif
}
