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


namespace spixels {


LEDStrip::LEDStrip(int pixelCount)
: pixelCount_(pixelCount) {
    redScale_ = 1.0f;
    greenScale_ = 1.0f;
    blueScale_ = 1.0f;

    redScale16_ = 0x10000;
    greenScale16_ = 0x10000;
    blueScale16_ = 0x10000;
}


namespace {


/////////////////////////////////////////////////
//#pragma mark - WS2801LedStrip:

#define WS_start_frame_bytes    0
#define WS_pixel_bytes          3
#define WS_end_frame_bytes      0

class WS2801LedStrip : public LEDStrip {
public:
    WS2801LedStrip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount, ComponentOrder component_order)
    : LEDStrip(pixelCount)
    , spi_(spi)
    , pin_(pin) 
    , red_offset_(component_order >> 8)
    , green_offset_((component_order >> 4) & 0xF)
    , blue_offset_(component_order & 0xF)
    {
        const size_t frame_bytes = WS_start_frame_bytes + WS_pixel_bytes * pixelCount + WS_end_frame_bytes;

        spi_->RegisterDataGPIO(pin, frame_bytes);
    }

    virtual void SetPixel8(uint32_t pixel_index, uint8_t red, uint8_t green, uint8_t blue) {
        if (pixel_index < (uint32_t)pixelCount_) {
            red = (uint8_t)std::min(red * redScale16_ / 0x10000, (uint32_t)0xFF);
            green = (uint8_t)std::min(green * greenScale16_ / 0x10000, (uint32_t)0xFF);
            blue = (uint8_t)std::min(blue * blueScale16_ / 0x10000, (uint32_t)0xFF);

            uint32_t        const offset = WS_start_frame_bytes + WS_pixel_bytes * pixel_index;

            spi_->SetBufferedByte(pin_, offset + red_offset_, blue);
            spi_->SetBufferedByte(pin_, offset + green_offset_, green);
            spi_->SetBufferedByte(pin_, offset + blue_offset_, red);
        }
    }

private:
    MultiSPI*       const spi_;
    MultiSPI::Pin   const pin_;
    unsigned long   const red_offset_;
    unsigned long   const green_offset_;
    unsigned long   const blue_offset_;
};


/////////////////////////////////////////////////
//#pragma mark - LPD6803LedStrip:

#define LPD6803_start_frame_bytes   4
#define LPD6803_pixel_bytes         2
#define LPD6803_end_frame_bytes     4

class LPD6803LedStrip : public LEDStrip {
public:
    LPD6803LedStrip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount, ComponentOrder component_order)
    : LEDStrip(pixelCount)
    , spi_(spi)
    , pin_(pin)
    , component_order_(component_order)
    {
        const size_t frame_bytes = LPD6803_start_frame_bytes + LPD6803_pixel_bytes * pixelCount + LPD6803_end_frame_bytes;

        spi_->RegisterDataGPIO(pin, frame_bytes);

        // These zeroings shouldn't be required since RegisterDataGPIO() clears all bytes
        // But until it can be tested, we leave this in.
        // Four zero bytes as start-bytes for lpd6803
        spi_->SetBufferedByte(pin_, 0, 0x00);
        spi_->SetBufferedByte(pin_, 1, 0x00);
        spi_->SetBufferedByte(pin_, 2, 0x00);
        spi_->SetBufferedByte(pin_, 3, 0x00);
        for (int pixel_index = 0; pixel_index < pixelCount; ++pixel_index) {
            SetPixel8(pixel_index, 0, 0, 0);     // Initialize all top-bits.
        }
    }

