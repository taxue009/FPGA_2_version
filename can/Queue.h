#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
//#define false 0
//#define true 1

typedef struct Node
{
	unsigned char data;
	//uint16_t command;
	struct Node *next;
}QNode,*QueuePtr;

typedef struct
{
	QueuePtr front;
	QueuePtr rear;
}LinkQueue;


int InitQueue(LinkQueue *Q)
{
	Q ->front=Q ->rear = (QueuePtr)malloc(sizeof(QNode));
	if (Q ->front == NULL)
		return 0;
	Q ->front ->next = NULL;
	return 1; 
}

int DestroyQueue(LinkQueue *Q)
{
	while(Q ->front){
	 Q ->rear=Q ->front ->next;
	 free(Q ->front);
         Q ->front=Q ->rear;
	}
	return 1;
}

int EnQueue(LinkQueue *Q,unsigned char data)
{
	QueuePtr p;
	p = (QueuePtr)malloc(sizeof(QNode));
	if(!p) return 0;
	//memcpy(p ->data,data,1);
	p->data = data;
	p->next = NULL;
	//p->command = command; 
	Q ->rear->next=p;
	Q ->rear=p;
	return 1;	
}

int DeQueue(LinkQueue *Q,unsigned char* data)
{
	QueuePtr p;
	if(Q ->front==Q ->rear) return 0;
	p=Q ->front ->next;
	//memcpy(data,p ->data,1);
	*data = p->data;
	//command = p->command;
	Q ->front->next=p ->next;
	if(p==Q->rear) Q->rear = Q->front;
	free(p);
	return 1;
}

int IsEmpty(LinkQueue *Q){
	if (Q->front->next == NULL)
		return 1;
	else
		return 0;
}
