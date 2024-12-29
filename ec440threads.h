#ifndef __EC440THREADS__
#define __EC440THREADS__

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

unsigned long int ptr_demangle(unsigned long int p)
{
    unsigned long int ret;

    asm("movq %1, %%rax;\n"
        "rorq $0x11, %%rax;"
        "xorq %%fs:0x30, %%rax;"
        "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}

unsigned long int ptr_mangle(unsigned long int p)
{
    unsigned long int ret;

    asm("movq %1, %%rax;\n"
        "xorq %%fs:0x30, %%rax;"
        "rolq $0x11, %%rax;"
        "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}

/*void pthread_retsv(){
        unsigned long int reg;

        asm("movq %%rax, %0\n"
                :"=r"(reg));

    pthread_exit((void *) reg);
}
*/

void *start_thunk() {
  asm("popq %%rbp;\n"           //clean up the function prolog
      "movq %%r13, %%rdi;\n"    //put arg in $rdi
      "pushq %%r12;\n"          //push &start_routine
      "retq;\n"                 //return to &start_routine
      :
      :
      : "%rdi"
  );
  __builtin_unreachable();
}

// different thread status:
// exit, ready, running, blocked

/* thread control block struct */
typedef struct TCB{
    pthread_t tid;                  // thread ID, needed for scheduling
    void *stack;
    jmp_buf registers;              // register values for the thread (env)
    int status;
} TCB;

void round_robin_schedule();        // schedule the next thread to run, 50ms quantum

void first_time();                  // used when the application calls pthread_create for the first time, transition to multithreaded system

int pthread_create(                 // create a new thread within a proces. Upon successful completion,
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine)(void *),
    void *arg
);

void pthread_exit(void *value_ptr); // terminates the calling thread. ignore value passed in as the first argument and clean up all information related to t terminating thread.

pthread_t pthread_self(void);       // return the thread ID of the calling thread



#endif