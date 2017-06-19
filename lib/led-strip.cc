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

#define DO_CIE1931		FALSE


/*
typedef uint16_t CIEValue;

// Do CIE1931 luminance correction and scale to maximum expected output bits.
static CIEValue luminance_cie1931_internal(uint8_t c, uint8_t brightness) {
    const float out_factor = 0xFFFF;
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
static CIEValue luminance_cie1931(uint8_t value, uint8_t bright) {
    static const CIEValue *const luminance_lookup = CreateCIE1931LookupTable();
    return luminance_lookup[bright * 256 + value];
}
*/

namespace spixels {
LEDStrip::LEDStrip(int pixelCount)
    : pixelCount_(pixelCount)
/*
    , values_(new RGBc[pixelCount])
*/
    , brightnessScale16_(0x10000)
{
}

LEDStrip::~LEDStrip()
{
//	delete values_;
}

/*
void LEDStrip::SetPixel(int pos, const RGBc& c) {
    if (pos < 0 || pos >= pixelCount()) return;
    values_[pos] = c;
    SetLinearValues(pos,
                    luminance_cie1931(c.r, brightness_),
                    luminance_cie1931(c.g, brightness_),
                    luminance_cie1931(c.b, brightness_));
}

void LEDStrip::SetBrightness(uint8_t new_brightness) {
    if (new_brightness == brightness_) return;
    brightness_ = new_brightness;
    for (int i = 0; i < pixelCount_; ++i) {
        SetPixel(i, values_[i]);  // Force recalculation.
    }
}
*/


namespace {


/////////////////////////////////////////////////
//#pragma mark - WS2801LedStrip:

#define WS_start_frame_bytes	0
#define WS_pixel_bytes			3
#define WS_end_frame_bytes		0

class WS2801LedStrip : public LEDStrip
{
public:
    WS2801LedStrip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount)
    : LEDStrip(pixelCount)
    , spi_(spi)
    , pin_(pin)
    {
        const size_t frame_bytes = WS_start_frame_bytes + WS_pixel_bytes * pixelCount + WS_end_frame_bytes;
        
        spi_->RegisterDataGPIO(pin, frame_bytes);
    }

    virtual void SetPixel8(uint32_t pixel_index, uint8_t red, uint8_t green, uint8_t blue)
    {
    	uint32_t		const scale = brightnessScale16_;
    	
    	if (scale > 0x10000)
    	{
    		red = (uint8_t)std::min(red * scale / 0x10000, (uint32_t)0xFF);
    		green = (uint8_t)std::min(green * scale / 0x10000, (uint32_t)0xFF);
    		blue = (uint8_t)std::min(blue * scale / 0x10000, (uint32_t)0xFF);
    	}
    	else if (scale < 0x10000)
    	{
    		red = (uint8_t)(red * scale / 0x10000);
    		green = (uint8_t)(green * scale / 0x10000);
    		blue = (uint8_t)(blue * scale / 0x10000);
    	}

		uint32_t		const offset = WS_start_frame_bytes + WS_pixel_bytes * pixel_index;

        spi_->SetBufferedByte(pin_, offset + 0, red);
        spi_->SetBufferedByte(pin_, offset + 1, green);
        spi_->SetBufferedByte(pin_, offset + 2, blue);
    }

private:
    MultiSPI*		const spi_;
    MultiSPI::Pin	const pin_;
};


/////////////////////////////////////////////////
//#pragma mark - LPD6803LedStrip:

#define LPD_start_frame_bytes	4
#define LPD_pixel_bytes			2
#define LPD_end_frame_bytes		4

class LPD6803LedStrip : public LEDStrip
{
public:
    LPD6803LedStrip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount)
    : LEDStrip(pixelCount)
    , spi_(spi)
    , pin_(pin)
    {
        const size_t frame_bytes = LPD_start_frame_bytes + LPD_pixel_bytes * pixelCount + LPD_end_frame_bytes;
        
        spi_->RegisterDataGPIO(pin, frame_bytes);

		// These zeroings shouldn't be required since RegisterDataGPIO() clears all bytes
		// But until it can be tested, we leave this in.
        // Four zero bytes as start-bytes for lpd6803
        spi_->SetBufferedByte(pin_, 0, 0x00);
        spi_->SetBufferedByte(pin_, 1, 0x00);
        spi_->SetBufferedByte(pin_, 2, 0x00);
        spi_->SetBufferedByte(pin_, 3, 0x00);
        for (int pixel_index = 0; pixel_index < pixelCount; ++pixel_index)
        {
            SetPixel8(pixel_index, 0, 0, 0);     // Initialize all top-bits.
        }
    }

