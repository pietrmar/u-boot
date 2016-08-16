/*
 * adc.h
 *
 * Copyright (C) 2016, StreamUnlimited Engineering GmbH, http://www.streamunlimited.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR /PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ADC_H__
#define __ADC_H__

int init_adc(int adc_num);
int shutdown_adc(int adc_num);
int read_adc_channel(int adc_num, int channel);

#endif /* __ADC_H__ */
