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
 *
 */

/*
#define	DEBUG
#define	TRACE
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _D_PARSER
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "libmondai.h"
#include "mhandler.h"
#include "DDparser.h"
#include "Dlex.h"
#include "LDparser.h"
#include "DBparser.h"
#include "directory.h"
#include "dirs.h"
#include "debug.h"

static TokenTable tokentable[] = {{"data", T_DATA},
                                  {"host", T_HOST},
                                  {"name", T_NAME},
                                  {"home", T_HOME},
                                  {"port", T_PORT},
                                  {"spa", T_SPA},
                                  {"window", T_WINDOW},
                                  {"cache", T_CACHE},
                                  {"arraysize", T_ARRAYSIZE},
                                  {"textsize", T_TEXTSIZE},
                                  {"db", T_DB},
                                  {"multiplex_group", T_MGROUP},
                                  {"bind", T_BIND},
                                  {"bindapi", T_BINDAPI},

                                  {"handler", T_HANDLER},
                                  {"class", T_CLASS},
                                  {"serialize", T_SERIALIZE},
                                  {"start", T_START},
                                  {"locale", T_CODING},
                                  {"coding", T_CODING},
                                  {"encoding", T_ENCODING},
                                  {"handlerpath", T_HANDLERPATH},
                                  {"loadpath", T_LOADPATH},
                                  {"bigendian", T_BIGENDIAN},

                                  {"", 0}};

static GHashTable *Reserved;

static GHashTable *Records;

extern RecordStruct *GetWindow(char *name) {
  RecordStruct *rec;
  char fname[SIZE_LONGNAME + 1];

  if (name != NULL) {
    rec = (RecordStruct *)g_hash_table_lookup(Records, name);
    if (rec == NULL) {
      snprintf(fname,SIZE_LONGNAME,"%s.rec", name);
      fname[SIZE_LONGNAME] = 0;
      rec = ReadRecordDefine(fname,!GetScrRecMemSave());
      if (rec != NULL) {
        g_hash_table_insert(Records, g_strdup(name), rec);
      } else {
        Warning("window record not found [%s].", name);
        rec = NULL;
      }
    }
  } else {
    rec = NULL;
    Warning("window name is null.");
  }
  return (rec);
}

static void ParWindow(CURFILE *in, LD_Struct *ld) {
  RecordStruct *window;
  RecordStruct **wn;
  RecordStructMeta **wnm;
  char wname[SIZE_NAME + 1],rname[SIZE_LONGNAME+1];

  window = NULL;
  if (GetSymbol != '{') {
    ParError("syntax error");
  } else {
    while (GetName != '}') {
      if (ComToken == T_SYMBOL) {
        strcpy(wname, ComSymbol);
        if ((int)(long)g_hash_table_lookup(ld->whash, wname) > 0) {
          ParError("duplicate window name");
        } else {
          window = GetWindow(wname);
        }
        wn = (RecordStruct **)xmalloc(sizeof(RecordStruct *) *
                                      (ld->cWindow + 1));
        wnm = (RecordStructMeta **)xmalloc(sizeof(RecordStructMeta *) *
                                      (ld->cWindow + 1));
        if (ld->cWindow > 0) {
          memcpy(wn, ld->windows, sizeof(RecordStruct *) * ld->cWindow);
          xfree(ld->windows);
          memcpy(wnm, ld->windowsmeta, sizeof(RecordStructMeta *) * ld->cWindow);
          xfree(ld->windowsmeta);
        }
        ld->windows = wn;
        ld->windows[ld->cWindow] = window;
        ld->windowsmeta = wnm;
        snprintf(rname,SIZE_LONGNAME,"%s.rec",wname);
        rname[SIZE_LONGNAME] = 0;
        ld->windowsmeta[ld->cWindow] = NewRecordStructMeta(rname,NULL);
        ld->cWindow++;
        if (window != NULL) {
          g_hash_table_insert(ld->whash, window->name, (void *)ld->cWindow);
        } else {
          Error("window is NULL");
        }
      } else {
        ParError("record name not found");
      }
      if (GetSymbol != ';') {
        ParError("Window ; missing");
      }
    }
  }
}

static void _ParDB(CURFILE *in, LD_Struct *ld, char *dbgname,
                   char *table_name) {
  char name[SIZE_BUFF];
  char buff[SIZE_BUFF];
  char *p, *q;
  RecordStruct *db = NULL;
  RecordStruct **rtmp;
  RecordStructMeta **mtmp;

  strcpy(buff, RecordDir);
  p = buff;
  do {
    if ((q = strchr(p, ':')) != NULL) {
      *q = 0;
    }
    sprintf(name, "%s/%s.db", p, table_name);
    dbgprintf("Parsed %s\n", table_name);
    if (g_hash_table_lookup(ld->DB_Table, table_name) == NULL) {
      db = DB_Parser(name, dbgname, TRUE);
    } else {
      ParError("same db appier");
    }
    if (db != NULL) {
      rtmp = (RecordStruct **)xmalloc(sizeof(RecordStruct *) * (ld->cDB + 1));
      memcpy(rtmp, ld->db, sizeof(RecordStruct *) * ld->cDB);
      mtmp = (RecordStructMeta **)xmalloc(sizeof(RecordStructMeta *) * (ld->cDB + 1));
      memcpy(mtmp, ld->dbmeta, sizeof(RecordStructMeta *) * ld->cDB);
      if (ld->db != NULL) {
        xfree(ld->db);
      }
      if (ld->dbmeta != NULL) {
        xfree(ld->dbmeta);
      }
      ld->db = rtmp;
      ld->db[ld->cDB] = db;
      ld->dbmeta = mtmp;
      ld->dbmeta[ld->cDB] = NewRecordStructMeta(name, dbgname);
      ld->cDB++;
      g_hash_table_insert(ld->DB_Table, StrDup(table_name), (void *)ld->cDB);
    }
    p = q + 1;
  } while ((q != NULL) && (db == NULL));
  if (db == NULL) {
    ParError("db not found");
  }
}

static void ParDB(CURFILE *in, LD_Struct *ld, char *dbgname, int parse_type) {
  while (GetSymbol != '}') {
    if ((ComToken == T_SYMBOL) || (ComToken == T_SCONST)) {
      if (parse_type >= P_ALL) {
        _ParDB(in, ld, dbgname, ComSymbol);
      }
    }
    if (GetSymbol != ';') {
      ParError("DB ; missing");
    }
    ERROR_BREAK;
  }
  xfree(dbgname);
}

static void ParDATA(CURFILE *in, LD_Struct *ld) {
  char buff[SIZE_LONGNAME + 1];

  if (GetSymbol == '{') {
    while (GetSymbol != '}') {
      switch (ComToken) {
      case T_SPA:
        GetName;
        if (ComToken == T_SYMBOL) {
          sprintf(buff, "%s.rec", ComSymbol);
          if ((ld->sparec = ReadRecordDefine(buff,TRUE)) == NULL) {
            ParError("spa record not found");
          }
        } else {
          ParError("spa must be integer");
        }
        break;
      case T_WINDOW:
        ParWindow(in, ld);
        break;
      default:
        ParError("syntax error 1");
        break;
      }
      if (GetSymbol != ';') {
        ParError("DATA ; missing");
      }
      ERROR_BREAK;
    }
  } else {
    ParError("DATA syntax error");
  }
}

static void ParBIND(CURFILE *in, LD_Struct *ret, Bool fAPI) {
  WindowBind *bind, **binds;
  char *name;

  if ((GetSymbol == T_SCONST) || (ComToken == T_SYMBOL)) {
    name = ComSymbol;
    if ((bind = g_hash_table_lookup(ret->bhash, name)) == NULL) {
      bind = New(WindowBind);
      bind->name = StrDup(name);
      bind->fAPI = fAPI;
      if (*name != 0) {
        bind->rec = GetWindow(name);
      } else {
        bind->rec = NULL;
      }
      g_hash_table_insert(ret->bhash, bind->name, bind);
      binds = (WindowBind **)xmalloc(sizeof(WindowBind *) * (ret->cBind + 1));
      if (ret->cBind > 0) {
        memcpy(binds, ret->binds, sizeof(WindowBind *) * ret->cBind);
        xfree(ret->binds);
      }
      ret->binds = binds;
      ret->binds[ret->cBind] = bind;
      ret->cBind++;
    }
    if ((GetSymbol == T_SCONST) || (ComToken == T_SYMBOL)) {
      bind->handler = (void *)StrDup(ComSymbol);
    } else {
      ParError("handler name error");
    }
    if ((GetSymbol == T_SCONST) || (ComToken == T_SYMBOL)) {
      bind->module = StrDup(ComSymbol);
    } else {
      ParError("module name error");
    }
  } else {
    ParError("window name error");
  }
}

static LD_Struct *NewLD(void) {
  LD_Struct *ld;
  ld = New(LD_Struct);
  ld->name = NULL;
  ld->group = "";
  ld->ports = NULL;
  ld->whash = NewNameHash();
  ld->bhash = NewNameHash();
  ld->nCache = 0;
  ld->cDB = 1;
  ld->db = (RecordStruct **)xmalloc(sizeof(RecordStruct *));
  ld->db[0] = NULL;
  ld->dbmeta = (RecordStructMeta **)xmalloc(sizeof(RecordStructMeta *));
  ld->dbmeta[0] = NULL;
  ld->cWindow = 0;
  ld->cBind = 0;
  ld->arraysize = SIZE_DEFAULT_ARRAY_SIZE;
  ld->textsize = SIZE_DEFAULT_TEXT_SIZE;
  ld->DB_Table = NewNameHash();
  ld->home = NULL;
  ld->loadpath = NULL;
  ld->handlerpath = NULL;
  return (ld);
}

static LD_Struct *LD_Par(CURFILE *in, int parse_type) {
  LD_Struct *ret;
  char *gname;

  ret = NewLD();
  while (GetSymbol != T_EOF) {
    switch (ComToken) {
    case T_NAME:
      if (GetName != T_SYMBOL) {
        ParError("no name");
      } else {
        ret->name = StrDup(ComSymbol);
      }
      break;
    case T_ARRAYSIZE:
      if (GetSymbol == T_ICONST) {
        ret->arraysize = ComInt;
      } else {
        ParError("invalid array size");
      }
      break;
    case T_TEXTSIZE:
      if (GetSymbol == T_ICONST) {
        ret->textsize = ComInt;
      } else {
        ParError("invalid text size");
      }
      break;
    case T_CACHE:
      if (GetSymbol == T_ICONST) {
        ret->nCache = ComInt;
      } else {
        ParError("invalid cache size");
      }
      break;
    case T_MGROUP:
      GetName;
      ret->group = StrDup(ComSymbol);
      break;
    case T_DB:
      if ((GetSymbol == T_SCONST) || (ComToken == T_SYMBOL)) {
        gname = StrDup(ComSymbol);
        if (GetSymbol != '{') {
          ParError("DB { missing");
        }
      } else if (ComToken == '{') {
        gname = StrDup("");
      } else {
        gname = NULL;
        ParError("DB error");
      }
      ParDB(in, ret, gname, parse_type);
      break;
    case T_DATA:
      if (ret->name == NULL) {
        ParError("name directive is required");
      }
      ParDATA(in, ret);
      break;
    case T_HOME:
      if (GetSymbol == T_SCONST) {
        ret->home = StrDup(ExpandPath(ComSymbol, ThisEnv->BaseDir));
      } else {
        ParError("home directory invalid");
      }
      break;
    case T_LOADPATH:
      if (GetSymbol == T_SCONST) {
        ret->loadpath = StrDup(ExpandPath(ComSymbol, ThisEnv->BaseDir));
      } else {
        ParError("load path invalid");
      }
      break;
    case T_HANDLERPATH:
      if (GetSymbol == T_SCONST) {
        ret->handlerpath = StrDup(ExpandPath(ComSymbol, ThisEnv->BaseDir));
      } else {
        ParError("handler path invalid");
      }
      break;
    case T_BIND:
      ParBIND(in, ret, FALSE);
      break;
    case T_BINDAPI:
      ParBIND(in, ret, TRUE);
      break;
    case T_HANDLER:
      ParHANDLER(in);
      break;
    default:
      printf("[%s]\n", ComSymbol);
      ParError("invalid directive");
      break;
    }
    if (GetSymbol != ';') {
      ParErrorPrintf("missing ;(semicolon).\n");
    }
    ERROR_BREAK;
  }
  if (ret->name == NULL) {
    ParError("name directive is required");
  }
  return (ret);
}

static void _BindHandler(char *name, WindowBind *bind, void *dummy) {
  BindMessageHandlerCommon(&bind->handler);
}

static void BindHandler(LD_Struct *ld) {
  g_hash_table_foreach(ld->bhash, (GHFunc)_BindHandler, NULL);
}

extern LD_Struct *LD_Parser(char *name, int parse_type) {
  LD_Struct *ret;
  struct stat stbuf;
  CURFILE *in, root;

  dbgmsg(name);
  root.next = NULL;
  if (stat(name, &stbuf) == 0) {
    if ((in = PushLexInfo(&root, name, ThisEnv->D_Dir, Reserved)) != NULL) {
      ret = LD_Par(in, parse_type);
      DropLexInfo(&in);
      BindHandler(ret);
    } else {
      Warning("LD file not found [%s]", name);
      ret = NULL;
    }
  } else {
    ret = NULL;
  }
  return (ret);
}

extern void LD_ParserInit(void) {
  LexInit();
  Reserved = MakeReservedTable(tokentable);

  Records = NewNameHash();
  MessageHandlerInit();
}
