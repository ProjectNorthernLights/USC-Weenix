#include "kernel.h"
#include "config.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"
#include "proc/proc.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/kmalloc.h"

#include "vm/vmmap.h"

#include "fs/vfs.h"
#include "fs/vfs_syscall.h"
#include "fs/vnode.h"
#include "fs/file.h"

proc_t *curproc = NULL; /* global */
static slab_allocator_t *proc_allocator = NULL;

static list_t _proc_list;
static proc_t *proc_initproc = NULL; /* Pointer to the init process (PID 1) */

void
proc_init()
{
        list_init(&_proc_list);
        proc_allocator = slab_allocator_create("proc", sizeof(proc_t));
        KASSERT(proc_allocator != NULL);
}

static pid_t next_pid = 0;

/**
 * Returns the next available PID.
 *
 * Note: Where n is the number of running processes, this algorithm is
 * worst case O(n^2). As long as PIDs never wrap around it is O(n).
 *
 * @return the next available PID
 */
static int
_proc_getid()
{
        proc_t *p;
        pid_t pid = next_pid;
        while (1) {
failed:
                list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                        if (p->p_pid == pid) {
                                if ((pid = (pid + 1) % PROC_MAX_COUNT) == next_pid) {
                                        return -1;
                                } else {
                                        goto failed;
                                }
                        }
                } list_iterate_end();
                next_pid = (pid + 1) % PROC_MAX_COUNT;
                return pid;
        }
}

/*
 * The new process, although it isn't really running since it has no
 * threads, should be in the PROC_RUNNING state.
 *
 * Don't forget to set proc_initproc when you create the init
 * process. You will need to be able to reference the init process
 * when reparenting processes to the init process.
 */

proc_t *
proc_create(char *name)
{
        int i;

        proc_t *new = (proc_t *)slab_obj_alloc(proc_allocator);
        pid_t pid = _proc_getid();

        memset(new,0,sizeof(proc_t));

        if(!pid)
          KASSERT(PID_IDLE != pid || list_empty(&_proc_list));
        

        if(pid == PID_INIT)
        {  
          KASSERT(PID_INIT != pid || PID_IDLE == curproc->p_pid);
          proc_initproc = new; 
        }

        /*VFS params*/
        for(i = 0; i < NFILES; i++)
        {
          new->p_files[i] = NULL;
        }

        if(pid > 2)
        {
            dbg_print("***** %s *****", curproc->p_comm);
            new->p_cwd = curproc->p_cwd;
            vref(new->p_cwd);
        }

        new->p_pid = pid;
        new->p_pproc = curproc;
        new->p_state = PROC_RUNNING;
        new->p_pagedir = pt_get();
        new->p_status = 0;
        strcpy(new->p_comm,name);

        list_init(&(new->p_threads));
        list_init(&(new->p_children));
        sched_queue_init(&(new->p_wait));
        
        list_insert_before(&_proc_list, &(new->p_list_link));
        
        if(pid > 0)
          list_insert_before(&((new->p_pproc)->p_children), &(new->p_child_link)); 
       
        return new;
}

/**
 * Cleans up as much as the process as can be done from within the
 * process. This involves:
 *    - Closing all open files (VFS)
 *    - Cleaning up VM mappings (VM)
 *    - Waking up its parent if it is waiting
 *    - Reparenting any children to the init process
 *    - Setting its status and state appropriately
 *
 * The parent will finish destroying the process within do_waitpid (make
 * sure you understand why it cannot be done here). Until the parent
 * finishes destroying it, the process is informally called a 'zombie'
 * process.
 *
 * This is also where any children of the current process should be
 * reparented to the init process (unless, of course, the current
 * process is the init process. However, the init process should not
 * have any children at the time it exits).
 *
 * Note: You do _NOT_ have to special case the idle process. It should
 * never exit this way.
 *
 * @param status the status to exit the process with
 */
