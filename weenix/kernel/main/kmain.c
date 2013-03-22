#include "types.h"
#include "globals.h"
#include "kernel.h"

#include "util/gdb.h"
#include "util/init.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pagetable.h"
#include "mm/pframe.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/cpuid.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/tty/virtterm.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "test/kshell/kshell.h"

#define NO_TEST 0
#define TEST_1 1        /* Processes will be createad and wil exit normaly */ 
#define TEST_2 2        /* "proc_kill()" in multiple processes to terminate bunch of processes */
#define TEST_3 3        /* "proc_kill_all()" Terminate all the processes*/
#define TEST_4 4        /* "kthread_cancel()" Cancel the specific thread */
#define TEST_5 5        /* "kthread_exit()" Exit the current thread */
#define TEST_6 6        
#define TEST_7 7       
#define TEST_8 8         /*  Producer  Consumer test */     
#define TEST_9 9         /*  Deadlock check */       
#define TEST_10 10       /*  Reader and writer problem */
#define TEST_11 11       /*  kshell testing */
static int curtest = TEST_10;

GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);
static void       hard_shutdown(void);

static context_t bootstrap_context;

/**
 * This is the first real C function ever called. It performs a lot of
 * hardware-specific initialization, then creates a pseudo-context to
 * execute the bootstrap function in.
 */
void
kmain()
{
        GDB_CALL_HOOK(boot);

        dbg_init();
        dbgq(DBG_CORE, "Kernel binary:\n");
        dbgq(DBG_CORE, "  text: 0x%p-0x%p\n", &kernel_start_text, &kernel_end_text);
        dbgq(DBG_CORE, "  data: 0x%p-0x%p\n", &kernel_start_data, &kernel_end_data);
        dbgq(DBG_CORE, "  bss:  0x%p-0x%p\n", &kernel_start_bss, &kernel_end_bss);

        page_init();

        pt_init();
        slab_init();
        pframe_init();

        acpi_init();
        apic_init();
        intr_init();

        gdt_init();

        /* initialize slab allocators */
#ifdef __VM__
        anon_init();
        shadow_init();
#endif
        vmmap_init();
        proc_init();
        kthread_init();

#ifdef __DRIVERS__
        bytedev_init();
        blockdev_init();
#endif

        void *bstack = page_alloc();
        pagedir_t *bpdir = pt_get();
        KASSERT(NULL != bstack && "Ran out of memory while booting.");
        context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
        context_make_active(&bootstrap_context);

        panic("\nReturned to kmain()!!!\n");
}

/**
 * This function is called from kmain, however it is not running in a
 * thread context yet. It should create the idle process which will
 * start executing idleproc_run() in a real thread context.  To start
 * executing in the new process's context call context_make_active(),
 * passing in the appropriate context. This function should _NOT_
 * return.
 *
 * Note: Don't forget to set curproc and curthr appropriately.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *bootstrap(int arg1, void *arg2)
{
        /* necessary to finalize page table information */
        pt_template_init();
        char name[]="IDLE";                 
        curproc=proc_create(name);
       KASSERT(curproc != NULL && "Could not create Idle process"); /* make sure that the "idle" process has been created successfully */
        KASSERT(curproc->p_pid == PID_IDLE);
        KASSERT(PID_IDLE == curproc->p_pid && "Process created is not Idle");        
      
        curthr=kthread_create(curproc,idleproc_run,arg1,arg2);
        KASSERT(curthr != NULL && "Could not create thread for Idle process");      

        context_make_active(&(curthr->kt_ctx));
        NOT_YET_IMPLEMENTED("PROCS: bootstrap");
        panic("weenix returned to bootstrap()!!! BAD!!!\n");
        return NULL;
}

/**
 * Once we're inside of idleproc_run(), we are executing in the context of the
 * first process-- a real context, so we can finally begin running
 * meaningful code.
 *
 * This is the body of process 0. It should initialize all that we didn't
 * already initialize in kmain(), launch the init process (initproc_run),
 * wait for the init process to exit, then halt the machine.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *idleproc_run(int arg1, void *arg2)
{
        int status;
        pid_t child;

        /* create init proc */
        kthread_t *initthr = initproc_create();        
        init_call_all();
        GDB_CALL_HOOK(initialized);

        /* Create other kernel threads (in order) */

