#ifndef __LIST_H
#define __LIST_H

struct node {
	void * data;
	struct node * next;
	struct node * prev;
};

void list_add_tail(struct node ** head_ref, void * data);

#endif