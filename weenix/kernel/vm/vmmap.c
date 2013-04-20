#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/mmobj.h"

static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{
        vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
        KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
        vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
        KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
        vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
        if (newvma) {
                newvma->vma_vmmap = NULL;
        }
        return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
        KASSERT(NULL != vma);
        slab_obj_free(vmarea_allocator, vma);
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
	vmmap_t *newvmmap = (vmmap_t*)slab_obj_alloc(vmmap_allocator);
	if(newvmmap){
		newvmmap->vmm_proc = NULL;
		list_init(&(newvmmap->vmm_list));
	}
        /*NOT_YET_IMPLEMENTED("VM: vmmap_create");*/
        return newvmmap;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
	KASSERT(NULL!=map);
	if(!list_empty(&(map->vmm_list))){
		vmarea_t * area;
		list_iterate_begin(&(map->vmm_list), area, vmarea_t, vma_plink){
			list_remove(&(area->vma_plink));
		}list_iterate_end();
	}
	map->vmm_proc = NULL;
	slab_obj_free(vmmap_allocator, map);
        /*NOT_YET_IMPLEMENTED("VM: vmmap_destroy");*/
}

/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
	KASSERT(NULL!=map && NULL!=newvma);
	KASSERT(NULL == newvma->vma_vmmap);
	KASSERT(newvma->vma_start < newvma->vma_end);
	KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);

	newvma->vma_vmmap = map;
	int before = 0;

	if(!list_empty(&(map->vmm_list))){
		vmarea_t *area;
		list_iterate_begin(&(map->vmm_list), area, vmarea_t,vma_plink){
			if(area->vma_start > newvma->vma_start){
				before =1;
				break;
			}
		}list_iterate_end();
		if(before){
			list_insert_before(&(area->vma_plink),&(newvma->vma_plink));
		}else{
			list_insert_tail(&(map->vmm_list),&(newvma->vma_plink));
		}
	}else{
		list_insert_head(&(map->vmm_list),&(newvma->vma_plink));
	}

        /*NOT_YET_IMPLEMENTED("VM: vmmap_insert");*/
}

