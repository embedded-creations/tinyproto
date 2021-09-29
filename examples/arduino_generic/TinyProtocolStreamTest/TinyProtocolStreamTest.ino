#include <TinyProtocolStream.h>

FdStream<64> proto(Serial2);

//uint8_t serialExtraRxBuffer[4096];

void setup() {
  Serial.begin(115200);
  //Serial.addMemoryForRead(serialExtraRxBuffer, sizeof(serialExtraRxBuffer));

  Serial.println("\r\n\n\nSerial");

  SerialUSB1.println("\r\n\n\nSerialUSB1");

  // manually init stream so it's ready for use by tinyproto
  Serial2.begin(115200);

  proto.begin();
}

void loop() {
  uint32_t loopStartTime = millis();

  // write anything received from Serial to proto
  while (Serial.available() && proto.availableForWrite()) {
    char tempchar = Serial.read();
    //SerialUSB1.write(tempchar);
    int result = proto.write(tempchar);

    if(result < 1) {
      SerialUSB1.print("proto.write < 1: ");
      SerialUSB1.println(result);
    }
  }

  while (proto.available() && Serial.availableForWrite()) {
    char tempchar = proto.read();
    Serial.write(tempchar);
  }

  proto.loop();

  uint32_t loopEndTime = millis();
  if(loopEndTime - loopStartTime > 100) {
    SerialUSB1.print("loop time: ");
    SerialUSB1.println(loopEndTime - loopStartTime);
  } else {
    //SerialUSB1.print("*");
  }
}
