#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include "ec440threads.h"

/* libc indexes for registers in jmp_buf */
#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC 7

/* different thread status:
    EXIT: 0
    READY: 1
    RUNNING: 2
    BLOCKED: 3
    EMPTY: 5                    // TCB is empty, for array initalization
*/
#define NUM_THREADS 128         // maximum number of threads alive at the same time
#define STACK_BYTE 32767        // number of bytes to allocate in a threads stack
#define SCHEDULE_TIME_MS 50000  // 50ms quantum, accepted by ualarm in microseconds

TCB TCB_array[NUM_THREADS];     // array of TCBs, used for each thread
pthread_t TID = 0;              // thread ID for currently running thread, needed for scheduling and pthread_self
bool first_call = false;        // flag to indicate if pthread_create has been called before -> switching between single and multithreaded system

struct sigaction handler;       // signal handler for SIGALRM
/*
* struct sigaction {
*     void (*sa_handler)(int);
*     void (*sa_sigaction)(int, siginfo_t *, void *);
*     sigset_t sa_mask;
*     int sa_flags;
*     void (*sa_restorer)(void);
* }
*/
/* helper for initalizing thread subsystem when calling pthread_create for the first time */
void round_robin_schedule()
{
    if(TCB_array[TID].status == 2)      // if it is the current thread is running
    {
        TCB_array[TID].status = 1;      // set status to ready
    }
    pthread_t cur_tid = TID;            // get the current thread ID
    while(1)
    {
        int counter = 1;
        if(cur_tid == NUM_THREADS - 1)
        {
            cur_tid = 0;                // wrap around to the first thread
        }
        else{
            cur_tid++;
        }

        if(TCB_array[cur_tid].status == 1)
        {
            break;                      // found the next ready thread
        }
        if(counter > (NUM_THREADS+100)) // if no ready threads found, exit. Give it some slack just in case...
        {
            perror("No ready threads found");
            exit(EXIT_FAILURE);
        }
    }

    int jmp = 0;                                    // setjmp returns nonzero if returning from longjmp

    if(TCB_array[TID].status != 0)                  // if not ready to exit, then the environment needs to be saved
    {
        jmp = setjmp(TCB_array[TID].registers);     // save the current context for the next thread
    }

    if(!jmp)
    {
        TID = cur_tid;                              // set the current thread to the next ready thread
        TCB_array[TID].status = 2;                  // set status to running
        longjmp(TCB_array[TID].registers, 1);       // jump to the next thread
    }
}

