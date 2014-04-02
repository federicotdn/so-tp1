#include <stdio.h>
#include "dbaccess.h"

int main(int argc, char *argv[])
{
	char name_buf[100];
	char pass_buf[100];
	int status;

	printf("Starting DB login test.\n");

	struct db_handle *db = open_db("db.txt");

	if (db == NULL)
	{
		printf("open_db returned NULL.\n");
		return 1;
	}

	printf("Enter username:\n");

	scanf("%s", name_buf);

	printf("Enter password:\n");

	scanf("%s", pass_buf);

	printf("Login user: %s.\n", name_buf);

	status = db_check_login(db, name_buf, pass_buf);

	if (status == -1)
	{
		printf("Login failed: invalid username or password.\n\n");
	}
	else
	{
		printf("Login success. User type: %d\n\n", status);
	}

	close_db(db);
	
	return 0;
}