#ifdef __VFS__
        /* Once you have VFS remember to set the current working directory
         * of the idle and init processes */

        /* Here you need to make the null, zero, and tty devices using mknod */
        /* You can't do this until you have VFS, check the include/drivers/dev.h
         * file for macros with the device ID's you will need to pass to mknod */
#endif

        /* Finally, enable interrupts (we want to make sure interrupts
         * are enabled AFTER all drivers are initialized) */
        intr_enable();
        
        /* Run initproc */
        sched_make_runnable(initthr);
/*        dbg_print("\nidleproc_run returned from sched_make_rinnable and calling do wait_pid \n");*/
        /* Now wait for it */        
        child = do_waitpid(-1, 0, &status);
        dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        KASSERT(PID_INIT == child);

#ifdef __MTP__
        kthread_reapd_shutdown();
#endif


#ifdef __VFS__
        /* Shutdown the vfs: */
        dbg_print("weenix: vfs shutdown...\n");
        vput(curproc->p_cwd);
        if (vfs_shutdown())
                panic("vfs shutdown FAILED!!\n");

#endif

        /* Shutdown the pframe system */
#ifdef __S5FS__
        pframe_shutdown();
#endif

        dbg_print("\nweenix: halted cleanly!\n");
        GDB_CALL_HOOK(shutdown);
        hard_shutdown();
        return NULL;
}

/**
 * This function, called by the idle process (within 'idleproc_run'), creates the
 * process commonly refered to as the "init" process, which should have PID 1.
 *
 * The init process should contain a thread which begins execution in
 * initproc_run().
 *
 * @return a pointer to a newly created thread which will execute
 * initproc_run when it begins executing
 */
static kthread_t *initproc_create(void)
{
        proc_t *procc;
        char name[]="INIT";
        procc=proc_create(name);
        KASSERT(NULL != procc);
        KASSERT(PID_INIT == procc->p_pid);
        kthread_t *initthr;
        initthr=kthread_create(procc,initproc_run,NULL,NULL);       
        KASSERT(initthr != NULL && "Could not create thread for Idle process");      
        return initthr;
}

/**
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/bin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
void *init_child1(int arg1,void *arg2);
void *init_child2(int arg1,void *arg2);
void *get_mul(int arg1,void *arg2);
void *prockill(int arg1, void *arg2);

void *dead1(int arg1,void *arg2);
void *dead2(int arg1,void *arg2);
void *produce(int arg1,void *arg2);
void *consume(int arg1,void *arg2);
void *readers(int arg1,void *arg2);
void *writers(int arg1,void *arg2);
void *kshell_test(int arg1, void *arg2);

void processSetUp();
void deadlock();
void producer_consumer();
void reader_writer();
void shellTest();

kmutex_t m1;
kmutex_t m2;
kmutex_t lock;
ktqueue_t special_q;
int MAX = 10, buffer = 0;

kmutex_t tala;
ktqueue_t rq,wq;
int reader=0, writer=0, active_writer=0;

static void *initproc_run(int arg1, void *arg2)
{
	switch(curtest)
	{
		case 1:	
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7: processSetUp();break;
		case 8: deadlock();break;
		case 9: producer_consumer();break;
		case 10: reader_writer();break;
		case 11: shellTest();break;
	}
        
        
	   int status;
        while(!list_empty(&curproc->p_children))
        {
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
        
        return NULL;
}
void processSetUp(){
	/* 1st child of init  */
        /*proc_t *proc3 = proc_create("proc3");
        KASSERT(proc3 != NULL);
        kthread_t *thread1 = kthread_create(proc3,init_child1,10,(void*)20);
        KASSERT(thread1 !=NULL);
*/
        /* 2nd child init */
        /*proc_t *proc4 = proc_create("proc4");
        KASSERT(proc4 != NULL);
        kthread_t *thread2 = kthread_create(proc4,init_child2,40,(void*)20);
        KASSERT(thread2 !=NULL);
	
		sched_make_runnable(thread1);
        sched_make_runnable(thread2);*/
}

void shellTest()
{ 
        proc_t* new_shell = proc_create("kshell");
        kthread_t *new_shell_thread = kthread_create(new_shell,kshell_test, NULL, NULL);
        sched_make_runnable(new_shell_thread);     
}

