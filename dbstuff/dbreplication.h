/*
 * PANDA -- a simple transaction monitor
 * Copyright (C) 2000-2008 Ogochan & JMA (Japan Medical Association).
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef	_DBREPLICATION_H
#define	_DBREPLICATION_H

#include "libmondai.h"
#include "comm.h"
#include "port.h"
#include "dblog.h"

#define DBREPLICATION_COMMAND_OK     0x00
#define DBREPLICATION_COMMAND_NG     0x01
#define DBREPLICATION_COMMAND_AUTH   0x02
#define DBREPLICATION_COMMAND_REQ    0x03
#define DBREPLICATION_COMMAND_RECORD 0x04
#define DBREPLICATION_COMMAND_EOR    0x05



#endif