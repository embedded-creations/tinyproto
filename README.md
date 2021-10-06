## TinyProtocolStream Fork of tinyproto

This fork of tinyproto adds a new TinyProtocolStream class that encapsulates the (in my opinion) difficult to use tinyproto full duplex communication class (with error checking and retries) into an easy to use and familiar Arduino Stream class.  This has been tested by artificially introducing frequent communication errors with a loopback test (example code coming soon) and with a real world project and in both cases the Stream data comes through without errors.

I used the ESP32 and Teensy 4.1 for testing (Teensy 4.1 <-> Teensy 4.1, and Teensy 4.1 <-> ESP32)

I'm happy for this code to be merged into tinyproto if there's [interest by lexus2k](https://github.com/lexus2k/tinyproto/issues/24)

### Getting Started

Check out the `examples/arduino_generic/TinyProtocolStreamTest/` example

### Enabling Debug Messages

- `src/proto/fd/tiny_fd.c`
    - Set `TINY_FD_DEBUG` to 1
- `src/hal/tiny_debug.h`
    - Set `TINY_LOG_LEVEL_DEFAULT` to a level _one greater than_ the log level you want to see printed
    - Set `TINY_DEBUG` to 1
- `src/TinyProtocolStream.h`
    - change `DEBUG_PRINT*` macros
- To change serial port on Teensy:
    - `src/hal/arduino_hal.h`
    - Update the serial write function in `_write()`
- To change serial port on ESP32:
    - Add a line like this (which sets output to Serial port 1) to each thread that calls tinyproto:
    - `stderr = fopen("/dev/uart/1", "w");`

### Note on Teensy Debug Messages

tinypico uses `fprintf(stderr)` for printing debug messages, but this seems to be broken on the Teensy.  `%08lu` or even `%8lu` results in `0`'s being printed instead of the value.` %lu` doesn't work either as part of a longer string, the value is printed but nothing else following it.  This fork uses `printf()` for the Teensy instead


### Introducing Errors for testing robustness

- set `RANDOM_READ_ERRORS` and/or `RANDOM_WRITE_ERRORS` to e.g. 1000 in TinyProtocolStream.h, and edit the `DEBUG_PRINT*` macros so you can see when errors were introduced

------

![Tiny Protocol](.travis/tinylogo.svg)<br>
[![Build Status](https://circleci.com/gh/lexus2k/tinyproto.svg?style=svg)](https://circleci.com/gh/lexus2k/tinyproto)
[![Coverage Status](https://coveralls.io/repos/github/lexus2k/tinyproto/badge.svg?branch=master)](https://coveralls.io/github/lexus2k/tinyproto?branch=master)
[![Documentation](https://codedocs.xyz/lexus2k/tinyproto.svg)](https://codedocs.xyz/lexus2k/tinyproto/)

[tocstart]: # (toc start)

  * [Introduction](#introduction)
  * [Key Features](#key-features)
  * [Supported platforms](#supported-platforms)
  * [Easy to use](#easy-to-use)
    * [Cpp](#cpp)
    * [Python](#python)
  * [Setting up](#setting-up)
  * [How to buid](#how-to-build)
  * [Using tiny_loopback tool](#using-tiny_loopback-tool)
  * [License](#license)

[tocend]: # (toc end)

## Introduction

If you want to get stable code, please, refer to [stable branch](https://github.com/lexus2k/tinyproto/tree/stable).
HD (half duplex) protocol is removed from this version. If you need it, please refer to stable branch.

Tiny Protocol is layer 2 protocol. It is intended to be used for the systems with low resources.
It is also can be compiled for desktop Linux system, and it can be built it for Windows.
Using this library you can easy implement data transfer between 2 microcontrollers or between microcontroller and pc via UART, SPI,
I2C or any other communication channels.
You don't need to think about data synchronization between points. The library use no dynamic allocation of memory.
TinyProto is based on RFC 1662, it implements the following frames:
 * U-frames (SABM, UA)
 * S-frames (REJ, RR)
 * I-frames

## Key Features

Main features:
 * Hot plug/unplug support
 * Connection autorecover for Full duplex and Light protocols (with enabled crc)
 * Platform independent hdlc framing implementation (hdlc low level API: hdlc_ll_xxxx)
 * High level hdlc implementation for backward compatibility with previous releases (hdlc_xxxx API)
 * Easy to use Light protcol (tiny_light_xxxx API, see examples)
 * Full-duplex protocol (tiny_fd_xxxx true RFC 1662 implementation, supports confirmation, frames retransmissions)
 * Error detection (low level, high level hdlc and full duplex (fd) protocols)
   * Simple 8-bit checksum (sum of bytes)
   * FCS16 (CCITT-16)
   * FCS32 (CCITT-32)
 * Frames of maximum 32K or 2G size (payload limit depends on platform).
 * Low SRAM consumption (starts at 50 bytes).
 * Low Flash consumption (starts at 1KiB, features can be disabled and enabled at compilation time)
 * No dynamic memory allocation
 * Special serial loopback tool for debug purposes and performance testing

## Supported platforms

 * Any platform, where C/C++ compiler is available (C99, C++11)

| **Platform** | **Examples** |
| :-------- |:---------|
| ESP32 | [IDF](examples/esp32_idf) |
| Cortex M0 | [Zero](examples/arduino_zero_m0) |
| Linux | [Linux](examples/linux/loopback) |
| Windows | [Win32](examples/win32/loopback) |
| Other | Any platform with implemented HAL |

### What if my platform is not yet supported?

 That's not a problem. Just implement abstraction layer for your platform (timing and mutex functions).
Refer to `tiny_hal_init()` function. To understand HAL implementation refer to
[Linux](https://github.com/lexus2k/tinyproto/blob/master/src/hal/linux/linux_hal.inl) and
[ESP32](https://github.com/lexus2k/tinyproto/blob/master/src/hal/esp32/esp32_hal.inl) examples in
 [HAL abstraction layer](https://github.com/lexus2k/tinyproto/tree/master/src/hal).
Do not forget to add TINY_CUSTOM_PLATFORM define to your compilation flags. You may use template code
[platform_hal.c](tools/hal_template_functions/platform_hal.c)

## Easy to use

### Cpp

Usage of light Tiny Protocol in C++ can look like this:
```.cpp
#include "TinyProtocol.h"

tinyproto::Light  proto;
tinyproto::Packet<256> packet;

void setup() {
    ...
    proto.beginToSerial();
}

void loop() {
    if (Serial.available()) {
        int len = proto.read( packet );
        if (len > 0) {
            /* Send message back */
            proto.write( packet );
        }
    }
}
```

Example of using full duplex Tiny Protocol in C++ is a little bit bigger, but it is still simple:
```.cpp
#include "TinyProtocol.h"

tinyproto::Fd<FD_MIN_BUF_SIZE(64,4)>  proto;

void onReceive(void *udata, tinyproto::IPacket &pkt) {
    // Process message here, you can do with the message, what you need
    // Let's send it back to the sender ;)
    if ( proto.write(pkt) == TINY_ERR_TIMEOUT ) {
        // Do what you need to do if looping back failed on timeout.
        // But never use blocking operations inside callback
    }
}

void setup() {
    ...
    // Here we say FD protocol object, which callback to call once new msg is received
    proto.setReceiveCallback( onReceive );
    proto.begin();
}

void loop() {
    if (Serial.available()) {
        uint8_t byte = Serial.read();
        proto.run_rx( &byte, 1 ); // run FD protocol parser to process data received from the channel
    }
    uint8_t byte;
    if ( proto.run_tx( &byte, 1 ) == 1 ) // FD protocol fills buffer with data, we need to send to the channel
    {
        while ( Serial.write( byte ) == 0 ); // Just send the data
    }
}
```

### Python

```.py
import tinyproto

p = tinyproto.Hdlc()
def on_read(a):
    print("Received bytes: " + ','.join( [ "{:#x}".format(x) for x in a ] ) )

# setup protocol
p.on_read = on_read
p.begin()

# provide rx bytes to the protocol, obtained from hardware rx channel
p.rx( bytearray([ 0x7E, 0xFF, 0x3F, 0xF3, 0x39, 0x7E  ]) )
```

## How to build

### Linux
```.txt
make
# === OR ===
mkdir build
cd build
cmake -DEXAMPLES=ON ..
make
```

### Windows
```.txt
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -DEXAMPLES=ON ..
```

### ESP32
Just place the library to your project components folder.

## Setting up

 * Arduino Option 1 (with docs and tools)
   * Download source from https://github.com/lexus2k/tinyproto
   * Put the downloaded library content to Arduino/libraries/tinyproto folder
   * Restart the Arduino IDE
   * You will find the examples in the Arduino IDE under File->Examples->tinyproto

 * Arduino Option 2 (only library without docs)
   * Go to Arduino Library manager
   * Find and install tinyproto library
   * Restart the Arduino IDE
   * You will find the examples in the Arduino IDE under File->Examples->tinyproto

 * ESP32 IDF
   * Download sources from https://github.com/lexus2k/tinyproto and put to components
     folder of your project
   * Run `make` for your project

 * Linux
   * Download sources from https://github.com/lexus2k/tinyproto
   * Run `make` command from tinyproto folder, and it will build library and tools for you

 * Plain AVR
   * Download sources from https://github.com/lexus2k/tinyproto
   * Install avr gcc compilers
   * Run `make ARCH=avr`

 * Python
   * Download sources from https://github.com/lexus2k/tinyproto
   * Run `python setup.py install`

## Using tiny_loopback tool

 * Connect your Arduino board to PC
 * Run your sketch or tinylight_loopback
 * Compile tiny_loopback tool
 * Run tiny_loopback tool: `./bld/tiny_loopback -p /dev/ttyUSB0 -t light -g -c 8 -a -r`

 * Connect your Arduino board to PC
 * Run your sketch or tinyfd_loopback
 * Compile tiny_loopback tool
 * Run tiny_loopback tool: `./bld/tiny_loopback -p /dev/ttyUSB0 -t fd -c 8 -w 3 -g -a -r`

For more information about this library, please, visit https://github.com/lexus2k/tinyproto.
Doxygen documentation can be found at [Codedocs xyz site](https://codedocs.xyz/lexus2k/tinyproto).
If you found any problem or have any idea, please, report to Issues section.

## License

Copyright 2016-2021 (C) Alexey Dynda

This file is part of Tiny Protocol Library.

Protocol Library is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Protocol Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Protocol Library.  If not, see <http://www.gnu.org/licenses/>.