    virtual void SetPixel8(uint32_t pixel_index, uint8_t red, uint8_t green, uint8_t blue)
    {
    	uint32_t		const scale = brightnessScale16_;
    	
    	if (scale > 0x10000)
    	{
    		red = (uint8_t)std::min(red * scale / 0x10000, (uint32_t)0xFF);
    		green = (uint8_t)std::min(green * scale / 0x10000, (uint32_t)0xFF);
    		blue = (uint8_t)std::min(blue * scale / 0x10000, (uint32_t)0xFF);
    	}
    	else if (scale < 0x10000)
    	{
    		red = (uint8_t)(red * scale / 0x10000);
    		green = (uint8_t)(green * scale / 0x10000);
    		blue = (uint8_t)(blue * scale / 0x10000);
    	}
    	
        uint16_t		data;
        
        data = (1<<15);  // start bit
        data |= (red >> 3) << 10;
        data |= (green >> 3) <<  5;
        data |= (blue >> 3) <<  0;

		uint32_t		const offset = LPD_start_frame_bytes + LPD_pixel_bytes * pixel_index;
		
        spi_->SetBufferedByte(pin_, offset + 0, data >> 8);
        spi_->SetBufferedByte(pin_, offset + 1, data & 0xFF);
    }

private:
    MultiSPI*		const spi_;
    MultiSPI::Pin	const pin_;
};


/////////////////////////////////////////////////
//#pragma mark - APA102LedStrip:

#define APA_start_frame_bytes	4
#define APA_pixel_bytes			4
#define APA_latch_frame_bytes	4	// extra end-frame bytes for compatibility with SK9822 chips

