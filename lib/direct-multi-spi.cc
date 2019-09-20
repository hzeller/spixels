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
#include <time.h>       // for ::clock_gettime()
#include <stdio.h>      // for ::printf()


#define DO_LOG_TIMING   0

namespace spixels {

// This should be in a multi-spi.cc, but that would be the only function. So
// here is good as well.
MultiSPI::Pin MultiSPI::SPIPinForConnector(int connector) {
    switch (connector) {
    default :
        assert(false);
    case 1:  return SPI_P1;
    case 2:  return SPI_P2;
    case 3:  return SPI_P3;
    case 4:  return SPI_P4;
    case 5:  return SPI_P5;
    case 6:  return SPI_P6;
    case 7:  return SPI_P7;
    case 8:  return SPI_P8;

    case 9:  return SPI_P9;
    case 10: return SPI_P10;
    case 11: return SPI_P11;
    case 12: return SPI_P12;
    case 13: return SPI_P13;
    case 14: return SPI_P14;
    case 15: return SPI_P15;
    case 16: return SPI_P16;
    }
}

namespace {

class DirectMultiSPI : public MultiSPI {
public:
    explicit DirectMultiSPI(double speed_mhz, MultiSPI::Pin clockPin);
    virtual ~DirectMultiSPI();

    virtual bool RegisterDataGPIO(int gpio, size_t serial_byte_size);
    virtual void SetBufferedByte(MultiSPI::Pin pin, size_t pos, uint8_t data);
    virtual void SendBuffers();

private:
    MultiSPI::Pin   const clockPin_;
    double          const gpio_mhz_;
    ft::GPIO        gpio_;
    size_t          size_;
    uint32_t*       gpio_data_;
    double          static gpio_duration_;          // how long it takes to write to GPIO
    double          static busy_cycle_duration_;    // how long it takes to loop doing a single memory write
    bool            static durations_measured_;     // whether the above durations have been measured.
    int             static const minimum_gpio_count_ = 3;
    int             extra_gpio_count_;
    int             busy_cycle_count_;
    int             volatile busy_cycle_sink_;      // written to during busy cycles

