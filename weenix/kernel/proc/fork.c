#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int
do_fork(struct regs *regs)
{

		/* Creating a new process named fork_process */
		proc_t *child_process = proc_create("child_process");
		
		/* Copying the vmmap_t from parent process to child process */
		child_process->p_vmmap = vmmap_clone(curproc->p_vmmap);
		
		/* Remember to check the reference counts on the underlying memory objects */
		
		/* Finding the vm_areas in the parent process that have vma_flags set to 
		private to find private mapping is present or not */
		vmarea_t *parent_vmarea;
		list_iterate_begin(&(curproc->p_vmmap->vmm_list), parent_vmarea, vmarea_t, vma_plink)
		{
			if (parent_vmarea->vma_flags == MAP_PRIVATE)
			{
				/* Creating new shadow objects which in turn point to the original underlying memory
				object */
				mmobj_t *shadowobject_parent = shadow_create();
				
				/* Pointing the shadow object of the parent to the underlying memory object */
				shadowobject_parent = parent_vmarea->vma_obj;
				
				/* Pointing the vm_area of the parent process to the above shadow object */
				parent_vmarea->vma_obj = shadowobject_parent;	
			}
			/*
			else (parent_vmarea->vma_flags == MAP_SHARED)
			{
				DO NOTHING AS SAID IN DOCUMENT
			}
			*/
		}
		list_iterate_end();
		
		/* Finding the vm_areas in the child process that have vma_flags set to 
		private to find private mapping is present or not */
		vmarea_t *child_vmarea;
		list_iterate_begin(&(child_process->p_vmmap->vmm_list), child_vmarea, vmarea_t, vma_plink)
		{
			if (child_vmarea->vma_flags == MAP_PRIVATE)
			{
				/* Creating new shadow objects which in turn point to the original underlying memory
				object */
				mmobj_t *shadowobject_child = shadow_create();
				
				/* Pointing the shadow object of the child to the underlying memory object */
				shadowobject_child = parent_vmarea->vma_obj;
				
				/* Pointing the vm_area of the child process to the above shadow object */
				child_vmarea->vma_obj= shadowobject_child;
			}
			/*
			else (child_vmarea->vma_flags == MAP_SHARED)
			{
				DO NOTHING AS SAID IN DOCUMENT
			}
			*/
		}
		list_iterate_end();
		
		/* Unmap the userland page table entries of the parent process */
		pt_unmap_range(curproc->p_pagedir, 0, 0);
		
		/* Flush the TLB */
		tlb_flush_all();
		
		/* Create a thread of the chilf process */
		kthread_t *child_thread = kthread_create(child_process, NULL, 0, 0);
		/*child_thread->kt_ctx->c_esp = fork_setup_stack(regs, void *kstack)*/
		NOT_YET_IMPLEMENTED("VM: do_fork");

	
	NOT_YET_IMPLEMENTED("VM: do_fork");

        return 0;
}
