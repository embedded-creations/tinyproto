#pragma once

#include "TinyProtocol.h"

// We need this hack for very small controllers.
#include <proto/fd/tiny_fd_int.h>

// TODO: incorporate modified version into this library?
#include <LoopbackStream.h>

#if (defined(ESP8266) || defined(ESP32))
  #define DEBUG_PRINT(...) Serial1.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial1.println(__VA_ARGS__)
  //#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
  //#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...) SerialUSB1.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) SerialUSB1.println(__VA_ARGS__)
#endif

typedef void (*MidUpdateCallbackTp)(void);

template <int S> class FdStream: public tinyproto::IFd, public Stream
{
public:
    FdStream(Stream &stream)
        : tinyproto::IFd(m_data, FD_MIN_BUF_SIZE(S,3)), // TODO: #define 3
          Stream(),
          proto_stream(&stream),
          rx_stream(S*3),
          readyToSend(false),
          write_timeout(0),
          midUpdateCallback(NULL)
    {
    }

    void begin() {
      setUserData(this);
      setReceiveCallback( onReceive );

      /* No timeout, since we want non-blocking Stream operations. */
      proto_stream->setTimeout(0);

      /* Lets use 8-bit checksum, available on all platforms */
      enableCheckSum(); // TODO: upgrade to CRC?

      tinyproto::IFd::begin();

#if 1
      mtu = S;
#else
      // not necessary now that we control m_window and set the buffer sizes, but maybe useful later if we make it dynamic?
      tiny_fd_handle_t protohandle = getHandle();
      tiny_frames_info_t frames = protohandle->frames;
      mtu = frames.mtu;

      //SerialUSB1.print("mtu: ");
      //SerialUSB1.println(mtu);
      //SerialUSB1.print("out_packet.maxSize(): ");
      //SerialUSB1.println(out_packet.maxSize());

      mtu = min(mtu, out_packet.maxSize());

      //SerialUSB1.print("min(mtu,maxSize): ");
      //SerialUSB1.println(mtu);
#endif
    }

    size_t write(uint8_t v) {
      uint32_t writeStart = millis();

#if 0
      if(out_packet.size() >= mtu)
        return 0;
#else
      while(out_packet.size() >= mtu && (millis() - writeStart < write_timeout2)) {
        loop();
      }
      if(out_packet.size() >= mtu)
        return 0;
#endif
      out_packet.put(v);

      if(out_packet.size() == 1) {
        packet_start_time = millis();
      }

      return 1;
    }
    
    static void onReceive(void *udata, tinyproto::IPacket &pkt) {
      ((FdStream<S>*)udata)->onReceive(pkt);
    }

    void onReceive(tinyproto::IPacket &pkt) {
      for(size_t i=0; i<pkt.size(); i++) {
        // TODO: handle overflow
        rx_stream.write(pkt.getByte());
      }
    }

    static int readFunc(void *pdata, void *buffer, int size) {
      return ((FdStream<S>*)pdata)->readFunc(buffer, size);
    }

    int readFunc(void *buffer, int size) {
      return proto_stream->readBytes((uint8_t *)buffer, size);
    }

    static int writeFunc(void *pdata, const void *buffer, int size) {
      return ((FdStream<S>*)pdata)->writeFunc(buffer, size);
    }

    int writeFunc(const void *buffer, int size) {
      return proto_stream->write((const uint8_t *)buffer, size);
    }

    // this is ambiguous with IFd::write(const char *buf, int size), we must use the Arduino::Print version
    size_t write(const uint8_t *buffer, size_t size) {
      uint32_t writeStart = millis();
      size_t count = 0;
      for(size_t i=0; i<size; i++) {
        if(write(buffer[i]) != 1)
          break;

        count++;

        if(millis() - writeStart > write_timeout2)
          break;
      }
      return count;
    }

    // we provide an alternate method name to access for IFd::write(const char *buf, int size)
    int write_packet(const char *buf, int size) { return tinyproto::IFd::write(buf, size); }

    int availableForWrite() { return mtu - out_packet.size(); }

    int available() { return rx_stream.available(); }

    int read() { return rx_stream.read(); }

    void flush() { readyToSend = true; }

    int peek() { return rx_stream.peek(); }

    void loop() {
      callMidUpdateCallback();

      if (proto_stream->available())
      {
          run_rx(readFunc);
      }
      run_tx(writeFunc);

      // TODO: handle error
      if(out_packet.size() > mtu) {

      }

      if(out_packet.size() == mtu)
        readyToSend = true;

      // if there's data to send, and we've been idle long enough, send the data
      if(out_packet.size() && (millis() - packet_start_time >= write_timeout))
        readyToSend = true;

      // we might get readyToSend == true on an empty packet if flush() is called
      if(!out_packet.size())
        readyToSend = false;

      if(readyToSend && getStatus() == TINY_SUCCESS) {
        int result = tinyproto::IFd::write(out_packet);
        if(result == 0) {
          DEBUG_PRINT("write: ");
          DEBUG_PRINTLN(out_packet.size());
          out_packet.clear();
          readyToSend = false;
        } else {
          // keep readyToSend, so it will try again

          // TINY_ERR_TIMEOUT is common as we're using 0ms timeout for write, otherwise print
          if(result != TINY_ERR_TIMEOUT) {
            DEBUG_PRINT("readyToSend result: ");
            DEBUG_PRINTLN(result);
          }
        }
      }
    }

    void setWriteTimeout(uint32_t timeout) { write_timeout = timeout; }

    void addMidUpdateCallback(MidUpdateCallbackTp handler) {
      midUpdateCallback = handler;
    }

    void callMidUpdateCallback() {
      if(midUpdateCallback) midUpdateCallback();
    }

private:
    TINY_ALIGNED_STRUCT uint8_t m_data[FD_MIN_BUF_SIZE(S,3)]{}; // TODO: #define 3
    Stream * proto_stream;
    LoopbackStream rx_stream;
    tinyproto::Packet<S> out_packet;
    uint32_t packet_start_time;
    bool readyToSend;
    size_t mtu;
    uint32_t write_timeout;
    uint32_t write_timeout2 = 1000; // TODO: rename and set value appropriately
    MidUpdateCallbackTp midUpdateCallback;
};
