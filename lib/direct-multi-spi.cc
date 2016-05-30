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

#include "multi-spi.h"

#include "ft-gpio.h"

#include <math.h>
#include <assert.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

namespace spixels {
namespace {
class DirectMultiSPI : public MultiSPI {
public:
    explicit DirectMultiSPI(int speed_mhz, int clock_gpio);
    virtual ~DirectMultiSPI();

    virtual bool RegisterDataGPIO(int gpio, size_t serial_byte_size);
    virtual void FinishRegistration() {}
    virtual void SetBufferedByte(int data_gpio, size_t pos, uint8_t data);
    virtual void SendBuffers();

private:
    const int clock_gpio_;
    const int write_repeat_;  // how often write operations to repeat to slowdown
    ft::GPIO gpio_;
    size_t size_;
    uint32_t *gpio_data_;
};
}  // end anonymous namespace

DirectMultiSPI::DirectMultiSPI(int speed_mhz, int clock_gpio)
    : clock_gpio_(clock_gpio),
      write_repeat_(std::max(2, (int)roundf(30.0 / speed_mhz))),
      size_(0), gpio_data_(NULL) {
    bool success = gpio_.Init();
    assert(success);  // gpio couldn't be initialized
    success = gpio_.AddOutput(clock_gpio);
    assert(success);  // clock pin not valid
}

DirectMultiSPI::~DirectMultiSPI() {
    free(gpio_data_);
}

bool DirectMultiSPI::RegisterDataGPIO(int gpio, size_t serial_byte_size) {
    if (serial_byte_size > size_) {
        const int prev_size = size_ * 8 * sizeof(uint32_t);
        const int new_size = serial_byte_size * 8 * sizeof(uint32_t);
        size_ = serial_byte_size;
	if (gpio_data_ == NULL) {
            gpio_data_ = (uint32_t*)malloc(new_size);
        } else {
            gpio_data_ = (uint32_t*)realloc(gpio_data_, new_size);
        }
        bzero(gpio_data_ + prev_size, new_size-prev_size);
    }

    return gpio_.AddOutput(gpio);
}

void DirectMultiSPI::SetBufferedByte(int data_gpio, size_t pos, uint8_t data) {
    assert(pos < size_);
    uint32_t *buffer_pos = gpio_data_ + 8 * pos;
    for (uint8_t bit = 0x80; bit; bit >>= 1, buffer_pos++) {
        if (data & bit) {   // set
            *buffer_pos |= (1 << data_gpio);
        } else {  // reset
            *buffer_pos &= ~(1 << data_gpio);
        }
    }
}

void DirectMultiSPI::SendBuffers() {
    uint32_t *end = gpio_data_ + 8 * size_;
    for (uint32_t *data = gpio_data_; data < end; ++data) {
        uint32_t d = *data;
        for (int i = 0; i < write_repeat_; ++i) gpio_.Write(d);
        d |= (1 << clock_gpio_);   // pos clock edge.
        for (int i = 0; i < write_repeat_; ++i) gpio_.Write(d);
    }
    gpio_.Write(0);  // Reset clock.
}

// Public interface
MultiSPI *CreateDirectMultiSPI(int speed_mhz, int clock_gpio) {
    return new DirectMultiSPI(speed_mhz, clock_gpio);
}
}  // namespace spixels
