/*
 * PANDA -- a simple transaction monitor
 * Copyright (C) 2001-2009 Ogochan & JMA (Japan Medical Association).
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_POSTGRES
#include	<stdio.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<signal.h>
#include 	<errno.h>
#include	<sys/wait.h>
#include	<libpq-fe.h>
#include	<libpq/libpq-fs.h>
#include	"dbgroup.h"
#include	"PostgreSQLlib.h"
#include	"PostgreSQLutils.h"
#include	"debug.h"

static char *ignore_message = "ERROR:  must be owner of schema public";

const char *PG_DUMP = "pg_dump";
const char *PSQL = "psql";

extern PGconn	*
pg_connect(
	DBG_Struct	*dbg)
{
	PGconn *conn = NULL;
	
	if (dbg->process[PROCESS_UPDATE].dbstatus == DB_STATUS_CONNECT) {
		/* database is already connected. */
		conn = PGCONN(dbg, DB_UPDATE);
	} else {
		if ((conn = PgConnect(dbg, DB_UPDATE)) != NULL){
			dbg->process[PROCESS_UPDATE].conn = (void *)conn;
			dbg->process[PROCESS_UPDATE].dbstatus = DB_STATUS_CONNECT;
		}
	}
	return conn;
}

extern void
pg_disconnect(
	DBG_Struct	*dbg)
{
	if (dbg->process[PROCESS_UPDATE].dbstatus == DB_STATUS_CONNECT) {
		PQfinish(PGCONN(dbg, DB_UPDATE));
		dbg->process[PROCESS_UPDATE].dbstatus = DB_STATUS_DISCONNECT;
	}
}

extern	Bool
pg_trans_begin(
	DBG_Struct	*dbg)
{
	Bool ret = FALSE;
	PGconn *conn = NULL;
	
	conn = pg_connect(dbg);
	ret = db_command(conn, "BEGIN;");
	return ret;
}

extern	Bool
pg_trans_commit(
	DBG_Struct	*dbg)
{
	Bool ret = FALSE;
	PGconn *conn = NULL;
	
	conn = PGCONN(dbg, DB_UPDATE);
	ret = db_command(conn, "COMMIT;");
	return ret;
}

extern	void
pg_lockredirector(
	DBG_Struct	*dbg)
{
	PGconn *conn = NULL;

	conn = PGCONN(dbg, DB_UPDATE);
	LockRedirectorConnect(conn);
}

extern PGconn	*
template1_connect(
	DBG_Struct	*dbg)
{
	LargeByteString *conninfo;
	PGconn	*conn;

	conninfo = Template1Conninfo(dbg,DB_UPDATE);
	conn = PQconnectdb(LBS_Body(conninfo));
	if		(  PQstatus(conn)  !=  CONNECTION_OK  ) {
		Message("%s", PQerrorMessage(conn));
		conn = NULL;
	}
	FreeLBS(conninfo);
	
	return conn;
}

extern Bool
template1_check(
	DBG_Struct	*dbg)
{
	Bool ret = FALSE;
	PGconn	*conn;
	
	conn = template1_connect(dbg);
	if (conn){
		ret = TRUE;
		PQfinish(conn);		
	}
	return ret;
}

extern PGresult	*
db_exec(
	PGconn	*conn,
	char *query)
{
	PGresult	*res;
	
	res = PQexec(conn, query);

	return res;
}

extern Bool
db_command(
	PGconn	*conn,
	char *query)
{
	Bool ret;
	PGresult	*res;
	
	res = PQexec(conn, query);
	if ( 	(res != NULL )
		 && (PQresultStatus(res) == PGRES_COMMAND_OK ) ) {
		ret = TRUE;
	} else {
		fprintf(stderr, "%s\n", PQerrorMessage(conn));
		ret = FALSE;
	}
	PQclear(res);
	return ret;
}


char **
make_pgopts(
	const char *command,
	DBG_Struct	*dbg)
{
	int pgoptc;
	char **pgoptv;

	char *option;

	pgoptv = malloc((255)*sizeof(char *));

	pgoptc = 0;	
	pgoptv[pgoptc++] = strdup(command);

	if ((option = GetDB_Host(dbg, DB_UPDATE)) != NULL){
		pgoptv[pgoptc++] = "-h";
		pgoptv[pgoptc++] = option;
	}
	if ((option = GetDB_PortName(dbg, DB_UPDATE)) != NULL){
		pgoptv[pgoptc++] = "-p";
		pgoptv[pgoptc++] = option;
	}
	if ((option = GetDB_User(dbg, DB_UPDATE)) != NULL){
		pgoptv[pgoptc++] = "-U";
		pgoptv[pgoptc++] = option;
	}

	pgoptv[pgoptc] = NULL;
	
	return pgoptv;
}

