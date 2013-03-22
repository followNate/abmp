
GROUP MEMBERS INFORMATION
-------------------------
The team consist of following members:
Aditya Deore, Bikramjit Singh, Mohit Aggarwal and Pradipta Ghosh


GROUP GRADING
-------------
The project was divided equally into 4 logical modules and each member was assigned ownership of a module. Each team member was responsible for implementing the missing functionality as well as coming up with test cases for their modules.
Pradipta Ghosh:		25%
Mohit Aggarwal:		25%
Bikramjit Singh:	25%
Aditya Deore:		25%
			----	
			100%


DESIGN
-------
Following functions have been implemented in Kernel Assignment#1.
proc_create()
proc_cleanup()
proc_kill()   
proc_kill_all()
proc_thread_exited()    
do_waitpid()    
do_exit()    
kthread_create()    
kthread_cancel()    
kthread_exit()    
sched_sleep_on()    
sched_cancellable_sleep_on()    
sched_wakeup_on()    
sched_broadcast_on()    
sched_cancel()    
sched_switch()    
sched_make_runnable()    
kmutex_init()    
kmutex_lock()    
kmutex_lock_cancellable()    
kmutex_unlock()    
bootstrap()

KASSERT Statements are used at appropriate locations as mentioned in the requirements document. dbg() statements are added in the code to generate console logs for debugging and testing purpose. These logs can also be used for validating the test cases.


Submiting files:
- kthread.c
- proc.c
- kmain.c
- config.mk
- kmutex.c
- sched.c
- procs-README.txt



KNOWN BUGS
----------
None.


DEVIATIONS
----------
None.


REFERNCES & CREDITS
-------------------
None.



TESTING GUIDELINES
------------------					
There are 11 test cases written for Kernel #1 assignment. These can be executed by providing the test case number thorugh the variable 'curtest' in the 'kmain.c' file.
The grader has to build the project everytime he/she provides a test case number. A grader can validate the test case by viewing the logs generated on the console.

The description of test cases as follows.
Test Case 0:	PREMATURE EXIT
Description: The test case exits immideately from the init_proc().

Test Case 1:	NORMAL EXECUTION
Description: We are creating a tree hierarchy of 10 processes with tree depth 4 and let them terminate normally. The purpose of this test case is to ensure that processes are terminated cleanly in normal execution flow.

Test Case 2:	PROC_KILL TEST
Description: We are killing a specific process (pid=3) inside the tree hierarchy. The kernel should exit gracefully upon execution of the test case. The child processes of the deleted/killed process are re-parented under INIT process, if any.

Test Case 3:	PROC_KILL_ALL TEST
Description: We are testing proc_kill_all functionality. Here we are calling process_kill_all() from the leaf node in the hierarchy, which will kill all the processes under INIT Process cleanly and exit from the kernel gracefully.

Test Case 4:	KTHREAD_ CANCEL TEST
Description: We are calling kthread_cancel() on a specific process thread in the tree heirachy and the target thread will be scheduled to be canceled. The kernel should exit gracefully upon execution of the test case.

Test Case 5:	KTHREAD_EXIT TEST
Description: We are calling kthread_exit() on currently running thread. The kernel should exit gracefully upon execution of the test case.

Test Case 6:	PRODUCER-CONSUMER TEST
Description: We have defined two processes 'producer' and 'consumer'. The 'producer' process has 'produce_thread'. The 'consumer' process has 'consume_thread'.The resource 'buffer' is commonly shared by both threads. There are two condition variable queue's 'cons' and 'prod'. The 'produce_thread' fill the item's in the buffer, if the buffer is full it sleeps in the queue 'prod'. The 'consume_thread' removes the item from the buffer,if the buffer is empty it sleeps in the queue 'cons'.


Test Case 7:	DEADLOCK TEST
Description: We have defined two processes 'DEADLOCK1' and 'DEADLOCK2'. The 'DEADLOCK1' process has 'dead1'. The 'DEADLOCK2' process has 'dead2'. The 'dead1' thread locks the mutex 'm1' and try to lock the mutex 'm2'. The 'dead2' thread locks the mutex 'm2' and try to lock the mutex 'm1'. As a result, both the threads are in the wait queue and deadlock is done and console will be hung.

Test Case 8:	READER_WRITER TEST
Description: This test case is to validate the readers writers scenario. Here we are creating readers and writers, there number is controlled by MAX_WRITER and MAX_READER macros defined in the kmain.c file. Readers and Writers are created alternatively. From the console logs the grader can validate the number of active readers and writers as well as there number waiting in the writers queue. We are preferring writers over readers.

Test Case 9:	KSHELL TEST
Description: The kshell is implemented. User will be provided with the Kshell prompt where he/she can enter following available commands: 'help' , 'echo' , 'exit'. Here we are creating a new shell everytiime user enters a line. The kshell will execute that line and is destroyed. The whole operation is runing in a while loop which give end user an impression of persistent kshell window. The kshell window can be exited by entering the 'exit' command.

Test Case 10:	DEADLOCK ON SELF TEST
Description: In this test we are testing the deadloack condition on the thread itself. Here a thread who owns a mutex lock try to lock it again. This will cause an KASSERT error and the kernel will be halted without cleanly.