    void            MeasureSendBuffersDurations();
    void            CalculateSendBuffersTiming();
    void            SendBuffers(int extra_gpio_count, int busy_cycle_count);
};

}  // end anonymous namespace

double          DirectMultiSPI::gpio_duration_ = 17.2 / 1000000000;      // default, measured on Pi 3B
double          DirectMultiSPI::busy_cycle_duration_ = 5.0 / 1000000000; // default, measured on Pi 3B
bool            DirectMultiSPI::durations_measured_ = false;             // causes above durations to be measured in SendBuffer()

DirectMultiSPI::DirectMultiSPI(double speed_mhz, MultiSPI::Pin clockPin)
: clockPin_(clockPin),
  gpio_mhz_(speed_mhz),
  size_(0),
  gpio_data_(NULL) {
    CalculateSendBuffersTiming();
    bool success = gpio_.Init();
    assert(success);  // gpio couldn't be initialized
    success = gpio_.AddOutput(clockPin);
    assert(success);  // clock pin not valid
}

DirectMultiSPI::~DirectMultiSPI() {
    if (gpio_data_) {
        free(gpio_data_);
    }
}

void DirectMultiSPI::MeasureSendBuffersDurations()
{
    struct timespec spec;

    if (::clock_gettime(CLOCK_MONOTONIC, &spec) == 0) {
        double          const start_time = spec.tv_sec + spec.tv_nsec * 0.000000001;

        SendBuffers(100, 0);
        // if clock_gettime() worked the first time, we'll assume it will again:
        ::clock_gettime(CLOCK_MONOTONIC, &spec);

        double          const mid_time = spec.tv_sec + spec.tv_nsec * 0.000000001;
        int             i;

        for (i = 0; i < 0x100000; ++i) {
            busy_cycle_sink_ = i;
        }
        ::clock_gettime(CLOCK_MONOTONIC, &spec);

        double          const end_time = spec.tv_sec + spec.tv_nsec * 0.000000001;
        double          const gpio_duration = mid_time - start_time;
        double          const busy_duration = end_time - mid_time;
        unsigned long   const bit_count = 8 * size_ + 1;

        gpio_duration_ = gpio_duration / (bit_count * (minimum_gpio_count_ + 100) + 1);
        busy_cycle_duration_ = busy_duration / 0x100000;
#if DO_LOG_TIMING
        ::printf("DirectMultiSPI::MeasureSendBuffersDurations() - GPIO duration = %1.3fns, busy duration = %1.3fns\n",
                    gpio_duration_ * 1000000000, busy_cycle_duration_ * 1000000000);
#endif
        durations_measured_ = true;
        CalculateSendBuffersTiming();
    }
}
void DirectMultiSPI::CalculateSendBuffersTiming() {
    double          const target_bit_duration = 0.000001 / gpio_mhz_;
    double          const minimum_bit_duration = minimum_gpio_count_ * gpio_duration_;
    double          const delay = target_bit_duration - minimum_bit_duration;
    double          delay_remainder;

    if (delay > 0.0) {
        extra_gpio_count_ = (int)(delay / gpio_duration_);
        delay_remainder = delay - extra_gpio_count_ * gpio_duration_;
        busy_cycle_count_ = (int)(delay_remainder / busy_cycle_duration_);
    } else {
        // Set the busy delay to zero.
        extra_gpio_count_ = 0;
        busy_cycle_count_ = 0;
    }
    // For some reason, the measured data frequency is more accurate if we
    // tweak the count like this:
    extra_gpio_count_++;

#if DO_LOG_TIMING
    if (durations_measured_) {
        ::printf("DirectMultiSPI::CalculateSendBuffersTiming() - %1.1fMHz, delay = %1.3fns, extra GPIOs = %u, busy cycles = %u\n",
                    gpio_mhz_, delay * 1000000000, extra_gpio_count_, busy_cycle_count_);
    }
#endif
}

bool DirectMultiSPI::RegisterDataGPIO(int gpio, size_t serial_byte_size) {
    if (serial_byte_size > size_) {
        const size_t prev_size = size_ * 8 * sizeof(uint32_t);
        const size_t new_size = serial_byte_size * 8 * sizeof(uint32_t);
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

void DirectMultiSPI::SetBufferedByte(MultiSPI::Pin pin, size_t pos, uint8_t data) {
    assert(pos < size_);
    if (pos < size_) {
        uint32_t        const pin_bit = 1 << pin;
        uint32_t        const pin_not_bit = ~pin_bit;
        uint32_t*       buffer_pos = gpio_data_ + 8 * pos;

#if 1
        for (uint8_t bit = 0x80; bit; bit >>= 1) {
            if (data & bit) {
                // set
                *buffer_pos |= pin_bit;
            } else {
                // reset
                *buffer_pos &= pin_not_bit;
            }
            buffer_pos++;
        }
#elif 0
        // This unwound loop has no conditional branches, no broken pipeline!
        // Yet it goes slower than the above loop on the Pi.
        // Maybe someday figure out why...
        buffer_pos[7] = (buffer_pos[7] & pin_not_bit) | ((data & 1) << pin);
        data >>= 1;
        buffer_pos[6] = (buffer_pos[6] & pin_not_bit) | ((data & 1) << pin);
        data >>= 1;
        buffer_pos[5] = (buffer_pos[5] & pin_not_bit) | ((data & 1) << pin);
        data >>= 1;
        buffer_pos[4] = (buffer_pos[4] & pin_not_bit) | ((data & 1) << pin);
        data >>= 1;
        buffer_pos[3] = (buffer_pos[3] & pin_not_bit) | ((data & 1) << pin);
        data >>= 1;
        buffer_pos[2] = (buffer_pos[2] & pin_not_bit) | ((data & 1) << pin);
        data >>= 1;
        buffer_pos[1] = (buffer_pos[1] & pin_not_bit) | ((data & 1) << pin);
        data >>= 1;
        buffer_pos[0] = (buffer_pos[0] & pin_not_bit) | ((data & 1) << pin);
#else
        // This option is slower as well, even though it has fewer conditional branches...
        uint32_t*       const buffer_end = buffer_pos;

        buffer_pos += 7;
        do
        {
            *buffer_pos = (*buffer_pos & pin_not_bit) | ((data & 1) << pin);
            data >>= 1;
        }
        while (--buffer_pos >= buffer_end);
#endif
    }
}

void DirectMultiSPI::SendBuffers() {
    if (!durations_measured_) {
        MeasureSendBuffersDurations();
    }
    SendBuffers(extra_gpio_count_, busy_cycle_count_);
}
void DirectMultiSPI::SendBuffers(int extra_gpio_count, int busy_cycle_count) {
    int             const extra_gpio_count1 = extra_gpio_count / 2;
    int             const extra_gpio_count2 = extra_gpio_count - extra_gpio_count1;
    int             const busy_cycle_count1 = busy_cycle_count / 2;
    int             const busy_cycle_count2 = busy_cycle_count - busy_cycle_count1;
    unsigned long   const bit_count = 8 * size_;
    uint32_t*       const end = gpio_data_ + bit_count;

#if DO_LOG_TIMING
    struct timespec spec;

    ::clock_gettime(CLOCK_MONOTONIC, &spec);

    double          const start_time = spec.tv_sec + spec.tv_nsec * 0.000000001;
#endif

    for (uint32_t *data = gpio_data_; data < end; ++data) {
        uint32_t        d;
        int             i;
/*
        The way this SHOULD work is that we write to GPIO 3 times (1 to clear bits,
        1 to set the data bits, 1 to add the clock bit), then spend many cycles
        in a loop doing nothing.  For some reason, this causes flickery display
        on the LEDs.  What DOES work is to spend time setting the GPIO pins
        repeatedly, so that's what we do!
*/
        // first, set the data bits
        d = *data;
        gpio_.Write(d);     // one GPIO clear-bits and one GPIO set-bits
        for (i = 0; i < extra_gpio_count1; ++i) {
            gpio_.Set(d);
        }
        for (i = 0; i < busy_cycle_count1; ++i) {
            busy_cycle_sink_ = i;
        }

        // then set the clock bit high so that the LED chips will take in the data bits
        d |= 1 << clockPin_;
        gpio_.Set(d);
        for (i = 0; i < extra_gpio_count2; ++i) {
            gpio_.Set(d);
        }
        for (i = 0; i < busy_cycle_count2; ++i) {
            busy_cycle_sink_ = i;
        }
    }

    // clear the clock and data bits
    gpio_.Clear(0xFFFFFFFF);    // one GPIO clear-bits

#if DO_LOG_TIMING
    ::clock_gettime(CLOCK_MONOTONIC, &spec);

    double          const end_time = spec.tv_sec + spec.tv_nsec * 0.000000001;
    double          const duration = end_time - start_time;
    double          const ns_per_bit = duration * 1000000000 / bit_count;
    double          const mhz = 1000.0 / ns_per_bit;

    ::printf("DirectMultiSPI::SendBuffers() - bit duration = %1.3fns : %1.3fMHz\n", ns_per_bit, mhz);
#endif
}

// Public interface
MultiSPI *CreateDirectMultiSPI(double speed_mhz, MultiSPI::Pin clockPin) {
    return new DirectMultiSPI(speed_mhz, clockPin);
}


}  // namespace spixels
