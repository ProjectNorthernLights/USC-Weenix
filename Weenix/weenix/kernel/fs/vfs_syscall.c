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
        /*NOT_YET_IMPLEMENTED("VFS: do_read DONE");*/
        dbg_print("ENTERING READ()\n");
        if((fd < 0)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }
        if((fd > NFILES)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }

        if (!curproc->p_files[fd])
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF; 
        }

        file_t *file_ = fget(fd);

        int mode = file_->f_vnode->vn_mode;
        vnode_t *f_vnode_ = file_->f_vnode;
        
        if(S_ISDIR(mode))
        {
                dbg_print("%s\n", strerror(EISDIR));
                return -EISDIR;    
        }

        KASSERT(NULL != file_);
        
        if(file_->f_mode == 2 || file_->f_mode == 4 || file_->f_mode == 6)
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;
        }

        off_t offset = file_->f_pos;
        dbg_print("READING NOW offset: %d\n", offset);
        int count = f_vnode_->vn_ops->read(f_vnode_, offset, buf, nbytes);
        file_->f_pos = offset + count;
        fput(file_);
        dbg_print("EXITING READ() after reading %d bytes\n", count);
        return count;

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
        /*NOT_YET_IMPLEMENTED("VFS: do_write DONE");*/
        dbg_print("ENTERING WRITE()\n");
        
        if((fd < 0)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }
        if((fd > NFILES)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }

        if (!curproc->p_files[fd])
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF; 
        }

        file_t *file_ = fget(fd);        

        int mode = file_->f_vnode->vn_mode;
        
        if(S_ISDIR(mode))
        {
                dbg_print("%s\n", strerror(EISDIR));
                return -EISDIR;    
        }

        vnode_t *f_vnode_ = file_->f_vnode;
        
        if(file_->f_mode == FMODE_READ)
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;
        }
        else if(file_->f_mode & 4)
        {
                file_->f_pos = do_lseek(fd, 0, SEEK_END);       
        }

        KASSERT(NULL != file_);

        off_t offset = file_->f_pos;
        int count = f_vnode_->vn_ops->write(f_vnode_, offset, buf, nbytes);
        file_->f_pos = offset + nbytes;
        fput(file_);
        dbg_print("EXITING WRITE() after reading %d bytes\n", count);
        return count;
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
        /*NOT_YET_IMPLEMENTED("VFS: do_close DONE");*/
        dbg_print("ENTERING CLOSE()\n");
        
        if((fd < 0)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }
        if((fd > NFILES)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }

        if (!curproc->p_files[fd])
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF; 
        }

        file_t *file_ = curproc->p_files[fd];
        KASSERT(NULL != file_);

        curproc->p_files[fd] = NULL;
        fput(file_);

        dbg_print("EXITING CLOSE()\n");
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
        /*NOT_YET_IMPLEMENTED("VFS: do_dup DONE");*/
        dbg_print("ENTERING DUP()\n");
        
        if((fd < 0)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }
        if((fd > NFILES)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }

        if (!curproc->p_files[fd])
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF; 
        }

        file_t *file_ = fget(fd);
        KASSERT(NULL != file_);

        int new_fd = get_empty_fd(curproc);

        if(new_fd > NFILES)   
        {
                dbg_print("%s\n", strerror(EMFILE));
                return -EMFILE;   
        }

        curproc->p_files[new_fd] = curproc->p_files[fd];
        dbg_print("EXITING DUP()\n");
        return new_fd;
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
        /*NOT_YET_IMPLEMENTED("VFS: do_dup2 DONE");*/
        dbg_print("ENTERING DUP2()\n");
        
        if((ofd < 0)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }
        if((ofd > NFILES)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }
        if((nfd < 0)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }
        if((nfd > NFILES)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }

        if (!curproc->p_files[ofd])
        {
                dbg_print("%s\n", strerror(EBADF));
                return EBADF; 
        }

        if ((curproc->p_files[nfd]) && (nfd != ofd))
        {
                do_close(nfd);
        }

        curproc->p_files[nfd] = curproc->p_files[ofd];
        
        fref(curproc->p_files[nfd]);
        dbg_print("EXITING DUP2()\n");
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
        /*NOT_YET_IMPLEMENTED("VFS: do_mknod DONE");*/
        
        dbg_print("INSIDE DO_MKNOD()\n");

        if(!S_ISCHR(mode) && !S_ISBLK(mode))
        {      
            dbg_print("%s\n", strerror(EINVAL));
            return -EINVAL;   
        }

        if(strlen(path) > MAXPATHLEN)
        {
            dbg_print("%s\n", strerror(ENAMETOOLONG));
            return -ENAMETOOLONG;
        }

        size_t namelen = 0;
        char *name;
        memset(name,0,sizeof(name));

        vnode_t *res_vnode = NULL;
        vnode_t *result = NULL;

        /*CHECK PROPERLY FOR ENOENT AND ENOTDIR AFTER dir_namev is implemented*/

        int retval = dir_namev(path, &namelen,(const char **) &name, NULL, &res_vnode);
        
        if(retval)
        {
            dbg_print("%s\n", strerror(retval));
            return retval; 
        }
        
        

        retval = lookup(res_vnode, name, namelen, &result);
        vput(res_vnode);
        if(!retval)
        {
            dbg_print("%s\n", strerror(EEXIST));
            vput(result);

            return -EEXIST;
        }
         if(retval == -ENOTDIR)
        {
            dbg_print("%s\n", strerror(ENOTDIR));
           /*vput(res_vnode);*/
           
           return -ENOTDIR; 

        }
        if(retval == -ENAMETOOLONG)
         {
            dbg_print("%s\n", strerror(ENAMETOOLONG));
           /*vput(res_vnode);*/
           
           return -ENAMETOOLONG; 

        }

        int ino = res_vnode->vn_ops->mknod(res_vnode, name, namelen, mode, devid);

        if(!ino)
        {
          /*INITIALIZING VNODE*/
            (void)lookup(res_vnode, name, namelen, &result);
        }
        
        dbg_print("MK_NOD() RETVAL : %d\n", ino);
        
        return ino;
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
        /*NOT_YET_IMPLEMENTED("VFS: do_mkdir DONE");*/
        dbg_print("ENTERING MKDIR()\n");
    size_t namelen;
    const char *name;
    vnode_t *base;
    vnode_t *res_vnode;
    vnode_t *result;
    
    int err = dir_namev(path, &namelen ,&name , NULL, &res_vnode);
    
    if (err)
      {  
           dbg_print("%s\n", strerror(err));
           return err; 
      }

    err = lookup(res_vnode, name, namelen, &result);   
    vput(res_vnode);

    if (!err)
      {  
           dbg_print("%s\n", strerror(EEXIST));
           /*vput(res_vnode);*/
           vput(result);
           return -EEXIST; 
      }
    if(err == -ENOTDIR)
    {
        dbg_print("%s\n", strerror(ENOTDIR));
           /*vput(res_vnode);*/
           
           return -ENOTDIR; 

    }
    if(err == -ENAMETOOLONG)
    {
        dbg_print("%s\n", strerror(ENAMETOOLONG));
           /*vput(res_vnode);*/
           
           return -ENAMETOOLONG; 

    }

    /*if(res_vnode && !result)
        {
            result = res_vnode;
        } C */

    int retval = res_vnode->vn_ops->mkdir(res_vnode, name, namelen);
     /*C*/

    if(!retval)
    {
        /*INITIALIZING VNODE*/
        (void)lookup(res_vnode, name, namelen, &result);
    }

    dbg_print("MKDIR() RETVAL: %d\n", retval);
    return retval;
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
        /*NOT_YET_IMPLEMENTED("VFS: do_rmdir DONE");*/
       dbg_print("ENTERING RMDIR()\n");
    size_t namelen;
    char *name;
    vnode_t *base;
    vnode_t *res_vnode;
    vnode_t *result;

     if(!strcmp(path,"."))
        {
            dbg_print("%s\n", strerror(EINVAL));
            return -EINVAL;
        }

        if(!strcmp(path,".."))
        {
            dbg_print("%s\n", strerror(ENOTEMPTY));
            return -ENOTEMPTY;
        }

    int err = dir_namev(path , &namelen , (const char **)&name , NULL, &res_vnode);

    if(err)
      {  
           dbg_print("%s\n", strerror(err));
           return err; 
      }

    if(!strcmp(name,"."))
        {
            dbg_print("%s\n", strerror(EINVAL));
            return -EINVAL;
        }

        if(!strcmp(name,".."))
        {
            dbg_print("%s\n", strerror(ENOTEMPTY));
            return -ENOTEMPTY;
        }


    err = lookup(res_vnode, name, namelen, &result);

    vput(res_vnode);

    if(err)
      {  
           dbg_print("%s\n", strerror(err));                 
           return err; 
      }

     if (S_ISDIR(result->vn_mode))
     { 
   
        dbg_print("EXITING RMDIR\n");
        vput(result);
        err = res_vnode->vn_ops->rmdir(res_vnode,  name, namelen); 
        if(!err)
            return err;
        else
        {
           dbg_print("%s\n", strerror(err));
            return err; 
        } 

     }
     else
     {
            dbg_print("%s\n", strerror(ENOTDIR));
            vput(result);
            return -ENOTDIR; 
     }

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
        /*NOT_YET_IMPLEMENTED("VFS: do_unlink DONE");*/
    dbg_print("ENTERING UNLINK()\n");
    size_t namelen;
    char *name;
    vnode_t *base;
    vnode_t *res_vnode;
    vnode_t *result;

    /*char *base_ptr = strrchr(path, '/');
    base_ptr = base_ptr + 1;

    if(!strcmp(base_ptr,"."))
    {
            dbg_print("%s\n", strerror(EINVAL));
           return EINVAL; 
    }

    if(!strcmp(base_ptr,".."))
    {
            dbg_print("%s\n", strerror(ENOTEMPTY));
           return ENOTEMPTY; 
    }*/

    int err = dir_namev(path, &namelen ,(const char **)&name , NULL, &res_vnode);

     if (err)
      {  
           dbg_print("%s\n", strerror(err));
           return err; 
      }

      
    err = lookup(res_vnode, name, namelen, &result);
    
    
    if (err)
      {  
           dbg_print("%s\n", strerror(err));
           vput(res_vnode);
           return err; 
      }

    if (!S_ISDIR(result->vn_mode))
     { 
        dbg_print("EXITING UNLINK()\n");
        
        err = res_vnode->vn_ops->unlink(res_vnode, name, namelen);
        vput(result);
        vput(res_vnode);
        if(!err)
            return err;
        else
        {
           /*dbg_print("%s\n", strerrorerr(err));*/
            return err; 
        } 

     }           
    else
     {
             dbg_print("%s\n", strerror(EISDIR));
             vput(res_vnode);
             vput(result);
             return -EISDIR;
     }    
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
        /*NOT_YET_IMPLEMENTED("VFS: do_link DONE");*/
        dbg_print("ENTERING LINK()\n");
        int dir_ret, open_ret;
        vnode_t *from_vnode = NULL;
        vnode_t *to_vnode = NULL;
        vnode_t *res_vnode = NULL;
        
        open_ret = open_namev(from, 0, &from_vnode, NULL);
        
        switch(open_ret)
        {
            case -EEXIST: dbg_print("%s\n", strerror(EEXIST));
                         return -EEXIST;
            case -ENOENT: dbg_print("%s\n", strerror(ENOENT));
                         return -ENOENT;
            case -ENOTDIR: dbg_print("%s\n", strerror(ENOTDIR));
                         return -ENOTDIR;
            case -ENAMETOOLONG: dbg_print("%s\n", strerror(ENAMETOOLONG));
                               return -ENAMETOOLONG;
            default: break;
        }

        vput(from_vnode);
        size_t namelen = 0;
        char *name;

        dir_ret = dir_namev(to, &namelen, (const char **)&name, NULL, &to_vnode);
        
        switch(dir_ret)
        {
            case -EEXIST: dbg_print("%s\n", strerror(EEXIST));
                         return -EEXIST;
            case -ENOENT: dbg_print("%s\n", strerror(ENOENT));
                         return -ENOENT;
            case -ENOTDIR: dbg_print("%s\n", strerror(ENOTDIR));
                         return -ENOTDIR;
            case -ENAMETOOLONG: dbg_print("%s\n", strerror(ENAMETOOLONG));
                               return -ENAMETOOLONG;
            default: break;
        }

        if(!lookup(to_vnode, name, namelen, &res_vnode))
        {
            dbg_print("%s\n", strerror(EEXIST));
            vput(to_vnode);
            vput(res_vnode);
            return -EEXIST;
        }

        vput(to_vnode);
        int ret = to_vnode->vn_ops->link(from_vnode, to_vnode, name, namelen);

        dbg_print("EXITING LINK\n");
        return ret;
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
        /*NOT_YET_IMPLEMENTED("VFS: do_rename DONE");*/
        dbg_print("ENTERING RENAME()\n");
        do_link(oldname,newname);
        dbg_print("EXITING RENAME\n");
        return do_unlink(oldname);

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
        /*NOT_YET_IMPLEMENTED("VFS: do_chdir DONE");*/
    dbg_print("ENTERING CHDIR()\n");
    size_t namelen;
    char *name;
    vnode_t *base;
    vnode_t *res_vnode;
    vnode_t *result;
    int err;

    if(!strcmp(path,"."))
    {
        return 0; 
    }

    if(!strcmp(path,".."))
    {
        err = lookup(curproc->p_cwd, path, strlen(path), &result);
        
        if (err)
        {  
           dbg_print("%s\n", strerror(err));       
           return err; 
        }

        vput(curproc->p_cwd);
        curproc->p_cwd = result;
        return 0;
    }

    err = dir_namev(path , &namelen , (const char **)&name , NULL, &res_vnode);
    if (err)
      {  
           dbg_print("%s\n", strerror(err));
           return err; 
      }
    
    err = lookup(res_vnode, name, namelen, &result);
    vput(res_vnode);
    
    if (err)
      {  
           dbg_print("%s\n", strerror(err));       
           return err; 
      }
    
    if(S_ISDIR(result->vn_mode)) 
    {
        vput(curproc->p_cwd);
        curproc->p_cwd = result;
    }
    else
    {
        dbg_print("%s\n", strerror(ENOTDIR));       
        vput(result);
        return -ENOTDIR;    
    }

    dbg_print("EXITING CHDIR\n");
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
        /*NOT_YET_IMPLEMENTED("VFS: do_getdent DONE");*/
        dbg_print("ENTERING GETDENT()\n");
        
        if((fd < 0)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }
        if((fd > NFILES)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }

        if (!curproc->p_files[fd])
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF; 
        }

        file_t *file_ = fget(fd);
        fput(file_);
        KASSERT(NULL != file_);
        
        vnode_t *file_vnode = file_->f_vnode;
        int file_type = file_vnode->vn_mode;
        
        if(!S_ISDIR(file_type))
        {
            dbg_print("%s\n", strerror(ENOTDIR));
            return -ENOTDIR;
        }
        
        int read_ret;

        if(NULL == file_vnode->vn_ops->readdir) /*CHECKING FOR THE FUNCTION READDIR()*/
        {
            dbg_print("%s\n", strerror(EBADF));
            return -EBADF;

        }

        read_ret = file_vnode->vn_ops->readdir(file_vnode, file_->f_pos, dirp);
        
        file_->f_pos += read_ret;

        dbg_print("EXITING GETDENT\n");

        if(read_ret > 0)
            return sizeof(dirent_t);
        else
            return 0;

}

