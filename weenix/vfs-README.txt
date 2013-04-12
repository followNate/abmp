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
lookup()
dir_namev()
open_namev()
do_open()
do_read()
do_write()
do_close()
do_dup()
do_dup2()
do_mknod()
do_mkdir()
do_rmdir()
do_unlink()
do_link()
do_rename()
do_chdir()
do_getdent()
do_lseek()
do_stat()
special_file_read()
special_file_write()


KASSERT Statements are used at appropriate locations as mentioned in the requirements document. dbg() statements are added in the code to generate console logs for debugging and testing purpose. These logs can also be used for validating the test cases.


Submiting files:
----------------
- kthread.c
- proc.c
- kmain.c
- kmutex.c
- sched.c
- Config.mk
- vfs-README.txt
- namev.c
- open.c
- vfs_syscall.c
- vnode.c



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
There are 3 test procedures written for Kernel #2 assignment. These can be executed by providing the test case number thorugh the variable 'curtest' in the 'kmain.c' file.
The grader has to build the project everytime he/she provides a test case number. The grader can validate the test case by viewing the logs generated on the console.

The description of test cases as follows.
Test Case 9:	KSHELL TEST
Description: This test case launches the kshell terminal. The Kshell terminal help command will list out all the commands applicable for VFS. The grader can use these commands to validate underlying VFS implementation.

Test Case 11:	EXECUTING vfstest.c
Description: This test case exectues the test cases defined in vfstest.c file. The grader can view final report in the logs.

Test Case 12:	CUSTOM TEST CASES
Description: Here we are validating following system calls to file system.
do_mkdir, do_rmdir, do_chdir, do_link, do_unlink, do_lseek, do_open, do_read, do_write, do_close, do_dup etc. The grader can validate the result from the logs.

Here we have created a main directory "dir" and created four directories "dir1", "dir2", "dir3", "dir4" in this main directory.In directory "dir1" we have created a file
"file1.txt" and in directory "dir2" we have created a file "file2.txt". We linked the directory "dir1" into a new directory "linkdir1" and "the file1.txt" present in directory
"dir1" into a new directory "linkdir2". We rename the directory "dir3" to "renameddir3" and "file1.txt" in directory "dir1" to "renamedfile1.txt". We removed the directory
"dir4". We open the file "file2.txt" in directory "dir2" for reading 18 characters and display on the terminal. We used do_lseek() function and set offset to -19 and whence 
to SEEK_END and display on the terminal. Lastly we duplicate the file "file2.txt" present in directory "dir2" and display the new file descriptor.