void
proc_cleanup(int status)
{
        /*NOT_YET_IMPLEMENTED("PROCS: proc_cleanup");*/
          
  proc_t * p;
  kthread_t *kt;

  KASSERT(1 <= curproc->p_pid);
  KASSERT(NULL != curproc->p_pproc);

  if( curproc == proc_initproc )
  { 
      KASSERT(NULL != proc_initproc);
     
      while(1)
      {
        if(list_empty(&(curproc->p_children)))
        {
           break;
        } 
        else
          sched_switch();

        /*CHECK FOR THE CONDITION IF IT WORKS FINE*/
      }
  }

   dbg(DBG_THR,"CLEANING UP PROCESS\n");

  /*CLOSING ALL THE FILES*/

  file_t *cleaner;
  
  int i;
  
  for(i = 0; i < NFILES ; i++)
  {
    cleaner = curproc->p_files[i];
    
    if(cleaner)
      {
        dbg_print("VPUT THE VNODE\n");
        
        (void)do_close(i);

      }
  
  }

  vput(curproc->p_cwd);
  
/*Reparenting all process children*/
  list_iterate_begin(&curproc->p_children, p, proc_t, p_child_link) {

    if (list_link_is_linked(&(p->p_child_link))){
                list_remove(&(p->p_child_link));
                p->p_pproc = proc_initproc;
                dbg_print("REPARENTING PROCESS %s TO INIT\n", p->p_comm);
            list_insert_before(&(proc_initproc->p_children), &(p->p_child_link));}
        } list_iterate_end();


/*setting state of a process      */
  curproc->p_state =  PROC_DEAD;
  curproc->p_status = status;

/*wake up parent */
kthread_t *t;

list_iterate_begin(&(curproc->p_pproc->p_threads), t, kthread_t, kt_plink) {
                
          if(t->kt_state == KT_SLEEP)
          {
            dbg(DBG_THR,"WAKING UP PARENT\n");           
            sched_wakeup_on(&(curproc->p_pproc->p_wait));
            
          }

    }list_iterate_end();
}

/*
 * This has nothing to do with signals and kill(1).
 *
 * Calling this on the current process is equivalent to calling
 * do_exit().
 *
 * In Weenix, this is only called from proc_kill_all.
 */
void
proc_kill(proc_t *p, int status)
{

  kthread_t *t;
       
  if( p == curproc )
    do_exit(status);

  else
  {
    list_iterate_begin(&(p->p_threads), t, kthread_t, kt_plink) {
                
          if(t->kt_state != KT_EXITED)
          {
            kthread_cancel(t, (void*) &status);
          }
    }list_iterate_end();
         /*NOT_YET_IMPLEMENTED("PROCS: proc_kill");*/
  }

}


/*
 * Remember, proc_kill on the current process will _NOT_ return.
 * Don't kill direct children of the idle process.
 *
 * In Weenix, this is only called by sys_halt.
 */

void
proc_kill_all()
{
        /*NOT_YET_IMPLEMENTED("PROCS: proc_kill_all");*/

  proc_t *p;
  kthread_t *old_thr, *init, *t;

  list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                if (p->p_pid != PID_INIT && p->p_pid != PID_IDLE && p->p_pproc->p_pid != PID_IDLE) 
                { 
                        p->p_state = PROC_DEAD;
                        list_iterate_begin(&(p->p_threads), t, kthread_t, kt_plink) {
                        
                        t->kt_state = KT_EXITED;    
            
                    }list_iterate_end();
                }
                if(p->p_pid == PID_INIT)
                {

                    list_iterate_begin(&(p->p_threads), t, kthread_t, kt_plink) {
                        
                        init = t;    
            
                    }list_iterate_end();

                }
  } list_iterate_end();
  
  old_thr = curthr;
  curthr = init;
  curproc = init->kt_proc;
  
  if(old_thr != init)
  {
    /*list_remove(curthr->kt_wchan->tq_list.l_next);
    list_remove(curthr->kt_wchan->tq_list.l_prev);
    */
    list_remove(&(curthr->kt_qlink));
    curthr->kt_wchan = NULL;
  }

  context_switch(&(old_thr->kt_ctx), &(curthr->kt_ctx));

  return;
}

proc_t *
proc_lookup(int pid)
{
        proc_t *p;
        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                if (p->p_pid == pid) {
                        return p;
                }
        } list_iterate_end();
        return NULL;
}

list_t *
proc_list()
{
        return &_proc_list;
}

/*
 * This function is only called from kthread_exit.
 *
 * Unless you are implementing MTP, this just means that the process
 * needs to be cleaned up and a new thread needs to be scheduled to
 * run. If you are implementing MTP, a single thread exiting does not
 * necessarily mean that the process should be exited.
 */
void
proc_thread_exited(void *retval)
{
        
        /*remove the thread from parent's list of threads.*/
     
         proc_cleanup(*((int *)retval)); 
          
           
}

