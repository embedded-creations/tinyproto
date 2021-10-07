/*
  This example paired with TinyProtocolStreamLoopbackSender implements a loopback test between two Arduino devices
    looking for identical Stream data to be returned from the remote loopback over TinyProtocolStream

  If there are errors in the stream, they will be printed out by the sender sketch.  The most likely error is if this sketch's
    rx buffer size is set too low and it can't keep up with the amount of data it needs to loop back to the sender.
    If DEBUG_PRINT is defined in TinyProtocolStream.h, you'll see "onReceive(): rx_stream.write() fail!" here on the receiver side,
    while the mismatched data is detected on the sender side.

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

// 64*3 specifies the receive buffer, which may need to be increased when there are a lot of communication errors and retries lowering tinyproto's TX bandwidth
// For this paired example set this value to roughly match or exceed the sender sketch's NUM_CHARS_UNCONFIRMED value indicating the number of bytes that may be sitting in the TinyProtocolStream's RX buffer waiting for free space in tinyproto's queues
FdStream<64, 64*3> proto(STREAM_TP);

void setup() {
  STREAM_TERMINAL.begin(115200);

  STREAM_TERMINAL.println("\r\n\n\nLoopback Receiver STREAM_TERMINAL");

  #ifdef STREAM_DEBUG
    // TODO: does it matter if STREAM_TERMINAL.begin() is called twice?
    if((void*)&STREAM_TERMINAL != (void*)&STREAM_DEBUG)
      STREAM_DEBUG.begin(115200);
    STREAM_DEBUG.println("\r\n\n\nLoopback Receiver STREAM_DEBUG");
  #endif

  // manually init stream so it's ready for use by tinyproto
  STREAM_TP.begin(115200);
  proto.begin();
}

void loop() {
  static uint32_t numLoopbackChars = 0;
  static uint32_t lastStatusUpdate = 0;

  uint32_t loopStartTime = millis();

  // loopback anything received from proto
  while (proto.available() && proto.availableForWrite()) {
    proto.write(proto.read());
    numLoopbackChars++;
  }

  proto.loop();

  if(millis() - lastStatusUpdate > 1000) {
    STREAM_TERMINAL.print("numLoopbackChars: ");
    STREAM_TERMINAL.println(numLoopbackChars);
    lastStatusUpdate = millis();
  }

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