    virtual void SetPixel8(uint32_t pixel_index, uint8_t red, uint8_t green, uint8_t blue) {
        if (pixel_index < (uint32_t)pixelCount_) {
            red = (uint8_t)std::min(red * redScale16_ / 0x10000, (uint32_t)0xFF);
            green = (uint8_t)std::min(green * greenScale16_ / 0x10000, (uint32_t)0xFF);
            blue = (uint8_t)std::min(blue * blueScale16_ / 0x10000, (uint32_t)0xFF);

            uint16_t        data;

            data = (1<<15);  // start bit
            
            switch (component_order_)
            {
            case Order_RGB :
                data |= (uint16_t)(red >> 3) << 10;
                data |= (uint16_t)(green >> 3) << 5;
                data |= (uint16_t)(blue >> 3) << 0;
                break;
            case Order_RBG :
                data |= (uint16_t)(red >> 3) << 10;
                data |= (uint16_t)(blue >> 3) << 5;
                data |= (uint16_t)(green >> 3) <<   0;
                break;
            case Order_GRB :
                data |= (uint16_t)(green >> 3) << 10;
                data |= (uint16_t)(red >> 3) << 5;
                data |= (uint16_t)(blue >> 3) << 0;
                break;
            case Order_GBR :
                data |= (uint16_t)(green >> 3) << 10;
                data |= (uint16_t)(blue >> 3) << 5;
                data |= (uint16_t)(red >> 3) << 0;
                break;
            case Order_BRG :
                data |= (uint16_t)(blue >> 3) << 10;
                data |= (uint16_t)(red >> 3) << 5;
                data |= (uint16_t)(green >> 3) <<   0;
                break;
            case Order_BGR :
            default :
                data |= (uint16_t)(blue >> 3) << 10;
                data |= (uint16_t)(green >> 3) << 5;
                data |= (uint16_t)(red >> 3) << 0;
                break;
            }

            uint32_t        const offset = LPD6803_start_frame_bytes + LPD6803_pixel_bytes * pixel_index;

            spi_->SetBufferedByte(pin_, offset + 0, data >> 8);
            spi_->SetBufferedByte(pin_, offset + 1, data & 0xFF);
        }
    }

private:
    MultiSPI*       const spi_;
    MultiSPI::Pin   const pin_;
    ComponentOrder  const component_order_;
};


/////////////////////////////////////////////////
//#pragma mark - LPD8806LedStrip:

#define LPD8806_start_frame_bytes   1
#define LPD8806_pixel_bytes         3
#define LPD8806_end_frame_bytes     0

class LPD8806_LedStrip : public LEDStrip {
public:
    LPD8806_LedStrip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount, ComponentOrder component_order)
    : LEDStrip(pixelCount)
    , spi_(spi)
    , pin_(pin)
    , red_offset_(component_order >> 8)
    , green_offset_((component_order >> 4) & 0xF)
    , blue_offset_(component_order & 0xF)
    {
        const size_t frame_bytes = LPD8806_start_frame_bytes +
                                    LPD8806_pixel_bytes * pixelCount +
                                    LPD8806_end_frame_bytes;

        spi_->RegisterDataGPIO(pin, frame_bytes);

        // These zeroings shouldn't be required since RegisterDataGPIO() clears all bytes
        // But until it can be tested, we leave this in.
        // Four zero bytes as start-bytes for lpd6803
        spi_->SetBufferedByte(pin_, 0, 0x00);
        for (int pixel_index = 0; pixel_index < pixelCount; ++pixel_index) {
            SetPixel8(pixel_index, 0, 0, 0);     // Initialize all top-bits.
        }
    }

    virtual void SetPixel8(uint32_t pixel_index, uint8_t red, uint8_t green, uint8_t blue) {
        if (pixel_index < (uint32_t)pixelCount_) {
            uint32_t        const offset = LPD8806_start_frame_bytes + LPD8806_pixel_bytes * pixel_index;

            red = (uint8_t)std::min(red * redScale16_ / 0x10000, (uint32_t)0xFF);
            green = (uint8_t)std::min(green * greenScale16_ / 0x10000, (uint32_t)0xFF);
            blue = (uint8_t)std::min(blue * blueScale16_ / 0x10000, (uint32_t)0xFF);

            spi_->SetBufferedByte(pin_, offset + red_offset_, (uint8_t)((red >> 1) | 0x80));
            spi_->SetBufferedByte(pin_, offset + green_offset_, (uint8_t)((green >> 1) | 0x80));
            spi_->SetBufferedByte(pin_, offset + blue_offset_, (uint8_t)((blue >> 1) | 0x80));
        }
    }

private:
    MultiSPI*       const spi_;
    MultiSPI::Pin   const pin_;
    unsigned long   const red_offset_;
    unsigned long   const green_offset_;
    unsigned long   const blue_offset_;
};


/////////////////////////////////////////////////
//#pragma mark - APA102LedStrip:

