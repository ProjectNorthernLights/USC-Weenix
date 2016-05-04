#include     "kernel.h"
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function, but you may want to special case
 * "." and/or ".." here depnding on your implementation.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
        /*NOT_YET_IMPLEMENTED("VFS: lookup DONE");*/

        if(dir->vn_mode != S_IFDIR)
            {
                *result = NULL;
                return -ENOTDIR;
            }

        if(!strcmp(name,"."))
        {
            int temp_retval = dir->vn_ops->lookup(dir, name, strlen(name), result);
            return temp_retval; 
        }

        if(!strcmp(name,".."))
        {
            int temp_retval = dir->vn_ops->lookup(dir, name, strlen(name), result);
            return temp_retval; 
        }

        int retval = dir->vn_ops->lookup(dir, name, len, result);
        
        if(retval == -ENOENT)
        {
            *result = NULL;
            return -ENOENT;
        }

        return retval;

}


/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,
          vnode_t *base, vnode_t **res_vnode)
{
        /*NOT_YET_IMPLEMENTED("VFS: dir_namev DONE");*/

        /**res_vnode = NULL;*/
    

        dbg_print("pathname : %s\n", pathname);
        
        if(strlen(pathname) > 1024)
        {
            *res_vnode = NULL;
            *namelen = NULL;
            dbg_print("%s\n", strerror(ENAMETOOLONG));
            return -ENAMETOOLONG;
        }

        char final_path[MAXPATHLEN];
        memset(final_path, 0, sizeof(final_path));

        if(pathname[0] != '/' && pathname[0] != '.')
        {
            strcat(final_path, "./");
            strcat(final_path, pathname);
        }

        else
        {
            strcat(final_path, pathname);
        }
        vnode_t *curr_dir;
        char *f_name;

        if(!strncmp(final_path,"./",2))
        {    
            f_name = strchr(final_path,'/');
            curr_dir = curproc->p_cwd;
        }

        else if(!strncmp(final_path,"../",3))
        {
            f_name = strchr(final_path,'/');
            int retval = lookup(curproc->p_cwd, "..", (size_t)2, &curr_dir);
            
            if(retval != 0)
                return retval;
        
        }

        else
        {
            f_name = final_path;
            curr_dir = vfs_root_vn;

        }

        char dup_path[MAXPATHLEN + 1];
        memset(dup_path,0,sizeof(dup_path));
        char *temp_name = NULL;
        char *dup_path_ptr = NULL;
        char search_name[NAME_LEN + 1];
        memset(search_name,0,sizeof(search_name));
        
        dup_path_ptr = dup_path;

        int err = 0;

        char *temp = NULL;
        dbg_print("f_path : %s\n", f_name);
        
        *name = strrchr(pathname, '/');
        
        if(*name)
            *name = *name + 1; 
        else
            *name = pathname;

        *namelen = (size_t) strlen(*name);

        if(*namelen > NAME_LEN)
        {
                *res_vnode = NULL;
                return -ENAMETOOLONG;
        }

        strncpy(dup_path, f_name, strlen(f_name) - *namelen - 1);
        
        
        if(!strlen(dup_path_ptr))
        {
            *res_vnode = curr_dir;
            vref(*res_vnode);
            return 0;
        }

        char *x = NULL;
        char hallelujah[MAXPATHLEN + 1];
        /*memset(hallelujah, '\0', sizeof(hallelujah));*/

        int i, j;
        for(i = 0, j = 0; i < MAXPATHLEN + 1; i++)
        {
            if(dup_path[i] != '/')
            {
                hallelujah[j] = dup_path[i];
                j++;
            }
            if(dup_path[i] == '/' && dup_path[i+1] != '/')
            {
                hallelujah[j] = dup_path[i];
                j++;
            }
        }
        memset(dup_path, '\0', sizeof(dup_path));
        strncpy(dup_path, hallelujah, MAXPATHLEN + 1);

        while((x = strchr(dup_path_ptr + 1,'/'))!= NULL)
        {
            
            temp_name = strchr(dup_path_ptr + 1, '/'); 
            
            strncpy(search_name, dup_path_ptr + 1, temp_name - dup_path_ptr - 1);

            search_name[strlen(search_name)] = '\0';
            
            if(strlen(search_name) > NAME_LEN)
            {
                *res_vnode = NULL;
                return -ENAMETOOLONG;
            }
            
            dbg_print("LOOKING UP: %s\n", search_name);
            err = lookup(curr_dir, search_name, strlen(search_name), res_vnode);
            memset(search_name,0,sizeof(search_name));
            
            if(err)
            {
                return err;
            }

            curr_dir = *res_vnode; 
            vput(*res_vnode);
            
            dup_path_ptr = temp_name;
    
        }

        if(!strcmp(pathname,"."))
             strcpy(search_name,".");
        else if(!strcmp(pathname,".."))
            strcpy(search_name,"..");
        else 
        {   
                memset(search_name,0,sizeof(search_name));
                
                strncpy(search_name, dup_path_ptr + 1, strlen(dup_path_ptr) - 1);
        }
        if(strlen(search_name) > NAME_LEN)
        {
                *res_vnode = NULL;
                return -ENAMETOOLONG;
        }

        err = lookup(curr_dir, search_name, strlen(search_name), res_vnode);
        return err;

}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fnctl.h>.  If the O_CREAT flag is specified, and the file does
 * not exist call create() in the parent directory vnode.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
        /*NOT_YET_IMPLEMENTED("VFS: open_namev DONE");*/

        char *name;
        char *f_name;
        int namelen = 0;
        vnode_t *curr_dir;     
        int retval; 
        
        if(strlen(pathname) > MAXPATHLEN)
        {
            return -ENAMETOOLONG;
        }

        if (flag == O_CREAT)
        {
            /* GET vnode_t FOR PARENT DIRECTORY*/

            dbg_print("CALLING CREATE NOW: %p\n", res_vnode);

            dbg_print("COMPARING\n");
            if(!strncmp(pathname,"./",2))
            {
                f_name = strchr(pathname,'/');
                curr_dir = curproc->p_cwd;
            }

            else if(!strncmp(pathname,"../",3))
            {
                f_name = strchr(pathname,'/');
                int retval = lookup(curproc->p_cwd, "..", strlen(name), &curr_dir);
                switch(retval)
                {
                    case -ENOENT: dbg_print("%s\n", strerror(ENOENT));
                                 return -ENOENT;
                    case -ENOTDIR: dbg_print("%s\n", strerror(ENOTDIR));
                                 return -ENOTDIR;
                    default: break;
                }
            }

            else
            {
                f_name = (char *)pathname;
                retval = dir_namev(pathname, (size_t *) &namelen, (const char **) &name, base, &curr_dir);
               
                switch(retval)
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

                vput(curr_dir);
            }

            if(!S_ISDIR(curr_dir->vn_mode))
            {

                dbg_print("%s\n", strerror(ENOTDIR));
                *res_vnode = NULL;
                return -ENOTDIR; 

            }

            int ret = (curr_dir)->vn_ops->create(curr_dir, (const char *)name, (size_t)strlen(name), res_vnode);
            vref(*res_vnode);

            dbg_print("EXITING CREATE NOW\n");
            return ret;

        }

        if(strcmp(pathname,".") && strcmp(pathname,".."))
            {   
                retval = dir_namev(pathname, (size_t *) &namelen, (const char **) &name, base, &curr_dir);
                vput(curr_dir); 
            }
        else
        {

/*            *res_vnode = curproc->p_cwd;
            res_vnode = &(curproc->p_cwd);*/

            curr_dir = curproc->p_cwd;
            
            if(!strcmp(pathname,".")) name = ".";
            else name = "..";
        }

        retval = lookup(curr_dir, (const char *)name, strlen(name), res_vnode);

        switch(retval)
        {
            case -ENOENT: dbg_print("%s\n", strerror(ENOENT));
                         return -ENOENT;
            case -ENOTDIR: dbg_print("%s\n", strerror(ENOTDIR));
                         return -ENOTDIR;
            default: break;
        }
}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
