/*
 *  FILE: vfs_syscall.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Wed Apr  8 02:46:19 1998
 *  $Id: vfs_syscall.c,v 1.1 2012/10/10 20:06:46 william Exp $
 */

#include "kernel.h"
#include "errno.h"
#include "globals.h"
#include "fs/vfs.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/fcntl.h"
#include "fs/lseek.h"
#include "mm/kmalloc.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/stat.h"
#include "util/debug.h"

/* To read a file:
 *      o fget(fd)
 *      o call its virtual read f_op
 *      o update f_pos
 *      o fput() it
 *      o return the number of bytes read, or an error
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for reading.
 *      o EISDIR
 *        fd refers to a directory.
 *
 * In all cases, be sure you do not leak file refcounts by returning before
 * you fput() a file that you fget()'ed.
 */
int
do_read(int fd, void *buf, size_t nbytes)
{
        KASSERT(curproc!=NULL);
        
        if(fd<0||fd>=NFILES||(curproc->p_files[fd]==NULL))
        {
                dbg(DBG_ERROR | DBG_VFS,"ERROR: do_read: Not a valid file descriptor\n");
		return -EBADF;       
        }
        
        file_t *open_file=fget(fd);
        
        KASSERT(open_file!=NULL);
       
        if(!((open_file->f_mode)  & FMODE_READ))
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_read: File is not meant for reading\n");
                 fput(open_file);
                 return -EBADF;       
        }
               
        if(S_ISDIR(open_file->f_vnode->vn_mode))
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_read: File descriptor points to a Directory\n");
                fput(open_file);
                return -EISDIR;
        }
        KASSERT(open_file->f_vnode->vn_ops->read);
        int i=(open_file->f_vnode->vn_ops->read)(open_file->f_vnode,open_file->f_pos,buf,nbytes);
        if(i<0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_read: Error while the reading the file\n");
                fput(open_file);
                return i;
        }
        
        open_file->f_pos=open_file->f_pos+i;
        fput(open_file);
	dbg(DBG_VFS,"INFO: Successfully opens the file with fd=%d\n",fd);
	 /* NOT_YET_IMPLEMENTED("VFS: do_read");*/
        return i;
}

/* Very similar to do_read.  Check f_mode to be sure the file is writable.  If
 * f_mode & FMODE_APPEND, do_lseek() to the end of the file, call the write
 * f_op, and fput the file.  As always, be mindful of refcount leaks.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for writing.
 */
 
 
int
do_write(int fd, const void *buf, size_t nbytes)
{
        KASSERT(curproc!=NULL);
        
        if(fd<0||fd>=NFILES||(curproc->p_files[fd]==NULL))
        {
		dbg(DBG_ERROR | DBG_VFS, "ERROR: do_write: Invalid file descriptor\n");
                return -EBADF;       
        }
        
        file_t *open_file=fget(fd);
       
        KASSERT(open_file!=NULL);

        if(!((open_file->f_mode) & FMODE_WRITE))
        {
                dbg(DBG_ERROR | DBG_VFS,"ERROR: do_write: Invalid file descriptor\n"); 
		fput(open_file);
                return -EBADF;       
        }
        
        if((open_file->f_mode) & FMODE_APPEND)
        { 
                int j=do_lseek(fd,NULL,SEEK_END);
                if(j<0)
                {
                        dbg(DBG_ERROR | DBG_VFS,"ERROR:do_write:  Unable to write file in APPEND mode\n"); 
			fput(open_file);
                        return j;
                }
        }
        KASSERT(open_file->f_vnode->vn_ops->write);
        int i=(open_file->f_vnode->vn_ops->write)(open_file->f_vnode,open_file->f_pos,buf,nbytes);
        if(i<0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_write: Unable to write to file\n");
                 fput(open_file);
                 return i;
        }else{
		KASSERT((S_ISCHR(open_file->f_vnode->vn_mode)) ||
                                (S_ISBLK(open_file->f_vnode->vn_mode)) ||
                                ((S_ISREG(open_file->f_vnode->vn_mode)) && (open_file->f_pos <= open_file->f_vnode->vn_len)));
	}
        
        open_file->f_pos=open_file->f_pos+i;
        fput(open_file);
	dbg(DBG_VFS,"INFO: INFO: Successfully performed write operation on the file with fd=%d\n",fd);
        /*NOT_YET_IMPLEMENTED("VFS: do_write");*/
        return i;
}