class APA102LedStrip : public LEDStrip
{
public:
    APA102LedStrip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount)
	: LEDStrip(pixelCount)
	, spi_(spi)
	, pin_(pin)
	{
        const size_t pixels_bytes = APA_pixel_bytes * pixelCount;
        const size_t end_frame_bytes = APA_latch_frame_bytes + (pixelCount + 15) / 16;
        const size_t frame_bytes = APA_start_frame_bytes + pixels_bytes + end_frame_bytes;

        spi_->RegisterDataGPIO(pin, frame_bytes);

		uint32_t		index;
		
		index = 0;
		hardwareScaleInverseTable8_[0] = 0;
		while (++index < 32u)
		{
			hardwareScaleInverseTable8_[index] = (32u << 8) / index;
		}
		componentsScale16_ = 0x10000;
		hardwareBrightnessByte_ = 0x1F;

/*		These zeroings aren't needed since RegisterDataGPIO() zeros all bytes
        // Four zero bytes as start-bytes
        spi_->SetBufferedByte(pin_, 0, 0x00);
        spi_->SetBufferedByte(pin_, 1, 0x00);
        spi_->SetBufferedByte(pin_, 2, 0x00);
        spi_->SetBufferedByte(pin_, 3, 0x00);

        // Make sure the start bits are properly set.
        for (int pixel_index = 0; pixel_index < pixelCount; ++pixel_index)
        {
            SetPixel8(pixel_index, 0, 0, 0);
        }

        // We need a couple of more bits clocked at the end.
        for (size_t tail = APA_start_frame_bytes + pixels_bytes; tail < frame_bytes; ++tail)
        {
            spi_->SetBufferedByte(pin_, tail, 0xff);	// endframe should be 0s, not 1s!
        }
*/
    }

	virtual void SetBrightnessScale16(uint32_t brightnessScale16)
	{
		brightnessScale16_ = brightnessScale16;
		
    	if (brightnessScale16 < 0x10000)
    	{
    		uint32_t		hardwareScale5;
    		
    		// brightnessScale16 is 0x10000 for no scaling.
    		// For APA102, we squeeze this range down to 0...32.
    		// We fudge 32 into 31 so that 50%, 25%, and other common scales have pretty math.
    		// We disallow 0 since that would just result in blackness.
    		hardwareScale5 = brightnessScale16 >> 11;
    		hardwareScale5 = std::min(hardwareScale5, 31u);
    		hardwareScale5 = std::max(hardwareScale5, 1u);
    		hardwareBrightnessByte_ = (uint8_t)hardwareScale5 | 0xE0;
    		
    		// componentsScale16_ scales down the compoent values directly.
    		// It provides more precision, in addition to the crude, 5-bit hardware brightness.
    		componentsScale16_ = brightnessScale16 * 32u / hardwareScale5;
    	}
    	else
    	{
    		// If scaling up or not scaling, don't use any hardware scaling.
    		// Just let componentsScale16_ do it.
    		hardwareBrightnessByte_ = 0xFF;
    		componentsScale16_ = brightnessScale16;
    	}
//		fprintf(stderr, "SetBrightnessScale16() - hardware=0x%02X, components=0x%04X\n",
//				hardwareBrightnessByte_ & 0x1F, componentsScale16_);
	}
	
    virtual void SetPixel8(uint32_t pixel_index, uint8_t red, uint8_t green, uint8_t blue)
    {
    	uint32_t		const scale = componentsScale16_;
    	
    	if (scale > 0x10000)
    	{
    		red = (uint8_t)std::min(red * scale / 0x10000, 0xFFu);
    		green = (uint8_t)std::min(green * scale / 0x10000, 0xFFu);
    		blue = (uint8_t)std::min(blue * scale / 0x10000, 0xFFu);
    	}
    	else if (scale < 0x10000)
    	{
    		red = (uint8_t)(red * scale / 0x10000);
    		green = (uint8_t)(green * scale / 0x10000);
    		blue = (uint8_t)(blue * scale / 0x10000);
    	}

        int				const offset = APA_start_frame_bytes + APA_pixel_bytes * pixel_index;

		spi_->SetBufferedByte(pin_, offset + 0, hardwareBrightnessByte_);
        spi_->SetBufferedByte(pin_, offset + 1, blue);
        spi_->SetBufferedByte(pin_, offset + 2, green);
        spi_->SetBufferedByte(pin_, offset + 3, red);
    }
    virtual void SetPixel16(uint32_t pixel_index, uint16_t red, uint16_t green, uint16_t blue)
    {
    	uint32_t		const scale = brightnessScale16_;

    	if (scale > 0x10000)
    	{
    		red = (uint16_t)std::min(red * scale / 0x10000, 0xFFFFu);
    		green = (uint16_t)std::min(green * scale / 0x10000, 0xFFFFu);
    		blue = (uint16_t)std::min(blue * scale / 0x10000, 0xFFFFu);
    	}
    	else if (scale < 0x10000)
    	{
    		red = (uint16_t)(red * scale / 0x10000);
    		green = (uint16_t)(green * scale / 0x10000);
    		blue = (uint16_t)(blue * scale / 0x10000);
    	}

		uint32_t		const maxHigh5Bits = std::max(std::max(red, green), blue) >> 11;
		uint32_t		const hardwareScale = std::min(maxHigh5Bits + 1u, 31u);
		uint32_t		const inverse8 = hardwareScaleInverseTable8_[hardwareScale];
		
		red = (uint16_t)(red * inverse8 / 0x10000);
		green = (uint16_t)(green * inverse8 / 0x10000);
		blue = (uint16_t)(blue * inverse8 / 0x10000);
		
        int				const offset = APA_start_frame_bytes + APA_pixel_bytes * pixel_index;

		spi_->SetBufferedByte(pin_, offset + 0, (uint8_t)(0xE0 | hardwareScale));
        spi_->SetBufferedByte(pin_, offset + 1, (uint8_t)blue);
        spi_->SetBufferedByte(pin_, offset + 2, (uint8_t)green);
        spi_->SetBufferedByte(pin_, offset + 3, (uint8_t)red);
    }

private:
    MultiSPI*		const spi_;
    MultiSPI::Pin	const pin_;
    uint32_t		componentsScale16_;					// scales component values before sending to spi_
    uint8_t			hardwareBrightnessByte_;			// byte sent to APA102 when SetPixel8() used
   	uint32_t		hardwareScaleInverseTable8_[32];	// used by SetPixel16() to quickly divide
};


}  // anonymous namespace


// Public interface
LEDStrip *CreateWS2801Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount) {
    return new WS2801LedStrip(spi, pin, pixelCount);
}
LEDStrip *CreateLPD6803Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount) {
    return new LPD6803LedStrip(spi, pin, pixelCount);
}
LEDStrip *CreateAPA102Strip(MultiSPI *spi, MultiSPI::Pin pin, int pixelCount) {
    return new APA102LedStrip(spi, pin, pixelCount);
}


}  // spixels namespace
