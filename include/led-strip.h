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
#include <algorithm>

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
    inline int pixelCount() const { return pixelCount_; }
/*
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
*/
	// 0x10000 for no change in brightness
	// SetBrightnessScale16() is virtual so that subclasses can prepare special data for scaling
	void SetBrightnessScale16(uint32_t scale16)
	{ SetBrightnessScale16(scale16, scale16, scale16); }
	virtual void SetBrightnessScale16(uint32_t redScale16, uint32_t greenScale16, uint32_t blueScale16)
    { redScale16_ = redScale16;
      greenScale16_ = greenScale16;
      blueScale16_ = blueScale16; }
    uint32_t redScale16() const
    { return redScale16_; }
    uint32_t greenScale16() const
    { return greenScale16_; }
    uint32_t blueScale16() const
    { return blueScale16_; }
    
	// The following methods provide different ways of setting a pixel's value:
	// The maximum value of a uint16_t component is 0xFFFF.  Most subclasses will just throw
	// away the low byte.  APA102/SK9822 will make use of the extra bnits to set
	// their "global brightness" values.
    virtual void SetPixel8(uint32_t pixel_index, uint8_t red, uint8_t green, uint8_t blue) = 0;
    virtual void SetPixel16(uint32_t pixel_index, uint16_t red, uint16_t green, uint16_t blue)
	{
		// This default implementation will do for most subclasses, not for APA102/SK9822
		SetPixel8(pixel_index,
					(uint8_t)std::min(((uint32_t)red + 0x7Fu) >> 8, 0xFFu),
					(uint8_t)std::min(((uint32_t)green + 0x7Fu) >> 8, 0xFFu),
					(uint8_t)std::min(((uint32_t)blue + 0x7Fu) >> 8, 0xFFu));
	}
    
protected:
    LEDStrip(int pixelCount);

    const int pixelCount_;
    uint32_t redScale16_;
    uint32_t greenScale16_;
    uint32_t blueScale16_;
/*
    RGBc *const values_;
*/
};

// Factories for various LED strips.
// Parameters
// "spi"       The MultiSPI instance
// "connector" The connector on the breakout board, such as MultiSPI::SPI_P1
// "pixelCount"     Number of LEDs.
LEDStrip *CreateWS2801Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount);
LEDStrip *CreateLPD6803Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount);
LEDStrip *CreateLPD8806Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount);
LEDStrip *CreateAPA102Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount);
LEDStrip *CreateSK9822Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount);
}

#endif // SPIXELS_LED_STRIP_H
