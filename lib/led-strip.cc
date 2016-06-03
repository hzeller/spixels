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
typedef uint16_t CIEValue;

// Do CIE1931 luminance correction and scale to maximum expected output bits.
static CIEValue luminance_cie1931_internal(uint8_t c, uint8_t brightness) {
    const float out_factor = ((1 << kBitPlanes) - 1);
    const float v = 100.0f * (brightness/255.0f) * (c/255.0f);
    return out_factor * ((v <= 8) ? v / 902.3 : pow((v + 16) / 116.0, 3));
}

static CIEValue *CreateCIE1931LookupTable() {
    CIEValue *result = new CIEValue[256 * 256];
    for (int v = 0; v < 256; ++v) {      // Value
        for (int b = 0; b < 256; ++b) {  // Brightness
            result[b * 256 + v] = luminance_cie1931_internal(v, b);
        }
    }
    return result;
}

// Return a CIE1931 corrected value from given desired lumninace value and
// brightness.
static CIEValue luminance_cie1931(uint8_t bits, uint8_t value, uint8_t bright) {
    static const CIEValue *const luminance_lookup = CreateCIE1931LookupTable();
    return luminance_lookup[bright * 256 + value] >> (kBitPlanes - bits);
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
        const uint8_t br = 255;  // brightness.
        spi_->SetBufferedByte(gpio_, 3 * pos + 0, luminance_cie1931(8, c.r, br));
        spi_->SetBufferedByte(gpio_, 3 * pos + 1, luminance_cie1931(8, c.g, br));
        spi_->SetBufferedByte(gpio_, 3 * pos + 2, luminance_cie1931(8, c.b, br));
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
        data |= luminance_cie1931(5, c.r, 255) << 10;
        data |= luminance_cie1931(5, c.g, 255) <<  5;
        data |= luminance_cie1931(5, c.b, 255) <<  0;

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
        : LEDStrip(count), spi_(spi), gpio_(gpio), global_brightness_(255),
          values_(new RGBc[count]) {
        const size_t startframe_size = 4;
        const size_t endframe_size = (count+15) / 16;
        const size_t bytes_needed = startframe_size + 4*count + endframe_size;

        spi_->RegisterDataGPIO(gpio, bytes_needed);

        // Four zero bytes as start-bytes
        spi_->SetBufferedByte(gpio_, 0, 0x00);
        spi_->SetBufferedByte(gpio_, 1, 0x00);
        spi_->SetBufferedByte(gpio_, 2, 0x00);
        spi_->SetBufferedByte(gpio_, 3, 0x00);

        // Make sure the start bits are properly set.
        for (int i = 0; i < count; ++i) {
            SetPixel(i, 0x000000);
        }

        // We need a couple of more bits clocked at the end.
        for (size_t tail = 4 + 4*count; tail < bytes_needed; ++tail) {
            spi_->SetBufferedByte(gpio_, tail, 0xff);
        }
    }

    virtual ~APA102LedStrip() { delete values_; }

    virtual void SetPixel(int pos, const RGBc &c) {
        if (pos < 0 || pos >= count_) return;
        values_[pos] = c;
        const int base = 4 + 4 * pos;
        uint16_t rr = luminance_cie1931(12, c.r, global_brightness_);
        uint16_t gg = luminance_cie1931(12, c.g, global_brightness_);
        uint16_t bb = luminance_cie1931(12, c.b, global_brightness_);

        // If value is dim, use the APA global brightness adjustment for
        // more resolution. We essentially get 4 bits at the bottom end.
        const uint16_t bit_use = rr | gg | bb;  // find highest bit used.
        if (bit_use < 16) {
            spi_->SetBufferedByte(gpio_, base + 0, 0xE0 | 0x01);
        } else if (bit_use < 32) {
            spi_->SetBufferedByte(gpio_, base + 0, 0xE0 | 0x03);
            rr >>= 1; gg >>= 1; bb >>= 1;
        } else if (bit_use < 64) {
            spi_->SetBufferedByte(gpio_, base + 0, 0xE0 | 0x07);
            rr >>= 2; gg >>= 2; bb >>= 2;
        } else if (bit_use < 128) {
            spi_->SetBufferedByte(gpio_, base + 0, 0xE0 | 0x0F);
            rr >>= 3; gg >>= 3; bb >>= 3;
        } else {
            spi_->SetBufferedByte(gpio_, base + 0, 0xE0 | 0x1F);
            rr >>= 4; gg >>= 4; bb >>= 4;
        }

        spi_->SetBufferedByte(gpio_, base + 1, bb);
        spi_->SetBufferedByte(gpio_, base + 2, gg);
        spi_->SetBufferedByte(gpio_, base + 3, rr);
    }

    virtual void SetBrightness(uint8_t new_brightness) {
        if (new_brightness != global_brightness_) {
            global_brightness_ = new_brightness;
            for (int i = 0; i < count_; ++i) {
                SetPixel(i, values_[i]);  // Force recalculation.
            }
        }
    }

private:
    MultiSPI *const spi_;
    const int gpio_;
    uint8_t global_brightness_;
    RGBc *values_;
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
