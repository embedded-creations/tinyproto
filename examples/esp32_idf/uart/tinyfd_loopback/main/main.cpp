/*
    MIT License

    Copyright (c) 2020, Alexey Dynda

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/
/*
 * This example sends back every buffer received over UART.
 *
 * !README!
 * The sketch is developed to perform UART tests between Arduino
 * and PC.
 * 1. Burn this program to Arduino
 * 2. Compile tiny_loopback tool (see tools folder) for your system
 * 3. Connect Arduino TX and RX lines to your PC com port
 * 4. Run tiny_loopback on the PC (use correct port name on your system)
 * 5. tiny_loopback will print the test speed results
 *
 * Also, this example demonstrates how to pass data between 2 systems
 * By default the sketch and tiny_loopback works as 115200 speed.
 */

#include "hal/tiny_serial.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <TinyProtocolFd.h>
#include <stdio.h>

#define TINY_MULTITHREAD

#define BUF_SIZE (128)

/* Creating protocol object is simple. Lets define 128 bytes as maximum. *
 * size for the packet and use 7 packets in outgoing queue.             */
tinyproto::FdD proto(tiny_fd_buffer_size_by_mtu(128, 7));
tiny_serial_handle_t s_serial = TINY_SERIAL_INVALID;

void onReceive(void *udata, tinyproto::IPacket &pkt)
{
    if ( proto.write(pkt) == TINY_ERR_TIMEOUT )
    {
        // Do what you need to do if there is no place to put new frame to.
        // But never use blocking operations inside callback
    }
}

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#if defined(TINY_MULTITHREAD)
void tx_task(void *arg)
{
    for ( ;; )
    {
        proto.run_tx(
            [](void *p, const void *b, int s) -> int { return tiny_serial_send_timeout(s_serial, b, s, 100); });
    }
    vTaskDelete(NULL);
}
#endif

void main_task(void *args)
{
    s_serial = tiny_serial_open("uart1,4,5,-1,-1", 115200);

    /* Lets use 16-bit checksum as ESP32 allows that */
    proto.enableCrc16();
    /* Lets use 7 frames window for outgoing messages */
    proto.setWindowSize(7);
    /* Lets process all incoming frames */
    proto.setReceiveCallback(onReceive);
    /* Redirect all protocol communication to Serial0 UART */
    proto.begin();

#if defined(TINY_MULTITHREAD)
    xTaskCreate(tx_task, "tx_task", 2096, NULL, 1, NULL);
#endif
    for ( ;; )
    {
#if defined(TINY_MULTITHREAD)
        proto.run_rx([](void *p, void *b, int s) -> int { return tiny_serial_read_timeout(s_serial, b, s, 100); });
#else
        proto.run_rx([](void *p, void *b, int s) -> int { return tiny_serial_read_timeout(s_serial, b, s, 0); });
        proto.run_tx([](void *p, const void *b, int s) -> int { return tiny_serial_send_timeout(s_serial, b, s, 0); });
#endif
    }
    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    xTaskCreatePinnedToCore(main_task, "mainTask", 8192, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
}