int optsize(char **optv)
{
	int i;
	
	for (i=0; optv[i] != NULL; i++ ){};
	return i;
}

static Bool
sync_wait(pid_t pg_dump_pid, pid_t restore_pid)
{
	pid_t	pid;
	int status;
	Bool ret = FALSE;
	
	for (;;)
	{
		pid = wait(&status);
		if ( pg_dump_pid ==  pid ) {
			if ( WIFEXITED(status) && (WEXITSTATUS(status) != 0) ) {
				fprintf(stderr, "Dump error(%d)\n", pid );
				break;				
			}
		} else if (restore_pid ==  pid ) {
			if ( WIFEXITED(status) && (WEXITSTATUS(status) != 0) ) {
				fprintf(stderr, "Restore error(%d)\n", pid);
				break;
			} else {
				ret = TRUE;
			}
		} else if (pid == -1) {
			if (!ret) {
				if (errno == ECHILD) {
					fprintf(stderr, "ECHILD\n");
				} else if (errno == EINTR) {
					fprintf(stderr, "EINTR\n");
				} else if (errno == EINVAL) {
					fprintf(stderr, "EINVAL\n");
				}
			}
			break;
		}
	}
	return ret;
}

static void
err_check(int err_fd)
{
	int len;
	char buff[SIZE_BUFF];	

	len = read(err_fd, buff, SIZE_BUFF);
	if (len > 0) {
		buff[len] = '\0';
		if (strncmp(ignore_message, buff, strlen(ignore_message)) != 0 ){
			fprintf(stderr, "%s\n", buff);
			exit(1);
		}
	}
}

static void
db_dump(int fd, char *pass, char **argv)
{
	char *sql;
	char command[SIZE_BUFF];
	
	snprintf(command, SIZE_BUFF, "%s/%s", POSTGRES_BINDIR, PG_DUMP);
	setenv("PGPASSWORD", pass, 1);

	sql = LockRedirectorQuery();
	write(fd, sql, strlen(sql));
	xfree(sql);
	execv(command, argv);

	fprintf( stderr, "load program pg_dump\n" );
	perror("");
	exit(1);
}

static void
db_restore(int fd, char *pass, char **argv)
{
	char command[SIZE_BUFF];
	
	snprintf(command, SIZE_BUFF, "%s/%s", POSTGRES_BINDIR, PSQL);

	setenv("PGPASSWORD", pass, 1);
	execv(command, argv);

	fprintf( stderr, "load program psql\n" );
	perror("");
	exit(1);
}

extern 	Bool
dbexist(DBG_Struct	*dbg)
{
	PGconn	*conn;
	PGresult	*res;
	Bool ret = FALSE;
	char sql[SIZE_BUFF];	

	conn = template1_connect(dbg);
	if (conn){
		snprintf(sql, SIZE_BUFF, "SELECT datname FROM pg_database WHERE datname = '%s';\n", GetDB_DBname(dbg,DB_UPDATE));
		res = db_exec(conn, sql);
		if (PQntuples(res) == 1) {
			ret = TRUE;
		}
		PQclear(res);
		PQfinish(conn);
	}
	
	return ret;
}

extern 	int
dbactivity(DBG_Struct	*dbg)
{
	PGconn	*conn;
	PGresult	*res;
	int	ret = 0;
	char sql[SIZE_BUFF];

	pg_disconnect(dbg);
	conn = template1_connect(dbg);
	if (conn){
		snprintf(sql, SIZE_BUFF, "SELECT datname FROM pg_stat_activity WHERE datname = '%s';\n", GetDB_DBname(dbg,DB_UPDATE));
		res = db_exec(conn, sql);
		ret = PQntuples(res);
		PQclear(res);
		PQfinish(conn);
	}
	
	return ret;
}