/*
 * Zero curproc->p_files[fd], and fput() the file. Return 0 on success
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't a valid open file descriptor.
 */
int
do_close(int fd)
{
        KASSERT(curproc!=NULL);
        
        if(fd<0||fd>=NFILES||(curproc->p_files[fd]==NULL))
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_close: Invalid file descriptor\n");
                return -EBADF;       
        }
       
        file_t *open_file=fget(fd);
       
        KASSERT(open_file!=NULL);
        
        fput(open_file);
        
        curproc->p_files[fd]=NULL;
        
        fput(open_file);
         
         /*NOT_YET_IMPLEMENTED("VFS: do_close");*/
	dbg(DBG_VFS,"INFO: Successfully closes the file with fd=%d\n",fd);
        return 0;
}

/* To dup a file:
 *      o fget(fd) to up fd's refcount
 *      o get_empty_fd()
 *      o point the new fd to the same file_t* as the given fd
 *      o return the new file descriptor
 *
 * Don't fput() the fd unless something goes wrong.  Since we are creating
 * another reference to the file_t*, we want to up the refcount.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't an open file descriptor.
 *      o EMFILE
 *        The process already has the maximum number of file descriptors open
 *        and tried to open a new one.
 */
int
do_dup(int fd)
{
        KASSERT(curproc!=NULL);
        
        if(fd<0||fd>=NFILES||(curproc->p_files[fd]==NULL))
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_dup: Invalid file descriptor\n");
                return -EBADF;       
        }
              
        file_t *open_file=fget(fd);
        KASSERT(open_file!=NULL);
        
        int dup_fd=get_empty_fd(curproc);
        if(!dup_fd)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_dup: Invalid duplicate file descriptor\n");
		fput(open_file);
                return -EMFILE; 
        }
        
        curproc->p_files[dup_fd]=open_file;
        
        dbg(DBG_VFS,"INFO: Successfully performed dup operation on file with fd=%d\n",fd);
        /*NOT_YET_IMPLEMENTED("VFS: do_dup");*/
        return dup_fd; 
}

/* Same as do_dup, but insted of using get_empty_fd() to get the new fd,
 * they give it to us in 'nfd'.  If nfd is in use (and not the same as ofd)
 * do_close() it first.  Then return the new file descriptor.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        ofd isn't an open file descriptor, or nfd is out of the allowed
 *        range for file descriptors.
 */
int
do_dup2(int ofd, int nfd)
{
        KASSERT(curproc!=NULL);
        
        if(ofd<0||ofd>=NFILES||(curproc->p_files[ofd]==NULL))
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_dup2: Invalid old file descriptor\n");
                return -EBADF;       
        }
        if(nfd<0||nfd>=NFILES)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_dup2: Invalid new file descriptor\n");
                return -EBADF;       
        }
        if(ofd!=nfd)   
        {
        if(curproc->p_files[nfd]!=NULL)
        {
		dbg(DBG_VFS, "INFO: do_dup2: New file descriptor is already in use.. calling do_close\n");
                int j=do_close(nfd);
                if (j <0)
                {
                        return j;
                }               
        }
                 
        file_t *open_file=fget(ofd);
        KASSERT(open_file!=NULL);
        
        curproc->p_files[nfd]=open_file;
        }
	dbg(DBG_VFS,"INFO: Successfully performed dup2 operation on the files with old fd=%d and new fd=%d\n",ofd,nfd);
        /*NOT_YET_IMPLEMENTED("VFS: do_dup2");*/
        return nfd;
}

