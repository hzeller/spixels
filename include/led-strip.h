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
//
// For an example see ../examples (https://github.com/hzeller/spixels/examples)

#ifndef SPIXELS_LED_STRIP_H
#define SPIXELS_LED_STRIP_H

#include <stdint.h>

#include "multi-spi.h"

namespace spixels {

// Red Green Blue color. A color represented by RGB values.
struct RGBc {
    // Creating a color with the Red/Green/Blue components. If you are compiling
    // with C++11, you can even do that in-line
    // LEDStrip::SetPixel(0, {255, 255, 255});
    RGBc(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue){}

    // Setting a color from a single 24Bit 0xRRGGBB hex-value, e.g. 0xff00ff
    RGBc(uint32_t hexcolor)
        : r((hexcolor >> 16) & 0xFF),
          g((hexcolor >>  8) & 0xFF),
          b((hexcolor >>  0) & 0xFF) {}

    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// Simplest possible way for a LED strip.
class LEDStrip {
public:
    virtual ~LEDStrip() {}

    // Set pixel color. Input it sRGB, output is luminance corrected.
    virtual void SetPixel(int pos, const RGBc& c) = 0;

    void SetPixel(int pos, uint8_t r, uint8_t g, uint8_t b) {
        SetPixel(pos, RGBc(r, g, b));
    }

    // Set overall brightness for all pixels. Range of [0.0 ... 1.0].
    // Not all LEDStrips might have this implemented (only APA102 right now).
    virtual void SetBrightness(float brightness) {}

    // Return number of attached LEDs.
    inline int count() const { return count_; }

protected:
    LEDStrip(int count) : count_(count) {}

    const int count_;
};

// Factories for various LED strips.
// Parameters
// "spi"       The MultiSPI instance
// "connector" The connector on the breakout board, such as MultiSPI::SPI_P1
// "count"     Number of LEDs.
LEDStrip *CreateWS2801Strip(MultiSPI *spi, int connector, int count);
LEDStrip *CreateLPD6803Strip(MultiSPI *spi, int connector, int count);
LEDStrip *CreateAPA102Strip(MultiSPI *spi, int connector, int count);
}

#endif // SPIXELS_LED_STRIP_H
