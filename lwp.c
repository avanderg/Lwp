/* Names: Denis Pyryev (dpyryev)
 *        Aaron VanderGraaff (avanderg)
 * 
 * Date: April 26, 2019
 * Assignment: Program 2
 * Instructor: Professor Phillip Nico
 * Course: CPE 453-03
 *
 * Description: This source file supports the creation and use of lightweight
 *              processes (threads) for external programs. The user is able
 *              to use the lwp functions to create, yield, start, stop, and
 *              exit threads. In addition, there are other helper functions 
 *              to be able to find out which thread is running and if there
 *              is a thread that exists. A Round Robin scheduler is used
 *              for the default scheduler implementation and is able to
 *              switch and transfer threads to a different scheduler if the
 *              user prefers it. The Round Robin scheduler uses the sched_one
 *              and sched_two thread pointers (can be manipulated with admit,
 *              remove, start, and exit) while the lib_one and lib_two
 *              pointers are used to keep track of which threads (which is a
 *              Linked List that is only manipulated through create and exit).
 */

/******************************* LIBRARIES ***********************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "lwp.h"


#define STACK_SLOT 1    /* Used to decrement from the thread's stack */

static struct scheduler rr_sched = {NULL, NULL, sched_admit, 
                                    sched_remove, sched_next};

static scheduler sched = &rr_sched;  /* sched is the scheduler in use */
static thread cur_thread = NULL;    /* Keeps track of current running lwp */
static thread shead = NULL;          /* Head of scheduler list of threads */
static thread lhead = NULL;          /* Head of library list of threads */
static int counter = 1;              /* Used to create thread tids */
static rfile orig_sys_context;       /* Used to save and restore from 
                                        original state (at time of lwp_start)
                                      */


/* Lwp functions */
tid_t lwp_create(lwpfun func, void *arg, size_t stacksize) {
    thread add_thread;
    unsigned long *tos;     /* Thread's top of stack pointer*/

    /* Create the new thread */
    add_thread = safe_malloc(sizeof(context));
    add_thread->stack = safe_malloc(stacksize*sizeof(unsigned long));
    add_thread->tid = counter;
    counter++;
    add_thread->stacksize = stacksize;
    add_thread->state.fxsave = FPU_INIT;

    /* Assign rdi to the argument to the lwpfun */
    add_thread->state.rdi = (unsigned long)(arg);
    
    /* Start building the stack according to x86 calling convention
       Calling convention:

        Stack:
                rbp
                rip
                RA
        What we will do is:

        Stack:
                <Never used (doesn't matter)>
                lwpfun
                lwp_exit
        
        When we change context to this stack, the instruction pointer
        will be set to lwpfun, so our function will run. When the function
        tries to return, it will instead call lwp_exit to clean itself up
        before exiting.
     */

    add_thread->state.rbp = (unsigned long)(add_thread->stack + stacksize 
        - 1);
    
    tos = (void*)add_thread->state.rbp;
    *tos = (unsigned long)(&lwp_exit);
    tos -= STACK_SLOT;
    *tos = (unsigned long)(func);
    tos -= STACK_SLOT;
    *tos = (unsigned long)(add_thread->state.rbp);
    /* Set rbp to the top of stack to be used when this thread is loaded */
    add_thread->state.rbp = (unsigned long)tos;
  
    /* Add the thread to both the library list and scheduler pool */
    lib_add(add_thread);
    sched->admit(add_thread);
    
    return add_thread->tid;
}

void lwp_exit(void) {
    
    /* Set stack pointer to somewhere safe */
    SetSP((void*)(orig_sys_context.rsp));
    
    /* Load a function onto that stack that performs the exit */
    really_exit();
}


tid_t lwp_gettid(void) {
    /* Return the current thread's tid, NO_THREAD if no thread is running */
    if (!cur_thread) {
        return NO_THREAD;
    }
    return cur_thread->tid;
}

void lwp_yield(void) {
   /* Yield current thread to next thread as defined by the scheduler */ 

    thread old = cur_thread;
    cur_thread = sched->next();
    
    if (cur_thread) {
        swap_rfiles(&(old->state), &(cur_thread->state));
    }
    
    /* If there is no thread to run, go home */
    else {
        load_context(&(orig_sys_context));
    }
}

void lwp_start(void) {
    /* Starts the lwp threads */

    /* Set cur_thread as defined by the scheduler */
    cur_thread = sched->next();

    /* If there's a thread to run, run it. Otherwise the function will just
       exit, same as loading orig_sys_context since that's where start is 
       called from. Save orig_sys_context for calls to lwp_stop or for when
       there are no threads in the scheduler.
    */
    if (cur_thread)  {
        swap_rfiles(&(orig_sys_context), &(cur_thread->state));
    }
}

void lwp_stop(void) {
    /* Temporarily stops the lwp processes, more like a pause.
       Can be resumed using lwp_start()
    */

    /* If there is a thread running, swap context back to orig */
    if (cur_thread) {
        swap_rfiles(&(cur_thread->state), &(orig_sys_context));
    }
}

