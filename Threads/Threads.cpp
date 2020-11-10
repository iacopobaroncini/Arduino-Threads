/*

  Threads.cpp - Threads library
  (c)2019 Iacopo Baroncini

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  16 Oct 2019 - initial release 

*/


#include <Threads.h>


#define PUSH_REGS \
    asm ( \
        "push r0\n""push r1\n""push r2\n""push r3\n""push r4\n" \
        "push r5\n""push r6\n""push r7\n""push r8\n""push r9\n" \
        "push r10\n""push r11\n""push r12\n""push r13\n""push r14\n" \
        "push r15\n""push r16\n""push r17\n""push r18\n""push r19\n" \
        "push r20\n""push r21\n""push r22\n""push r23\n""push r24\n" \
        "push r25\n""push r26\n""push r27\n""push r28\n""push r29\n" \
        "push r30\n""push r31\n" \
    )

#define POP_REGS \
    asm ( \
        "pop r31\n""pop r30\n" \
        "pop r29\n""pop r28\n""pop r27\n""pop r26\n""pop r25\n" \
        "pop r24\n""pop r23\n""pop r22\n""pop r21\n""pop r20\n" \
        "pop r19\n""pop r18\n""pop r17\n""pop r16\n""pop r15\n" \
        "pop r14\n""pop r13\n""pop r12\n""pop r11\n""pop r10\n" \
        "pop r9\n""pop r8\n""pop r7\n""pop r6\n""pop r5\n" \
        "pop r4\n""pop r3\n""pop r2\n""pop r1\n""pop r0\n" \
    )


typedef enum {
    T_STATE_NONE,
    T_STATE_TO_BE_STARTED,
    T_STATE_RUNNABLE,
    T_STATE_DELAYED,
    T_STATE_WAIT_FOR_PERIOD,
    T_STATE_SUSPENDED
} ThreadState;


volatile static bool os_started = false;
volatile static uint8_t os_count = 0;
volatile static uint8_t os_active_count = 0;


static void os_scheduler();


class OS_Context {

    public:
        
        Thread *thread;
        ThreadState state;
        uint8_t spl;
        uint8_t sph;
        uint8_t sreg;
        uint32_t msPeriod;
        uint32_t msDelay;
        uint32_t msSleep;
        uint32_t msLoop;
        uint32_t msLoopDuration;
    
        OS_Context() {
            thread = NULL;
            state = T_STATE_NONE;
            spl = sph = sreg = 0;
            msPeriod = msDelay = msSleep = msLoop = msLoopDuration = 0;
        }

        void run() {
            state = T_STATE_RUNNABLE;            
            thread->body();            
            thread->wrapup();
            state = T_STATE_NONE;
            os_active_count--;
            os_scheduler();
        }

        inline void periodSync(uint32_t ms) {
            msPeriod = ms;
            if (msLoop) {
                state = T_STATE_WAIT_FOR_PERIOD;
                os_scheduler();
                msLoopDuration = millis() - msLoop;
            }
            msLoop = millis();
        }

};


volatile static OS_Context os_context[MAX_NUM_OF_THREADS];


static void os_scheduler() {
    volatile static int8_t os_main_spl;
    volatile static int8_t os_main_sph;
    volatile static int8_t os_main_sreg;
    volatile static int8_t os_running = -1;
    volatile static uint8_t id;
    volatile static OS_Context *os_running_context = NULL;
    volatile static OS_Context *tmp;
    volatile static OS_Context *next;
    if (!os_started || !os_count) return;
    if (!os_active_count) {
        cli();
        SPL = os_main_spl;
        SPH = os_main_sph;
        SREG = os_main_sreg;
        sei();
        return;
    }
    id = os_running;
    next = NULL;
    while (!next) {
        id = (id + 1) % os_count;
        tmp = &os_context[id];
        switch (tmp->state) {
            case T_STATE_TO_BE_STARTED:
            case T_STATE_RUNNABLE:
                next = tmp;
                break;
            case T_STATE_DELAYED:
                if ((millis() - tmp->msSleep) < tmp->msDelay) break;
                tmp->state = T_STATE_RUNNABLE;
                next = tmp;
                break;
            case T_STATE_WAIT_FOR_PERIOD:
                if ((millis() - tmp->msLoop) < tmp->msPeriod) break;
                tmp->state = T_STATE_RUNNABLE;
                next = tmp;
                break;
        }
    }
    os_running = id;
    if (os_running_context == next) return;
    cli();
    if (os_running_context) {
        os_running_context->sreg = SREG;
        PUSH_REGS;
        os_running_context->spl = SPL;
        os_running_context->sph = SPH;
    } else {
        os_main_spl = SPL;
        os_main_sph = SPH;
        os_main_sreg = SREG;
    }
    os_running_context = next;
    SPL = os_running_context->spl;
    SPH = os_running_context->sph;
    bool threadStarted = os_running_context->state != T_STATE_TO_BE_STARTED;
    if (threadStarted) POP_REGS;
    SREG = os_running_context->sreg;
    sei();
    if (!threadStarted) os_running_context->run();
}


