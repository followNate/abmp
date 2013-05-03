#include "types.h"
#include "globals.h"
#include "kernel.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/proc.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/pagetable.h"

#include "vm/pagefault.h"
#include "vm/vmmap.h"

/*
 * This gets called by _pt_fault_handler in mm/pagetable.c The
 * calling function has already done a lot of error checking for
 * us. In particular it has checked that we are not page faulting
 * while in kernel mode. Make sure you understand why an
 * unexpected page fault in kernel mode is bad in Weenix. You
 * should probably read the _pt_fault_handler function to get a
 * sense of what it is doing.
 *
 * Before you can do anything you need to find the vmarea that
 * contains the address that was faulted on.Make sure to check
 * the permissions on the area to see if the process has 
 * permission to do [cause].  If either of these checks does not
 * pass kill the offending process, setting its exit status to
 * EFAULT (normally we would send the SIGSEGV signal, however
 * Weenix does not support signals).
 *
 * Now it is time to find the correct page (don't forget
 * about shadow objects, especially copy-on-write magic!). Make
 * sure that if the user writes to the page it will be handled
 * correctly.
 *
 * Finally call pt_map to have the new mapping placed into the
 * appropriate page table.
 *
 * @param vaddr the address that was accessed to cause the fault
 *
 * @param cause this is the type of operation on the memory
 *              address which caused the fault, possible values
 *              can be found in pagefault.h
 */
void
handle_pagefault(uintptr_t vaddr, uint32_t cause)
{
        /* This function is called from mm/pagetable.c
        There are 5 types of faults:-
        1. FAULT_USER-Fault from user space
        2. FAULT_PRESENT-Fault present or not
        3. FAULT_RESERVED-Fault if access is PROT_NONE
        4. FAULT_WRITE-Fault if access is PROT_WRITE
        5. FAULT_EXEC-Fault if access is PROT_EXEC
        NOTE: FAULT_READ cannnot take place in OS
        */
        uint32_t page_addr = (uint32_t)ADDR_TO_PN(vaddr);
     
        vmarea_t *faulted_vmarea= vmmap_lookup(curproc->p_vmmap, ADDR_TO_PN(vaddr));
        mmobj_t *obj = faulted_vmarea->vma_obj;
        dbg_print("== anon object = 0x%p 0x%p 0x%p 0x%p\n ",obj,vaddr,page_addr,faulted_vmarea->vma_start);
        
        if(faulted_vmarea==NULL){ 
                dbg(DBG_TEST,"Null vmarea recieved\n"); 
                proc_kill(curproc, EFAULT); 
                return;
             }
        
        if ((cause & FAULT_WRITE))
        {
                if (!(faulted_vmarea->vma_prot & PROT_WRITE))

                {
				proc_kill(curproc, EFAULT); return;
		}
	}
		/* Check the protection of the vmarea to PROT_EXEC and if cause is
        other than FAULT_EXEC kill the process */
        if (cause & FAULT_EXEC)
        {
			if (!(faulted_vmarea->vma_prot & PROT_EXEC))
			{
				proc_kill(curproc, EFAULT);return;
			}
	}
		
		/* Check the protection of the vmarea to PROT_NONE and if cause is
        other than FAULT_RESERVED kill the process */
        if (cause & FAULT_RESERVED)
        {
			if (faulted_vmarea->vma_prot ==PROT_NONE)
			{
				proc_kill(curproc, EFAULT);return;
			}

	}
	 if (cause & FAULT_PRESENT)
        {
			if (!(faulted_vmarea->vma_prot & PROT_READ))
			{
				proc_kill(curproc, EFAULT);return;
			}
	}	
		/* Finding the correct page physical address */

        dbg_print("gggg\n");
        pframe_t *needed_frm = NULL;
         mmobj_t *area_mmobj = faulted_vmarea->vma_obj;


        pframe_t *pf;
        int i= page_addr-faulted_vmarea->vma_start;
        
        list_iterate_begin(&(area_mmobj->mmo_respages), pf, pframe_t, pf_olink)
        {
             if(i==0)
             break;
             i--;
        

        } list_iterate_end();
        dbg_print("fff %p Ox%p 0x%p\n",(area_mmobj->mmo_nrespages),pf->pf_addr,(USER_MEM_HIGH));
        dbg_print("gggg %d\n", needed_frm);
       /*if(obj->mmo_shadowed == NULL)*/
        
        dbg_print("ssss\n");

                /*int ret=0;
                ret=obj->mmo_ops->lookuppage(obj, needed_frm->pf_pagenum , 1, &needed_frm);
                dbg_print("ssss\n");

                if(ret<0)
                {
                        dbg_print("ERR\n");
                        return;
                }*/
               int ret = pf->pf_obj->mmo_ops->fillpage(obj, pf);
                if(ret<0)
                {
                        dbg_print("ERR\n");
                        return;
                }

                dbg_print("ssss\n");
/*
        else
        {
            shadow_lookuppage(obj, page_addr, 1, &needed_frm);
                shadow_fillpage(obj, needed_frm);
           
        }*/

        
        dbg_print("bbb\n");
    /*pframe_get(obj, vfn, &resFrame);*/

    /*Call pt_map to have the new mapping placed into the appropriate page table.*/

    /*uintptr_t paddr = pt_virt_to_phys((uintptr_t)pf->pf_addr);*/
    dbg_print("bbb\n");

    pagedir_t *pageTable = pt_get();

   
dbg_print("bbb\n");
    uint32_t ptflags = (pf)->pf_flags;

        pt_map(pageTable,(uintptr_t)PAGE_ALIGN_DOWN(vaddr),(uintptr_t)pf->pf_addr, ptflags, ptflags);

        NOT_YET_IMPLEMENTED("VM: handle_pagefault");
}
