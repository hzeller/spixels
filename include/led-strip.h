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

// Simplest possible way for a LED strip.
class LEDStrip {
public:
    virtual ~LEDStrip() {};

    inline int pixelCount() const {
        return pixelCount_;
    }

    void SetBrightnessScale(float scale16) {
        SetBrightnessScale(scale16, scale16, scale16);
    }
    // SetBrightnessScale() is virtual so that subclasses can prepare special data for scaling
    virtual void SetBrightnessScale(float redScale, float greenScale, float blueScale) {
        redScale_ = redScale;
        greenScale_ = greenScale;
        blueScale_ = blueScale;

        redScale16_ = (uint32_t)(redScale * 0x10000 + 0.5f);
        greenScale16_ = (uint32_t)(greenScale * 0x10000 + 0.5f);
        blueScale16_ = (uint32_t)(blueScale * 0x10000 + 0.5f);
    }
    float redScale() const {
        return redScale_;
    }
    float greenScale() const {
        return greenScale_;
    }
    float blueScale() const {
        return blueScale_;
    }

    // The following methods provide different ways of setting a pixel's value:
    // The maximum value of a uint16_t component is 0xFFFF.  Most subclasses will just throw
    // away the low byte.  APA102/SK9822 will make use of the extra bnits to set
    // their "global brightness" values.
    virtual void SetPixel8(uint32_t pixel_index, uint8_t red, uint8_t green, uint8_t blue) = 0;
    virtual void SetPixel16(uint32_t pixel_index, uint16_t red, uint16_t green, uint16_t blue) {
        // This default implementation will do for most subclasses, not for APA102/SK9822
        SetPixel8(pixel_index,
                    (uint8_t)std::min(((uint32_t)red + 0x7Fu) >> 8, 0xFFu),
                    (uint8_t)std::min(((uint32_t)green + 0x7Fu) >> 8, 0xFFu),
                    (uint8_t)std::min(((uint32_t)blue + 0x7Fu) >> 8, 0xFFu));
    }

protected:
    LEDStrip(int pixelCount);

    int         const pixelCount_;

    float       redScale_;
    float       greenScale_;
    float       blueScale_;

    uint32_t    redScale16_;
    uint32_t    greenScale16_;
    uint32_t    blueScale16_;
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