void *kshell_test(int a, void *b)
{
    kshell_t *new_shell;
    int i;
    while (1)
    {
        new_shell = kshell_create(0);
        i = kshell_execute_next(new_shell);
        if(i>0){dbg(DBG_TERM,"Error Executing the command\n");}
        kshell_destroy(new_shell);
        if(i==0){break;}
    }
    return NULL;
}

void producer_consumer()
{
        kmutex_init(&lock);
        sched_queue_init(&special_q);

	proc_t *producer = proc_create("producer");
        KASSERT(producer != NULL);
        kthread_t *produce_thread = kthread_create(producer,produce,0,NULL);
        KASSERT(produce_thread != NULL);
        
        proc_t *consumer = proc_create("consumer");
        KASSERT(consumer != NULL);
        kthread_t *consume_thread = kthread_create(consumer,consume,0,NULL);
        KASSERT(consume_thread != NULL);
         
	    sched_make_runnable(produce_thread);
        sched_make_runnable(consume_thread);
        
        int status;
        while(!list_empty(&curproc->p_children))
        {
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
}

void *produce(int arg1,void *arg2) 
{
	int i=0;
	for (i=1;i<=MAX;i++)
	{
		kmutex_lock(&lock);
		while(buffer != 0)
		{
			kmutex_unlock(&lock);
			sched_sleep_on(&special_q);
			kmutex_lock(&lock);
		}
		buffer++;
		dbg_print("\n PRODUCER THREAD PRODUCE %d ITEM IN THE BUFFER \n",buffer);
		sched_wakeup_on(&special_q);
		kmutex_unlock(&lock);
	}
	return NULL;
}

void *consume(int arg1,void *arg2) 
{     
	int i=0;
	for(i=1;i<=MAX;i++)
	{
		kmutex_lock(&lock);
		while(buffer == 0)
		{
			kmutex_unlock(&lock);
			sched_sleep_on(&special_q);
			kmutex_lock(&lock);
		}
		buffer--;
		dbg_print("\n CONSUMER THREAD CONSUME 1 ITEM, THERE ARE %d ITEM IN THE BUFFER\n",buffer);
		sched_wakeup_on(&special_q);
		kmutex_unlock(&lock);	
	}	
	sched_make_runnable(curthr);
	sched_switch();   
	return NULL;
}
#define MAX_WRITER 2
#define MAX_READER 5
void reader_writer()
{
	kmutex_init(&tala);
        sched_queue_init(&rq);
        sched_queue_init(&wq);
        int i=0;
        proc_t *reader[MAX_READER];
        /* = proc_create("reader1");*/
       /* KASSERT(reader1 != NULL);*/
        kthread_t *thrr[MAX_READER];/* = kthread_create(reader1,readers,0,NULL);*/
        /*KASSERT(thrr1 != NULL);*/
        for(i=0;i<MAX_READER;i++)
        {
                dbg_print("\ngg\n");
                reader[i]=proc_create("reader");
                KASSERT(reader[i] != NULL);
                thrr[i]=kthread_create(reader[i],readers,0,NULL);
                KASSERT(thrr[i] != NULL);
                sched_make_runnable(thrr[i]);
        }
       
        sched_make_runnable(curthr);
	sched_switch();
        proc_t *writer[MAX_WRITER];/* = proc_create("writer");
        KASSERT(writer != NULL);*/
        kthread_t *thrw[MAX_WRITER];/* = kthread_create(writer,writers,0,NULL);
		KASSERT(thrw != NULL);*/
         for(i=0;i<MAX_WRITER;i++)
        {
                dbg_print("\ngg\n");
                writer[i]=proc_create("writer");
                KASSERT(writer[i] != NULL);
                thrw[i]=kthread_create(writer[i],writers,0,NULL);
                KASSERT(thrw[i] != NULL);
                sched_make_runnable(thrw[i]);
                sched_make_runnable(curthr);
        	sched_switch();
        }
        /*sched_make_runnable(thrr1);
        sched_make_runnable(thrr2);
        sched_make_runnable(thrr3)
        sched_make_runnable(thrw);*/
        
}

void *readers(int arg1,void *arg2)
{
	int i=0;
	for(i=0;i<=5;i++)
	{
	kmutex_lock(&tala);
	while(!(writer==0))
	{
		kmutex_unlock(&tala);
		sched_sleep_on(&rq);
		kmutex_lock(&tala);
	}
	reader++;
	dbg_print("\n THE VALUE OF READER PLUS is %d \n",reader);
	kmutex_unlock(&tala);
	sched_make_runnable(curthr);
	sched_switch();
	kmutex_lock(&tala);
	reader--;
	dbg_print("\n THE VALUE OF READER MINUS is %d \n",reader);
	if(reader==0)
	{
		sched_wakeup_on(&rq);
		sched_wakeup_on(&wq);
	}
	kmutex_unlock(&tala);

	sched_make_runnable(curthr);
	sched_switch();
        }
	return NULL;
}

void *writers(int arg1,void *arg2)
{
	int i =0;
	for(i=0;i<=5;i++)
	{
	kmutex_lock(&tala);
	writer++;
	dbg_print("\n THE VALUE OF WRITER is %d \n",writer);
	while(!((reader==0)&&(active_writer==0)))
	{
		kmutex_unlock(&tala);
		sched_sleep_on(&wq);
		kmutex_lock(&tala);
	}
	active_writer++;
	dbg_print("\n THE VALUE OF ACTIVE WRITER BEFORE is %d \n",active_writer);
	kmutex_unlock(&tala);
	sched_make_runnable(curthr);
	sched_switch();
	kmutex_lock(&tala);
	active_writer--;
	dbg_print("\n THE VALUE OF ACTIVE WRITER AFTER is %d \n",active_writer);
	writer--;
	if(writer==0)
	{
		sched_broadcast_on(&rq);
	}
	else
	{
		sched_wakeup_on(&wq);
	}
	kmutex_unlock(&tala);

	sched_make_runnable(curthr);
	sched_switch();
}
	return NULL;
}

void deadlock()  
{
        kmutex_init(&m1); 
       	kmutex_init(&m2);
        proc_t *DEADLOCK1 = proc_create("DEADLOCK1");
        KASSERT(DEADLOCK1 != NULL);
        kthread_t *deadly1 = kthread_create(DEADLOCK1,dead1,0,NULL);
        KASSERT(deadly1 != NULL);
        
        proc_t *DEADLOCK2 = proc_create("DEADLOCK2");
        KASSERT(DEADLOCK2 != NULL);
        kthread_t *deadly2 = kthread_create(DEADLOCK2,dead2,0,NULL);
	    KASSERT(deadly2 != NULL);
       
	    sched_make_runnable(deadly1);
        sched_make_runnable(deadly2);
     
	    int status;
        while(!list_empty(&curproc->p_children))
        {
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
}

void *dead1(int arg1,void *arg2) 
{
	kmutex_lock(&m1);
	dbg_print("\n MUTEX 1 LOCKED BY DEAD 1 \n");
	sched_make_runnable(curthr);
	sched_switch();
	kmutex_lock(&m2);
	dbg_print("\n DEADLOCK DONE BY DEAD 1 \n");
	dbg_print("\n MUTEX 2 LOCKED BY DEAD 1 \n");
	kmutex_unlock(&m2);
	dbg_print("\n MUTEX 2 LOCKED BY DEAD 1 \n");
	kmutex_unlock(&m1);
	dbg_print("\n MUTEX 1 LOCKED BY DEAD 1 \n");   
	return NULL;
}

void *dead2(int arg1,void *arg2) 
{   
	kmutex_lock(&m2);
	dbg_print("\n MUTEX 2 LOCKED BY DEAD 2 \n");
	sched_make_runnable(curthr);
	sched_switch();
	dbg_print("\n DEADLOCK DONE BY DEAD 2 \n");
	kmutex_lock(&m1);
	dbg_print("\n MUTEX 1 LOCKED BY DEAD 2 \n");
	kmutex_unlock(&m1);
	dbg_print("\n MUTEX 1 LOCKED BY DEAD 2 \n");
	kmutex_unlock(&m2); 
	dbg_print("\n MUTEX 2 LOCKED BY DEAD 2 \n");
	return NULL;
	
}

void *prockill(int arg1, void *arg2)
{

if(curtest == 2)
 {
   proc_t *proc = proc_lookup(6);
   proc_kill(proc,0);
 } 
else if(curtest == 3)
  {
        proc_kill_all();
  }

return NULL;
}


/**
 * Clears all interrupts and halts, meaning that we will never run
 * again.
 */
static void
hard_shutdown()
{
#ifdef __DRIVERS__
        vt_print_shutdown();
#endif
        __asm__ volatile("cli; hlt");
}
