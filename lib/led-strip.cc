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
    const float v = 100.0 * c / 255.0;
    return out_factor * ((v <= 8) ? v / 902.3 : pow((v + 16) / 116.0, 3));
}

static int *CreateCIE1931LookupTable() {
    int *result = new int[256];  // TODO: more of these when brightness control
  for (int i = 0; i < 256; ++i) {
      result[i] = luminance_cie1931_internal(i);
  }
  return result;
}

static int luminance_cie1931(uint8_t output_bits, uint8_t color) {
    static const int *const luminance_lookup = CreateCIE1931LookupTable();
    return luminance_lookup[color] >> (kBitPlanes - output_bits);
}

namespace spixels {
namespace {
class WS2801LedStrip : public LEDStrip {
public:
    WS2801LedStrip(MultiSPI *spi, int gpio, int count)
        : LEDStrip(count), spi_(spi), gpio_(gpio) {
        spi_->RegisterDataGPIO(gpio, count * 3);
    }
    virtual void SetPixel(int pos, uint8_t red, uint8_t green, uint8_t blue) {
        if (pos < 0 || pos >= count_) return;
        spi_->SetBufferedByte(gpio_, 3 * pos + 0, luminance_cie1931(8, red));
        spi_->SetBufferedByte(gpio_, 3 * pos + 1, luminance_cie1931(8, green));
        spi_->SetBufferedByte(gpio_, 3 * pos + 2, luminance_cie1931(8, blue));
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
            SetPixel(pos, 0, 0, 0);     // Initialize all top-bits.
    }
    virtual void SetPixel(int pos, uint8_t red, uint8_t green, uint8_t blue) {
        if (pos < 0 || pos >= count_) return;
        uint16_t data = 0;
        data |= (1<<15);  // start bit
        data |= luminance_cie1931(5, red)   << 10;
        data |= luminance_cie1931(5, green) <<  5;
        data |= luminance_cie1931(5, blue)  <<  0;

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
        : LEDStrip(count), spi_(spi), gpio_(gpio) {
        const size_t startframe_size = 4;
        const size_t endframe_size = 8 + 8*(count / 16);
        const size_t bytes_needed = startframe_size + 4*count + endframe_size;

        spi_->RegisterDataGPIO(gpio, bytes_needed);

        // Four zero bytes as start-bytes
        spi_->SetBufferedByte(gpio_, 0, 0x00);
        spi_->SetBufferedByte(gpio_, 1, 0x00);
        spi_->SetBufferedByte(gpio_, 2, 0x00);
        spi_->SetBufferedByte(gpio_, 3, 0x00);

        // Initialize all the top bits.
        for (int pos = 0; pos < count; ++pos)
            SetPixel(pos, 0, 0, 0);

        // We need a couple of more bits clocked at the end.
        for (size_t tail = 4 + 4*count; tail < bytes_needed; ++tail) {
            spi_->SetBufferedByte(gpio_, tail, 0xff);
        }
    }
    virtual void SetPixel(int pos, uint8_t red, uint8_t green, uint8_t blue) {
        if (pos < 0 || pos >= count_) return;
        const uint8_t global_brigthness = 0x1F;  // full brightness for now
        const int base = 4 + 4 * pos;
        spi_->SetBufferedByte(gpio_, base + 0, 0xE0 | global_brigthness);
        spi_->SetBufferedByte(gpio_, base + 1, luminance_cie1931(8, red));
        spi_->SetBufferedByte(gpio_, base + 1, luminance_cie1931(8, green));
        spi_->SetBufferedByte(gpio_, base + 1, luminance_cie1931(8, blue));
    }

private:
    MultiSPI *const spi_;
    const int gpio_;
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
