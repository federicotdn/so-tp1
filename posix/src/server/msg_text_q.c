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
	int count;
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
	queue->count = 0;

	return queue;
}

int push_message(msg_text_q *queue, char *msg)
{
	if (queue->last == NULL)
	{
		msg_node *node = malloc(sizeof(msg_node));
		if (node == NULL)
		{
			return -1;
		}

		queue->count = 1;
		queue->last = node;
		queue->head = node;
		node->next = NULL;
		strcpy(node->text, msg);
		return 0;
	}

	msg_node *aux = malloc(sizeof(msg_node));
	if (aux == NULL)
	{
		return -1;
	}

	queue->count++;
	queue->last->next = aux;
	queue->last = aux;
	aux->next = NULL;
	strcpy(aux->text, msg);

	if (queue->count > CHT_HIST_SIZE)
	{
		aux = queue->head->next;
		free(queue->head);
		queue->head = aux;
		queue->count--;
	}

	return 0;
}

void iter_reset(msg_text_q *queue)
{
	queue->current = queue->head;
}

char *iter_next(msg_text_q *queue)
{
	if (queue->current == NULL)
	{
		return NULL;
	}

	char *msg = queue->current->text;
	queue->current = queue->current->next;
	return msg;
}
