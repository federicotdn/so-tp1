#include "chatroom-local.h"
#include <string.h>

#define TRUE 1
#define FALSE 0

struct chatroom_state {
	int in_pipe;
	char *name;
	pid_t creator;
	int new;
};

int start_chatroom(chatroom_state_t *state);

int init_chatroom(int in_pipe, char *name, pid_t creator)
{
	chatroom_state_t state;
	state.new = TRUE;
}