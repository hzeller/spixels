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

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "multi-spi.h"
#include "led-strip.h"

static const int kBitPlanes = 16;

// Do CIE1931 luminance correction and scale to maximum expected output bits.
static int luminance_cie1931_internal(uint8_t c) {
    const float out_factor = ((1 << kBitPlanes) - 1);
    const float v = 100.0f * c / 255.0f;
    return out_factor * ((v <= 8) ? v / 902.3 : pow((v + 16) / 116.0, 3));
}

static int *CreateCIE1931LookupTable() {
    int *result = new int[256];
    for (int c = 0; c < 256; ++c) {
        result[c] = luminance_cie1931_internal(c);
    }
    return result;
}

static int luminance_cie1931(uint8_t output_bits, uint8_t value) {
    static const int *const luminance_lookup = CreateCIE1931LookupTable();
    return luminance_lookup[value] >> (kBitPlanes - output_bits);
}

namespace spixels {
namespace {
class WS2801LedStrip : public LEDStrip {
public:
    WS2801LedStrip(MultiSPI *spi, int gpio, int count)
        : LEDStrip(count), spi_(spi), gpio_(gpio) {
        spi_->RegisterDataGPIO(gpio, count * 3);
    }
    virtual void SetPixel(int pos, const RGBc &c) {
        if (pos < 0 || pos >= count_) return;
        spi_->SetBufferedByte(gpio_, 3 * pos + 0, luminance_cie1931(8, c.r));
        spi_->SetBufferedByte(gpio_, 3 * pos + 1, luminance_cie1931(8, c.g));
        spi_->SetBufferedByte(gpio_, 3 * pos + 2, luminance_cie1931(8, c.b));
    }

private:
    MultiSPI *const spi_;
    const int gpio_;
};

class LPD6803LedStrip : public LEDStrip {
public:
    LPD6803LedStrip(MultiSPI *spi, int gpio, int count)
        : LEDStrip(count), spi_(spi), gpio_(gpio) {
        const size_t bytes_needed = 4 + 2 * count + 4;
        spi_->RegisterDataGPIO(gpio, bytes_needed);
        // Four zero bytes as start-bytes for lpd6803
        spi_->SetBufferedByte(gpio_, 0, 0x00);
        spi_->SetBufferedByte(gpio_, 1, 0x00);
        spi_->SetBufferedByte(gpio_, 2, 0x00);
        spi_->SetBufferedByte(gpio_, 3, 0x00);
        for (int pos = 0; pos < count; ++pos)
            SetPixel(pos, 0x000000);     // Initialize all top-bits.
    }
    virtual void SetPixel(int pos, const RGBc &c) {
        if (pos < 0 || pos >= count_) return;
        uint16_t data = 0;
        data |= (1<<15);  // start bit
        data |= luminance_cie1931(5, c.r) << 10;
        data |= luminance_cie1931(5, c.g) <<  5;
        data |= luminance_cie1931(5, c.b) <<  0;

        spi_->SetBufferedByte(gpio_, 2 * pos + 4 + 0, data >> 8);
        spi_->SetBufferedByte(gpio_, 2 * pos + 4 + 1, data & 0xFF);
    }

private:
    MultiSPI *const spi_;
    const int gpio_;
};

class APA102LedStrip : public LEDStrip {
public:
    APA102LedStrip(MultiSPI *spi, int gpio, int count)
        : LEDStrip(count), spi_(spi), gpio_(gpio), global_brightness_(0) {
        const size_t startframe_size = 4;
        const size_t endframe_size = (count+15) / 16;
        const size_t bytes_needed = startframe_size + 4*count + endframe_size;

        spi_->RegisterDataGPIO(gpio, bytes_needed);

        // Four zero bytes as start-bytes
        spi_->SetBufferedByte(gpio_, 0, 0x00);
        spi_->SetBufferedByte(gpio_, 1, 0x00);
        spi_->SetBufferedByte(gpio_, 2, 0x00);
        spi_->SetBufferedByte(gpio_, 3, 0x00);

        // Initialize the global brightness bits.
        SetBrightness(1.0);

        // We need a couple of more bits clocked at the end.
        for (size_t tail = 4 + 4*count; tail < bytes_needed; ++tail) {
            spi_->SetBufferedByte(gpio_, tail, 0xff);
        }
    }
    virtual void SetPixel(int pos, const RGBc &c) {
        if (pos < 0 || pos >= count_) return;
        const int base = 4 + 4 * pos;
        // TODO: use the global brigthness values smarter by using this
        // as additional bits. Right now, they are set whenver SetBrightness()
        // is called. No need to set here.
        //spi_->SetBufferedByte(gpio_, base + 0, 0xE0 | global_brigthness_);
        spi_->SetBufferedByte(gpio_, base + 1, luminance_cie1931(8, c.b));
        spi_->SetBufferedByte(gpio_, base + 2, luminance_cie1931(8, c.g));
        spi_->SetBufferedByte(gpio_, base + 3, luminance_cie1931(8, c.r));
    }

    virtual void SetBrightness(float brightness) {
        if (brightness < 0.0f) brightness = 0;
        if (brightness > 1.0f) brightness = 1.0f;
        uint8_t new_brightness = (int) roundf(brightness * 0x1F);
        if (new_brightness != global_brightness_) {
            global_brightness_ = new_brightness;
            for (int i = 0; i < count_; ++i) {
                spi_->SetBufferedByte(gpio_, 4 + 4*i, 0xE0 | global_brightness_);
            }
        }
    }

private:
    MultiSPI *const spi_;
    const int gpio_;
    uint8_t global_brightness_;
};
}  // anonymous namespace

// Public interface
LEDStrip *CreateWS2801Strip(MultiSPI *spi, int connector, int count) {
    return new WS2801LedStrip(spi, connector, count);
}
LEDStrip *CreateLPD6803Strip(MultiSPI *spi, int connector, int count) {
    return new LPD6803LedStrip(spi, connector, count);
}
LEDStrip *CreateAPA102Strip(MultiSPI *spi, int connector, int count) {
    return new APA102LedStrip(spi, connector, count);
}
}  // spixels namespace