/*
 * This routine creates a special file of the type specified by 'mode' at
 * the location specified by 'path'. 'mode' should be one of S_IFCHR or
 * S_IFBLK (you might note that mknod(2) normally allows one to create
 * regular files as well-- for simplicity this is not the case in Weenix).
 * 'devid', as you might expect, is the device identifier of the device
 * that the new special file should represent.
 *
 * You might use a combination of dir_namev, lookup, and the fs-specific
 * mknod (that is, the containing directory's 'mknod' vnode operation).
 * Return the result of the fs-specific mknod, or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        mode requested creation of something other than a device special
 *        file.
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mknod(const char *path, int mode, unsigned devid)
{
	if(strlen(path)>MAXPATHLEN){
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_mknod: File path is too long\n");
		return -ENAMETOOLONG;
	}

        if(mode!=S_IFCHR&&mode!=S_IFBLK)
        {	
		dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mknod: Invalid mode used for creating device special file\n");
                return -EINVAL;
        }
        size_t namelen=0;
        const char *name=NULL;
        vnode_t *res_vnode;
        vnode_t *result;
        int i=dir_namev(path, &namelen,&name,NULL,&res_vnode);
        if(i<0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_mknod: Unable to resolve the file path\n");
                return i;
        } 
        if(strlen(name)>NAME_LEN)
        {
		dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mkdir: A component of name was too long\n");
		vput(res_vnode);
		return -ENAMETOOLONG;
	}
        if(res_vnode==NULL)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_mknod: Directory component in path doesn't exist\n");
                return -ENOENT;
        }
        else 
        {
                if(!S_ISDIR(res_vnode->vn_mode))
                {
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_mknod: A component in the path is not a directory\n");
                        vput(res_vnode);
                        return -ENOTDIR;
                }
        }     
        
        if(name!=NULL)
        {
                int j=lookup(res_vnode,name,namelen,&result);
                         
                if(j==0)
                {
			dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mknod: Path already exists\n");
                        vput(res_vnode);
                        vput(result);
                        return -EEXIST;
                }
        }
        vput(res_vnode);
        KASSERT(NULL!=res_vnode->vn_ops->mknod);
        i=(res_vnode->vn_ops->mknod)(res_vnode,name,namelen,mode,devid);
        dbg(DBG_VFS,"INFO: Making Device node successful. Path=%s, mode=%d, devid=%u\n",path,mode,devid);
         /*  NOT_YET_IMPLEMENTED("VFS: do_mknod");*/
        return i;
}

/* Use dir_namev() to find the vnode of the dir we want to make the new
 * directory in.  Then use lookup() to make sure it doesn't already exist.
 * Finally call the dir's mkdir vn_ops. Return what it returns.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mkdir(const char *path)
{
	if(strlen(path)>MAXPATHLEN){
		dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mkdir: A component of path was too long\n");
		return -ENAMETOOLONG;
	}
	
        size_t namelen=0;
        const char *name=NULL;
        vnode_t *res_vnode;
        vnode_t *result;
        KASSERT(path != NULL);
        int i=dir_namev(path, &namelen,&name,NULL,&res_vnode);
        if(i<0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_mkdir: Unable to resolve a component in the path\n");
                vput(res_vnode);
		return i;
        }
        if(strlen(name)>NAME_LEN)
        {
		dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mkdir: A component of name was too long\n");
		vput(res_vnode);
		return -ENAMETOOLONG;
	}
	
	if(res_vnode==NULL)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_mkdir: A directory path component is missing\n");
                return -ENOENT;
        }
        else 
        {
                if(!S_ISDIR(res_vnode->vn_mode))
                {
			dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mkdir: A component is not the path not directory\n");
                        vput(res_vnode);
                        return -ENOTDIR;
                }
        }     
        
        if(name!=NULL)
        {
                int j=lookup(res_vnode,name,namelen,&result);
                         
                if(j==0)
                {
			dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mkdir: Path already exists\n");
                        vput(res_vnode);
                        vput(result);
                        return -EEXIST;
                }
        }
        vput(res_vnode);
        KASSERT(NULL!=res_vnode->vn_ops->mkdir);                
        i=(res_vnode->vn_ops->mkdir)(res_vnode,name,namelen);
        dbg(DBG_VFS,"INFO: The new directory is successfully made. Path=%s\n",path);
        /*NOT_YET_IMPLEMENTED("VFS: do_mkdir");*/
        return i;
       
 
}

