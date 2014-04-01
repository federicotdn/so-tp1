#include "dbaccess.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DB_NAME "db.txt"
#define DB_SEPARATOR ':'
#define DB_MAX_USERLEN 30
#define DB_MIN_USERLEN 4
#define DB_MAX_PASSLEN 30
#define DB_MIN_PASSLEN 6

struct db_handle {
	int fd;
};

struct db_user_type {
	char *name;
	int len;
};

struct db_user {
	char *name;
	int name_len;
	char *pass;
	int pass_len;
	struct db_user_type *type;
};

struct db_user_type db_user_types[] = {
	{ "student", 7 }, 
	{ "teacher", 7 }
};

struct flock create_lock(short int type);
struct db_user *build_user(char *username, char *password, int type);
char *gen_user_string(struct db_user *user, int *str_len);

struct db_handle *open_db()
{
	struct db_handle *db = malloc(sizeof(struct db_handle));
	
	db->fd = open(DB_NAME, O_RDWR);
	if (db->fd == -1)
	{
		free(db);
		return NULL;
	}
	
	return db;
}

int get_db_lock(struct db_handle *db)
{
	if (db == NULL)
	{
		return -1;
	}

	/* Exclusive lock on whole file */
	struct flock lck = create_lock(F_WRLCK);
	
	return fcntl(db->fd, F_SETLKW, &lck);
}

int unlock_db(struct db_handle *db)
{
	if (db == NULL)
	{
		return -1;
	}

	/* Remove lock for file */
	struct flock lck = create_lock(F_UNLCK);

	return fcntl(db->fd, F_SETLKW, &lck);
}

int close_db(struct db_handle *db)
{
	if (db == NULL)
	{
		return -1;
	}

	int fd = db->fd;
	free(db);

	return close(fd); /* Destroys all locks */
}

int db_add_user(struct db_handle *db, char *username, char *password, int type)
{
	if (db == NULL || username == NULL || password == NULL)
	{
		return -1;
	}

	struct db_user *user = build_user(username, password, type);
	if (user == NULL)
	{
		return -1;
	}
	
	int str_len = 0;
	char *user_string = gen_user_string(user, &str_len);
	if (user_string == NULL)
	{
		return -1;
	}

	lseek(db->fd, 0, SEEK_END);
	int status = write(db->fd, user_string, str_len);

	free(user);
	free(user_string);

	return status;
}

struct db_user *build_user(char *username, char *password, int type)
{
	if (type != DB_STUDENT && type != DB_TEACHER)
	{
		return NULL;
	}

	int user_len = strlen(username);
	int pass_len = strlen(password);

	if (user_len > DB_MAX_USERLEN || user_len < DB_MIN_USERLEN)
	{
		return NULL;
	}

	if (pass_len > DB_MAX_PASSLEN || pass_len < DB_MIN_PASSLEN)
	{
		return NULL;
	}

	struct db_user *user = malloc(sizeof(struct db_user));
	if (user == NULL)
	{
		return NULL;
	}

	user->name = username;
	user->pass = password;
	user->name_len = user_len;
	user->pass_len = pass_len;
	user->type = &db_user_types[type];

	return user;
}

char *gen_user_string(struct db_user *user, int *str_len)
{
	int data_len = user->name_len + user->pass_len + user->type->len + 3;
	int offset = 0;
	char sep = DB_SEPARATOR;
	char newline = '\n';

	char *user_string = malloc(sizeof(char) * data_len);
	if (user_string == NULL)
	{
		return NULL;
	}

	strncpy(user_string, user->name, user->name_len);
	offset += user->name_len;

	strncpy(user_string + offset, &sep, 1);
	offset += 1;

	strncpy(user_string + offset, user->pass, user->pass_len);
	offset += user->pass_len;

	strncpy(user_string + offset, &sep, 1);
	offset += 1;

	strncpy(user_string + offset, user->type->name, user->type->len);
	offset += user->type->len;

	strncpy(user_string + offset, &newline, 1);
	offset += 1;

	*str_len = offset;

	return user_string;
}

int db_login(struct db_handle *db, char *username, char *password)
{
	if (db == NULL)
	{
		return -1;
	}

	//Tres opciones: 
	// 1) Leer toda la base, pasarla a structs db_user y comparar (reutilizable para evitar q se creen usuarios iguales)
	// 2) Comparar de a partes con strings el username y pass
	// 2) Usar funciones de GNU C
}

struct flock create_lock(short int type)
{
	struct flock lck;
	lck.l_type = type;
	lck.l_whence = SEEK_SET;
	lck.l_start = 0;
	lck.l_len = 0;
	lck.l_pid = getpid();
	return lck;
}