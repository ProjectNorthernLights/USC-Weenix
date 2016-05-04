#include "globals.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/kthread.h"
#include "proc/kmutex.h"
#include "main/interrupt.h"

/*
 * IMPORTANT: Mutexes can _NEVER_ be locked or unlocked from an
 * interrupt context. Mutexes are _ONLY_ lock or unlocked from a
 * thread context.
 */

void
kmutex_init(kmutex_t *mtx)
{
   /*
   Here we're initialising the mutex by setting the queue to be empty and the mutex holder to be NULL
   */
   
   if(mtx != NULL)
   {
		             
         sched_queue_init(&(mtx->km_waitq));  
         mtx->km_holder = NULL;   
   }
   else
		panic("Undefined reference to mutex\n");

}

/*
 * This should block the current thread (by sleeping on the mutex's
 * wait queue) if the mutex is already taken.
 *
 * No thread should ever try to lock a mutex it already has locked.
 */

void
kmutex_lock(kmutex_t *mtx)
{
  uint8_t curr_intr_level = apic_getipl();
   apic_setipl(IPL_HIGH);

   KASSERT(curthr && (curthr != mtx->km_holder));
   
   if(mtx->km_holder == NULL)
   {
         mtx->km_holder = curthr;
    }
   else
   {
          dbg_print("Mutex already latched by another process. %s sleeping on the Mutex queue\n",curproc->p_comm);
          sched_sleep_on(&(mtx->km_waitq));
   }

    apic_setipl(curr_intr_level);

}

/*
 * This should do the same as kmutex_lock, but use a cancellable sleep
 * instead.
 */

int
kmutex_lock_cancellable(kmutex_t *mtx)
{
        uint8_t curr_intr_level = apic_getipl();
        apic_setipl(IPL_HIGH);

        KASSERT(curthr && (curthr != mtx->km_holder));

        if(mtx->km_holder == NULL)
        {  
         mtx->km_holder = curthr;
        }
        else
         {          
          dbg_print("Mutex already latched by another process. %s sleeping on the Mutex queue in a cancellable sleep state\n",curproc->p_comm);
          sched_cancellable_sleep_on(&(mtx->km_waitq));
          }
        apic_setipl(curr_intr_level);

        return 0;
}

/*
 * If there are any threads waiting to take a lock on the mutex, one
 * should be woken up and given the lock.
 *
 * Note: This should _NOT_ be a blocking operation!
 *
 * Note: Don't forget to add the new owner of the mutex back to the
 * run queue.
 *
 * Note: Make sure that the thread on the head of the mutex's wait
 * queue becomes the new owner of the mutex.
 *
 * @param mtx the mutex to unlock
 */
void
kmutex_unlock(kmutex_t *mtx)
{
   uint8_t curr_intr_level = apic_getipl();
   apic_setipl(IPL_HIGH);

    KASSERT(curthr && (curthr == mtx->km_holder));


    if(mtx->km_holder == NULL)
    {
      dbg_print("The Mutex cannot be unlocked before locking\n");
        apic_setipl(curr_intr_level);

      return;
    }
   mtx->km_holder = sched_wakeup_on(&(mtx->km_waitq));   
     apic_setipl(curr_intr_level);

  KASSERT(curthr != mtx->km_holder);

  return;

}
