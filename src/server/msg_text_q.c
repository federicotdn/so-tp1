#include "msg_text_q.h"
#include "protocol.h"
#include <string.h>
#include <stdlib.h>

typedef struct msg_node_t {
	char text[CHT_MSG_SIZE];
	struct msg_node_t *next;
} msg_node;

struct msg_text_q_t {
	msg_node *head;
	msg_node *current;
	msg_node *last;
};

msg_text_q *create_queue()
{
	msg_text_q *queue = malloc(sizeof(struct msg_text_q_t));
	if (queue == NULL)
	{
		return NULL;
	}

	queue->head = NULL;
	queue->last = NULL;
	queue->current = NULL;

	return queue;
}

int push_message(msg_text_q *queue, char *msg)
{
	if (queue->head == NULL)
	{	}
}

void iter_reset(msg_text_q *queue)
{
	
}

char *iter_next(msg_text_q *queue)
{
	
}
