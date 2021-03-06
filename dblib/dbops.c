/*
 * PANDA -- a simple transaction monitor
 * Copyright (C) 2004-2008 Ogochan.
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

/*
#define	DEBUG
#define	TRACE
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <signal.h>

#include "const.h"
#include "enum.h"
#include "libmondai.h"
#include "directory.h"
#include "dbgroup.h"
#include "dbops.h"
#include "debug.h"

extern ValueStruct *_DBOPEN(DBG_Struct *dbg, DBCOMM_CTRL *ctrl) {
  dbg->dbstatus = DB_STATUS_CONNECT;
  ctrl->rc = MCP_OK;
  return (NULL);
}

extern ValueStruct *_DBDISCONNECT(DBG_Struct *dbg, DBCOMM_CTRL *ctrl) {
  dbg->dbstatus = DB_STATUS_DISCONNECT;
  ctrl->rc = MCP_OK;
  return (NULL);
}

extern ValueStruct *_DBSTART(DBG_Struct *dbg, DBCOMM_CTRL *ctrl) {
  ctrl->rc = MCP_OK;
  return (NULL);
}

extern ValueStruct *_DBCOMMIT(DBG_Struct *dbg, DBCOMM_CTRL *ctrl) {
  ctrl->rc = MCP_OK;
  return (NULL);
}

extern int _EXEC(DBG_Struct *dbg, char *sql) {
  return (MCP_OK);
}

extern ValueStruct *_QUERY(DBG_Struct *dbg, char *sql) {
  return NULL;
}

extern ValueStruct *_DBACCESS(DBG_Struct *dbg, DBCOMM_CTRL *ctrl,
                              RecordStruct *rec, ValueStruct *args) {
  ValueStruct *ret;

  ret = NULL;
  if (rec->type != RECORD_DB) {
    ctrl->rc = MCP_BAD_ARG;
  } else {
    ctrl->rc = MCP_OK;
  }
  return (ret);
}
