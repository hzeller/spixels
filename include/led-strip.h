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
    //
    // Brightness change will take effect with next SendBuffers().
    void SetBrightness(uint8_t brigthness);
    inline uint8_t brightness() const { return brightness_; }
*/
	// The following methods provide different ways of setting a pixel's value:
	// The "8" methods accept RGB components as bytes.  If the gammaTable is non-NULL
	// these values index the table to get 16-bit values, which are passed to the "16" method.
	// The "16" methods accept values normalized to 0xFFFF.  (max value == 0xFFFF)
	// the "brightnessScale" field is normalized to 0x10000 (can be above or below this)
	// the gammaTable array is 256 16-bit numbers, each normalized to 0xFFFF
	// brightnessScale is applied AFTER the gammaTable.
	
	// Most subclasses will simply use the high 8 bits in their implementations of these methods.
	// Others, such as APA102LedStrip will use all the bits to 

    virtual void SetPixel8(int pixelIndex,
							uint8_t red, uint8_t green, uint8_t blue) = 0;
    virtual void SetPixel16(int pixelIndex,
							uint16_t red, uint16_t green, uint16_t blue) = 0;
	virtual void SetPixel8(int pixelIndex,
							uint8_t red, uint8_t green, uint8_t blue,
							uint32_t brightnessScale)
	{
		// This default implementation will do for most subclasses, not for APA102
    	if (brightnessScale > 0x10000)
    	{
    		red = (uint8_t)std::min(red * brightnessScale / 0x10000, (uint32_t)0xFF);
    		green = (uint8_t)std::min(green * brightnessScale / 0x10000, (uint32_t)0xFF);
    		blue = (uint8_t)std::min(blue * brightnessScale / 0x10000, (uint32_t)0xFF);
    	}
    	else if (brightnessScale < 0x10000)
    	{
    		red = (uint8_t)(red * brightnessScale / 0x10000);
    		green = (uint8_t)(green * brightnessScale / 0x10000);
    		blue = (uint8_t)(blue * brightnessScale / 0x10000);
    	}
    	SetPixel8(pixelIndex, red, green, blue);
	}
    virtual void SetPixel16(int pixelIndex,
                            uint16_t red, uint16_t green, uint16_t blue,
                            uint32_t brightnessScale)
	{
		// This default implementation will do for most subclasses, not for APA102
    	if (brightnessScale > 0x10000)
    	{
    		red = (uint16_t)std::min(red * brightnessScale / 0x10000, (uint32_t)0xFFFF);
    		green = (uint16_t)std::min(green * brightnessScale / 0x10000, (uint32_t)0xFFFF);
    		blue = (uint16_t)std::min(blue * brightnessScale / 0x10000, (uint32_t)0xFFFF);
    	}
    	else if (brightnessScale < 0x10000)
    	{
    		red = (uint16_t)(red * brightnessScale / 0x10000);
    		green = (uint16_t)(green * brightnessScale / 0x10000);
    		blue = (uint16_t)(blue * brightnessScale / 0x10000);
    	}
    	SetPixel16(pixelIndex, red, green, blue);
	}
    void SetPixel8(int pixelIndex, uint8_t red, uint8_t green, uint8_t blue,
    				uint16_t const gammaTable[256], uint32_t brightnessScale)
    {
    	if (gammaTable)
    	{
	    	SetPixel16(pixelIndex,
    					gammaTable[red], gammaTable[green], gammaTable[blue],
    					brightnessScale);
	    }
	    else
	    {
	    	SetPixel8(pixelIndex, red, green, blue, brightnessScale);
	    }
    }
    
protected:
    LEDStrip(int pixelCount);

    const int pixelCount_;
/*
    RGBc *const values_;
    uint8_t brightness_;
*/
};

// Factories for various LED strips.
// Parameters
// "spi"       The MultiSPI instance
// "connector" The connector on the breakout board, such as MultiSPI::SPI_P1
// "pixelCount"     Number of LEDs.
LEDStrip *CreateWS2801Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount);
LEDStrip *CreateLPD6803Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount);
LEDStrip *CreateAPA102Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount);
}

#endif // SPIXELS_LED_STRIP_H
