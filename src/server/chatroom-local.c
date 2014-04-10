#include "chatroom-local.h"
#include <string.h>

struct chatroom_state {
	int in_pipe;
	char *name;
	pid_t creator;
};

int start_chatroom(chatroom_state_t *state);

int init_chatroom(int in_pipe, char *name, pid_t creator)
{
	chatroom_state_t state;
}