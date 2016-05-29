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

// Simplest possible way for a LED strip.
class LEDStrip {
public:
    virtual ~LEDStrip() {}
    virtual void SetPixel(int pos, uint8_t red, uint8_t green, uint8_t blue) = 0;
    // TODO: set global brightness ?

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
