#include "types.h"
#include "globals.h"
#include "kernel.h"

#include "util/gdb.h"
#include "util/init.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pagetable.h"
#include "mm/pframe.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/cpuid.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/tty/virtterm.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"

#include "test/kshell/kshell.h"
extern int vfstest_main(int , char **);

GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);
static void       hard_shutdown(void);
static context_t bootstrap_context;


int x = 0;

/**
 * This is the first real C function ever called. It performs a lot of
 * hardware-specific initialization, then creates a pseudo-context to
 * execute the bootstrap function in.
 */
void
kmain()
{
        GDB_CALL_HOOK(boot);

        dbg_init();
        dbgq(DBG_CORE, "Kernel binary:\n");
        dbgq(DBG_CORE, "  text: 0x%p-0x%p\n", &kernel_start_text, &kernel_end_text);
        dbgq(DBG_CORE, "  data: 0x%p-0x%p\n", &kernel_start_data, &kernel_end_data);
        dbgq(DBG_CORE, "  bss:  0x%p-0x%p\n", &kernel_start_bss, &kernel_end_bss);

        page_init();

        pt_init();
        slab_init();
        pframe_init();

        acpi_init();
        apic_init();
        intr_init();

        gdt_init();

        /* initialize slab allocators */
#ifdef __VM__
        anon_init();
        shadow_init();
#endif
        vmmap_init();
        proc_init();
        kthread_init();

#ifdef __DRIVERS__
        bytedev_init();
        blockdev_init();
#endif

        void *bstack = page_alloc();
        pagedir_t *bpdir = pt_get();
        KASSERT(NULL != bstack && "Ran out of memory while booting.");
        context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
        context_make_active(&bootstrap_context);

        panic("\nReturned to kmain()!!!\n");
}

/**
 * This function is called from kmain, however it is not running in a
 * thread context yet. It should create the idle process which will
 * start executing idleproc_run() in a real thread context.  To start
 * executing in the new process's context call context_make_active(),
 * passing in the appropriate context. This function should _NOT_
 * return.
 *
 * Note: Don't forget to set curproc and curthr appropriately.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
bootstrap(int arg1, void *arg2)
{
        /* necessary to finalize page table information */
        pt_template_init();

        /*NOT_YET_IMPLEMENTED("PROCS: bootstrap");*/


                proc_t  * p = proc_create("IDLE");
                curproc = p;
                kthread_t *t = kthread_create(p, idleproc_run , arg1, arg2);  
                curthr =  t;
                
                KASSERT(NULL != curproc);
                KASSERT(PID_IDLE == curproc->p_pid);
                KASSERT(NULL != curthr);

                context_make_active(&(t->kt_ctx)); 

                panic("weenix returned to bootstrap()!!! BAD!!!\n");
                
                return NULL;
}

/**
 * Once we're inside of idleproc_run(), we are executing in the context of the
 * first process-- a real context, so we can finally begin running
 * meaningful code.
 *
 * This is the body of process 0. It should initialize all that we didn't
 * already initialize in kmain(), launch the init process (initproc_run),
 * wait for the init process to exit, then halt the machine.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
idleproc_run(int arg1, void *arg2)
{
        int status;
        pid_t child;

        /* create init proc */

        kthread_t *initthr = initproc_create();
        
        init_call_all();

        GDB_CALL_HOOK(initialized);

        /* Create other kernel threads (in order) */

