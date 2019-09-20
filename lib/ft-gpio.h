// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
// SPI Pixels - Control SPI LED strips (spixels)
// Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
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

#ifndef RPI_GPIO_H
#define RPI_GPIO_H

#include <stdint.h>

// Putting this in our namespace to not collide with other things called like
// this.
namespace ft {
class GPIO {
public:
    // Available bits that actually have pins.
    static const uint32_t kValidBits;

    GPIO();

    // Initialize before use. Returns 'true' if successful, 'false' otherwise
    // (e.g. due to a permission problem).
    bool Init();

    // Add given gpio pin as output.
    bool AddOutput(int gpio);

    // Set the bits that are '1' in the output. Leave the rest untouched.
    inline void SetBits(uint32_t bits) {
        // This test avoids a write that takes a long time.
        // But direct-multi-spi{} needs this method to take the same amount of time,
        // no matter what the data is, so don't do it.
//      if (!bits) return;
        *gpio_set_bits_ = bits;
    }

    // Clear the bits that are '1' in the output. Leave the rest untouched.
    inline void ClearBits(uint32_t bits) {
        // This test avoids a write that takes a long time.
        // But direct-multi-spi{} needs this method to take the same amount of time,
        // no matter what the data is, so don't do it.
//      if (!bits) return;
        *gpio_clr_bits_ = bits;
    }

    // Write only the bits of "bits" mentioned in "mask".
    inline void WriteMaskedBits(uint32_t bits, uint32_t mask) {
        // Writing a word is two operations. The IO is actually pretty slow, so
        // this should probably  be unnoticable.
        ClearBits(~bits & mask);
        SetBits(bits & mask);
    }

    inline void Write(uint32_t bits) {
        ClearBits(~bits & output_bits_);
        SetBits(bits & output_bits_);
    }
    inline void Set(uint32_t bits) {
        SetBits(bits & output_bits_);
    }
    inline void Clear(uint32_t bits) {
        ClearBits(bits & output_bits_);
    }

private:
    uint32_t output_bits_;
    volatile uint32_t *gpio_port_;
    volatile uint32_t *gpio_set_bits_;
    volatile uint32_t *gpio_clr_bits_;
};
}  // end namespace ft
#endif  // RPI_GPIO_H
