                                                                    
                                                                     
                                             
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
The Virtual Memory program is having a bug in handle_pagefault function.


DEVIATIONS
----------
We are not able to enter in userland. S5F5 is working


REFERNCES & CREDITS
-------------------
None.



TESTING GUIDELINES
------------------					
We implemented 39 out of 40 functions  of the Virtual Memory kernel assignment. This includes
VM Areas, Anonymous Objects, Shadow Objects, Memory Management Unit, Traps and User Memory and 
Process Memory Management. We tried our best to make the VM run. The program is partially able to handle page fault.
It is able to handle page fault in the first go but later since invalid virtual address (vaddr)
is passed, our kernel get panic. We havent implemeted the do_brk() function. 
We are able to compile the entire code without any warning and error.  




