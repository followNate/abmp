
#include "config.h"
#include "globals.h"

#include "errno.h"

#include "util/init.h"
#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"

#include "mm/slab.h"
#include "mm/page.h"

kthread_t *curthr; /* global */

static slab_allocator_t *kthread_allocator = NULL;

#ifdef __MTP__
/* Stuff for the reaper daemon, which cleans up dead detached threads */
static proc_t *reapd = NULL;
static kthread_t *reapd_thr = NULL;
static ktqueue_t reapd_waitq;
static list_t kthread_reapd_deadlist; /* Threads to be cleaned */

static void *kthread_reapd_run(int arg1, void *arg2);
#endif

void
kthread_init()
{
        kthread_allocator = slab_allocator_create("kthread", sizeof(kthread_t));
        KASSERT(NULL != kthread_allocator);
}

/**
 * Allocates a new kernel stack.
 *
 * @return a newly allocated stack, or NULL if there is not enough
 * memory available
 */
static char *alloc_stack(void)
{
        /* extra page for "magic" data */
        char *kstack;
        int npages = 1 + (DEFAULT_STACK_SIZE >> PAGE_SHIFT);
        kstack = (char *)page_alloc_n(npages);

        return kstack;
}

/**
 * Frees a stack allocated with alloc_stack.
 *
 * @param stack the stack to free
 */
static void
free_stack(char *stack)
{
        page_free_n(stack, 1 + (DEFAULT_STACK_SIZE >> PAGE_SHIFT));
}

/*
 * Allocate a new stack with the alloc_stack function. The size of the
 * stack is DEFAULT_STACK_SIZE.
 *
 * Don't forget to initialize the thread context with the
 * context_setup function. The context should have the same pagetable
 * pointer as the process.
 */
kthread_t *kthread_create(struct proc *p, kthread_func_t func, long arg1, void *arg2)
{
        KASSERT(NULL != p);
        
        /* allocate a slab to a thread */
        kthread_t *new_kthread_t = slab_obj_alloc(kthread_allocator);
        KASSERT(new_kthread_t != NULL);
        
        /*empty the slab contents */
        memset(new_kthread_t, 0, sizeof(kthread_t));
        
        /* allocate the stack for new thread */
        new_kthread_t->kt_kstack = alloc_stack();
        KASSERT(new_kthread_t->kt_kstack != NULL);
        
        /* thread's process */
        new_kthread_t->kt_proc = p;
       
        /* insert the thread link into process list */
        list_insert_head(&(p->p_threads),&(new_kthread_t->kt_plink));


        /* set the current state of new thread */
        new_kthread_t->kt_wchan=NULL;
        new_kthread_t->kt_state = KT_NO_STATE;
        
        context_setup(&(new_kthread_t->kt_ctx),func,arg1,arg2,(new_kthread_t->kt_kstack),DEFAULT_STACK_SIZE,(p->p_pagedir));          
        
        dbg(DBG_THR,"A thread is created for process %s (PID=%d)\n",p->p_comm,p->p_pid);
        /*NOT_YET_IMPLEMENTED("PROCS: kthread_create");*/
        return new_kthread_t;
}

void
kthread_destroy(kthread_t *t)
{
        KASSERT(t && t->kt_kstack);
        dbg(DBG_THR,"The thread of process %s (PID=%d) is destroyed\n",t->kt_proc->p_comm,t->kt_proc->p_pid);
        free_stack(t->kt_kstack);
        if (list_link_is_linked(&t->kt_plink))
                list_remove(&t->kt_plink);
        
        slab_obj_free(kthread_allocator, t);     
}

/*
 * If the thread to be cancelled is the current thread, this is
 * equivalent to calling kthread_exit. Otherwise, the thread is
 * sleeping and we need to set the cancelled and retval fields of the
 * thread.
 *
 * If the thread's sleep is cancellable, cancelling the thread should
 * wake it up from sleep.
 *
 * If the thread's sleep is not cancellable, we do nothing else here.
 */