void lwp_set_scheduler(scheduler fun) {
    /* Sets a scheduler for the lwp library, default is Round Robin if this
       function isn't called
    */

    thread saved;
    thread new_sched_head;
    scheduler new_sched = fun;
    
    /* If the passed parameter is NULL, assign the Round Robin scheduler */
    if (!new_sched) {
        /* If the scheduler already is Round Robin, don't need to do 
           anything
        */
        if (sched == &rr_sched) {
            return;
        }
        new_sched = &rr_sched;
    }
   
    /* Init the new_sched if it is defined */
    if (new_sched->init) {
        new_sched->init();
    }
    
    /* The head of the new scheduler will be our current thread */
    new_sched_head = cur_thread;
    
    /* Loop  through the old scheduler, removing threads and admitting them to
       the new scheduler
    */
    while ((cur_thread = sched->next())) {
        saved = cur_thread;
        sched->remove(saved);
        new_sched->admit(saved);
    }

    /* Shutdown the old scheduler if it is defined */
    if (sched->shutdown) {    
        sched->shutdown();
    }
 
    /* Assign cur_thread back to the head of the new scheduler's list */
    /* Essentially, starting at the thread we left off on */
    cur_thread = new_sched_head;

    /* Install the new scheduler */
    sched = new_sched;
}

scheduler lwp_get_scheduler(void) {
    /* Returns the current scheduler */
    return sched;
}

thread tid2thread(tid_t tid) {
    /* Returns the thread associated with a tid, returns NULL if there is 
       no such thread.
     */
    thread out = lhead;

    while((out) && (out->tid != tid)) {
        out = out->lnext;
    }

    return out;
}

/* Round Robin Scheduler Functions */
void sched_admit(thread new) { 
    /* Add a thread to the scheduler's pool */
    
    thread prev;
    thread next;

    /* If there's no thread to add, just return */
    if (!new) {
        return;
    }
    
    /* If the scheduler is empty, initialize this thread as the head */
    if (!shead) {
        shead = new;
        /* For Round Robin, point the head's next and prev to itself to 
           start */
        shead->snext = shead;
        shead->sprev = shead;
        return;
    }
    
    /* Otherwise, add the thread to the list */
    next = shead;
    prev = next->sprev;
    prev->snext = new;
    next->sprev = new;
    new->snext = next;
    new->sprev = prev;
}

void sched_remove(thread victim) {
    /* Remove a thread from the Round Robin Scheduler */
    
    thread prev;
    thread next;

    /* If victim is passed as NULL, just return */
    if (!victim) {
        return;
    }
    
    prev = victim->sprev;
    next = victim->snext;

    /* If the victim is the same as its next, we only have one item in the
       list, so set the head to NULL 
    */
    if (victim->tid == next->tid) {
        cur_thread = NULL;
        shead = NULL;
    }

    /* Otherwise, remove victim from the list */
    else {
        if (victim->tid == shead->tid) {
            shead = next;   
        }
        /* If the victim happens to be the current thread, move the current
           thread to its previous so that when sched->next() is called, the 
           proper next is preserved.
        */
        if (victim->tid == cur_thread->tid) {
            cur_thread = prev;   
        }
        
        next->sprev = prev;
        prev->snext = next;
    }
}

thread sched_next(void) {
    /* Return the next thread in the scheduler */

    /* If there is a curren thread, return its snext */
    if (cur_thread) {
        return cur_thread->snext;
    }

    /* Otherwise, return the head of the list (start the scheduler) */
    return shead;
}

/* Helper Functions */
void *safe_malloc(size_t size) {
    /* Malloc safely with error checking */
    void *out;
    if ((out = malloc(size)) >= 0) {
        return out;
    }
    else {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
}

void lib_add(thread new) {
    /* Add a thread to the library list */
    thread next;

    /* If there is no head, make the new element the head */
    if (!lhead) {
        lhead = new;
        lhead->lnext = NULL;
        lhead->lprev = NULL;
        return;
    }
    
    /* Loop to find the end of the list */
    next = lhead;
    while (next->lnext) {    
        next = next->lnext;
    }

    /* Add new to the list */
    next->lnext = new;
    new->lnext = NULL;
    new->lprev = next;
}

void lib_remove(thread victim) {
    /* Remove a thread from the library list */
    thread next;
    thread prev;

    prev = victim->lprev;
    next = victim->lnext;

    /* if there is a previous, assign its next, otherwise the victim was
     the head, so set its next to the head */
    if (prev) {
        prev->lnext = next;
    }
    else {
        lhead = next;
    }
    /* If there is a next from the victim, assign it's prev to the victim's
       prev
    */
    if (next) {
        next->lprev = prev;
    }
}

void really_exit(void) {
    /* This is the workhorse for lwp_exit() */
   
    /* Remove the current thread from the library list and scheduler list.
      Also free it.
    */ 

    thread remove_thread = cur_thread;
    
    lib_remove(remove_thread);
    sched->remove(remove_thread);
    
    if (remove_thread) {    
        if (remove_thread->stack) {
            free(remove_thread->stack);
        }
        free(remove_thread);
    }

    /* If there is a next to run from the scheduler, go there */
    if ((cur_thread = sched->next())) {
        load_context(&(cur_thread->state));
    }
    /* Otherwise, go home (where lwp_start() was called */
    else {
        load_context(&(orig_sys_context));
    }
}