extern 	int
redirector_check(DBG_Struct	*dbg)
{
	PGconn	*conn;
	PGresult	*res;
	int	ret = 0;
	char sql[SIZE_BUFF];

	conn = pg_connect(dbg);
	if (conn){
		snprintf(sql, SIZE_BUFF,
						 "SELECT c.relname " \
						 "  FROM pg_class AS c,pg_namespace AS n " \
						 " WHERE c.relkind = 'r'" \
						 "   AND c.relnamespace = n.oid" \
						 "   AND n.nspname LIKE 'pg_temp%%'" \
						 "AND c.relname = '%s';", REDIRECT_LOCK_TABLE);
		res = db_exec(conn, sql);
		ret = PQntuples(res);
		PQclear(res);
		pg_disconnect(dbg);
	}
	
	return ret;
}

extern 	Bool
dropdb(DBG_Struct	*dbg)
{
	PGconn	*conn;
	Bool ret = FALSE;
	char sql[SIZE_BUFF];

	conn = template1_connect(dbg);
	if (conn){
		snprintf(sql, SIZE_BUFF, "DROP DATABASE %s;\n", GetDB_DBname(dbg,DB_UPDATE));
		ret = db_command(conn, sql);
		PQfinish(conn);
	}
	
	return ret;
}

static DBInfo *
NewDBInfo(void)
{
	DBInfo *dbinfo;
	dbinfo = (DBInfo *)malloc(sizeof(DBInfo));
	dbinfo->tablespace = NULL;
	dbinfo->template = NULL;
	dbinfo->encoding = NULL;
	dbinfo->lc_collate = NULL;
	dbinfo->lc_ctype = NULL;
	return dbinfo;
}

extern DBInfo *
getDBInfo(DBG_Struct	*dbg,
	char *dbname)
{
	PGconn	*conn;
	PGresult	*res;
	DBInfo *dbinfo;
	LargeByteString *sql;

	sql = NewLBS();
	dbinfo = NewDBInfo();
	
	conn = template1_connect(dbg);
	LBS_EmitString(sql, "SELECT pg_encoding_to_char(encoding) ");
	if ( PQserverVersion(conn)  >=  80400 ) {
		LBS_EmitString(sql, " , datcollate ");
		LBS_EmitString(sql, " , datctype ");
	}
	LBS_EmitString(sql, " FROM pg_database WHERE datname ='");
	LBS_EmitString(sql, dbname);
	LBS_EmitString(sql, "';\n");
	LBS_EmitEnd(sql);
	res = PQexec(conn, LBS_Body(sql));
	if ( (res == NULL) || (PQresultStatus(res) != PGRES_TUPLES_OK) ) {
		Warning("PostgreSQL: %s",PQerrorMessage(conn));
	} else {
		dbinfo->encoding = StrDup(PQgetvalue(res, 0,0));
		if ( PQserverVersion(conn)  >=  80400 ) {
			dbinfo->lc_collate = StrDup(PQgetvalue(res, 0,1));
			dbinfo->lc_ctype = StrDup(PQgetvalue(res, 0,2));
			/* for PostgreSQL 8.4 */
			dbinfo->template = StrDup("template0");
		}
	}
	PQclear(res);
	PQfinish(conn);
	return dbinfo;
}

extern 	Bool
createdb(DBG_Struct	*dbg,
	char *tablespace,
	char *template,
	char *encoding,
	char *lc_collate,
	char *lc_ctype)
{
	PGconn	*conn;
	LargeByteString *sql;
	Bool ret = FALSE;

	sql = NewLBS();
	pg_disconnect(dbg);
	conn = template1_connect(dbg);
	if (conn) {
		LBS_EmitString(sql, "CREATE DATABASE ");
		LBS_EmitString(sql, GetDB_DBname(dbg,DB_UPDATE));
		if (tablespace) {
			LBS_EmitString(sql," TABLESPACE ");
			LBS_EmitString(sql, tablespace);
		}
		if (template) {
			LBS_EmitString(sql," TEMPLATE ");
			LBS_EmitString(sql, template);
		}
		if (encoding) {
			LBS_EmitString(sql," ENCODING ");
			LBS_EmitString(sql, "'");
			LBS_EmitString(sql, encoding);
			LBS_EmitString(sql, "' ");
		}
		if ( PQserverVersion(conn)  >=  80400 ) {
			if (lc_collate) {
				LBS_EmitString(sql," LC_COLLATE ");
				LBS_EmitString(sql, "'");
				LBS_EmitString(sql, lc_collate);
				LBS_EmitString(sql, "' ");			
			}
			if (lc_ctype) {
				LBS_EmitString(sql," LC_CTYPE ");		
				LBS_EmitString(sql, "'");
				LBS_EmitString(sql, lc_ctype);
				LBS_EmitString(sql, "' ");			
			}
		}
		LBS_EmitString(sql,";\n");
		LBS_EmitEnd(sql);
		ret = db_command(conn, LBS_Body(sql));
		FreeLBS(sql);		
		PQfinish(conn);
	}
	
	return ret;
}

