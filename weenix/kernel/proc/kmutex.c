#include "globals.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/kthread.h"
#include "proc/kmutex.h"

/*
 * IMPORTANT: Mutexes can _NEVER_ be locked or unlocked from an
 * interrupt context. Mutexes are _ONLY_ lock or unlocked from a
 * thread context.
 */

void
kmutex_init(kmutex_t *mtx)
{
	KASSERT(mtx != NULL);
	sched_queue_init(&(mtx->km_waitq));
	(mtx->km_holder)=NULL;
	
	/*((mtx->km_waitq).tq_list).l_next = NULL;
	((mtx->km_waitq).tq_list).l_prev = NULL;
	((mtx->km_waitq).tq_size) = 0;        
	(mtx->km_holder) = NULL;*/ 	
      /*  NOT_YET_IMPLEMENTED("PROCS: kmutex_init");*/
}

/*
 * This should block the current thread (by sleeping on the mutex's
 * wait queue) if the mutex is already taken.
 *
 * No thread should ever try to lock a mutex it already has locked.
 */
void
kmutex_lock(kmutex_t *mtx)
{
       KASSERT(curthr && (curthr != mtx->km_holder) && "Current thread and mutex lock holder are same");
       if(mtx->km_holder != 0)
        {
		dbg(DBG_THR,"The thread of process no %d holds the mutex, So the thread of process no %d is added to mutex queue\n",mtx->km_holder->kt_proc->p_pid,curthr->kt_proc->p_pid);
		sched_sleep_on(&(mtx->km_waitq));
	}
	else
	{
		mtx->km_holder = curthr;
		dbg(DBG_THR,"The thread of process no %d now holds the mutex\n",curthr->kt_proc->p_pid);
	}
		
        
       /* NOT_YET_IMPLEMENTED("PROCS: kmutex_lock");*/
}

/*
 * This should do the same as kmutex_lock, but use a cancellable sleep
 * instead.
 */
int
kmutex_lock_cancellable(kmutex_t *mtx)
{
        KASSERT(curthr && (curthr != mtx->km_holder));
        int sleep_ret=0;
        if(mtx->km_holder !=NULL)
        {
               dbg(DBG_THR,"The thread of process no %d holds the mutex, So the thread of process no %d is added to mutex queue (Cancellable)\n",mtx->km_holder->kt_proc->p_pid,curthr->kt_proc->p_pid);
                sleep_ret=sched_cancellable_sleep_on(&(mtx->km_waitq));
        }
        else
	{
		mtx->km_holder = curthr;
		dbg(DBG_THR,"The thread of process no %d now holds the mutex\n",curthr->kt_proc->p_pid);
	}
        
       /* NOT_YET_IMPLEMENTED("PROCS: kmutex_lock_cancellable");*/
        return sleep_ret;
}

/*
 * If there are any threads waiting to take a lock on the mutex, one
 * should be woken up and given the lock.
 *
 * Note: This should _NOT_ be a blocking operation!
 *
 * Note: Don't forget to add the new owner of the mutex back to the
 * run queue.
 *
 * Note: Make sure that the thread on the head of the mutex's wait
 * queue becomes the new owner of the mutex.
 *
 * @param mtx the mutex to unlock
 */
void
kmutex_unlock(kmutex_t *mtx)
{
        KASSERT(curthr && (curthr == mtx->km_holder)); 
        mtx->km_holder = NULL;
        if(((mtx->km_waitq).tq_size)!=0)
        {
		KASSERT(curthr != mtx->km_holder);
		mtx->km_holder = sched_wakeup_on(&(mtx->km_waitq));
		dbg(DBG_THR,"The thread of process no %d now holds the mutex\n",mtx->km_holder->kt_proc->p_pid);
	}
	else
	{
	        dbg(DBG_THR,"No thread now holds the mutex\n");	
	 }
        /*NOT_YET_IMPLEMENTED("PROCS: kmutex_unlock");*/
}       