/* Use dir_namev() to find the vnode of the directory containing the dir to be
 * removed. Then call the containing dir's rmdir v_op.  The rmdir v_op will
 * return an error if the dir to be removed does not exist or is not empty, so
 * you don't need to worry about that here. Return the value of the v_op,
 * or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        path has "." as its final component.
 *      o ENOTEMPTY
 *        path has ".." as its final component.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_rmdir(const char *path)
{
	if(strlen(path)>MAXPATHLEN){
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_rmdir: Path component is too long\n");
		return -ENAMETOOLONG;
	}
	
        size_t namelen=0;
        const char *name=NULL;
        vnode_t *res_vnode;
        vnode_t *result;
        KASSERT(path != NULL);
        int i=dir_namev(path, &namelen,&name,NULL,&res_vnode);
        if(i<0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_rmdir: Error removing directory\n");
                vput(res_vnode);
		return i;
        }
        if(strlen(name)>NAME_LEN)
        {
		dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mkdir: A component of name was too long\n");
		vput(res_vnode);
		return -ENAMETOOLONG;
	}
	if(res_vnode==NULL)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_rmdir: A directory component in the path doesn't exist\n");
                return -ENOENT;
        }
        else 
        {
                if(!S_ISDIR(res_vnode->vn_mode))
                {
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_rmdir: A component in path is not directory\n");
                        vput(res_vnode);
                        return -ENOTDIR;
                }
        }     
        if(strcmp(name,".")==0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_rmdir: Path has \'.\'as final component\n");
                vput(res_vnode);
                return -EINVAL;
        }
        if(strcmp(name,"..")==0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_rmdir: Path had \'..\' as final component\n");
                vput(res_vnode);
                return -ENOTEMPTY;
        }
        if(name!=NULL)
        {
                int j=lookup(res_vnode,name,namelen,&result);     
                if(j!=0)
                {
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_rmdir: Unable to resolve the final component in the path\n");
                        vput(result);
        		vput(res_vnode);
			return j;
                }
        }
        if(result==NULL)
        {
		dbg(DBG_ERROR | DBG_VFS, "ERROR: do_rmdir: Directory component of path doesn't exist\n");
                vput(res_vnode);
                return -ENOENT;
        }
       
        KASSERT(NULL!=res_vnode->vn_ops->rmdir);                
        i=(res_vnode->vn_ops->rmdir)(res_vnode,name,namelen);
        vput(result);
        vput(res_vnode);
        dbg(DBG_VFS,"INFO: Directory remove successful. Path=%s\n",path);
        /*NOT_YET_IMPLEMENTED("VFS: do_rmdir");*/
        return i;
}

/*
 * Same as do_rmdir, but for files.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EISDIR
 *        path refers to a directory.
 *      o ENOENT
 *        A component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_unlink(const char *path)
{
       if(strlen(path)>MAXPATHLEN){
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_unlink: Path is too long\n");
		return -ENAMETOOLONG;
	}
	
	size_t namelen=0;
        const char *name=NULL;
        vnode_t *res_vnode;
        vnode_t *result;
        KASSERT(path != NULL);
        int i=dir_namev(path, &namelen,&name,NULL,&res_vnode);
        if(i<0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_unlink: Unable to resolve the path\n");
                vput(res_vnode);
		return i;
        }
        if(res_vnode==NULL)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_unlink: A component in path doesn't exist\n");
                return -ENOENT;
        }
        else 
        {
                if(!S_ISDIR(res_vnode->vn_mode))
                {
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_unlink: A component in the path is not directory\n");
                        vput(res_vnode);
                        return -ENOTDIR;
                }
        }
        if(strlen(name)>NAME_LEN)
        {
		dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mkdir: A component of name was too long\n");
		vput(res_vnode);
		return -ENAMETOOLONG;
	}     
        /*if(strcmp(name,".")==0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_unlink: The final component in the path is \'.\'");
                vput(res_vnode);
                return -EINVAL;
        }
        if(strcmp(name,"..")==0)
        {
		dbg(DBG_ERROR | DBG_VFS,"EEROR: do_unlink: The final component in the path is \'..\'");
                vput(res_vnode);
                return -ENOTEMPTY;
        }*/
        if(name!=NULL)
        {
                int j=lookup(res_vnode,name,namelen,&result);     
                if(j!=0)
                {
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_unlink: Lookup for final component in the path fails\n");
                        vput(result);
        		vput(res_vnode);
			return j;
                }
        }
        if(result==NULL)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_unlink: Component in the path doesn't exist\n");
                vput(res_vnode);
                return -ENOENT;
        }
        
        if(S_ISDIR(result->vn_mode))
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_unlink: Path refers to a directory\n");
                vput(res_vnode);
                vput(result);
                return -EISDIR;
        }
        
       
        KASSERT(NULL!=res_vnode->vn_ops->unlink); 
        i=(res_vnode->vn_ops->unlink)(res_vnode,name,namelen);
        vput(res_vnode);
        vput(result);
        dbg(DBG_VFS,"INFO: Unlink successful. Path=%s\n",path);
       /* NOT_YET_IMPLEMENTED("VFS: do_unlink");*/
        return i;
}

