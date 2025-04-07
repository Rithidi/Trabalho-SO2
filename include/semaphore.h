#pragma once
#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <semaphore.h>

class Semaphore {
public:
    Semaphore(int value = 0) {
        sem_init(&_sem, 0, value);
    }

    ~Semaphore() {
        sem_destroy(&_sem);
    }

    void p() { // wait
        sem_wait(&_sem);
    }

    void v() { // signal
        sem_post(&_sem);
    }

private:
    sem_t _sem;
};

#endif
