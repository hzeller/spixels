// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
// SPI Pixels - Control SPI LED strips (spixels)
// Copyright (C) 2016 Henner Zeller <h.zeller@acm.org>
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef SPIXELS_MULTI_SPI_H
#define SPIXELS_MULTI_SPI_H

#include <stdint.h>
#include <stddef.h>

namespace spixels {
// MultiSPI outputs multiple SPI streams in parallel on different GPIOs.
// The clock is on a single GPIO-pin. This way, we can transmit 25-ish
// SPI streams in parallel on a Pi with 40 IO pins.
// Current implementation assumes that all streams have the same amount
// of data.
// Also, there is no chip-select at this point (not needed for the LED strips).
//
// This can be used of course of LED strips (see led-strip.h for the API), but
// for all kinds of other SPI data you want to send to multiple devices in
// a fire-and-forget way.
class MultiSPI {
public:
    // Names of the pin-headers on the breakout board.
    enum {
        SPI_CLOCK = 27,

        SPI_P1  = 18,
        SPI_P2  = 23,
        SPI_P3  = 22,
        SPI_P4  =  5,
        SPI_P5  = 12,
        SPI_P6  = 16,
        SPI_P7  = 19,
        SPI_P8  = 21,

        SPI_P9  =  4,
        SPI_P10 = 17,
        SPI_P11 = 24,
        SPI_P12 = 25,
        SPI_P13 =  6,
        SPI_P14 = 13,
        SPI_P15 = 26,
        SPI_P16 = 20,
    };

    // A function that maps the connector number (P1..P16) to the value of
    // the corresponding SPI Pin SPI_P1..SPI_P16 constant.
    static int SPIPinForConnector(int connector);

    virtual ~MultiSPI() {}

    // Register a new data stream for the given GPIO. The SPI data is
    // sent with the common clock and this gpio pin. The gpio must be one
    // of the above SPI_xx constants or the return value of
    // SPIPinForConnector().
    //
    // Note, each channel might receive more bytes because they share the
    // same clock with everyone and it depends on what is the longest requested
    // length.
    // Overlength transmission bytes are all zero.
    virtual bool RegisterDataGPIO(int gpio, size_t serial_byte_size) = 0;

    // Set data byte for given gpio channel at given position in the
    // stream. "pos" needs to be in range [0 .. serial_bytes_per_stream)
    // Data is sent with next Send().
    virtual void SetBufferedByte(int data_gpio, size_t pos, uint8_t data) = 0;

    // Send data for all streams. Wait for completion. After SendBuffers()
    // has been called once, no new GPIOs can be registered.
    virtual void SendBuffers() = 0;
};

// Factory to create a MultiSPI implementation that directly writes to
// GPIO. Unless you use a WS2801 strip, this is typically what you want.
// Advantages:
//   - Fast
// Disadvantages:
//   - Potentially has jitter which is problematic with LED-strips that
//     use a time-component for triggering (WS2801).
// Parameter:
//   "speed_mhz" rough speed in Mhz of the SPI clock. Useful values 1..15
//   Default is 4. Increase if your set-up can do more and you need the
//   speed. Decrease if you see erratic behavior.
MultiSPI *CreateDirectMultiSPI(int speed_mhz = 4,
                               int clock_gpio = MultiSPI::SPI_CLOCK);

// Factory to create a MultiSPI implementation that uses DMA to output.
// Advantages:
//   - Does not use CPU
//   - Jitter does not exceed several 10 usec. Needed for WS2801.
// Disadvantage:
//   - Limited speed (1-2Mhz). Good for WS2801 which can't go faster
//     anyway, but wasting potential with LPD6803 or APA102 that can go
//     much faster.
MultiSPI *CreateDMAMultiSPI(int clock_gpio = MultiSPI::SPI_CLOCK);
}

#endif  // SPIXELS_MULTI_SPI_H