// The APA102 protocol has an extra byte for each pixel, 5 bits of which control its "global brightness".
// This value is multipled by each component value to adjust the brightness of the entire pixel.
// In the APA102, this is implemented by switching to 13-bit PWM.
// Therefore, it is assumed that brightness dims linearly with "global brightness".
// SetBrightnessScale() makes use of this parameter, choosing a 5-bit value to send for every pixel,
// every frame.  It also stores 32-bit values that are multipled by each component for finer adjustment.
// When SetPixel16() is called, the global brightness value for each pixel is calculated on the fly.

#define APA_start_frame_bytes   4
#define APA_pixel_bytes         4
#define APA_latch_frame_bytes   4   // extra end-frame bytes for compatibility with SK9822 chips

class APA102LedStrip : public LEDStrip {
public:
    APA102LedStrip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount, ComponentOrder component_order)
    : LEDStrip(pixelCount)
    , spi_(spi)
    , pin_(pin)
    , red_offset_(3 - (component_order >> 8))
    , green_offset_(3 - ((component_order >> 4) & 0xF))
    , blue_offset_(3 - (component_order & 0xF))
    {
        const size_t pixels_bytes = APA_pixel_bytes * pixelCount;
        const size_t end_frame_bytes = APA_latch_frame_bytes + (pixelCount + 15) / 16;
        const size_t frame_bytes = APA_start_frame_bytes + pixels_bytes + end_frame_bytes;

        spi_->RegisterDataGPIO(pin, frame_bytes);

        componentsRedScale16_ = 0x10000;
        componentsGreenScale16_ = 0x10000;
        componentsBlueScale16_ = 0x10000;
        hardwareBrightnessByte_ = 0x1F;

        if (!hardwareInverseTable8Created_) {
            uint32_t        hardwareScale5;

            hardwareScale5 = 0;
            hardwareInverseTable8_[0] = 0;
            while (++hardwareScale5 < 32u) {
                hardwareInverseTable8_[hardwareScale5] = (31u << 8) / hardwareScale5;
            }
            hardwareInverseTable8Created_ = true;
        }
    }

    virtual void SetBrightnessScale(float redScale, float greenScale, float blueScale) {
        LEDStrip::SetBrightnessScale(redScale, greenScale, blueScale);

        uint32_t        const maxScale = std::max(std::max(redScale16_, greenScale16_), blueScale16_);

        if (maxScale < 0x10000) {
            uint32_t        hardwareScale5;

            // maxScale is 0x10000 for no scaling.
            // For APA102, we squeeze this range down to 0...31
            // We disallow 0 since that would just result in blackness.
            hardwareScale5 = (maxScale + 0x03FFu) >> 11;
            hardwareScale5 = std::min(hardwareScale5, 31u);
            hardwareScale5 = std::max(hardwareScale5, 1u);
            hardwareBrightnessByte_ = (uint8_t)hardwareScale5 | 0xE0;

            // componentsScale16_ scales down the compoent values directly.
            // It provides more precision, in addition to the crude, 5-bit hardware brightness.
            componentsRedScale16_ = redScale16_ * 31u / hardwareScale5;
            componentsGreenScale16_ = greenScale16_ * 31u / hardwareScale5;
            componentsBlueScale16_ = blueScale16_ * 31u / hardwareScale5;
        } else {
            // If scaling up or not scaling, don't use any hardware scaling.
            // Just let componentsScale16_ do it.
            hardwareBrightnessByte_ = 0xFF;
            componentsRedScale16_ = redScale16_;
            componentsGreenScale16_ = greenScale16_;
            componentsBlueScale16_ = blueScale16_;
        }

//      fprintf(stderr, "APA102LedStrip::SetBrightnessScale() - input=0x%04X, hardware=0x%02X, red=0x%04X, green=0x%04X, blue=0x%04X\n",
//              maxScale, hardwareBrightnessByte_,
//              componentsRedScale16_, componentsGreenScale16_, componentsBlueScale16_);
    }

    virtual void SetPixel8(uint32_t pixel_index, uint8_t red, uint8_t green, uint8_t blue) {
        if (pixel_index < (uint32_t)pixelCount_) {
            red = (uint8_t)std::min(red * componentsRedScale16_ / 0x10000u, 0xFFu);
            green = (uint8_t)std::min(green * componentsGreenScale16_ / 0x10000u, 0xFFu);
            blue = (uint8_t)std::min(blue * componentsBlueScale16_ / 0x10000u, 0xFFu);

            int             const offset = APA_start_frame_bytes + APA_pixel_bytes * pixel_index;

            spi_->SetBufferedByte(pin_, offset + 0, hardwareBrightnessByte_);
            spi_->SetBufferedByte(pin_, offset + red_offset_, red);
            spi_->SetBufferedByte(pin_, offset + green_offset_, green);
            spi_->SetBufferedByte(pin_, offset + blue_offset_, blue);
        }
    }

    virtual void SetPixel16(uint32_t pixel_index, uint16_t red, uint16_t green, uint16_t blue) {
        if (pixel_index < (uint32_t)pixelCount_) {
            red = (uint16_t)std::min((uint32_t)((uint64_t)red * redScale16_ / 0x10000), (uint32_t)0xFFFFu);
            green = (uint16_t)std::min((uint32_t)((uint64_t)green * greenScale16_ / 0x10000), (uint32_t)0xFFFFu);
            blue = (uint16_t)std::min((uint32_t)((uint64_t)blue * blueScale16_ / 0x10000), (uint32_t)0xFFFFu);

            uint32_t        const maxComponent = std::max(std::max(red, green), blue);
            uint32_t        const high5Bits = (maxComponent + 1 + 0x03FF) >> 11;
            uint32_t        const hardwareScale5 = std::max(std::min(high5Bits, 31u), 1u);
            uint32_t        const hardwareInverse8 = hardwareInverseTable8_[hardwareScale5];

            red = (uint16_t)std::min(red * hardwareInverse8 / 0x10000, 0xFFu);
            green = (uint16_t)std::min(green * hardwareInverse8 / 0x10000, 0xFFu);
            blue = (uint16_t)std::min(blue * hardwareInverse8 / 0x10000, 0xFFu);

            int             const offset = APA_start_frame_bytes + APA_pixel_bytes * pixel_index;

            spi_->SetBufferedByte(pin_, offset + 0, (uint8_t)(0xE0 | hardwareScale5));
            spi_->SetBufferedByte(pin_, offset + red_offset_, (uint8_t)red);
            spi_->SetBufferedByte(pin_, offset + green_offset_, (uint8_t)green);
            spi_->SetBufferedByte(pin_, offset + blue_offset_, (uint8_t)blue);
        }
    }

