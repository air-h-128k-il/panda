/*
 * PANDA -- a simple transaction monitor
 * Copyright (C) 2004-2008 Kouji TAKAO & JMA (Japan Medical Association).
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

#ifndef _INC_DIALOGS_H
#define _INC_DIALOGS_H

#include <gtk/gtk.h>

extern void MessageDialog(GtkMessageType type, const char *message);
extern void InfoDialog(const char *format, ...);
extern void WarnDialog(const char *format, ...);
extern void ErrorDialog(const char *format, ...);
extern char *AskPassDialog(const char *prompt);
extern char *AskPINDialog();
extern Bool ConfirmDialog(const char *message);

#endif
