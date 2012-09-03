
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "eventqueue.h"

#define MAX_EVENTS 256

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    event_t* events[MAX_EVENTS];
    int eventno;
} eventqueue_t;


void* init_eventqueue() {
    eventqueue_t* queue = (eventqueue_t*)malloc(sizeof(eventqueue_t));
    memset(queue,0,sizeof(eventqueue_t));
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return queue;
}

void uninit_eventqueue(void* eventqueue) {
    eventqueue_t* queue = (eventqueue_t*)eventqueue;
    int n;

    if(!queue) {
        return;
    }
    for(n=0;n<queue->eventno;n++) {
        free(queue->events[n]);
    }
    queue->eventno=0;
    pthread_mutex_lock(&queue->mutex);
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
}

void clear_event(void* eventqueue) {
    eventqueue_t* queue = (eventqueue_t*)eventqueue;
    int n;

    if(!queue) {
        return;
    }
    for(n=0;n<queue->eventno;n++) {
        free(queue->events[n]);
    }
    queue->eventno=0;
    pthread_mutex_lock(&queue->mutex);
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

int push_event(void* eventqueue, event_t* event) {
    eventqueue_t* queue = (eventqueue_t*)eventqueue;

    if(!queue) {
        return 1;
    }
    if(queue->eventno==MAX_EVENTS) {
        return 1;
    }
    pthread_mutex_lock(&queue->mutex);
    queue->events[queue->eventno]=malloc(sizeof(event_t));
    memcpy(queue->events[queue->eventno],event,sizeof(event_t));
    queue->eventno++;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

event_t* wait_event(void* eventqueue) {
    eventqueue_t* queue = (eventqueue_t*)eventqueue;
    event_t* event = NULL;
    int n;

    if(!queue) {
        return NULL;
    }
    pthread_mutex_lock(&queue->mutex);
    if(!queue->eventno) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    if(!queue->eventno) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    event = queue->events[0];
    for(n=1;n<queue->eventno;n++) {
        queue->events[n-1]=queue->events[n];
    }
    queue->events[queue->eventno]=NULL;
    queue->eventno--;
    pthread_mutex_unlock(&queue->mutex);
    return event;
}

event_t* get_event(void* eventqueue, int type) {
    eventqueue_t* queue = (eventqueue_t*)eventqueue;
    event_t* event = NULL;
    int n;

    if(!queue) {
        return NULL;
    }
    pthread_mutex_lock(&queue->mutex);
    for(n=0;n<queue->eventno;n++) {
        if(queue->events[n]->type==type) {
            event=queue->events[n];
            break;
        }
    }
    pthread_mutex_unlock(&queue->mutex);
    return event;
}

event_t* pop_event(void* eventqueue) {
    eventqueue_t* queue = (eventqueue_t*)eventqueue;
    event_t* event = NULL;
    int n;

    if(!queue) {
        return NULL;
    }
    pthread_mutex_lock(&queue->mutex);
    if(!queue->eventno) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    event = queue->events[0];
    for(n=1;n<queue->eventno;n++) {
        queue->events[n-1]=queue->events[n];
    }
    queue->events[queue->eventno]=NULL;
    queue->eventno--;
    pthread_mutex_unlock(&queue->mutex);
    return event;
}