#ifdef __VFS__
        /* Once you have VFS remember to set the current working directory
         * of the idle and init processes */

        proc_t *p_set = proc_lookup(0);
        p_set->p_cwd = vfs_root_vn;
        vref(vfs_root_vn);
        /* Here you need to make the null, zero, and tty devices using mknod */
        /* You can't do this until you have VFS, check the include/drivers/dev.h
         * file for macros with the device ID's you will need to pass to mknod */

        KASSERT(-1 != do_mkdir("/dev"));
        /*do_chdir("dev");*/

        /*FOR NULL*/
        KASSERT(-1 != do_mknod("/dev/null", S_IFCHR, MKDEVID(1, 0)));
        dbg_print("/dev/null CREATED\n");
        /*FOR ZERO*/
        KASSERT(-1 != do_mknod("/dev/zero", S_IFCHR, MKDEVID(1, 1)));
        dbg_print("/dev/zero CREATED\n");
        /*FOR TTY*/
        KASSERT(-1 != do_mknod("/dev/tty0", S_IFCHR, MKDEVID(2, 0)));
        dbg_print("/dev/tty0 CREATED\n");

#endif

        /* Finally, enable interrupts (we want to make sure interrupts
         * are enabled AFTER all drivers are initialized) */
        intr_enable();

        /* Run initproc */
        sched_make_runnable(initthr);
        /* Now wait for it */

        child = do_waitpid(-1, 0, &status);
        KASSERT(PID_INIT == child);

#ifdef __MTP__
        kthread_reapd_shutdown();
#endif


#ifdef __VFS__
        /* Shutdown the vfs: */
        dbg_print("weenix: vfs shutdown...\n");
        vput(curproc->p_cwd);
        if (vfs_shutdown())
                panic("vfs shutdown FAILED!!\n");

#endif

        /* Shutdown the pframe system */
#ifdef __S5FS__
        pframe_shutdown();
#endif

        dbg_print("\nweenix: halted cleanly!\n");
        GDB_CALL_HOOK(shutdown);
        hard_shutdown();
        return NULL;
}

/**
 * This function, called by the idle process (within 'idleproc_run'), creates the
 * process commonly refered to as the "init" process, which should have PID 1.
 *
 * The init process should contain a thread which begins execution in
 * initproc_run().
 *
 * @return a pointer to a newly created thread which will execute
 * initproc_run when it begins executing
 */
static kthread_t *
initproc_create(void)
{
           int arg1 = 0;
           void *arg2 = NULL;
           int status;
           proc_t  * p = proc_create("INIT");
           
           KASSERT(NULL != p);
           KASSERT(PID_INIT == p->p_pid);

           kthread_t *t = kthread_create(p, initproc_run , arg1, arg2); 
           KASSERT(t != NULL); 

           return t;      
}

/**
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/bin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
 
 static void *
 tester_func(int argc, char **argv)
 {
    vfstest_main(1, NULL);


  /*  do_mkdir("ryan");
    do_chdir("ryan");
*/    
    dbg_print("EXITED FROM VFSTEST\n");
    do_exit(0);

    return NULL;
 }


static void *
initproc_run(int arg1, void *arg2)
{  
        proc_t *p_set = proc_lookup(1);
        p_set->p_cwd = vfs_root_vn;
        vref(vfs_root_vn);

        dbg_print("STARTED RUNNING INIT PROCESS\n");

        /*KSHELL*/
        int err = 0;
        kshell_t *ksh = kshell_create(0);
        kshell_add_command("test", tester_func, "Tests vfstest.c");
        KASSERT(ksh && "did not create a kernel shell as expected");
        while ((err = kshell_execute_next(ksh)) > 0);
        KASSERT(err == 0 && "kernel shell exited with an error\n");
        
        kshell_destroy(ksh);

           /*int status;
           proc_t  * p = proc_create("TESTER");

           kthread_t *t = kthread_create(p, (kthread_func_t)tester_func , 1, NULL); 
           KASSERT(t != NULL);
            sched_make_runnable(t);
        
        do_waitpid(-1, 0, &status);*/
        
        proc_kill(curproc, 0); /*EXIT INIT_PROC WITH STATUS = 0*/
        
        return NULL;
}



/**
 * Clears all interrupts and halts, meaning that we will never run
 * again.
 */
static void
hard_shutdown()
{
#ifdef __DRIVERS__
        vt_print_shutdown();
#endif
        __asm__ volatile("cli; hlt");
}
