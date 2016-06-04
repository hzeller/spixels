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
    RGBc(): r(0), g(0), b(0) {}

    // Creating a color with the Red/Green/Blue components. If you are compiling
    // with C++11, you can even do that in-line
    // LEDStrip::SetPixel(0, {255, 255, 255});
    RGBc(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}

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
    virtual ~LEDStrip();

    // Return number of attached LEDs.
    inline int count() const { return count_; }

    // Set pixel color. Input it RGB, output is luminance corrected
    // you don't have to apply pre-correction.
    // This is typically the function to use.
    void SetPixel(int pos, const RGBc& c);

    // Same with explicitly spelled out r,g,b.
    void SetPixel(int pos, uint8_t r, uint8_t g, uint8_t b) {
        SetPixel(pos, RGBc(r, g, b));
    }

    // Set overall brightness for all pixels. Range of [0 .. 255].
    // This scales the brightness so that it looks linear luminance corrected
    // for the eye.
    // This will be only having a somewhat pleasing result for LED strips with
    // higher PWM resolution (such as APA102).
    //
    // Brightness change will take effect with next SendBuffers().
    void SetBrightness(uint8_t brigthness);
    inline uint8_t brightness() const { return brightness_; }

    // Set the raw, linear RGB value as provided by the LED strip, normalized
    // to the range [0 .. 0xFFFF]. This is LED-Strip dependent.
    //
    // The range is always [0 .. 0xFFFF], but the implementation internally
    // scales it to whatever the hardware can do (LPD6803 only uses the
    // 5 Most significant bits, while APA102 can provide up to 12 bit
    // resolution depending on the circumstances).
    //
    // Note, this is the _linear_ range provided by the RGB strip as
    // opposed to the luminance corrected RGB value in SetPixel(). So only
    // use if you need the direct values.
    virtual void SetLinearValues(int pos,
                                 uint16_t r, uint16_t g, uint16_t b) = 0;
protected:
    LEDStrip(int count);

    const int count_;
    RGBc *const values_;
    uint8_t brightness_;
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