private:
    MultiSPI*       const spi_;
    MultiSPI::Pin   const pin_;
    unsigned long   const red_offset_;
    unsigned long   const green_offset_;
    unsigned long   const blue_offset_;
    uint32_t        componentsRedScale16_;              // scales component values before sending to spi_
    uint32_t        componentsGreenScale16_;            // scales component values before sending to spi_
    uint32_t        componentsBlueScale16_;             // scales component values before sending to spi_
    uint8_t         hardwareBrightnessByte_;            // byte sent to APA102 when SetPixel8() used

static bool         hardwareInverseTable8Created_;
static uint32_t     hardwareInverseTable8_[32];         // used by SetPixel16() to quickly divide
};

bool            APA102LedStrip::hardwareInverseTable8Created_ = false;
uint32_t        APA102LedStrip::hardwareInverseTable8_[32];


/////////////////////////////////////////////////
//#pragma mark - SK9822LedStrip:

// APA102 and SK9822 strips have the same protocol, but they implement
// "global brightness" differently.
// APA102 uses 13-bit PWM to implement global brightness.  This is assumed
// to be linear.
// SK9822 uses a variable current driver, which is inherently nonlinear.
// When pixels are dimmed with global brightness, they appear brighter than they
// do in an APA102 chip.  Also, pixels appear to get more blue/green as they
// are dimmed this way.
// Therefore, SK9822LedStrip{} has separate "hardware inverse" tables for red, green, and blue.
// They are created slightly differently, to adjust color as lower and lower global brightness
// values are used.  When no hardware dimmings is used, they don't scale components at all.

