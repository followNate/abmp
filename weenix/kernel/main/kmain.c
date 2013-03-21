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
#define TEST_6 6        /*  Producer  Consumer test */
#define TEST_7 7        /*  Deadlock check */
#define TEST_8 8        /*  Reader and writer problem */
#define TEST_9 9        /*  kshell testing */

static int curtest = TEST_3;

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
void *produce1(int arg1,void *arg2);
void *consume1(int arg1,void *arg2);
void *dead1(int arg1,void *arg2);
void *dead2(int arg1,void *arg2);
void *prockill(int arg1, void *arg2);
void shellTest();
void processSetUp();
kmutex_t m1;
kmutex_t m2;
void test_cancel();
void *kshell_test(int a, void *b)
{
    kshell_t *new_shell;
    int       i;
    while (1)
    {
        new_shell = kshell_create(0);
        i = kshell_execute_next(new_shell);
        if(i>0)
        {        dbg(DBG_TERM,"Error Executing the command\n");
        }
        
        kshell_destroy(new_shell);
        if(i==0)
        {       break;
        }
    }
    return NULL;
}

kmutex_t lock1;
ktqueue_t prod1, cons1;
int MAX = 10, buffer = 0;

static void *initproc_run(int arg1, void *arg2)
{
	switch(curtest){
		case 1:	
		case 2:
		case 3:
		case 4:
		case 5:
		case 6: processSetUp();
		        break;
		case 7: processSetUp();
			break;
		case 8:
			break;
		case 9:	shellTest();
			break;
	}
        
        
	int status;
        while(!list_empty(&curproc->p_children))
        {
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
        
        return NULL;
}

void shellTest(){
	/* entering into kshell */
        if(curtest == 9)
        {
        proc_t* new_shell = proc_create("shell1");
        kthread_t *new_shell_thread = kthread_create(new_shell, kshell_test, NULL, NULL);
        sched_make_runnable(new_shell_thread);
        }
}

void processSetUp(){
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

void *init_child1(int arg1,void *arg2) /* proc3 */
{

        kmutex_init(&lock1);
        sched_queue_init(&prod1);
        sched_queue_init(&cons1);

   if(curtest == 6 || curtest == 1 || curtest == 2 || curtest==3)
     {
        proc_t *producer1 = proc_create("producer1");
        KASSERT(producer1 != NULL);
        kthread_t *thread3 = kthread_create(producer1,produce1,0,NULL);
        KASSERT(thread3 != NULL);
        
        proc_t *consumer1 = proc_create("consumer1");
        KASSERT(consumer1 != NULL);
        kthread_t *thread4 = kthread_create(consumer1,consume1,0,NULL);
        KASSERT(thread4 != NULL);
         
	sched_make_runnable(thread3);
        sched_make_runnable(thread4);
        
     }  
        int status;
        while(!list_empty(&curproc->p_children))
        {
                        test_cancel();
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
       	proc_kill_all();
return NULL;
}


void *init_child2(int arg1,void *arg2)  /* proc4 */
{

     if(curtest ==7 || curtest == 1 )
     {
        kmutex_init(&m1);
	kmutex_init(&m2);
        proc_t *DEADLOCK1 = proc_create("DEADLOCK1");
        KASSERT(DEADLOCK1 != NULL);
        kthread_t *thread5 = kthread_create(DEADLOCK1,dead1,0,NULL);
        KASSERT(thread5 != NULL);
        
        proc_t *DEADLOCK2 = proc_create("DEADLOCK2");
        KASSERT(DEADLOCK2 != NULL);
        kthread_t *thread6 = kthread_create(DEADLOCK2,dead2,0,NULL);
	KASSERT(thread6 != NULL);
       
	sched_make_runnable(thread5);
        sched_make_runnable(thread6);
     }
      
	    int status;
        
        while(!list_empty(&curproc->p_children))
        {        test_cancel();
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
        /*NOT_YET_IMPLEMENTED("PROCS: initproc_run");
        */
        return NULL;
}

void *produce1(int arg1,void *arg2) /* proc5 */
{

  if(curtest == 6)
     {
	int i=0;
	for (i=1;i<=MAX;i++)
	{
		kmutex_lock(&lock1);
		while(buffer != 0)
		{
			kmutex_unlock(&lock1);
			sched_sleep_on(&prod1);
                         test_cancel();
			kmutex_lock(&lock1);
		}
		buffer++;
		dbg_print("\n PRODUCING %d \n",buffer);
		sched_wakeup_on(&cons1);
		kmutex_unlock(&lock1);
	}
     }
     else if(curtest ==1 || curtest ==2 || curtest == 3)
        {             
             proc_t *kill_test= proc_create("killtest");
             KASSERT(kill_test != NULL);
             kthread_t *kill_thr = kthread_create(kill_test,prockill,0,NULL);
             KASSERT(kill_thr != NULL);
             sched_make_runnable(kill_thr);
        }
	int status;
        while(!list_empty(&curproc->p_children))
        {
                test_cancel();
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }

	return NULL;
}

void *consume1(int arg1,void *arg2) /* proc6 */
{     
          test_cancel();
  if(curtest == 6)
     {
	int i=0;
	for (i=1;i<=MAX;i++)
	{
		kmutex_lock(&lock1);
		while(buffer == 0)
		{
			kmutex_unlock(&lock1);
			sched_sleep_on(&cons1);
			kmutex_lock(&lock1);
		}
		buffer--;
		dbg_print("\n CONSUMING %d \n",buffer);
		
		sched_wakeup_on(&prod1);
		kmutex_unlock(&lock1);	
	}	
	sched_make_runnable(curthr);
	sched_switch();
     }	
	int status;
        while(!list_empty(&curproc->p_children))
        {
                test_cancel();
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
        
	return NULL;
}

void *dead1(int arg1,void *arg2) /* proc3 / proc7 */
{

   if(curtest == 7)
     {
	kmutex_lock(&m1);
	dbg_print("\n MUTEX1 LOCKED BY DEAD1 \n");
	sched_make_runnable(curthr);
	sched_switch();
	kmutex_lock(&m2);
	dbg_print("\n DEADLOCK DONE BY DEAD 1 \n");
	dbg_print("\n MUTEX2 LOCKED BY DEAD1 \n");
	kmutex_unlock(&m2);
	dbg_print("\n MUTEX2 LOCKED BY DEAD1 \n");
	kmutex_unlock(&m1);
	dbg_print("\n MUTEX1 LOCKED BY DEAD1 \n");
     }
	int status;
        while(!list_empty(&curproc->p_children))
        {
                test_cancel();
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
        
	return NULL;
}

void *dead2(int arg1,void *arg2) /* proc4 / proc8 */
{

   if(curtest == 7)
     {
	kmutex_lock(&m2);
	dbg_print("\n MUTEX2 LOCKED BY DEAD2 \n");
	sched_make_runnable(curthr);
	sched_switch();
	dbg_print("\n DEADLOCK DONE BY DEAD 2 \n");
	kmutex_lock(&m1);
	dbg_print("\n MUTEX1 LOCKED BY DEAD2 \n");
	kmutex_unlock(&m1);
	dbg_print("\n MUTEX1 LOCKED BY DEAD2 \n");
	kmutex_unlock(&m2);
	dbg_print("\n MUTEX2 LOCKED BY DEAD2 \n");
     }
	int status;
        while(!list_empty(&curproc->p_children))
        {
                 test_cancel();
                pid_t child = do_waitpid(-1, 0, &status);
                dbg(DBG_INIT,"Process %d cleaned successfully\n", child);
        }
        
	return NULL;
	
}

void *prockill(int arg1, void *arg2)
{

if(curtest == 2)
 {
   proc_t *proc = proc_lookup(3);
   proc_kill(proc,0);
 } 
else if(curtest == 3)
  {
       
        proc_kill_all();
  }

return NULL;
}

void test_cancel()
{

	if(curthr->kt_cancelled == 1)
	        {
        		dbg(DBG_TEST, "cancellation point reached for process %d's thread\n", curproc->p_pid);	        
        
	        	kthread_exit(0);	        
	        }
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

