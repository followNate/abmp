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
#define MAX_WRITER 5
#define MAX_READER 10
#define TEST_0 0        /*  NO test performed OS returns from INIT */
#define TEST_1 1        /*  Processes will be createad and wil exit normaly */ 
#define TEST_2 2        /* "proc_kill()" in multiple processes to terminate bunch of processes */
#define TEST_3 3        /* "proc_kill_all()" Terminate all the processes*/
#define TEST_4 4        /* "kthread_cancel()" Cancel the specific thread */
#define TEST_5 5        /* "kthread_exit()" Exit the current thread */
#define TEST_6 6        /*  Producer  Consumer test */
#define TEST_7 7        /*  Deadlock check when two threads competing for the mutex */
#define TEST_8 8        /*  Reader and writer problem */
#define TEST_9 9        /*  kshell testing */
#define TEST_10 10      /*  Deadlock check when same thread again trying to lock the same mutex */
static int curtest = TEST_0;

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
void *deadlock_own(int arg1, void *arg2);
void processSetUp();
void deadlock();
void producer_consumer();
void reader_writer();
void dead_own();
void shellTest();

kmutex_t m1;
kmutex_t m2;
kmutex_t lock;
ktqueue_t prod;
ktqueue_t cons;
int MAX = 5, buffer = 0;

kmutex_t tala, self;
ktqueue_t rq,wq;
int reader=0, writer=0, active_writer=0;