/* If pid is -1 dispose of one of the exited children of the current
 * process and return its exit status in the status argument, or if
 * all children of this process are still running, then this function
 * blocks on its own p_wait queue until one exits.
 *
 * If pid is greater than 0 and the given pid is a child of the
 * current process then wait for the given pid to exit and dispose
 * of it.
 *
 * If the current process has no children, or the given pid is not
 * a child of the current process return -ECHILD.
 *
 * Pids other than -1 and positive numbers are not supported.
 * Options other than 0 are not supported.
 */
pid_t
do_waitpid(pid_t pid, int options, int *status)
{
/*        NOT_YET_IMPLEMENTED("PROCS: do_waitpid");*/
  proc_t *p = NULL;
  int count = 0;
  status = status;

  if(p == proc_lookup(pid))
      KASSERT(-1 == pid || p->p_pid == pid);

  p = NULL;

  if(options != 0 && pid < -1)
      return  -ECHILD;      
  
  if (pid == -1)
  {

     
      if(list_empty(&(curproc->p_children)))
      {
          return -ECHILD;
      }   
  
      for(;;)
      {

          count = 0;  

          if(list_empty(&(curproc->p_children)))
          {
                break;
          } 

          list_iterate_begin(&(curproc->p_children), p, proc_t, p_child_link) {
                KASSERT(NULL != p);
                KASSERT(NULL != p->p_pagedir);
                if (p->p_state == PROC_DEAD){ 
                  count = count + 1;
                  pid = p->p_pid;
                  kthread_t *thr;
                  status = &(p->p_status);

                  if (list_link_is_linked(&(p->p_child_link))){
                          list_remove(&(p->p_child_link));
                          
                          proc_t *p2;
          list_iterate_begin(&(p->p_children), p2, proc_t, p_child_link){

                      p2->p_pproc = proc_initproc;

                      list_insert_before(&(proc_initproc->p_children), &(p2->p_child_link));
                  
                   } list_iterate_end(); 
                          
                      
                  }

                  list_iterate_begin(&(p->p_threads), thr, kthread_t, kt_plink) {
                      if (thr->kt_state == KT_EXITED) 
                      {
                          KASSERT(KT_EXITED == thr->kt_state);
                          kthread_destroy(thr); 
                          goto haha;
                      }
                  } list_iterate_end();   
                }   
          }list_iterate_end();
haha:  
          
          if ( count == 0 )
          { 
               sched_sleep_on(&(curproc->p_wait)); 
          }
          else if (count != 0 )
          {
             if (list_link_is_linked(&(p->p_list_link)))
                    list_remove(&(p->p_list_link));
              if (list_link_is_linked(&(p->p_child_link)))
                    list_remove(&(p->p_child_link)); 

              dbg_print("FREEING RESOURCES FOR PROCESS %s\n", p->p_comm);        
             slab_obj_free(proc_allocator,p);
             break;
          } 

      }
  }
  else
  {
      p = proc_lookup(pid);
      
      if (p->p_pproc != curproc)
            return -ECHILD;   

        /*check if child belongs to the set of children*/
        count = 0; 
        list_iterate_begin(&(curproc->p_children), p, proc_t, p_child_link) {
        
                if (p->p_pid == pid)
                { 
                   count = 1;                    
                   break;    
                }

        } list_iterate_end(); 

       if (count == 0 ) /*// pid does not belong to these children*/
       {
            return -ECHILD;       
       }
       
       for(;;)
       {
         count = 0;

            if (p->p_state == PROC_DEAD)
            { 
                  count = 1;  
                  kthread_t *thr;


                  if (list_link_is_linked(&(p->p_child_link)))
                  {
                          list_remove(&(p->p_child_link));
                          
                          proc_t *p2;
                      list_iterate_begin(&(p->p_children), p2, proc_t, p_child_link){

                            p2->p_pproc = proc_initproc;

                      list_insert_before(&(proc_initproc->p_children), &(p2->p_child_link));
                  
                   } list_iterate_end(); 
                  }  
            }            
           if (count == 0)
           {            
                sched_sleep_on(&(curproc->p_wait));
                /*sched_switch();*/   /*// thread continues from here after it is switched back in by some other thread*/
           }
          else if (count == 1)
          {
             break;
          } 
      }
    
      kthread_t *thr; 
      status = &(p->p_status);
      list_iterate_begin(&(p->p_threads), thr, kthread_t, kt_plink) {
                    if (thr->kt_state == KT_EXITED)
                        kthread_destroy(thr);  
                   } list_iterate_end();
          
       if (list_link_is_linked(&(p->p_list_link)))
                    list_remove(&(p->p_list_link));
       if (list_link_is_linked(&(p->p_child_link)))
                    list_remove(&(p->p_child_link));         

             dbg_print("FREEING RESOURCES FOR PROCESS %s\n", p->p_comm); 
             slab_obj_free(proc_allocator,p);         
    }   
        return pid;
}