/* to be used when switching the unithreaded program to a multithreaded program */
void first_time()
{
    useconds_t time = SCHEDULE_TIME_MS;     // set time to 50ms
    ualarm(time, time);                     // set up alarm for scheduler
    sigemptyset(&handler.sa_mask);          // ensure that the signal mask is empty
    handler.sa_handler = &round_robin_schedule; // handler is the scheduler
    handler.sa_flags = SA_NODEFER;          // alarms are automatically blocked
    sigaction(SIGALRM, &handler, NULL);     // set up signal handler for SIGALRM, assignment states sigaction
}
/* creates a new thread within a process */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    attr = NULL;                        // Proj states attr always set to NULL
    int thread_cur = 0;                 // running the calling thread, needs to be started as if in context first time making it multithreaded

    if(!first_call)                     // switching to multithreaded system
    {
        /* initalize scheduler */
        for(int i = 0; i < NUM_THREADS; i++)
        {
            TCB_array[i].status = 5;    // set all TCBs to empty
            TCB_array[i].tid = i;       // unique id is index in array
        }

        /* SIGALRM for scheduler timing since multiple threads */
        first_time();
        first_call = true;
        TCB_array[0].status = 2;        // main thread is running
        thread_cur = setjmp(TCB_array[0].registers);    // save current context for thread 0
        /* if returning directly, jump is 0. Else, nonzero when returning from longjmp */
    }
    if(thread_cur == 0)                 // if zero, means that the current thread is running
    {
        pthread_t iter = 1;
        while(TCB_array[iter].status != 5 && iter < NUM_THREADS)    // find the first empty TCB. This works because irst empty TCB is behind the current thread
        {
            iter++;
        }
        if(iter == NUM_THREADS)         // if no empty TCBs, return error. Should never happen.
        {
            perror("No empty TCBs available");
            exit(EXIT_FAILURE);         // assuming that this is not possible, but if there are more than 128 threads it will need to be termianted
        }
        *thread = iter;                 // set thread ID to the first empty TCB

        setjmp(TCB_array[iter].registers);  // save current context for the new thread

        TCB_array[iter].registers[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long)start_thunk);      // set program counter to start_thunk
        TCB_array[iter].registers[0].__jmpbuf[JB_R13] = (unsigned long)arg;                         // set argument to R13
        TCB_array[iter].registers[0].__jmpbuf[JB_R12] = (unsigned long)start_routine;               // set start_routine to R12

        TCB_array[iter].stack = malloc(STACK_BYTE);             // allocate stack for the new thread
        void* stackB = TCB_array[iter].stack + STACK_BYTE;      // set stack pointer to the end of the stack

        void* stackPtr = stackB - sizeof(void*);                // set stack pointer to the end of the stack
        void(*temp)(void*) = (void*) &pthread_exit;             // set temp to pthread_exit
        stackPtr = memcpy(stackPtr, &temp, sizeof(temp));       // copy pthread_exit to the stack

        TCB_array[iter].registers[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long)stackPtr);    // set stack pointer to the stack pointer

        TCB_array[iter].status = 1;     // set status to ready
        TCB_array[iter].tid = iter;     // set thread ID to the index in the array

        round_robin_schedule();         // schedule the new thread
    }
    else
    {
        thread_cur = 0;
    }
    return 0;
}

/* terminates the calling thread */
void pthread_exit(void *value_ptr)
{
    if(TCB_array[TID].tid == TID)
    {
        TCB_array[TID].status = 0;                  // set status to exit
    }

    /* scheduling the next running thread once we are exiting */
    bool blocked = false;
    int i, j;
    for(i = 0; i < NUM_THREADS; i++)
    {
        if(TCB_array[i].status == 3 || TCB_array[i].status == 1)    // if blocked or ready
        {
            blocked = true;
        }
    }

    if(blocked)                             // if there is a blocked or ready thread
    {
        round_robin_schedule();
    }

    for(j = 0; j < NUM_THREADS; j++)        // free the stack of the thread, once we are out of the thread
    {
        if(TCB_array[j].status == 0)
        {
            free(TCB_array[j].stack);       // free the stack of the thread
            TCB_array[j].status = 5;        // set status to empty
        }
    }
    exit(0);
}
/* return the thread ID of the calling thread */
pthread_t pthread_self(void)
{
    return TID;
}

// maximum numebr of threads that can be alive at the same time: 128
// tcb should be stored in a list or array
// create helper function that initalizes thread subsystem after/while application calls pthread_create first time.
// to create a new thread, properly initalize TCB for new thread:
/*
create new thread ID
allocate new stack (malloc) of 32767 byte size
initialize thread's state so that it resumes execution from start function that is given as argument to the pthread_create function
*/


// need a custom signal handler for SIGALRM to manage scheduling

// setjmp: saves the current thread's state
// longjmp: restores the saved state of another thread

// when creating a new thread
// allocate a new stack for the thread
// setjmp to save the current context and modify it so that new thea will start executing the given function
// modive RIP (program counter) and stack pointer (RSP) in the saved context with a helper function

// the stack for each thread must be properly initalized to include the function it needs to execute (start_routine) and the argument. add return address to make sure that the thread calls pthread_exit upon completion
// mangle pointers before saving htem in jmp_buf, using ptr_mangle. decrypt them using ptr_demangle when needed