/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range exists.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
	KASSERT(NULL!=map);
	KASSERT(0<npages);
	
	/*Mohit: Assumptions: Every memory region has mmobj_t object
	 * which manages pframes (which in turn deals with physical
	 * pages).
	 * The memory region also consists of virtual memory areas(VMA)
	 * which are defined by start and end vfn. So I am assuming that
	 * if there is any virtual memory address which doesn't have a
	 * corressponding vma will be considered free. i.e, the underlying
	 * physical pages are free.*/
	
	int startvfn = -1, i=-1;
	vmarea_t *area;
	
	if(!list_empty(&(map->vmm_list))){
		if(dir==VMMAP_DIR_HILO){
			list_iterate_reverse(&(map->vmm_list), area, vmarea_t,vma_plink){
				i = vmmap_is_range_empty(map,area->vma_start-(npages),area->vma_start-1);
				if(i==1){
					startvfn = area->vma_start-(npages);
					break;
				}
                	}list_iterate_end();
		}else{
			list_iterate_begin(&(map->vmm_list), area, vmarea_t,vma_plink){
				i = vmmap_is_range_empty(map,area->vma_end+1,area->vma_end+npages);
                                if(i==1){
                                        startvfn = area->vma_end+1;
                                        break;
                                }
			}list_iterate_end();
		}
	}else{
		if(dir==VMMAP_DIR_HILO)
		        startvfn=ADDR_TO_PN(USER_MEM_HIGH)-npages+1;
		else
		        startvfn=ADDR_TO_PN(USER_MEM_LOW);
	}	

        /*NOT_YET_IMPLEMENTED("VM: vmmap_find_range");*/
        return startvfn;
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
	KASSERT(NULL!=map);
	
	vmarea_t *vma = NULL;
	if(!list_empty(&(map->vmm_list))){
		vmarea_t *area;
                list_iterate_begin(&(map->vmm_list), area, vmarea_t, vma_plink){
                        if(area->vma_start <= vfn && area->vma_end >= vfn){
				vma = area;
				break;
			}
                }list_iterate_end();
        }

        /*NOT_YET_IMPLEMENTED("VM: vmmap_lookup");*/
        return vma;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
	vmmap_t *newmap = NULL;
	
	newmap = vmmap_create();
	if(newmap){
		vmarea_t *area, *newarea;
		list_iterate_begin(&(map->vmm_list), area, vmarea_t, vma_plink){
                	newarea = vmarea_alloc();
                 	if(!newarea){
				return NULL;
			}
			newarea->vma_start = area->vma_start;
			newarea->vma_end = area->vma_end;
			newarea->vma_off = area->vma_off;
			newarea->vma_prot = area->vma_prot;
			newarea->vma_flags = area->vma_flags;
			newarea->vma_vmmap = newmap;
			/*not adding mmobj to cloned vmareas*/
			vmmap_insert(newmap, newarea);
                }list_iterate_end();		
	}
	
        /*NOT_YET_IMPLEMENTED("VM: vmmap_clone");*/
        return newmap;
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages, int prot, int flags, off_t off, int dir, vmarea_t **new)
{	KASSERT(NULL != map);
        KASSERT(0 < npages);
        KASSERT(!(~(PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC) & prot));
        KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags));
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage));
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages)));
        KASSERT(PAGE_ALIGNED(off));
	
	int start=0;
	mmobj_t *newmmobj=NULL;
	vmarea_t *newarea=NULL;
	if(lopage==0)
	{
	        start=vmmap_find_range(map,npages,dir);
	        if(start==-1)
	                return -1;
	        newarea=vmarea_alloc();
	        if(newarea==NULL)
	                return -1;
	        else
	        {
	                newarea->vma_start = start;
			newarea->vma_end = start+npages-1;
			newarea->vma_off = off;
			newarea->vma_prot = prot;
			newarea->vma_flags = flags;
			newarea->vma_vmmap = map;
			/*not adding mmobj to cloned vmareas*/
			vmmap_insert(map, newarea);
			if(file)
			{
			       KASSERT(file->vn_ops!=NULL&&file->vn_ops->mmap!=NULL);
			       int i=(file->vn_ops->mmap)(file,newarea,&newmmobj);
			       if(i<0)
			                return i;
			}
			else
			{
			        if(flags!=MAP_PRIVATE)
        	                        newmmobj=anon_create();
        	                else
        	                        newmmobj=shadow_create();
        	                        
			        if(newmmobj==NULL)
			                return -1;
			}
			
			*new=newarea;
	        }
	        
	        
	}
	else
	{
	        if(!vmmap_is_range_empty(map,lopage,npages))
	        {
	                if(vmmap_remove(map,lopage,npages)!=0)
	                        return -1;
	        }
	        newarea=vmarea_alloc();
	        if(newarea==NULL)
	                return -1;
	        else
	        {
	                newarea->vma_start = lopage;
			newarea->vma_end = lopage+npages-1;
			newarea->vma_off = off;
			newarea->vma_prot = prot;
			newarea->vma_flags = flags;
			newarea->vma_vmmap = map;
			/*not adding mmobj to cloned vmareas*/
			vmmap_insert(map, newarea);
			if(file)
			{
			       KASSERT(file->vn_ops!=NULL&&file->vn_ops->mmap!=NULL);
			       int i=(file->vn_ops->mmap)(file,newarea,&newmmobj);
			       if(i<0)
			                return i;
			}
			else
			{
			        if(flags!=MAP_PRIVATE)
        	                        newmmobj=anon_create();
        	                else
        	                        newmmobj=shadow_create();
        	                        
			        if(newmmobj==NULL)
			                return -1;
			}
		}
			
			
	}
	        
	*new=newarea;
       /* NOT_YET_IMPLEMENTED("VM: vmmap_map");*/
        return 0;
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *          [             ]   Existing VM Area
 *        *******             Region to be unmapped
 *
 * Case 1:  [   ******    ]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [      *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****        ]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{
	 if(!list_empty(&(map->vmm_list))){
                vmarea_t *area;
                list_iterate_begin(&(map->vmm_list), area, vmarea_t, vma_plink){
	
			if(area->vma_start < lopage && area->vma_end > lopage+npages-1){
				/*split this vma into two new vma*/
				vmarea_t *newvma = vmarea_alloc();
				newvma->vma_start = lopage+npages;
				newvma->vma_end = area->vma_end;
				area->vma_end = lopage-1;
				newvma->vma_off = area->vma_off + npages;
				newvma->vma_prot = area->vma_prot;
				vmmap_insert(map, newvma);
				newvma->vma_obj = area->vma_obj;	
				if(newvma->vma_obj)
				        (newvma->vma_obj->mmo_ops->ref)(newvma->vma_obj);
				/*Doubt at this point revisit later*/
				
				/*check if there is file object associated with the old vmarea
				*if there is one then increase its refcount by one
				* ----- but HOW????*/
				
			}else if (area->vma_start < lopage && area->vma_end <= lopage+npages-1&&area->vma_end>=lopage){
				area->vma_end = lopage -1;
			}else if (area->vma_start >= lopage && area->vma_end > lopage+npages-1&&area->vma_start <= lopage+npages-1){
				area->vma_off = area->vma_off +lopage+npages-area->vma_start;
				area->vma_start = lopage+npages;
			}else if (area->vma_start >= lopage && area->vma_end <= lopage+npages-1){
				list_remove(&(area->vma_plink));
			}
			else
			{
			        continue;
			}
		
		}list_iterate_end();
	}

        /*NOT_YET_IMPLEMENTED("VM: vmmap_remove");*/
        return 0;
}

