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
Description: This test case launches the kshell terminal. The Kshell terminal help command will list out all the commands applicable for VFS. The grader can use these commands to validate underlying VFS implementation. For Example, 
- mkdir test: will create a directory with name 'test' under the traversed path.
- rmdir test: will remove 'test' directory provided it is empty. If it contain directories or files then an error message will be displayed.
- stat FILE: will display status of FILE.
- link <src> <dst>: will call link funtion on src and dst
- cd PATH: changes the working directory
- echo CONTENT>FILE: will create a file and writes the CONTENT in the FILE.
- cat FILE: will concatenate the content of FILE and print on standard output.

Test Case 11:	EXECUTING vfstest.c
Description: This test case exectues the test cases defined in vfstest.c file. The grader can view final report in the logs.

Test Case 12:	CUSTOM TEST CASES
Description: Here we are validating following system calls to file system. At the end of execution we launch the kshell.
do_mkdir, do_rmdir, do_chdir, do_link, do_unlink, do_lseek, do_open (in different modes e.g, RDWR, RDONLY,WRONLY), do_read, do_write, do_close, do_dup etc. The grader can validate the result from the logs as well as in the kshell.

-we have created a main directory "dir" and created four directories "dir1", "dir2", "dir3", "dir4" in this main directory.In directory "dir1" 
-we have created a file "file1.txt" in O_CREAT|O_WRITE mode and further we are performing operations on this file which are very clear in the logs 
-in directory "dir2" we have created a file "file2.txt" in O_RDONLY|O_WRITE mode and further operations are performed on the file.
-Operations like APPEND|WRONLY , APPEND|RDONLY are performed on file2.txt
-link() system call has been used to perform the linking of the directiories and files.
We linked the directory "dir/dir2" into a new directory "/dir/linkofdir2" and "the dir/dir1/file1.txt" present in directory "dir1" into a new file "/dir/linkoffile1". 
-Unlinking is being done in rename test case therefore we did not test it explicitly.
-We renamed the directory "dir3" to "renameddir3" and "file1.txt" in directory "/dir/dir1" to "/dir/renamedfile1.txt". 
-We removed the directory "dir4". 
-We opened the file "file2.txt" in directory "dir2" for reading 18 characters and displayed on the terminal. (Please check the terminal log mesage for the same)

-We used do_lseek() function and set offset to -19 and whence to SEEK_END and displayed on the terminal. 
-Lastly we validated do_dup and do_dup2 funtions on "file2.txt an displayed the new file descriptors.



NOTE: We are not removing the created files and directories for the TESTING purpose. ! 


Please reach us ASAP if you find any doubts.

THANKS..