class SK9822LedStrip : public LEDStrip {
public:
    SK9822LedStrip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount, ComponentOrder component_order)
    : LEDStrip(pixelCount)
    , spi_(spi)
    , pin_(pin)
    , red_offset_(3 - (component_order >> 8))
    , green_offset_(3 - ((component_order >> 4) & 0xF))
    , blue_offset_(3 - (component_order & 0xF))
    {
        const size_t pixels_bytes = APA_pixel_bytes * pixelCount;
        const size_t end_frame_bytes = APA_latch_frame_bytes + (pixelCount + 15) / 16;
        const size_t frame_bytes = APA_start_frame_bytes + pixels_bytes + end_frame_bytes;

        spi_->RegisterDataGPIO(pin, frame_bytes);

        componentsRedScale16_ = 0x10000;
        componentsGreenScale16_ = 0x10000;
        componentsBlueScale16_ = 0x10000;
        hardwareBrightnessByte_ = 0x1F;

        if (!hardwareInverseTablesCreated_) {
            uint32_t        hardwareScale5;

            hardwareScale5 = 0;
            hardwareRedInverseTable8_[0] = 0;
            hardwareGreenInverseTable8_[0] = 0;
            hardwareBlueInverseTable8_[0] = 0;
            while (++hardwareScale5 < 32u) {
                hardwareRedInverseTable8_[hardwareScale5] = (31u * 0xF0) / hardwareScale5 + (0x100 - 0xF0);
                hardwareGreenInverseTable8_[hardwareScale5] = (31u * 0x80) / hardwareScale5 + (0x100 - 0x80);
                hardwareBlueInverseTable8_[hardwareScale5] = (31u * 0x80) / hardwareScale5 + (0x100 - 0x80);
            }
            hardwareInverseTablesCreated_ = true;
        }
    }

    virtual void SetBrightnessScale(float redScale, float greenScale, float blueScale) {
        LEDStrip::SetBrightnessScale(redScale, greenScale, blueScale);

        uint32_t        const maxScale = std::max(std::max(redScale16_, greenScale16_), blueScale16_);

        if (maxScale < 0x10000) {
            uint32_t        hardwareScale5;

            // maxScale is 0x10000 for no scaling.
            // For SK9822, we squeeze this range down to 0...31
            // We disallow 0 since that would just result in blackness.
            hardwareScale5 = (maxScale + 0x03FFu) >> 11;
            hardwareScale5 = std::min(hardwareScale5, 31u);
            hardwareScale5 = std::max(hardwareScale5, 1u);
            hardwareBrightnessByte_ = (uint8_t)hardwareScale5 | 0xE0;

            // componentsScale16_ scales down the compoent values directly.
            // It provides more precision, in addition to the crude, 5-bit hardware brightness.
            componentsRedScale16_ = redScale16_ * hardwareRedInverseTable8_[hardwareScale5] / 0x100;
            componentsGreenScale16_ = greenScale16_ * hardwareGreenInverseTable8_[hardwareScale5] / 0x100;
            componentsBlueScale16_ = blueScale16_ * hardwareBlueInverseTable8_[hardwareScale5] / 0x100;
        } else {
            // If scaling up or not scaling, don't use any hardware scaling.
            // Just let componentsScale16_ do it.
            hardwareBrightnessByte_ = 0xFF;
            componentsRedScale16_ = redScale16_;
            componentsGreenScale16_ = greenScale16_;
            componentsBlueScale16_ = blueScale16_;
        }

//      fprintf(stderr, "SK9822LedStrip::SetBrightnessScale() - input=0x%04X, hardware=0x%02X, red=0x%04X, green=0x%04X, blue=0x%04X\n",
//              maxScale, hardwareBrightnessByte_ & 0x1F,
//              componentsRedScale16_, componentsGreenScale16_, componentsBlueScale16_);
    }

    virtual void SetPixel8(uint32_t pixel_index, uint8_t red, uint8_t green, uint8_t blue) {
        if (pixel_index < (uint32_t)pixelCount_) {
            red = (uint8_t)std::min(red * componentsRedScale16_ / 0x10000u, 0xFFu);
            green = (uint8_t)std::min(green * componentsGreenScale16_ / 0x10000u, 0xFFu);
            blue = (uint8_t)std::min(blue * componentsBlueScale16_ / 0x10000u, 0xFFu);

            int             const offset = APA_start_frame_bytes + APA_pixel_bytes * pixel_index;

            spi_->SetBufferedByte(pin_, offset + 0, hardwareBrightnessByte_);
            spi_->SetBufferedByte(pin_, offset + red_offset_, red);
            spi_->SetBufferedByte(pin_, offset + green_offset_, green);
            spi_->SetBufferedByte(pin_, offset + blue_offset_, blue);
        }
    }

    virtual void SetPixel16(uint32_t pixel_index, uint16_t red, uint16_t green, uint16_t blue) {
        if (pixel_index < (uint32_t)pixelCount_) {
            red = (uint16_t)std::min((uint32_t)((uint64_t)red * redScale16_ / 0x10000), (uint32_t)0xFFFFu);
            green = (uint16_t)std::min((uint32_t)((uint64_t)green * greenScale16_ / 0x10000), (uint32_t)0xFFFFu);
            blue = (uint16_t)std::min((uint32_t)((uint64_t)blue * blueScale16_ / 0x10000), (uint32_t)0xFFFFu);

            uint32_t        const maxComponent = std::max(std::max(red, green), blue);
            uint32_t        const high5Bits = (maxComponent + 1 + 0x03FF) >> 11;
            uint32_t        const hardwareScale5 = std::max(std::min(high5Bits, 31u), 1u);

            red = (uint16_t)std::min(red * hardwareRedInverseTable8_[hardwareScale5] / 0x10000, 0xFFu);
            green = (uint16_t)std::min(green * hardwareGreenInverseTable8_[hardwareScale5] / 0x10000, 0xFFu);
            blue = (uint16_t)std::min(blue * hardwareBlueInverseTable8_[hardwareScale5] / 0x10000, 0xFFu);

            int             const offset = APA_start_frame_bytes + APA_pixel_bytes * pixel_index;

            spi_->SetBufferedByte(pin_, offset + 0, (uint8_t)(0xE0 | hardwareScale5));
            spi_->SetBufferedByte(pin_, offset + red_offset_, (uint8_t)red);
            spi_->SetBufferedByte(pin_, offset + green_offset_, (uint8_t)green);
            spi_->SetBufferedByte(pin_, offset + blue_offset_, (uint8_t)blue);
        }
    }

