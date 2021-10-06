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

#define SLOTS_TO_KEEP_FREE 2
#define RANDOM_READ_ERRORS 1000 // smaller number (min size S) introduces more errors, 0 disables
#define RANDOM_WRITE_ERRORS 1000 // smaller number (min size S) introduces more errors, 0 disables

#define DEFAULT_NUM_RETRIES 5 // tinyproto default of 2 is too small when there's a lot of errors

typedef void (*MidUpdateCallbackTp)(void);

template <int S, int rxBufferSize = S*IFD_DEFAULT_WINDOW_SIZE> class FdStream: public tinyproto::IFd, public Stream
{
public:
    FdStream(Stream &stream)
        : tinyproto::IFd(m_data, FD_MIN_BUF_SIZE(S, IFD_DEFAULT_WINDOW_SIZE)),
          Stream(),
          proto_stream(&stream),
          rx_stream(min(rxBufferSize, S*IFD_DEFAULT_WINDOW_SIZE)),
          readyToSend(false),
          write_timeout(0),
          midUpdateCallback(NULL)
    {
    }

    void begin(uint8_t numRetries = DEFAULT_NUM_RETRIES, uint16_t retryTimeout = IFD_DEFAULT_RETRY_TIMEOUT) {
      setUserData(this);
      setReceiveCallback( onReceive );

      /* No timeout, since we want non-blocking Stream operations. */
      proto_stream->setTimeout(0);

      // Crc16 is much better than checksum at finding errors
      enableCrc16();

      tinyproto::IFd::begin(numRetries, retryTimeout);

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
        if(rx_stream.write(pkt.getByte()) < 1)
          DEBUG_PRINTLN("rx_stream.write() fail!");
      }
    }

    static int readFunc(void *pdata, void *buffer, int size) {
      return ((FdStream<S>*)pdata)->readFunc(buffer, size);
    }

    int readFunc(void *buffer, int size) {
#if (!RANDOM_READ_ERRORS)
      return proto_stream->readBytes((uint8_t *)buffer, size);
#else
      int retval = proto_stream->readBytes((uint8_t *)buffer, size);
      long randomnum = random(RANDOM_READ_ERRORS);
      if(!randomnum && retval > 0) {
        DEBUG_PRINTLN("*** RANDOM READ ERROR ***");
        return retval-1;
      } else {
        return retval;
      }
#endif      
    }

    static int writeFunc(void *pdata, const void *buffer, int size) {
      return ((FdStream<S>*)pdata)->writeFunc(buffer, size);
    }

    int writeFunc(const void *buffer, int size) {
#if (!RANDOM_WRITE_ERRORS)
      return proto_stream->write((const uint8_t *)buffer, size);
#else
      long randomnum = random(RANDOM_WRITE_ERRORS);
      if(!randomnum && size > 0) {
        DEBUG_PRINTLN("*** RANDOM WRITE ERROR ***");
        size--;
      }

      return proto_stream->write((const uint8_t *)buffer, size);
#endif      
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

      // if we fill up the tinyproto transmit buffer, packets get dropped, keep some free
      if(readyToSend) {
          static const uint8_t seq_bits_mask = 0x07;
          tiny_fd_handle_t handle = getHandle();
          uint8_t busy_slots = ((uint8_t)(handle->frames.last_ns - handle->frames.confirm_ns) & seq_bits_mask);
          if(busy_slots) {
              //DEBUG_PRINT("busy_slots ");
              //DEBUG_PRINT(busy_slots);
              if ( (busy_slots+1) + SLOTS_TO_KEEP_FREE > handle->frames.max_i_frames ) {
                  //DEBUG_PRINT(" waiting ");
                  readyToSend = false;
              }
              //DEBUG_PRINTLN();
          }
      }

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
    TINY_ALIGNED_STRUCT uint8_t m_data[FD_MIN_BUF_SIZE(S, IFD_DEFAULT_WINDOW_SIZE)]{};
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
