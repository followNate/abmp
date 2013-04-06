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
	
	if(file_descriptor == -EMFILE){
		dbg(DBG_ERROR | DBG_VFS,"The current process pid= %d exceeds the maximum permissible number of files.",curproc->p_pid);
		return -EMFILE;
	}
	
        file_t *fresh_file = fget(file_descriptor);
	if(fresh_file == NULL){
		dbg(DBG_ERROR | DBG_VFS,"Unable to get the file-%s as kernel memory is insufficient.",filename);
		return -ENOMEM;
	}

	if(strlen(filename)>34){
		dbg(DBG_ERROR | DBG_VFS, "The file name= %s is too long",filename);
		return -ENAMETOOLONG;
	}
	
	
        switch(oflags)
        {
		case O_RDONLY:
		case O_RDONLY|O_CREAT:
		case O_RDONLY|O_TRUNC:	fresh_file->f_mode = FMODE_READ;
					break;   		
		case O_WRONLY:
		case O_WRONLY|O_TRUNC:
		case O_WRONLY|O_CREAT:	fresh_file->f_mode = FMODE_WRITE;
					break;
		case O_RDWR:
		case O_RDWR|O_TRUNC:
		case O_RDWR|O_CREAT:	fresh_file->f_mode = FMODE_WRITE|FMODE_READ;
					break;
		case O_RDONLY|O_APPEND:
		case O_WRONLY|O_APPEND:
		case O_RDWR|O_APPEND:	fresh_file->f_mode = FMODE_APPEND;
					break;
		default:		dbg(DBG_ERROR | DBG_VFS,"Not a valid flag for file=%s, in process pid=%d",filename,curproc->p_pid); 
					return -EINVAL;
	}

	vnode_t *base = NULL;
	vnode_t *res_vnode; 
	int doExist = open_namev(filename,oflags, &res_vnode,base);
	if(doExist!=0){
		dbg(DBG_ERROR | DBG_VFS,"The file with name= %s, doesn't exist.",filename);
		return -ENOENT;
	}

	if(S_ISDIR(res_vnode->vn_mode) && ((oflags & 3)==O_WRONLY || (oflags & 3)==O_RDWR)){
		dbg(DBG_ERROR | DBG_VFS,"The given filename= %s is a directory. No writing operations are allowed on directory.",filename);
		return -EISDIR;
	}


	if(res_vnode->vn_devid!=0 && (bytedev_lookup(res_vnode->vn_devid)==NULL) && (blockdev_lookup(res_vnode->vn_devid)==NULL)){
		dbg(DBG_ERROR | DBG_VFS,"The device (id=%d) associated with filename=%s is unavailable.",res_vnode->vn_devid,filename);
		return -ENXIO;
	}	
	
        fresh_file->f_vnode=res_vnode;
		
        /*NOT_YET_IMPLEMENTED("VFS: do_open");*/
	curproc->p_files[file_descriptor] = fresh_file;

        return file_descriptor;
}
