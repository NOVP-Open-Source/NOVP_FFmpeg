
#ifndef __EVENTQUEUE_H__
#define __EVENTQUEUE_H__

#define EVENTTYPE_USEREVENT     1000

typedef struct {
    int type;
    void* data;
    double vdouble;
} event_t;

void* init_eventqueue();
void uninit_eventqueue(void* eventqueue);
int push_event(void* eventqueue, event_t* event);
event_t* wait_event(void* eventqueue);
event_t* get_event(void* eventqueue, int type);
event_t* pop_event(void* eventqueue);
void clear_event(void* eventqueue);

#endif