extern 	Bool
delete_table(DBG_Struct	*dbg, char *table_name)
{
	PGconn	*conn;
	Bool ret = FALSE;
	char sql[SIZE_BUFF];

	conn = pg_connect(dbg);
	
	snprintf(sql, SIZE_BUFF, "DELETE FROM %s;\n", table_name);
	ret = db_command(conn, sql);
	
	return ret;
}

extern Bool
db_sync(
	char **master_argv,
	char *master_pass,
	char **slave_argv,
	char *slave_pass,
	int check)
{
	struct sigaction sa;
	int std_out[2], std_err[2];
	
	Bool ret = FALSE;
	pid_t	pg_dump_pid, restore_pid;
	
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = SIG_DFL;
	sa.sa_flags |= SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) != 0) {
		fprintf(stderr,"sigaction(2) failure\n");
	}

	if ( (pipe(std_out) == -1) || ( pipe(std_err) == -1 ) ) {
		fprintf( stderr, "cannot open pipe\n");
		return FALSE;
	}
	if ( (restore_pid = fork()) == 0 ){
		/* |psql */
		close( std_out[1] );
		close( std_err[0] );
		close( STDIN_FILENO );
		dup2(std_out[0], STDIN_FILENO); 
		dup2(std_err[1], STDERR_FILENO);
		db_restore(STDIN_FILENO, slave_pass, slave_argv);
	} else {
		if ( (pg_dump_pid = fork()) == 0 ) {
			/* pg_dump| */
			close( std_out[0] );
			close( std_err[0] );
			close( std_err[1] );
			close( STDOUT_FILENO );
			dup2( std_out[1], STDOUT_FILENO);
			db_dump(std_out[1], master_pass, master_argv);
		} else {
			/* parent */
			close( std_out[0] );
			close( std_out[1] );
			close( std_err[1] );
			printf("%d\n", check);
			if (check){
				err_check(std_err[0]);
			}
			ret = sync_wait(pg_dump_pid, restore_pid);
		}
	}
	return ret;
}

extern Bool
all_sync(
	DBG_Struct *master_dbg,
	DBG_Struct *slave_dbg,
	char *dump_opt,
	Bool verbose)
{
	int check = TRUE;
	int moptc, soptc;
	char **master_argv, **slave_argv;
	char *master_pass, *slave_pass;
	
	master_pass = GetDB_Pass(master_dbg, DB_UPDATE);
	slave_pass = GetDB_Pass(slave_dbg, DB_UPDATE);	

	master_argv = make_pgopts(PG_DUMP, master_dbg);
	moptc = optsize(master_argv);
	if (dump_opt){
		master_argv[moptc++] = dump_opt;
		check = FALSE;
	}
	master_argv[moptc++] = "-O";
	master_argv[moptc++] = "-x";
	master_argv[moptc++] = GetDB_DBname(master_dbg,DB_UPDATE);
	master_argv[moptc] = NULL;
	
	slave_argv = make_pgopts(PSQL, slave_dbg);
	soptc = optsize(slave_argv);
	if (!verbose){
		slave_argv[soptc++] = "-q";
	}
/*	slave_argv[soptc++] = "-v";
	slave_argv[soptc++] = "ON_ERROR_STOP=1"; */
	slave_argv[soptc++] = GetDB_DBname(slave_dbg,DB_UPDATE);
	slave_argv[soptc] = NULL;	

	return db_sync(master_argv, master_pass, slave_argv, slave_pass, check);
}