static void *initproc_run(int arg1, void *arg2)
{
	switch(curtest)
	{       
	        case 0: return NULL;
		case 1:	processSetUp(); break;
		case 2: processSetUp(); break;
		case 3: processSetUp(); break;
		case 4: processSetUp(); break;
       		case 5: processSetUp(); break;
		case 6: producer_consumer();break;
		case 7: deadlock();break;
		case 8: reader_writer();break;
		case 9: shellTest();break;
		case 10: dead_own(); break;
	}
                
	   int status;
        while(!list_empty(&curproc->p_children))
        {
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
        
        return NULL;
}

void *init_child10(int arg1,void *arg2) 
{
if(curtest == TEST_3){
 while(curthr->kt_cancelled != 1){
 sched_make_runnable(curthr);
 sched_switch();}}
return NULL;
}
void *init_child9(int arg1,void *arg2) 
{
if(curtest == TEST_3){
 while(curthr->kt_cancelled != 1){
 sched_make_runnable(curthr);
 sched_switch(); }}
return NULL;
}
void *init_child8(int arg1,void *arg2) 
{ 
if(curtest == 5)
 kthread_exit(0);
if(curtest == 4)
        {
            kthread_t *cur_proc_thd;
            proc_t *proc23 = proc_lookup(3); 
            list_iterate_begin(&(proc23->p_threads), cur_proc_thd, kthread_t, kt_plink)
            { }list_iterate_end();
            kthread_cancel(cur_proc_thd,0);
        }       
return NULL;
}
void *init_child7(int arg1,void *arg2) 
{
        if(curtest == TEST_2)
         {
           proc_t *proc = proc_lookup(3);
           proc_kill(proc,0);
         }
        else if(curtest == TEST_3)
         {
           proc_kill_all();
         }
return NULL;
}
void *init_child6(int arg1,void *arg2) 
{
 sched_make_runnable(curthr);
 sched_switch(); 
return NULL;
}
void *init_child5(int arg1,void *arg2) 
{
 sched_make_runnable(curthr);
 sched_switch();
return NULL;
}
void *init_child4(int arg1,void *arg2) 
{
        /* 7th child init */
        proc_t *proc11 = proc_create("proc11");KASSERT(proc11 != NULL);
        kthread_t *thread11 = kthread_create(proc11,init_child9,10,(void*)20);KASSERT(thread11 !=NULL);
        /* 8th child init */
        proc_t *proc12 = proc_create("proc12");KASSERT(proc12 != NULL);
        kthread_t *thread12 = kthread_create(proc12,init_child10,40,(void*)20);KASSERT(thread12 !=NULL);	
	sched_make_runnable(thread11); sched_make_runnable(thread12);
	int status;
        while(!list_empty(&curproc->p_children))
        {       pid_t child = do_waitpid(-1, 0, &status);
        if(child>0)      dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
return NULL;
}
void *init_child3(int arg1,void *arg2) 
{
        /* 7th child init */
        proc_t *proc9 = proc_create("proc9");KASSERT(proc9 != NULL);
        kthread_t *thread9 = kthread_create(proc9,init_child7,10,(void*)20);KASSERT(thread9 !=NULL);
        /* 8th child init */
        proc_t *proc10 = proc_create("proc10");KASSERT(proc10 != NULL);
        kthread_t *thread10 = kthread_create(proc10,init_child8,40,(void*)20);KASSERT(thread10 !=NULL);	
	sched_make_runnable(thread9); sched_make_runnable(thread10);
	int status;
        while(!list_empty(&curproc->p_children))
        {       pid_t child = do_waitpid(-1, 0, &status);
        if(child>0)     dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
return NULL;
}
void *init_child2(int arg1,void *arg2) 
{
        /* 5th child init */
        proc_t *proc7 = proc_create("proc7");KASSERT(proc7 != NULL);
        kthread_t *thread7 = kthread_create(proc7,init_child5,10,(void*)20);KASSERT(thread7 !=NULL);
        /* 6th child init */
        proc_t *proc8 = proc_create("proc8");KASSERT(proc8 != NULL);
        kthread_t *thread8 = kthread_create(proc8,init_child6,40,(void*)20);KASSERT(thread8 !=NULL);	
	sched_make_runnable(thread7); sched_make_runnable(thread8);
	int status;
        while(!list_empty(&curproc->p_children))
        {       pid_t child = do_waitpid(-1, 0, &status);
        if(child>0)     dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
return NULL;
}
void *init_child1(int arg1,void *arg2) 
{       
        /* 3rd child init */
        proc_t *proc5 = proc_create("proc5");KASSERT(proc5 != NULL);
        kthread_t *thread5 = kthread_create(proc5,init_child3,10,(void*)20);KASSERT(thread5 !=NULL);
        /* 4th child init */
        proc_t *proc6 = proc_create("proc6");KASSERT(proc6 != NULL);
        kthread_t *thread6 = kthread_create(proc6,init_child4,40,(void*)20);KASSERT(thread6 !=NULL);	
	sched_make_runnable(thread5); sched_make_runnable(thread6);
	int status;
	
        while(!list_empty(&curproc->p_children))
        {       pid_t child = do_waitpid(-1, 0, &status);
        if(child>0) dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
return NULL;
}

void processSetUp()
{
	/* 1st child of init  */
        proc_t *proc3 = proc_create("proc3");
        KASSERT(proc3 != NULL);
        kthread_t *thread1 = kthread_create(proc3,init_child1,10,(void*)20);
        KASSERT(thread1 !=NULL);
        /* 2nd child init */
        proc_t *proc4 = proc_create("proc4");
        KASSERT(proc4 != NULL);
        kthread_t *thread2 = kthread_create(proc4,init_child2,40,(void*)20);
        KASSERT(thread2 !=NULL);	
	sched_make_runnable(thread1);
        sched_make_runnable(thread2);
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
        sched_queue_init(&prod);
        sched_queue_init(&cons);

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
	for (i=1;i<=10*MAX;i++)
	{
		kmutex_lock(&lock);
		while(buffer == MAX)
		{
			kmutex_unlock(&lock);
			dbg(DBG_INIT," THE BUFFER IS CURRENTLY FULL \n");
			sched_sleep_on(&prod);
			kmutex_lock(&lock);
		}
		buffer++;
		dbg(DBG_INIT," PRODUCER THREAD PRODUCE 1 ITEM, NOW THERE ARE %d ITEM IN THE BUFFER \n",buffer);
		sched_wakeup_on(&cons);
		kmutex_unlock(&lock);
		if(i%3==0)
		{
		sched_make_runnable(curthr);
		sched_switch();
		}
	}
	return NULL;
}

void *consume(int arg1,void *arg2) 
{     
	int i=0;
	for(i=1;i<=10*MAX;i++)
	{
		kmutex_lock(&lock);
		while(buffer == 0)
		{
			kmutex_unlock(&lock);
			dbg(DBG_INIT," THE BUFFER IS CURRENTLY EMPTY \n");
			sched_sleep_on(&cons);
			kmutex_lock(&lock);
		}
		buffer--;
		dbg(DBG_INIT," CONSUMER THREAD CONSUME 1 ITEM, NOW THERE ARE %d ITEM IN THE BUFFER\n",buffer);
		sched_wakeup_on(&prod);
		kmutex_unlock(&lock);
		if (i%2 == 0)
		{
		sched_make_runnable(curthr);
		sched_switch();		
		}	
	}	
	 
	return NULL;
}


void reader_writer()
{
	kmutex_init(&tala);
        sched_queue_init(&rq);
        sched_queue_init(&wq);
        int i=0,j=0;
        proc_t *reader[MAX_READER];
        /* = proc_create("reader1");*/
       /* KASSERT(reader1 != NULL);*/
        kthread_t *thrr[MAX_READER];/* = kthread_create(reader1,readers,0,NULL);*/
        /*KASSERT(thrr1 != NULL);*/
   reader:     while(i<MAX_READER)
        {

                reader[i]=proc_create("reader");
                KASSERT(reader[i] != NULL);
                thrr[i]=kthread_create(reader[i],readers,0,NULL);
                KASSERT(thrr[i] != NULL);
                sched_make_runnable(thrr[i]);
                sched_make_runnable(curthr);
                i++;
        	sched_switch();
        	goto writer;
        }
       
        sched_make_runnable(curthr);
	sched_switch();
        proc_t *writer[MAX_WRITER];/* = proc_create("writer");
        KASSERT(writer != NULL);*/
        kthread_t *thrw[MAX_WRITER];/* = kthread_create(writer,writers,0,NULL);
		KASSERT(thrw != NULL);*/
   writer:      while(j<MAX_WRITER)
        {
                writer[j]=proc_create("writer");
                KASSERT(writer[j] != NULL);
                thrw[j]=kthread_create(writer[j],writers,0,NULL);
                KASSERT(thrw[j] != NULL);
                sched_make_runnable(thrw[j]);
                sched_make_runnable(curthr);
                j++;
        	sched_switch();
        	goto reader;
        }
                
}

void *readers(int arg1,void *arg2)
{
	int i=0;
	for(i=0;i<=1;i++)
	{
	kmutex_lock(&tala);
	while(!(writer==0))
	{
		kmutex_unlock(&tala);
		dbg(DBG_INIT," NEW READER ARRIVED AND SOME WRITER IS WRITING \n");
		sched_sleep_on(&rq);
		kmutex_lock(&tala);
	}
	reader++;
	dbg(DBG_INIT," (BEFORE READ) NUMBER OF ACTIVE READERS = %d NUMBER OF WAITING WRITERS = %d\n",reader,writer);
	kmutex_unlock(&tala);
	sched_make_runnable(curthr);
	sched_switch();
	kmutex_lock(&tala);
	reader--;
	dbg(DBG_INIT," (AFTER READ) NUMBER OF ACTIVE READERS = %d NUMBER OF WAITING WRITERS = %d\n",reader,writer);
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
	for(i=0;i<=1;i++)
	{
	kmutex_lock(&tala);
	writer++;
	dbg(DBG_INIT," NUMBER OF READERS = %d NEW WRITER ARRIVED, CURRENT NUMBER OF WRITERS = %d\n",reader,writer);
	while(!((reader==0)&&(active_writer==0)))
	{
		kmutex_unlock(&tala);
		sched_sleep_on(&wq);
		kmutex_lock(&tala);
	}
	active_writer++;
	dbg(DBG_INIT," (BEFORE WRITE) THE VALUE OF ACTIVE WRITER = %d AND TOTAL NO OF WRITERS = %d \n",active_writer,writer);
	kmutex_unlock(&tala);
	sched_make_runnable(curthr);
	sched_switch();
	kmutex_lock(&tala);
	active_writer--;
	writer--;
	dbg(DBG_INIT," (AFTER WRITE) THE VALUE OF ACTIVE WRITER = %d AND TOTAL NO OF WRITERS = %d \n",active_writer,writer);
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
	dbg(DBG_INIT," MUTEX m1 LOCKED BY A THREAD WITH PROCESS PID = %d\n",(curthr->kt_proc)->p_pid);
	sched_make_runnable(curthr);
	sched_switch();
	kmutex_lock(&m2);
	kmutex_unlock(&m2);
	kmutex_unlock(&m1);
	return NULL;
}

void *dead2(int arg1,void *arg2) 
{   
	kmutex_lock(&m2);
	dbg(DBG_INIT," MUTEX m2 LOCKED BY A THREAD WITH PROCESS PID = %d\n",(curthr->kt_proc)->p_pid);
	sched_make_runnable(curthr);
	sched_switch();
	dbg(DBG_INIT," DEADLOCK CREATED BY A THREAD WITH PROCESS PID = %d\n",(curthr->kt_proc)->p_pid);
	kmutex_lock(&m1);
	kmutex_unlock(&m1);
	kmutex_unlock(&m2); 
	return NULL;
	
}
void dead_own()
{


       	kmutex_init(&self);
        proc_t *BLOCK = proc_create("SELF_BLOCK");
        KASSERT(BLOCK != NULL);
        kthread_t *BLOCK1 = kthread_create(BLOCK,deadlock_own,0,NULL);
        KASSERT(BLOCK1 != NULL);
        
        sched_make_runnable(BLOCK1);
     
        int status;
        while(!list_empty(&curproc->p_children))
        {
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }

}
void *deadlock_own(int arg1, void *arg2)
{
dbg(DBG_INIT," MUTEX LOCKED BY A THREAD WITH PROCESS PID = %d\n",(curthr->kt_proc)->p_pid);
kmutex_lock(&self);
dbg(DBG_INIT," THREAD WITH PROCESS PID = %d TRYING TO LOCK THE MUTEX AGAIN \n",(curthr->kt_proc)->p_pid);
kmutex_lock(&self);
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