protected:
    MultiSPI*       const spi_;
    MultiSPI::Pin   const pin_;
    unsigned long   const red_offset_;
    unsigned long   const green_offset_;
    unsigned long   const blue_offset_;

private:
    uint32_t        componentsRedScale16_;              // scales component values before sending to spi_
    uint32_t        componentsGreenScale16_;            // scales component values before sending to spi_
    uint32_t        componentsBlueScale16_;             // scales component values before sending to spi_
    uint8_t         hardwareBrightnessByte_;            // byte sent to APA102 when SetPixel8() used

static bool         hardwareInverseTablesCreated_;
static uint32_t     hardwareRedInverseTable8_[32];      // used by SetPixel16() to quickly divide
static uint32_t     hardwareGreenInverseTable8_[32];    // used by SetPixel16() to quickly divide
static uint32_t     hardwareBlueInverseTable8_[32];     // used by SetPixel16() to quickly divide
};

bool            SK9822LedStrip::hardwareInverseTablesCreated_ = false;
uint32_t        SK9822LedStrip::hardwareRedInverseTable8_[32];
uint32_t        SK9822LedStrip::hardwareGreenInverseTable8_[32];
uint32_t        SK9822LedStrip::hardwareBlueInverseTable8_[32];


}  // anonymous namespace


// Public interface
LEDStrip *CreateWS2801Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount,
                            LEDStrip::ComponentOrder component_order) {
    return new WS2801LedStrip(spi, pin, pixelCount, component_order);
}
LEDStrip *CreateLPD6803Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount,
                                LEDStrip::ComponentOrder component_order) {
    return new LPD6803LedStrip(spi, pin, pixelCount, component_order);
}
LEDStrip *CreateLPD8806Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount,
                                LEDStrip::ComponentOrder component_order) {
    return new LPD8806_LedStrip(spi, pin, pixelCount, component_order);
}
LEDStrip *CreateAPA102Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount,
                            LEDStrip::ComponentOrder component_order) {
    return new APA102LedStrip(spi, pin, pixelCount, component_order);
}
LEDStrip *CreateSK9822Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount,
                            LEDStrip::ComponentOrder component_order) {
    return new SK9822LedStrip(spi, pin, pixelCount, component_order);
}


}  // spixels namespace
