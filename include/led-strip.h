// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
// Copyright (C) 2016 Henner Zeller <h.zeller@acm.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>
//
// For an example, https://github.com/hzeller/spixels/examples

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
LEDStrip *CreateWS2801Strip(MultiSPI *spi, int connector, int count);
LEDStrip *CreateLPD6803Strip(MultiSPI *spi, int connector, int count);
}

#endif // SPIXELS_LED_STRIP_H
