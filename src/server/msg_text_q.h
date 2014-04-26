typedef struct msg_text_q_t msg_text_q;

msg_text_q *create_queue();
void push_message(msg_text_q *queue, char *msg);
void iter_reset(msg_text_q *queue);
char *iter_next(msg_text_q *queue);
