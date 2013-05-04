                                                                    
                                                                     
                                             
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
We have implemented 39 out of 40 functions in this Kernel Assignment #3.
KASSERT Statements are used at appropriate locations as mentioned in the requirements document. dbg() statements are added in the code to generate console logs for debugging and testing purpose.
For grader's convinience KASSERT statemts are also printed on the console logs and grader can validate them by searching for keyword "GRADING:"
These logs can also be used for validating the test cases.

Submiting files:
----------------
- kmain.c
- kmutex.c
- kthread.c
- proc.c
- sched.c
- namev.c
- open.c
- vfs_syscall.c
- vnode.c
- access.c
- syscall.c
- fork.c
- kthread.c
- anon.c
- brk.c
- mmap.c
- pagefault.c
- shadow.c
- vmmap.c
- Config.mk
- vfs-README.txt


KNOWN BUGS
----------
The Virtual Memory program is having a bug in handle_pagefault function, as a result the flow to control enters the 'userland' and enter into page fault handler. It tries to resolve first few addresses in userland but then sufffers an error condition, which prompts termination of current running processd and initiates weenix shutdown. At the end of shut down the kernel doesn't halt cleanly as there are ref count is not zero for the vnode representing the disk.


DEVIATIONS
----------
The flow of control is able to reach the userland (This can be validated by viewing console logs) but suffers error in the page fault handler.


REFERNCES & CREDITS
-------------------
None.



TESTING GUIDELINES
------------------					
The grader can test following test cases for this assignment. To change the test case number, the grader has to change the 'curtest' varaible in the kmain.c and recompile weenix binaries. The case description as follows.

curtest==TEST_9: 
	This test case launches the Kshell and validates that underlying s5FS and VM functionalities are running fine. The grader can see new directories like /sbin, /usr/bin etc by executing 'ls' command on kshell.

curtest==TEST_13:
	This test case try executing /usr/bin/segfault executable to validate the underlying VM functionality.