/*
 * Cancel all threads, join with them, and exit from the current
 * thread.
 *
 * @param status the exit status of the process
 */
void
do_exit(int status)
{
/*if mtp puy in loop      */
  kthread_cancel(curthr, (void*) &status );
}

size_t
proc_info(const void *arg, char *buf, size_t osize)
{
        const proc_t *p = (proc_t *) arg;
        size_t size = osize;
        proc_t *child;

        KASSERT(NULL != p);
        KASSERT(NULL != buf);

        iprintf(&buf, &size, "pid:          %i\n", p->p_pid);
        iprintf(&buf, &size, "name:         %s\n", p->p_comm);
        if (NULL != p->p_pproc) {
                iprintf(&buf, &size, "parent:       %i (%s)\n",
                        p->p_pproc->p_pid, p->p_pproc->p_comm);
        } else {
                iprintf(&buf, &size, "parent:       -\n");
        }

#ifdef __MTP__
        int count = 0;
        kthread_t *kthr;
        list_iterate_begin(&p->p_threads, kthr, kthread_t, kt_plink) {
                ++count;
        } list_iterate_end();
        iprintf(&buf, &size, "thread count: %i\n", count);
#endif

        if (list_empty(&p->p_children)) {
                iprintf(&buf, &size, "children:     -\n");
        } else {
                iprintf(&buf, &size, "children:\n");
        }
        list_iterate_begin(&p->p_children, child, proc_t, p_child_link) {
                iprintf(&buf, &size, "     %i (%s)\n", child->p_pid, child->p_comm);
        } list_iterate_end();

        iprintf(&buf, &size, "status:       %i\n", p->p_status);
        iprintf(&buf, &size, "state:        %i\n", p->p_state);

#ifdef __VFS__
#ifdef __GETCWD__
        if (NULL != p->p_cwd) {
                char cwd[256];
                lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                iprintf(&buf, &size, "cwd:          %-s\n", cwd);
        } else {
                iprintf(&buf, &size, "cwd:          -\n");
        }
#endif /* __GETCWD__ */
#endif

#ifdef __VM__
        iprintf(&buf, &size, "start brk:    0x%p\n", p->p_start_brk);
        iprintf(&buf, &size, "brk:          0x%p\n", p->p_brk);
#endif

        return size;
}

size_t
proc_list_info(const void *arg, char *buf, size_t osize)
{
        size_t size = osize;
        proc_t *p;

        KASSERT(NULL == arg);
        KASSERT(NULL != buf);

#if defined(__VFS__) && defined(__GETCWD__)
        iprintf(&buf, &size, "%5s %-13s %-18s %-s\n", "PID", "NAME", "PARENT", "CWD");
#else
        iprintf(&buf, &size, "%5s %-13s %-s\n", "PID", "NAME", "PARENT");
#endif

        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
                char parent[64];
                if (NULL != p->p_pproc) {
                        snprintf(parent, sizeof(parent),
                                 "%3i (%s)", p->p_pproc->p_pid, p->p_pproc->p_comm);
                } else {
                        snprintf(parent, sizeof(parent), "  -");
                }

#if defined(__VFS__) && defined(__GETCWD__)
                if (NULL != p->p_cwd) {
                        char cwd[256];
                        lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
                        iprintf(&buf, &size, " %3i  %-13s %-18s %-s\n",
                                p->p_pid, p->p_comm, parent, cwd);
                } else {
                        iprintf(&buf, &size, " %3i  %-13s %-18s -\n",
                                p->p_pid, p->p_comm, parent);
                }
#else
                iprintf(&buf, &size, " %3i  %-13s %-s\n",
                        p->p_pid, p->p_comm, parent);
#endif
        } list_iterate_end();
        return size;
}
