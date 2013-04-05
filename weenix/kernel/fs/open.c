/*
 *  FILE: open.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Mon Apr  6 19:27:49 1998
 */

#include "globals.h"
#include "errno.h"
#include "fs/fcntl.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/stat.h"
#include "util/debug.h"

/* find empty index in p->p_files[] */
int
get_empty_fd(proc_t *p)
{
        int fd;

        for (fd = 0; fd < NFILES; fd++) {
                if (!p->p_files[fd])
                        return fd;
        }

        dbg(DBG_ERROR | DBG_VFS, "ERROR: get_empty_fd: out of file descriptors "
            "for pid %d\n", curproc->p_pid);
        return -EMFILE;
}

/*
 * There a number of steps to opening a file:
 *      1. Get the next empty file descriptor.
 *      2. Call fget to get a fresh file_t.
 *      3. Save the file_t in curproc's file descriptor table.
 *      4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on
 *         oflags, which can be O_RDONLY, O_WRONLY or O_RDWR, possibly OR'd with
 *         O_APPEND.
 *      5. Use open_namev() to get the vnode for the file_t.
 *      6. Fill in the fields of the file_t.
 *      7. Return new fd.
 *
 * If anything goes wrong at any point (specifically if the call to open_namev
 * fails), be sure to remove the fd from curproc, fput the file_t and return an
 * error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        oflags is not valid.
 *      o EMFILE
 *        The process already has the maximum number of files open.
 *      o ENOMEM
 *        Insufficient kernel memory was available.
 *      o ENAMETOOLONG
 *        A component of filename was too long.
 *      o ENOENT
 *        O_CREAT is not set and the named file does not exist.  Or, a
 *        directory component in pathname does not exist.
 *      o EISDIR
 *        pathname refers to a directory and the access requested involved
 *        writing (that is, O_WRONLY or O_RDWR is set).
 *      o ENXIO
 *        pathname refers to a device special file and no corresponding device
 *        exists.
 */

int
do_open(const char *filename, int oflags)
{
        int file_descriptor = get_empty_fd(curproc);
        file_t *fresh_file = fget(file_descriptor);
        curproc->p_files[file_descriptor] = fresh_file;
/*      The oflags is OR'ed one
		There are 12 cases
		Read - 0
		Write - 1
		ReadWrite - 2
		Read OR Create - 256
		Read OR Trun - 512
		Read OR Append - 1024
		Write OR Create - 257
		Write OR Trun - 513
		Write OR Append - 1025
		Readwrite OR Create - 258
		ReadWrite OR Trun - 514
		ReadWrite OR Append - 1026
*/
		vnode_t *base = NULL;
		vnode_t *res_vnode;
        switch(oflags)
        {
			case 0 : {
						fresh_file->f_mode = FMODE_READ;
						open_namev(filename,0, &res_vnode,base);
						fresh_file->f_vnode=res_vnode;
						
						fref(fresh_file);
						break;
					 }
			case 1 : {
						fresh_file->f_mode = FMODE_WRITE;
						open_namev(filename,1, &res_vnode,base);
						fresh_file->f_vnode=res_vnode;
						fref(fresh_file);
						break;
					 }
			case 2 : {
						fresh_file->f_mode = (FMODE_READ || FMODE_WRITE);
						open_namev(filename,2, &res_vnode,base);
						fresh_file->f_vnode=res_vnode;
						fref(fresh_file);
						break;
					 }
			case 256 : {
							fresh_file->f_mode = FMODE_READ;
							open_namev(filename,256, &res_vnode,base);
							fresh_file->f_vnode=res_vnode;
							fref(fresh_file);
							break;
					   }
			case 512 : {
							fresh_file->f_mode = FMODE_READ;
							open_namev(filename,512, &res_vnode,base);
							fresh_file->f_vnode=res_vnode;
							fref(fresh_file);
							break;
					   }
			
			case 1024 : {
							fresh_file->f_mode = (FMODE_READ || FMODE_APPEND);
							open_namev(filename,1024, &res_vnode,base);
							fresh_file->f_vnode=res_vnode;
							fref(fresh_file);
							break;
						}
			case 257 : {
							fresh_file->f_mode = FMODE_WRITE;
							open_namev(filename,257, &res_vnode,base);
							fresh_file->f_vnode=res_vnode;
							fref(fresh_file);
							break;
					   }
			case 513 : {
							fresh_file->f_mode = FMODE_WRITE;
							open_namev(filename,513, &res_vnode,base);
							fresh_file->f_vnode=res_vnode;
							fref(fresh_file);
							break;
					   }
			case 1025 : {
							fresh_file->f_mode = (FMODE_WRITE || FMODE_APPEND);
							open_namev(filename,1025, &res_vnode,base);
							fresh_file->f_vnode=res_vnode;
							fref(fresh_file);
							break;
						}
			case 258 : {
							fresh_file->f_mode = (FMODE_READ || FMODE_WRITE);
							open_namev(filename,258, &res_vnode,base);
							fresh_file->f_vnode=res_vnode;
							fref(fresh_file);
							break;
						}
			case 514 : {
							fresh_file->f_mode = (FMODE_READ || FMODE_WRITE);
							open_namev(filename,514, &res_vnode,base);
							fresh_file->f_vnode=res_vnode;
							fref(fresh_file);
							break;
						}
			case 1026 : {	fresh_file->f_mode = (FMODE_READ || FMODE_WRITE || FMODE_APPEND);
							open_namev(filename,1026, &res_vnode,base);
							fresh_file->f_vnode=res_vnode;
							fref(fresh_file);
							break;
						}
			default : return EINVAL;
		}
		
        NOT_YET_IMPLEMENTED("VFS: do_open");
        return file_descriptor;
}