void kthread_cancel(kthread_t *kthr, void *retval)
{
        KASSERT(NULL != kthr);
        if(kthr == curthr)
        {
                dbg(DBG_THR,"Current thread (thread of process %d) is cancelled \n",kthr->kt_proc->p_pid);
                kthread_exit(retval);             
        }
        else 
        {
                kthr->kt_retval=retval;
                sched_cancel(kthr);
                dbg(DBG_THR,"The thread of process %d is scheduled to be cancelled \n",kthr->kt_proc->p_pid);
        }
       /* NOT_YET_IMPLEMENTED("PROCS: kthread_cancel");*/
}

/*
 * You need to set the thread's retval field, set its state to
 * KT_EXITED, and alert the current process that a thread is exiting
 * via proc_thread_exited.
 *
 * It may seem unneccessary to push the work of cleaning up the thread
 * over to the process. However, if you implement MTP, a thread
 * exiting does not necessarily mean that the process needs to be
 * cleaned up.
 */
void kthread_exit(void *retval)
{
        KASSERT(!curthr->kt_wchan);
        KASSERT(!curthr->kt_qlink.l_next && !curthr->kt_qlink.l_prev);
        KASSERT(curthr->kt_proc == curproc);
        curthr->kt_retval = retval;
        curthr->kt_state = KT_EXITED;
        dbg(DBG_THR,"Exiting Current thread (thread of process %d)\n",curthr->kt_proc->p_pid);
        proc_thread_exited(retval);
        
       /* NOT_YET_IMPLEMENTED("PROCS: kthread_exit");*/
}

/*
 * The new thread will need its own context and stack. Think carefully
 * about which fields should be copied and which fields should be
 * freshly initialized.
 *
 * You do not need to worry about this until VM.
 */
kthread_t *
kthread_clone(kthread_t *thr)
{
		/* kthread_create() cannot be called as all the members of the struct
		kthread will be initailized which goea against the definition given 
		above. So we start from the basic used in kthread_create() */
       
		KASSERT(NULL != curproc);
		
		/* allocate a slab to a thread */
        kthread_t *clone_thread = slab_obj_alloc(kthread_allocator);
        KASSERT(clone_thread != NULL);
   
		/*empty the slab contents */
        memset(clone_thread, 0, sizeof(kthread_t));

		/* allocate the stack for new thread */
        clone_thread->kt_kstack = alloc_stack();
        KASSERT(clone_thread->kt_kstack != NULL);
        
		/* set the context of the clone thread */
        context_setup(&(clone_thread->kt_ctx),NULL,0,0,(clone_thread->kt_kstack),DEFAULT_STACK_SIZE,(curproc->p_pagedir));          
        
       
        
        
        /* thread's process */
		clone_thread->kt_proc = curproc;
       
        /* set the current state of new thread */
        clone_thread->kt_wchan = NULL;
        clone_thread->kt_state = KT_RUN;
		clone_thread->kt_cancelled = 0;
		
        /* insert the thread link into process list */
        list_insert_head(&(curproc->p_threads),&(clone_thread->kt_plink));
            
        dbg(DBG_THR,"A clone thread is created for process %s (PID=%d)\n",curproc->p_comm,curproc->p_pid);
       
        return clone_thread;
        NOT_YET_IMPLEMENTED("VM: kthread_clone");
        return NULL;
}

/*
 * The following functions will be useful if you choose to implement
 * multiple kernel threads per process. This is strongly discouraged
 * unless your weenix is perfect.
 */
#ifdef __MTP__
int
kthread_detach(kthread_t *kthr)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_detach");
        return 0;
}

int
kthread_join(kthread_t *kthr, void **retval)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_join");
        return 0;
}

/* ------------------------------------------------------------------ */
/* -------------------------- REAPER DAEMON ------------------------- */
/* ------------------------------------------------------------------ */
static __attribute__((unused)) void
kthread_reapd_init()
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_init");
}
init_func(kthread_reapd_init);
init_depends(sched_init);

void
kthread_reapd_shutdown()
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_shutdown");
}

static void *
kthread_reapd_run(int arg1, void *arg2)
{
        NOT_YET_IMPLEMENTED("MTP: kthread_reapd_run");
        return (void *) 0;
}
#endif

