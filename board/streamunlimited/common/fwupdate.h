/*
 * fwupdate.h
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

#ifndef __FWUPDATE_H__
#define __FWUPDATE_H__

#include "device_interface.h"

#define FWUP_FLAG_UPDATE_INDEX	0
#define FWUP_FLAG_FAIL_INDEX	1
#define FWUP_FLAG_SWRESET_REQ	2	/* This flag will be set when we request a reset using the `reset` command */

int fwupdate_init(const struct sue_device_info *device_info);

#endif /* __FWUPDATE_H__ */
