#include <stdio.h>
#include "dbaccess.h"

int main(int argc, char *argv[])
{
	printf("Starting DB test.\n");

	struct db_handle *db = open_db();

	if (db == NULL)
	{
		printf("open_db returned NULL.\n");
		return 1;
	}

	printf("Calling get_db_lock...\n");

	 /* locks process until lock is acquired (or error) */
	int status = get_db_lock(db);
	if (status == -1)
	{
		printf("get_db_lock returned -1.\n");
		return 1;
	}

	printf("Database lock acquired.\n");
	printf("Press enter to unlock\n");

	db_add_user(db, "fede", "holahola123", DB_STUDENT);
	db_add_user(db, "robert", "huehuehuehue", DB_TEACHER);

	getchar();

	status = unlock_db(db);
	if (status == -1)
	{
		printf("unlock_db returned -1.\n");
		return 1;
	}

	printf("Database unlocked.\n");

	close_db(db);
	
	return 0;
}