/* To link:
 *      o open_namev(from)
 *      o dir_namev(to)
 *      o call the destination dir's (to) link vn_ops.
 *      o return the result of link, or an error
 *
 * Remember to vput the vnodes returned from open_namev and dir_namev.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        to already exists.
 *      o ENOENT
 *        A directory component in from or to does not exist.
 *      o ENOTDIR
 *        A component used as a directory in from or to is not, in fact, a
 *        directory.
 *      o ENAMETOOLONG
 *        A component of from or to was too long.
 */
 
 
int
do_link(const char *from, const char *to)
{
	if(strlen(from)>MAXPATHLEN){
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_link: Path name is too long\n");
		return -ENAMETOOLONG;
	}
	
        KASSERT(from!=NULL&&to!=NULL);
        size_t namelen=0;
        const char *name=NULL;
        vnode_t *node1;
        vnode_t *node2;
        vnode_t *result;
        int i=0,j=0;
        j=open_namev(from,NULL,&node1,NULL);
        i=dir_namev(to, &namelen,&name,NULL,&node2);
        
        if(i<0)
        {
                if(j>0)
                        vput(node1);
                return i;
        }
        if(j<0)
        {
                if(i>0)
                        vput(node2);
                return j;
        }
        if(strlen(name)>NAME_LEN)
        {
		dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mkdir: A component of name was too long\n");
		vput(node1);
                vput(node2);
		return -ENAMETOOLONG;
	}
        if(node1==NULL||node2==NULL)
        {
                dbg(DBG_ERROR | DBG_VFS,"ERROR: do_link: A directory component in the path of either \'to\' or \'from\' doesn't exist\n");
		return -ENOENT;
        }
        else 
        {

                if((!S_ISDIR(node2->vn_mode))||(!S_ISDIR(node1->vn_mode)))
                {
                       dbg(DBG_ERROR | DBG_VFS,"ERROR: do_link: A component in the path of either \'to\' or \'from\' is not directory\n");
                        return -ENOTDIR;
                }
        }     
        
        if(name!=NULL)
        {
                int j=lookup(node2,name,namelen,&result);
                if(j==0)
                {
                        vput(node1);
                        vput(node2);
                        vput(result);
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_link: Either \'to\' or \'from\' path already exist\n");
                        return -EEXIST;
                }
        }
        KASSERT(node2->vn_ops->link);                   
        i=(node2->vn_ops->link)(node1,node2,name,namelen);
        vput(node1);
        vput(node2);
        dbg(DBG_VFS,"INFO: Linking successful. From:%s To:%s\n",from,to);
       /* NOT_YET_IMPLEMENTED("VFS: do_link");*/
        return i;
}

/*      o link newname to oldname
 *      o unlink oldname
 *      o return the value of unlink, or an error
 *
 * Note that this does not provide the same behavior as the
 * Linux system call (if unlink fails then two links to the
 * file could exist).
 */
int
do_rename(const char *oldname, const char *newname)
{
        KASSERT(oldname != NULL&&newname != NULL);
        
        int i=do_link(oldname,newname);
        if(i<0)
                return i;
        int j=do_unlink(oldname);
        dbg(DBG_VFS,"INFO: Rename Successful. oldname:%s newname:%s\n",oldname,newname);
        /*NOT_YET_IMPLEMENTED("VFS: do_rename");*/
        return j;
}

/* Make the named directory the current process's cwd (current working
 * directory).  Don't forget to down the refcount to the old cwd (vput()) and
 * up the refcount to the new cwd (open_namev() or vget()). Return 0 on
 * success.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        path does not exist.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o ENOTDIR
 *        A component of path is not a directory.
 */
 
