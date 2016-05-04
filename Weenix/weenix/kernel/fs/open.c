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
        /*NOT_YET_IMPLEMENTED("VFS: do_open DONE");*/

        dbg_print("OFLAGS: %d\n", oflags);
        if(!((oflags-3)%2) && (oflags-3) >= 0)
            {
                dbg_print("%s\n", strerror(EINVAL));
                return EINVAL;  
            }

        int new_fd, oflags_dup = oflags; 
        file_t *file_ptr;
        
        file_t *files = fget(-1);
        /*memset(files, 0 , sizeof(file_t)); C */

        vnode_t *base = NULL;
        vnode_t *res_vnode;
     
        new_fd = get_empty_fd(curproc);
        file_ptr = fget(new_fd);

        if(!file_ptr)
        {
            file_ptr = files;
        }
        else
            fput(files);

        curproc->p_files[new_fd] = file_ptr;

        switch(oflags)
        {
            case O_RDONLY|O_CREAT:
                            oflags_dup = O_CREAT;
                            break; 
            case O_WRONLY|O_CREAT:
                            oflags_dup = O_CREAT;
                            break;
            case O_RDWR|O_CREAT: 
                            oflags_dup = O_CREAT;
                            break;
            case O_WRONLY|O_APPEND|O_CREAT:
                            oflags_dup = O_CREAT;
                            break;
            case O_RDWR|O_APPEND|O_CREAT:
                            oflags_dup = O_CREAT;
                            break;
            default:
                            break;
        }

        int retval = open_namev(filename, oflags_dup, &res_vnode, base);

        if(retval)
        {
            dbg_print("%s\n", strerror(-retval));
            curproc->p_files[new_fd] = NULL;
            
            if(res_vnode)
                vput(res_vnode);

            return retval;
        }

        if(S_ISDIR(res_vnode->vn_mode))
        {
            if(oflags == O_WRONLY || oflags == O_RDWR)
            {
                    dbg_print("%s\n", strerror(EISDIR));
                        curproc->p_files[new_fd] = NULL;
                        if(res_vnode)
                            vput(res_vnode);
            
                        return -EISDIR;
            }
        }
        

        file_ptr->f_vnode = res_vnode;

        /*LOOK MORE INTO SETTING THE FILE MODE FLAGS*/

        switch(oflags)
        {
            case O_RDONLY:
                            files->f_mode = FMODE_READ;
                            break;
            case O_RDONLY|O_CREAT:    
                            files->f_mode = FMODE_READ;
                            break; 
            case O_WRONLY:
                            files->f_mode = FMODE_WRITE;
                            break;
            case O_WRONLY|O_CREAT:
                            files->f_mode = FMODE_WRITE;
                            break;
            case O_WRONLY|O_APPEND:
                            files->f_mode = FMODE_WRITE|FMODE_APPEND;  
                            break;
            case O_WRONLY|O_APPEND|O_CREAT:
                            files->f_mode = FMODE_WRITE|FMODE_APPEND;  
                            break;
            case O_RDWR:
                            files->f_mode = FMODE_READ|FMODE_WRITE;     
                            break;
            case O_RDWR|O_CREAT:    
                            files->f_mode = FMODE_READ|FMODE_WRITE;     
                            break;
            case O_RDWR|O_APPEND:
                            files->f_mode = FMODE_READ|FMODE_WRITE|FMODE_APPEND;
                            break;
            case O_RDWR|O_APPEND|O_CREAT:
                            files->f_mode = FMODE_READ|FMODE_WRITE|FMODE_APPEND;
                            break;             
            default:
                            break;

        }

        return new_fd;
}