/*
 * Modify f_pos according to offset and whence.22
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
        /*NOT_YET_IMPLEMENTED("VFS: do_lseek DONE");*/

        dbg_print("ENTERING LSEEK()\n");

        
        if((fd < 0)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }
        if((fd > NFILES)) /*----------------------------CHECK AGAIN*/
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF;   
        }

        if (!curproc->p_files[fd])
        {
                dbg_print("%s\n", strerror(EBADF));
                return -EBADF; 
        }

        file_t *file_ = fget(fd);
        KASSERT(NULL != file_);
        fput(file_);
        vnode_t *f_vnode_ = file_->f_vnode;
        int orig_seek = file_->f_pos;
        switch (whence)
        {
                case SEEK_SET: file_->f_pos = offset;
                               break;
                case SEEK_CUR: file_->f_pos += offset;
                               break;
                case SEEK_END: file_->f_pos = offset + f_vnode_->vn_len; 
                               break;
                default: dbg_print("%s\n", strerror(EINVAL));
                         return -EINVAL; 
        }
        
        dbg_print("EXITING LSEEK()\n");

        if(file_->f_pos < 0)
        {
            file_->f_pos = orig_seek;
            dbg_print("%s\n", strerror(EINVAL));
                return -EINVAL; 
        }

        return file_->f_pos;
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
        /*NOT_YET_IMPLEMENTED("VFS: do_stat DONE");*/
        dbg_print("ENTERING STAT()\n");
        vnode_t *res = (vnode_t *)kmalloc(sizeof(vnode_t *));

        dbg_print("PATH: %s\n", path);
        
        int ret = open_namev(path, 0, &res, NULL);
    
        switch(ret)
        {
            case -ENOENT: dbg_print("%s\n", strerror(ENOENT));
                         return -ENOENT;
            case -ENOTDIR: dbg_print("%s\n", strerror(ENOTDIR));
                         return -ENOTDIR;
            case -ENAMETOOLONG: dbg_print("%s\n", strerror(ENAMETOOLONG));
                               return -ENAMETOOLONG;
            default: break;
        }

        res->vn_ops->stat(res, buf);
        vput(res);

        dbg_print("EXITING STAT()\n");
        return 0;
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
        return EINVAL;
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
        return EINVAL;
}
#endif