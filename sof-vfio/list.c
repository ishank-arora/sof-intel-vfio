#include <stdlib.h>
#include <stdio.h>
#include "list.h"

void list_add_tail(struct node ** head_ref, void * data){
    struct node * new_node = (struct node *) malloc(sizeof(struct node));
    new_node->data = data;
    new_node->next = NULL;

    struct node * n = *head_ref;
    if(n == NULL){
        n = new_node;
        *head_ref = n;
        n->prev = NULL;
        return;
    }

    while(n->next != NULL){
        n = n->next;
    }

    n->next = new_node;
    new_node->prev = n;
}