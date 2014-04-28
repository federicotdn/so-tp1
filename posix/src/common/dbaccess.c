#include "dbaccess.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "file.h"

#define TRUE 1
#define FALSE 0

struct db_user_type {
	char *name;
	enum db_type_code code;
};

struct db_user {
	char *name;
	char *pass;
	struct db_user_type *type;
	struct db_user *next;
};

struct db_handle {
	FILE *file;
};

struct db_user_type db_user_types[] = {
	{ "student", DB_STUDENT },
	{ "teacher", DB_TEACHER }
};

struct flock create_lock(short int type);
struct db_user *build_user(char *username, char *password, enum db_type_code type);
int write_user_db(struct db_handle *db, struct db_user *user);
int db_user_exists(struct db_handle *db, struct db_user *user);
void free_users(struct db_user *user_list);
struct db_user *parse_db_users(struct db_handle *db);
struct db_user_type *get_type(char *type_name);
struct db_user *parse_db_line(char *line);
int match_credentials(struct db_user *user, char *name, char *pass);
int type_code_valid(enum db_type_code code);

struct db_handle *open_db(char *filename)
{
	struct db_handle *db = malloc(sizeof(struct db_handle));
	
	db->file = fopen(filename, "a+");
	if (db->file == NULL)
	{
		free(db);
		return NULL;
	}

	setvbuf(db->file, NULL, _IONBF, 0);

	return db;
}



int get_db_lock(struct db_handle *db)
{
	if (db == NULL)
	{
		return -1;
	}
	int fd = fileno(db->file);
	return get_file_lock(fd, F_WRLCK);
}


int get_db_read_lock(struct db_handle *db)
{
	if (db == NULL)
	{
		return -1;
	}
	int fd = fileno(db->file);
	return get_file_lock(fd, F_RDLCK);
}

int unlock_db(struct db_handle *db)
{
	if (db == NULL)
	{
		return -1;
	}

	/* Remove lock for file */
	int fd = fileno(db->file);
	return unlock_file(fd);
}



int close_db(struct db_handle *db)
{
	if (db == NULL)
	{
		return -1;
	}

	FILE *f = db->file;
	free(db);

	return fclose(f); /* Destroys all locks */
}

void free_users(struct db_user *user_list)
{
	struct db_user *aux = user_list;

	while (aux != NULL)
	{
		struct db_user *next = aux->next;
		free(aux->pass);
		free(aux->name);
		free(aux);
		aux = next;
	}
}

int db_add_user(struct db_handle *db, char *username, char *password, enum db_type_code type)
{
	if (db == NULL || username == NULL || password == NULL)
	{
		return -1;
	}

	if (!type_code_valid(type))
	{
		return -1;
	}

	struct db_user *user = build_user(username, password, type);
	if (user == NULL)
	{
		return -1;
	}
	int status = db_user_exists(db, user);
	if (status || status == -1)
	{
		free(user);
		return -1;
	}


	fseek(db->file, 0, SEEK_END);
	status = write_user_db(db, user);

	free(user);

	return status;
}

struct db_user *build_user(char *username, char *password, enum db_type_code type)
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

	char *name = strdup(username);
	char *pass = strdup(password);
	if (name == NULL || pass == NULL)
	{
		free(name);
		free(pass);
		free(user);
		return NULL;
	}

	user->name = name;
	user->pass = pass;
	user->type = &db_user_types[type];
	user->next = NULL;

	return user;
}

int write_user_db(struct db_handle *db, struct db_user *user)
{
	int status = fprintf(db->file, "%s:%s:%s\n", user->name,
									  			 user->pass,
									  			 user->type->name);

	return status;
}

int db_user_exists(struct db_handle *db, struct db_user *user)
{
	struct db_user *head = parse_db_users(db);

	struct db_user *aux = head;

	while (aux != NULL)
	{
		if (strncmp(user->name, aux->name, DB_MAX_USERLEN) == 0)
		{
			free_users(head);
			return TRUE;
		}

		aux = aux->next;
	}

	free_users(head);
	return FALSE;
}

struct db_user *parse_db_users(struct db_handle *db)
{
	char buf[DB_MAX_USERLEN + DB_MAX_PASSLEN + DB_MAX_USERTYPE_NAME + 3];
	int read = 0, end = FALSE;
	struct db_user *user_list = NULL;

	int status = get_db_read_lock(db);

	if (status == -1)
	{
		return NULL;
	}

	fseek(db->file, 0, SEEK_SET);

	while (!end)
	{
		read = fscanf(db->file, "%s\n", buf);

		if (read != 1)
		{
			end = TRUE;
		}
		else
		{
			struct db_user *user = parse_db_line(buf);
			if (user == NULL)
			{
				end = TRUE;
			}
			else
			{
				user->next = user_list;
				user_list = user;
			}
		}

	}

	status = unlock_db(db);
	if (status == -1)
	{
		
		return NULL;
	}

	return user_list;
}

struct db_user *parse_db_line(char *line)
{
	char *name_buf = NULL;
	char *pass_buf = NULL;
	char *type_buf = NULL;
	char *tkn;

	tkn = strtok(line, ":");
	name_buf = tkn;

	tkn = strtok(NULL, ":");
	pass_buf = tkn;

	tkn = strtok(NULL, ":");
	type_buf = tkn;

	if (name_buf == NULL || pass_buf == NULL || type_buf == NULL)
	{
		return NULL;
	}

	struct db_user_type *type = get_type(type_buf);
	if (type == NULL)
	{
		return NULL;
	}

	return build_user(name_buf, pass_buf, type->code);
}

struct db_user_type *get_type(char *type_name)
{
	int i;
	for (i = 0; i < sizeof(db_user_types); i++)
	{
		if (strncmp(type_name, db_user_types[i].name, DB_MAX_USERTYPE_NAME) == 0)
		{
			return &db_user_types[i];
		}
	}

	return NULL;
}

int type_code_valid(enum db_type_code code)
{
	int i;
	for (i = 0; i < sizeof(db_user_types); i++)
	{
		if (db_user_types[i].code == code)
		{
			return TRUE;
		}
	}

	return FALSE;
}

enum db_type_code db_check_login(struct db_handle *db, char *username, char *password)
{
	if (db == NULL || username == NULL || password == NULL)
	{
		return -1;
	}

	struct db_user *head = parse_db_users(db);

	if (head == NULL)
	{
		return -1;
	}

	struct db_user *aux = head;
	while (aux != NULL)
	{
		if (match_credentials(aux, username, password))
		{
			enum db_type_code code = aux->type->code;
			free_users(head);
			return code;
		}

		aux = aux->next;
	}

	free_users(head);
	return -1;
}

int match_credentials(struct db_user *user, char *name, char *pass)
{
	if (strncmp(user->name, name, DB_MAX_USERLEN) != 0)
	{
		return FALSE;
	}

	if (strncmp(user->pass, pass, DB_MAX_PASSLEN) != 0)
	{
		return FALSE;
	}

	return TRUE;
}


