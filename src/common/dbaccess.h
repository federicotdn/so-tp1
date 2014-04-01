#ifndef DBACCESS_H
#define DBACCESS_H

#define DB_STUDENT 0
#define DB_TEACHER 1

struct db_handle;

struct db_handle *open_db();
int get_db_lock(struct db_handle *db);
int unlock_db(struct db_handle *db);
int close_db(struct db_handle *db);
int db_add_user(struct db_handle *db, char *username, char *password, int type);
int db_login(struct db_handle *db, char *username, char *password);

#endif
/* DBACCESS_H */
