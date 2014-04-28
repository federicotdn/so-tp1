#ifndef DBACCESS_H
#define DBACCESS_H

#define DB_MAX_USERLEN 30
#define DB_MIN_USERLEN 4
#define DB_MAX_PASSLEN 30
#define DB_MIN_PASSLEN 6
#define DB_MAX_USERTYPE_NAME 7

struct db_handle;
enum db_type_code { DB_STUDENT, DB_TEACHER };

struct db_handle *open_db(char *filename);
int get_db_lock(struct db_handle *db);
int unlock_db(struct db_handle *db);
int close_db(struct db_handle *db);
int db_add_user(struct db_handle *db, char *username, char *password, enum db_type_code type);
enum db_type_code db_check_login(struct db_handle *db, char *username, char *password);

#endif
/* DBACCESS_H */
