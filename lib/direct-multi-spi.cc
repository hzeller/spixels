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
    explicit DirectMultiSPI(int speed_mhz, MultiSPI::Pin clockPin);
    virtual ~DirectMultiSPI();

    virtual bool RegisterDataGPIO(int gpio, size_t serial_byte_size);
    virtual void SetBufferedByte(MultiSPI::Pin pin, size_t pos, uint8_t data);
    virtual void SendBuffers();

private:
    const MultiSPI::Pin clockPin_;
    const int write_repeat_;  // how often write operations to repeat to slowdown
    ft::GPIO gpio_;
    size_t size_;
    uint32_t *gpio_data_;
};
}  // end anonymous namespace

DirectMultiSPI::DirectMultiSPI(int speed_mhz, MultiSPI::Pin clockPin)
: clockPin_(clockPin)
, write_repeat_(std::max(2, (int)roundf(30.0 / speed_mhz)))
, size_(0)
, gpio_data_(NULL)
{
    bool success = gpio_.Init();
    assert(success);  // gpio couldn't be initialized
    success = gpio_.AddOutput(clockPin);
    assert(success);  // clock pin not valid
}

DirectMultiSPI::~DirectMultiSPI()
{
	if (gpio_data_)
	{
	    free(gpio_data_);
	}
}

bool DirectMultiSPI::RegisterDataGPIO(int gpio, size_t serial_byte_size)
{
    if (serial_byte_size > size_)
    {
        const int prev_size = size_ * 8 * sizeof(uint32_t);
        const int new_size = serial_byte_size * 8 * sizeof(uint32_t);
        size_ = serial_byte_size;
		if (gpio_data_ == NULL)
		{
            gpio_data_ = (uint32_t*)malloc(new_size);
        } else {
            gpio_data_ = (uint32_t*)realloc(gpio_data_, new_size);
        }
        bzero(gpio_data_ + prev_size, new_size-prev_size);
    }

    return gpio_.AddOutput(gpio);
}

void DirectMultiSPI::SetBufferedByte(MultiSPI::Pin pin, size_t pos, uint8_t data)
{
	assert(pos < size_);
	
	uint32_t		const pinBit = 1 << pin;
	uint32_t		const pinNotBit = ~pinBit;
	uint32_t*		buffer_pos = gpio_data_ + 8 * pos;

#if 1
	for (uint8_t bit = 0x80; bit; bit >>= 1)
	{
		if (data & bit)
		{   // set
			*buffer_pos |= pinBit;
		}
		else
		{  // reset
			*buffer_pos &= pinNotBit;
		}
		buffer_pos++;
	}
#else
	// This unwound loop has no conditional branches, no broken pipeline!
	// Yet it goes slower than the above loop on the Pi.  Go figure...
	buffer_pos[7] = (buffer_pos[7] & pinNotBit) | ((data & 1) << pin);
	data >>= 1;
	buffer_pos[6] = (buffer_pos[6] & pinNotBit) | ((data & 1) << pin);
	data >>= 1;
	buffer_pos[5] = (buffer_pos[5] & pinNotBit) | ((data & 1) << pin);
	data >>= 1;
	buffer_pos[4] = (buffer_pos[4] & pinNotBit) | ((data & 1) << pin);
	data >>= 1;
	buffer_pos[3] = (buffer_pos[3] & pinNotBit) | ((data & 1) << pin);
	data >>= 1;
	buffer_pos[2] = (buffer_pos[2] & pinNotBit) | ((data & 1) << pin);
	data >>= 1;
	buffer_pos[1] = (buffer_pos[1] & pinNotBit) | ((data & 1) << pin);
	data >>= 1;
	buffer_pos[0] = (buffer_pos[0] & pinNotBit) | ((data & 1) << pin);
#endif
}

void DirectMultiSPI::SendBuffers()
{
	uint32_t*		const end = gpio_data_ + 8 * size_;
	
	for (uint32_t *data = gpio_data_; data < end; ++data)
	{
	    uint32_t		d;
	    
	    d = *data;
	    for (int i = 0; i < write_repeat_; ++i)
	    {
	    	gpio_.Write(d);
	    }
	    
	    d |= 1 << clockPin_;   // pos clock edge.
	    for (int i = 0; i < write_repeat_; ++i)
	    {
	    	gpio_.Write(d);
	    }
	}
	gpio_.Write(0);  // Reset clock.
}

// Public interface
MultiSPI *CreateDirectMultiSPI(int speed_mhz, MultiSPI::Pin clockPin) {
    return new DirectMultiSPI(speed_mhz, clockPin);
}


}  // namespace spixels
