#include <stdio.h>
#include "dbaccess.h"

int main(int argc, char *argv[])
{
	char name_buf[100];
	char pass_buf[100];
	int type;

	printf("Starting DB registration.\n");

	struct db_handle *db = open_db("db.txt");

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

	printf("Database lock acquired.\n\n");
	printf("Enter username:\n");

	scanf("%s", name_buf);

	printf("Enter password:\n");

	scanf("%s", pass_buf);

	printf("Enter type: (0 = Student, 1 = Teacher)\n");

	scanf("%d", &type);

	printf("Adding user: %s, password: %s, type: %d.\n", name_buf, pass_buf, type);

	status = db_add_user(db, name_buf, pass_buf, type);

	if (status == -1)
	{
		printf("Error: user already exists, or invalid user type.\n\n");
	}
	else
	{
		printf("User created.\n\n");
	}

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
