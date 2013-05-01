#include "globals.h"
#include "errno.h"

#include "util/string.h"
#include "util/debug.h"

#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "mm/slab.h"
#include "mm/tlb.h"

int anon_count = 0; /* for debugging/verification purposes */

static slab_allocator_t *anon_allocator;

static void anon_ref(mmobj_t *o);
static void anon_put(mmobj_t *o);
static int  anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  anon_fillpage(mmobj_t *o, pframe_t *pf);
static int  anon_dirtypage(mmobj_t *o, pframe_t *pf);
static int  anon_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t anon_mmobj_ops = {
        .ref = anon_ref,
        .put = anon_put,
        .lookuppage = anon_lookuppage,
        .fillpage  = anon_fillpage,
        .dirtypage = anon_dirtypage,
        .cleanpage = anon_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * anonymous page sub system. Currently it only initializes the
 * anon_allocator object.
 */
void
anon_init()
{
        anon_allocator = slab_allocator_create("anonobj", sizeof(mmobj_t));
        KASSERT(NULL != anon_allocator && "failed to create anonobj allocator!\n");
/*        NOT_YET_IMPLEMENTED("VM: anon_init");*/
}

/*
 * You'll want to use the anon_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
anon_create()
{
	mmobj_t *new_anon_obj = (mmobj_t*)slab_obj_alloc(anon_allocator);
	if(new_anon_obj){
	   	mmobj_init(new_anon_obj,&anon_mmobj_ops);
		/*MOHIT:setting the refcount to 1 when creating 
		 *anonymous object*/
		new_anon_obj->mmo_refcount++;
	}	
	
	/*NOT_YET_IMPLEMENTED("VM: anon_create");*/
        return new_anon_obj;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
anon_ref(mmobj_t *o)
{
        KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));
	dbg(DBG_VNREF,"before anon_ref: object = 0x%p , reference_count =%d, nrespages=%d\n",o,o->mmo_refcount,o->mmo_nrespages);
        o->mmo_refcount++;
	dbg(DBG_VNREF,"after anon_ref: object = 0x%p , reference_count =%d, nrespages=%d\n",o,o->mmo_refcount,o->mmo_nrespages);
        /*NOT_YET_IMPLEMENTED("VM: anon_ref");*/
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is an anonymous object, it will
 * never be used again. You should unpin and uncache all of the
 * object's pages and then free the object itself.
 */
static void
anon_put(mmobj_t *o)
{
        KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));
	dbg(DBG_VNREF,"before shadow_put: object = 0x%p , reference_count =%d, nrespages=%d\n",o,o->mmo_refcount,o->mmo_nrespages);
        if(o->mmo_nrespages == (o->mmo_refcount - 1))  
              {
                    /* Object has only one parent , look for all 
			*resident pages and clear one by one */
                    pframe_t *pf;
                    if(!(list_empty(&(o->mmo_respages))))
                      {  
                         list_iterate_begin(&(o->mmo_respages), pf, pframe_t, pf_olink)
                            {
                                  /* If page is dirty call cleanup. */
                                while(pframe_is_pinned(pf))
                                        pframe_unpin(pf);
                                if (pframe_is_busy(pf)){
                                       sched_sleep_on(&pf->pf_waitq);
                                } else if (pframe_is_dirty(pf)) {
                                        pframe_clean(pf);
                                } else {
                                        /* it's not busy, it's clean, free it */
                                        pframe_free(pf);
                                }
                             }list_iterate_end();
                      }
                }
        o->mmo_refcount--;
	dbg(DBG_VNREF,"after shadow_put: object = 0x%p , reference_count =%d, nrespages=%d\n",o,o->mmo_refcount,o->mmo_nrespages);
        if(0 == o->mmo_refcount && 0 == o->mmo_nrespages )
        {
                 o = NULL;
                 slab_obj_free(anon_allocator, o);
        }
        
	/* NOT_YET_IMPLEMENTED("VM: shadow_put");*/

}

/* Get the corresponding page from the mmobj. No special handling is
 * required. */
static int
anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
	pframe_t *pg_frame = pframe_get_resident(o,pagenum);
        if(pg_frame)
        {
        	while(!pframe_is_busy(pg_frame))
                {
                	*pf = pg_frame;
                        break;
                }
        }
        /*NOT_YET_IMPLEMENTED("VM: anon_lookuppage");*/
        return 0;
}

/* The following three functions should not be difficult. */
/*@author Mohit Aggarwal*/
static int
anon_fillpage(mmobj_t *o,        pframe_t *pf)
{
 	/*first get the page  pf->pf_obj and pf->pf_pagenum*/
	pframe_t *page = pframe_get_resident(pf->pf_obj,pf->pf_pagenum);
	if(page){
		/*fill the page with data and marked it a pinned*/
		memcpy(pf->pf_addr,page->pf_addr,PAGE_SIZE);
		if(!pframe_is_pinned(page)){
			pframe_pin(page);
		}
	}else{
dbg(DBG_VNREF|DBG_ERROR, "No resident page found for given object = 0x%p and  page number=0x%p",pf->pf_obj,pf->pf_addr);
		return -EFAULT;
	}	
        /*NOT_YET_IMPLEMENTED("VM: anon_fillpage");*/
        return 0;
}

static int
anon_dirtypage(mmobj_t *o, pframe_t *pf)
{
	/*if(pframe_is_busy(pf))
      		pframe_clear_busy(pf);
        if(!(pframe_is_dirty(pf)))
              	pframe_set_dirty(pf);*/
   /*NOT_YET_IMPLEMENTED("VM: shadow_dirtypage");*/
   	/*if(pframe_is_dirty(pf))
        	return 0;
   	else 
		return -1;*/
	/*MOHIT: IMHO the anonymous memory objects doesn't have any file
	 * to read and write, therefore this function should simply return 0*/
	return 0;
}

static int
anon_cleanpage(mmobj_t *o, pframe_t *pf)
{
	/*MOHIT: Even though we are writing the content at pf->pf_addr to the
	 * page identified by the pf->pf_obj and pf->pf_pagenum, we still don't
	 * have to do any preparation to write this data back to disk as 
	 * anonymous memory object doesn't have any associated file. This makes
	 * me wonder whether we really need to do any operation in this function???
	 * open question to all!!!*/
	
	pframe_t *page = pframe_get_resident(pf->pf_obj,pf->pf_pagenum);
	if(page){
		memcpy(page->pf_addr,pf->pf_addr,PAGE_SIZE);
	}else{
		return -EFAULT;
	}
	
        /*NOT_YET_IMPLEMENTED("VM: anon_cleanpage");*/
        return 0;
}