extern Bool
table_sync(DBG_Struct	*master_dbg, DBG_Struct *slave_dbg, char *table_name)
{
	int moptc, soptc;
	char *master_pass, *slave_pass;
	char **master_argv, **slave_argv;

	master_pass = GetDB_Pass(master_dbg, DB_UPDATE);
	slave_pass = GetDB_Pass(slave_dbg, DB_UPDATE);	

	master_argv = make_pgopts(PG_DUMP, master_dbg);
	moptc = optsize(master_argv);
	master_argv[moptc++] = "-a";
	master_argv[moptc++] = "-t";
	master_argv[moptc++] = table_name;
	master_argv[moptc++] = GetDB_DBname(master_dbg,DB_UPDATE);
	master_argv[moptc] = NULL;
	
	slave_argv = make_pgopts(PSQL, slave_dbg);
	soptc = optsize(slave_argv);
	slave_argv[soptc++] = "-q";
	slave_argv[soptc++] = "-v";
	slave_argv[soptc++] = "ON_ERROR_STOP=1";
	slave_argv[soptc++] = GetDB_DBname(slave_dbg,DB_UPDATE);
	slave_argv[soptc] = NULL;
	
	return db_sync(master_argv, master_pass, slave_argv, slave_pass, TRUE);
}

static char *
tableType( table_TYPE types )
{
	char		*type;
	type = (char *)malloc(1024);

	switch(types){
		case ALL:
			strcpy(type, "'r','v','i',''");
			break;
		case TABLE:
			strcpy(type, "'r'");
			break;
		case INDEX:
			strcpy(type, "'i'");
			break;
		case VIEW:
			strcpy(type, "'v'");
			break;
		default:
			break;
	}
	return type;
}

TableList	*
NewTableList(int count)
{
	TableList	*table_list;
	table_list = (TableList *) malloc(sizeof(TableList));
	table_list->count = 0;
	table_list->tables = NULL;
	table_list->tables = (Table **)malloc(count * sizeof(Table *));
	return table_list;
}

extern Table *
NewTable(void)
{
	Table		*table;
	table = (Table *)malloc(sizeof(Table));
	table->name = NULL;
	table->relkind = ' ';
	table->count = 0;	
	return table;
}

char *
queryTableList( table_TYPE types )
{
	char		*sql;
	char		*type;

	type = tableType(types);
	sql = (char *)malloc(1024);
	
	snprintf(sql, 1024,
			 " SELECT c.relname,  c.relkind"
			 "   FROM pg_catalog.pg_class AS c "
			 "   JOIN pg_catalog.pg_roles r ON r.oid = c.relowner "
			 "   LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
			 "  WHERE c.relkind IN (%s) "
			 "    AND n.nspname NOT IN ('pg_catalog', 'pg_toast')"
			 "    AND pg_catalog.pg_table_is_visible(c.oid) "
			 "  ORDER BY c.relkind, c.relname ;", type );
	free(type);
	
	return (sql);
}

void TableCount(
	PGconn	*conn,
	TableList *table_list )
{
	int i;
	char buff[1024];
	PGresult	*res;
	
	for (i = 0; i < table_list->count; i++) {
		if (table_list->tables[i]->relkind == 'i') continue;
		sprintf(buff, "SELECT count(*) FROM %s;\n", table_list->tables[i]->name);
		res = PQexec(conn, buff);
		table_list->tables[i]->count = atoi(PQgetvalue(res, 0,0));
		PQclear(res);
	}
}


TableList *
getTableList( PGconn	*conn )
{
	PGresult	*res;
	ExecStatusType status;
	TableList	*table_list;
	Table		*table;
	char		*sql;
	int			ntuples;
	int			i;

	sql = queryTableList(ALL);
	res = PQexec(conn, sql);
	free(sql);
	
	status = PQresultStatus(res);
	ntuples = PQntuples(res);
	table_list = NewTableList(ntuples);
	for (i = 0; i < ntuples; i++) {
		table = NewTable();
		table->name = strdup(PQgetvalue(res, i, 0));
		table->relkind = *(PQgetvalue(res, i, 1));
		table_list->tables[i] = table;
		table_list->count++;
	}
	PQclear(res);

	return table_list;
}

extern TableList *
get_table_info(
	DBG_Struct	*dbg,
	char opt)
{
	PGconn	*conn;
	TableList *tablelist;
	
	conn = PGCONN(dbg, DB_UPDATE);
	
	tablelist = getTableList(conn);
	
	if (opt == 'c'){
		TableCount(conn, tablelist);
	}

	return tablelist;
}

#endif /* #ifdef HAVE_POSTGRES */