Thread::Thread(ThreadStackSize stackSize) {
    id = -1;
    if (os_count == MAX_NUM_OF_THREADS) return;
    uint16_t n = 64 << stackSize;
    uint8_t *stack = (uint8_t *) malloc(n);
    if (!stack) return;
    id = os_count++;
    OS_Context *tmp = &os_context[id];
    stack += n - 1;
    cli();
    tmp->sreg = SREG;
    tmp->spl = (uint8_t) (((uint16_t) stack) & 0xff);
    tmp->sph = (uint8_t) ((((uint16_t) stack) >> 8) & 0xff);
    sei();
    tmp->state = T_STATE_TO_BE_STARTED;
    tmp->msDelay = tmp->msPeriod = tmp->msLoop = tmp->msLoopDuration = 0;
    tmp->thread = this;
    os_active_count++;
}


uint8_t Thread::getID() {
    return id;
}


void Thread::yield() {
    if (!os_started || id < 0) return;
    os_scheduler();
}
 
    
void Thread::yieldAndDelay(uint32_t ms) {
    if (!os_started || id < 0) return;
    OS_Context *tmp = &os_context[id];
    tmp->msDelay = ms;
    tmp->msSleep = millis();
    tmp->state = T_STATE_DELAYED;
    os_scheduler();
}

    
void Thread::suspend() {
    if (!os_started || id < 0) return;
    os_context[id].state = T_STATE_SUSPENDED;
    os_scheduler();
}


void Thread::resume() {
    if (!os_started || id < 0) return;
    OS_Context *tmp = &os_context[id];
    if (tmp->state != T_STATE_SUSPENDED) return;
    tmp->state = T_STATE_RUNNABLE;
    os_scheduler();
}


void Thread::terminate() {
    if (!os_started || id < 0) return;
    OS_Context *tmp = &os_context[id];
    if (tmp->state != T_STATE_NONE) {
        wrapup();
        tmp->state = T_STATE_NONE;
        os_active_count--;
    }
    os_scheduler();
}


bool Thread::periodSync(uint32_t ms) {
    if (!os_started || id < 0) return;
    os_context[id].periodSync(ms);
    return true;
}


uint32_t Thread::periodDelay() {
    if (!os_started || id < 0) return 0;
    OS_Context *tmp = &os_context[id];
    if (!tmp->msPeriod) return 0;
    return (tmp->msLoopDuration > tmp->msPeriod) ? (tmp->msLoopDuration - tmp->msPeriod) : 0; 
}


void Thread::terminateAll() {
    if (!os_started || !os_count) return;
    os_started = false;
    for (uint8_t i = 0; i < os_count; i++) {
        OS_Context *tmp = &os_context[i];
        if (tmp->thread->getID() < 0 || tmp->state == T_STATE_NONE) continue; 
        tmp->thread->wrapup();
    }
    os_started = true;
    os_active_count = 0;
    os_scheduler();
}


bool runAllThreads() {
    if (os_started || !os_count) return false;
    for (uint8_t i = 0; i < os_count; i++) {
        OS_Context *tmp = &os_context[i];
        if (tmp->thread->getID() < 0) continue;
        tmp->thread->setup();
    }
    os_started = true;
    os_scheduler();
    return true;
}
