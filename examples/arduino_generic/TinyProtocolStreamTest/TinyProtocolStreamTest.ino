#include <TinyProtocolStream.h>


// STREAM_TERMINAL is forwarded to another micro under tinyproto, data received from tinyproto is printed here
#define STREAM_TERMINAL Serial

// STREAM_TP communicates with another micro using tinyproto
#define STREAM_TP Serial2

// optional STREAM_DEBUG can print debug messages, and can be set to STREAM_TERMINAL if you prefer, but to get full tinyproto debug messages, follow instructions in the tinyrepo README
//#define STREAM_DEBUG SerialUSB1

FdStream<64> proto(STREAM_TP); // 64 is the number of bytes per message, default 3x messages can be queued, and the receive buffer is set to default 3x number of bytes per message
//FdStream<64, 2048> proto(STREAM_TP); // if you want a larger RX buffer size than 3x number of bytes per message, add the total buffer as a second template parameter

void setup() {
  STREAM_TERMINAL.begin(115200);

  STREAM_TERMINAL.println("\r\n\n\nSTREAM_TERMINAL");

  #ifdef STREAM_DEBUG
    // TODO: does it matter if STREAM_TERMINAL.begin() is called twice?
    if((void*)&STREAM_TERMINAL != (void*)&STREAM_DEBUG)
      STREAM_DEBUG.begin(115200);
    STREAM_DEBUG.println("\r\n\n\nSTREAM_DEBUG");
  #endif

  // manually init stream so it's ready for use by tinyproto
  STREAM_TP.begin(115200);
  proto.begin();
}

void loop() {
  uint32_t loopStartTime = millis();

  // write anything received from STREAM_TERMINAL to proto
  while (STREAM_TERMINAL.available() && proto.availableForWrite()) {
    char tempchar = STREAM_TERMINAL.read();
    //STREAM_DEBUG.write(tempchar);
    int result = proto.write(tempchar);

    #ifdef STREAM_DEBUG
      if(result < 1) {
        STREAM_DEBUG.print("proto.write < 1: ");
        STREAM_DEBUG.println(result);
      }
    #endif
  }

  while (proto.available() && STREAM_TERMINAL.availableForWrite()) {
    char tempchar = proto.read();
    STREAM_TERMINAL.write(tempchar);
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