/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
	uint32_t endvfn = startvfn+npages-1;
	KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn));
		
	int i=0;

        if(!list_empty(&(map->vmm_list))){
                vmarea_t *area;
                list_iterate_begin(&(map->vmm_list), area, vmarea_t, vma_plink){
                        if(area->vma_start > endvfn || area->vma_end < startvfn){
                                i=1;
                        }else{
				i=0;
				break;
			}
                }list_iterate_end();
        }

	/*NOT_YET_IMPLEMENTED("VM: vmmap_is_range_empty");*/
        return i;
}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing exist.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
	/*uint32_t *vfn = (uint32_t*)vaddr;
	
	if(!list_empty(&(map->vmm_list))){
                vmarea_t *area;
                list_iterate_begin(&(map->vmm_list), area, vmarea_t, vma_plink){
			if(area->vma_start <= *vfn && area->vma_end >=*vfn+count){
				The range resides completely within the area
				int i = 0;
				while(i<count){
					pframe_t *frame;
					pframe_get(area->vma_obj,ADDR_TO_PN(*vfn+i),&frame);
					
					i++;
				}
				break;
			}
		}
	}else{
		return -EFAULT;
	}

	vmarea_t *vma = vmmap_lookup(map,*vfn);
	
	if(vma){
		if(vma->vma_end <= *vfn+count){
			
		}
	}else{
		return -EFAULT;
	}
*/
        /*NOT_YET_IMPLEMENTED("VM: vmmap_read");*/
        return 0;
}

/* Write from 'buf' into the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do this, you will need to find the correct
 * vmareas to write into, then find the correct pframes within those vmareas,
 * and finally write into the physical addresses that those pframes correspond
 * to. You should not check permissions of the areas you use. Assume (KASSERT)
 * that all the areas you are accessing exist. Remember to dirty pages!
 * Returns 0 on success, -errno on error.
 */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{
        NOT_YET_IMPLEMENTED("VM: vmmap_write");
        return 0;
}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
        KASSERT(0 < osize);
        KASSERT(NULL != buf);
        KASSERT(NULL != vmmap);

        vmmap_t *map = (vmmap_t *)vmmap;
        vmarea_t *vma;
        ssize_t size = (ssize_t)osize;

        int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
                           "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
                           "VFN RANGE");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                size -= len;
                buf += len;
                if (0 >= size) {
                        goto end;
                }

                len = snprintf(buf, size,
                               "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
                               vma->vma_start << PAGE_SHIFT,
                               vma->vma_end << PAGE_SHIFT,
                               (vma->vma_prot & PROT_READ ? 'r' : '-'),
                               (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
                               (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
                               (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
                               vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
        } list_iterate_end();

end:
        if (size <= 0) {
                size = osize;
                buf[osize - 1] = '\0';
        }
        /*
        KASSERT(0 <= size);
        if (0 == size) {
                size++;
                buf--;
                buf[0] = '\0';
        }
        */
        return osize - size;
}