int
do_chdir(const char *path)
{
	if(strlen(path)>MAXPATHLEN){
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_chdir: Path name is too long\n");
		return -ENAMETOOLONG;
	}

        KASSERT(curproc!=NULL);
        vnode_t *res_vnode;
        int j=open_namev(path,NULL,&res_vnode,NULL);
        if(j<0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_chdir: Unable to change the directory\n");
                return j;
        }
        if(res_vnode==NULL)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_chdir: Path Doesn't exist\n");
                return -ENOENT;
        }
        else 
        {
                if(!S_ISDIR(res_vnode->vn_mode))
                {
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_chdir: A component of path is not a directory\n");
                        vput(res_vnode);
                        return -ENOTDIR;
                }
        }    
        vput(curproc->p_cwd);
        curproc->p_cwd=res_vnode;
        dbg(DBG_VFS,"INFO: Current directory is successfully changed. Path:%s\n",path);
        /*NOT_YET_IMPLEMENTED("VFS: do_chdir");*/
        return 0;
}

/* Call the readdir f_op on the given fd, filling in the given dirent_t*.
 * If the readdir f_op is successful, it will return a positive value which
 * is the number of bytes copied to the dirent_t.  You need to increment the
 * file_t's f_pos by this amount.  As always, be aware of refcounts, check
 * the return value of the fget and the virtual function, and be sure the
 * virtual function exists (is not null) before calling it.
 *
 * Return either 0 or sizeof(dirent_t), or -errno.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        Invalid file descriptor fd.
 *      o ENOTDIR
 *        File descriptor does not refer to a directory.
 */

int
do_getdent(int fd, struct dirent *dirp)
{
        KASSERT(curproc!=NULL);
        KASSERT(dirp!=NULL);
        if(fd<0||fd>=NFILES||(curproc->p_files[fd]==NULL))
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_getdent: Invalid file descriptor fd\n");
                return -EBADF;       
        }
        file_t *open_file=fget(fd);
        
        KASSERT(open_file!=NULL);
          
        if(!S_ISDIR(open_file->f_vnode->vn_mode))
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_getdent: File descriptor doesn't refer to a directory\n");
                fput(open_file);
                return -ENOTDIR;
        }
        KASSERT(open_file->f_vnode->vn_ops->readdir);
        int i=0;
        i=(open_file->f_vnode->vn_ops->readdir)(open_file->f_vnode,open_file->f_pos,dirp);
        open_file->f_pos=open_file->f_pos+i;
        if(i<=0)
        {
                 fput(open_file);
                 return i;
        }
        else
        {
                
                fput(open_file);
                return 0;
        }
        /*NOT_YET_IMPLEMENTED("VFS: do_getdent");*/
       dbg(DBG_VFS,"INFO: Successfully performed getdent operation on file with fd=%d\n",fd);
}

/*
 * Modify f_pos according to offset and whence.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not an open file descriptor.
 *      o EINVAL
 *        whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting
 *        file offset would be negative.
 */
 
int
do_lseek(int fd, int offset, int whence)
{
        KASSERT(curproc!=NULL);
        off_t i=0;
        if(fd<0||fd>=NFILES||(curproc->p_files[fd]==NULL))
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_lseek: fd is not an open file descriptor\n");
                return -EBADF;       
        }
        if((whence!=SEEK_SET)&&(whence!=SEEK_CUR)&&(whence!=SEEK_END))
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_lseek: \'whence\' is not SEEK_SET|SEEK_CUR|SEEK_END\n");
                return -EINVAL;       
        }
        file_t *open_file=fget(fd);
       
        KASSERT(open_file!=NULL);
        
        if(whence==SEEK_SET)
        {
                
                if(offset<0)
                {
                        fput(open_file);
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_lseek: File offset is negative\n");
                        return -EINVAL;
                }
                else
                {
                        i=offset;
                        open_file->f_pos=i;
                        fput(open_file);
                }
        }
        if(whence==SEEK_CUR)
        {
                if(open_file->f_pos+offset<0)
                {
                        fput(open_file);
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_lseek: File offset is negative\n");
                        return -EINVAL;
                } 
                else
                {
                        i=open_file->f_pos+offset;
                        open_file->f_pos=i;
                        fput(open_file);
                }
        }
        if(whence==SEEK_END)
        {       
                if(open_file->f_vnode->vn_len+offset<0)
                {
                        fput(open_file);
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_lseek: File offset is negative\n");
                        return -EINVAL;
                }      
                else
                {
                        i=open_file->f_vnode->vn_len+offset;
                        open_file->f_pos=i;
                        fput(open_file);
                }
        }
        /*NOT_YET_IMPLEMENTED("VFS: do_lseek");*/
	dbg(DBG_VFS,"INFO: Successfully performed seek operation on file with fd=%d, offset%d and whence=%d\n",fd,offset,whence);
        return i;
}

