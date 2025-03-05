#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64

struct ioqueue
{
    struct lock lock;
    //这把锁用来睡眠当缓冲区满了生产者仍然要生产和当缓冲区空了消费者仍然要消费的情况
    struct task_struct* producer;

    struct task_struct* consumer;

    char buf[bufsize];//缓冲区

    int32_t head; // 队首,数据往队首处写入
    int32_t tail; // 队尾,数据从队尾处读出

};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);
#endif