/*
 * Find the vnode associated with the path, and call the stat() vnode operation.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        A component of path does not exist.
 *      o ENOTDIR
 *        A component of the path prefix of path is not a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_stat(const char *path, struct stat *buf)
{
	if(strlen(path)>MAXPATHLEN){
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_stat: A component of path is too long\n");
		return -ENAMETOOLONG;
	}

        KASSERT(buf!=NULL);
        size_t namelen=0;
        const char *name=NULL;
        vnode_t *res_vnode;
        vnode_t *result;
        KASSERT(path != NULL);
        int i=dir_namev(path, &namelen,&name,NULL,&res_vnode);
        if(i<0)
        {
		vput(res_vnode);
                return i;
        }
        if(strlen(name)>NAME_LEN)
        {
		dbg(DBG_ERROR | DBG_VFS, "ERROR: do_mkdir: A component of name was too long\n");
		vput(res_vnode);
		return -ENAMETOOLONG;
	}
	if(res_vnode==NULL)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_stat: A component of path doesn't exist\n");
                return -ENOENT;
        }
        else 
        {
                if(!S_ISDIR(res_vnode->vn_mode))
                {
                        vput(res_vnode);
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_stat: A component of the path prefix of path is not directory\n");
                        return -ENOTDIR;
                }
        }     
        /*if(strcmp(name,".")==0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_stat: final component of path is \'.\'");
                vput(res_vnode);
                return -EINVAL;
        }
        if(strcmp(name,"..")==0)
        {
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_stat: final component of path is \'..\'");
                vput(res_vnode);
                return -ENOTEMPTY;
        }*/
        if(name!=NULL)
        {
                int j=lookup(res_vnode,name,namelen,&result);     
                if(j!=0)
                {
			vput(res_vnode);
			dbg(DBG_ERROR | DBG_VFS,"ERROR: do_stat: Lookup for the final component of path fails\n");
                        return j;
                }
        }
        if(result==NULL)
        {
                vput(res_vnode);
		dbg(DBG_ERROR | DBG_VFS,"ERROR: do_stat: A component of path doesn't exist\n");
                return -ENOENT;
        }
        
        KASSERT(res_vnode->vn_ops->stat); 
        i=(res_vnode->vn_ops->stat)(result,buf);
        
        vput(res_vnode);
        vput(result);
        
        /*NOT_YET_IMPLEMENTED("VFS: do_stat");*/
	dbg(DBG_VFS,"INFO: Successfully found the associated nodes with the path:%s\n",path);
        return i;
}

#ifdef __MOUNTING__
/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutely sure your Weenix is perfect.
 *
 * This is the syscall entry point into vfs for mounting. You will need to
 * create the fs_t struct and populate its fs_dev and fs_type fields before
 * calling vfs's mountfunc(). mountfunc() will use the fields you populated
 * in order to determine which underlying filesystem's mount function should
 * be run, then it will finish setting up the fs_t struct. At this point you
 * have a fully functioning file system, however it is not mounted on the
 * virtual file system, you will need to call vfs_mount to do this.
 *
 * There are lots of things which can go wrong here. Make sure you have good
 * error handling. Remember the fs_dev and fs_type buffers have limited size
 * so you should not write arbitrary length strings to them.
 */
int
do_mount(const char *source, const char *target, const char *type)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_mount");
        return -EINVAL;
}

/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutley sure your Weenix is perfect.
 *
 * This function delegates all of the real work to vfs_umount. You should not worry
 * about freeing the fs_t struct here, that is done in vfs_umount. All this function
 * does is figure out which file system to pass to vfs_umount and do good error
 * checking.
 */
int
do_umount(const char *target)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_umount");
        return -EINVAL;
}
#endif
