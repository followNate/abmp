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

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/shadowd.h"

#define SHADOW_SINGLETON_THRESHOLD 5

int shadow_count = 0; /* for debugging/verification purposes */
#ifdef __SHADOWD__
/*
 * number of shadow objects with a single parent, that is another shadow
 * object in the shadow objects tree(singletons)
 */
static int shadow_singleton_count = 0;
#endif

static slab_allocator_t *shadow_allocator;

static void shadow_ref(mmobj_t *o);
static void shadow_put(mmobj_t *o);
static int  shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  shadow_fillpage(mmobj_t *o, pframe_t *pf);
static int  shadow_dirtypage(mmobj_t *o, pframe_t *pf);
static int  shadow_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t shadow_mmobj_ops = {
        .ref = shadow_ref,
        .put = shadow_put,
        .lookuppage = shadow_lookuppage,
        .fillpage  = shadow_fillpage,
        .dirtypage = shadow_dirtypage,
        .cleanpage = shadow_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * shadow page sub system. Currently it only initializes the
 * shadow_allocator object.
 */
void
shadow_init()
{
        shadow_allocator = slab_allocator_create("sahdowobj", sizeof(mmobj_t));
        KASSERT(NULL != shadow_allocator && "failed to create shadowobj allocator!\n");
/*        NOT_YET_IMPLEMENTED("VM: shadow_init");*/
}

/*
 * You'll want to use the shadow_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
shadow_create()
{
        mmobj_t *new_shadow_obj = (mmobj_t*)slab_obj_alloc(shadow_allocator);
	if(new_shadow_obj)
	{
	        mmobj_init(new_shadow_obj,&shadow_mmobj_ops);
/*	        (new_shadow_obj)->mmo_ops = (shadow_mmobj_ops);
        	new_shadow_obj->mmo_refcount = 0;
        	new_shadow_obj->mmo_nrespages = 0;
        	list_init(&(new_shadow_obj->mmo_respages)); 
        	new_shadow_obj->mmo_shadowed = NULL; */
        	(new_shadow_obj)->mmo_un.mmo_bottom_obj = NULL;
        	new_shadow_obj->mmo_refcount++;
        }
        	
    /*  NOT_YET_IMPLEMENTED("VM: shadow_create");  */
        return new_shadow_obj;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
shadow_ref(mmobj_t *o)
{
        KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
dbg(DBG_VNREF,"before shadow_ref: object = 0x%p , reference_count =%d, nrespages=%d\n",o,o->mmo_refcount,o->mmo_nrespages);
        o->mmo_refcount++;
dbg(DBG_VNREF,"after shadow_ref: object = 0x%p , reference_count =%d, nrespages=%d\n",o,o->mmo_refcount,o->mmo_nrespages);
     /* NOT_YET_IMPLEMENTED("VM: shadow_ref"); */
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is a shadow object, it will never
 * be used again. You should unpin and uncache all of the object's
 * pages and then free the object itself.
 */
static void
shadow_put(mmobj_t *o)
{
        KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));

dbg(DBG_VNREF,"before shadow_put: object = 0x%p , reference_count =%d, nrespages=%d\n",o,o->mmo_refcount,o->mmo_nrespages);
  
        if(o->mmo_nrespages == (o->mmo_refcount - 1))  
              {
                        /* Object has only one parent , look for all resident pages and clear one by one */
                    pframe_t *pf;
                    if(!(list_empty(&(o->mmo_respages))))
                      {  
                         list_iterate_begin(&(o->mmo_respages), pf, pframe_t, pf_olink)
                            {
                                  /* unpin, check busy, check dirty: cleanup -- and free */
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
                 slab_obj_free(shadow_allocator, o);
              }
        
/*        NOT_YET_IMPLEMENTED("VM: shadow_put");*/
}

/* This function looks up the given page in this shadow object. The
 * forwrite argument is true if the page is being looked up for
 * writing, false if it is being looked up for reading. This function
 * must handle all do-not-copy-on-not-write magic (i.e. when forwrite
 * is false find the first shadow object in the chain which has the
 * given page resident). copy-on-write magic (necessary when forwrite
 * is true) is handled in shadow_fillpage, not here. */
static int
shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
        
dbg(DBG_VNREF,"lookuppage: searching for object: 0x%p, pagenum: %d, with forwrite: %d \n",o,pagenum,forwrite);
        /* need to call pframe_get_resident for each frame in the object */
        mmobj_t *temp = o; pframe_t *pg_frame; int flag=0;
     
            if(!forwrite) /* Readonly --- look for nearest object with this page */
               {
                 if(o->mmo_shadowed)  /* object is shadow object */
                  {
                     while( temp->mmo_shadowed != NULL )
                       {
                          pg_frame = pframe_get_resident(temp,pagenum);
                            if(pg_frame)
                             {
                               while(!pframe_is_busy(pg_frame))
                                  {
                                    *pf = pg_frame;
                                     flag=1; break;
                                  }
                             }
                       if(flag)break;
                       temp = temp->mmo_shadowed;                       
                       }
                       if(!flag) /* look into the bottom most object of the chain*/
                       {
                         temp = mmobj_bottom_obj(o);
                         pg_frame = pframe_get_resident(temp,pagenum);
                            if(pg_frame)
                             {
                               while(!pframe_is_busy(pg_frame))
                                  {
                                    *pf = pg_frame;
                                     break;
                                  }
                             }
                       }
                  }
                  if(!(o->mmo_shadowed)) 
                      {   /* object is not shadow object */
                          pg_frame = pframe_get_resident(o,pagenum);
                          if(pg_frame)
                            {
                                while(!pframe_is_busy(pg_frame))
                                    {
                                       *pf = pg_frame;
                                        break;
                                    }
                            }
                       }
               }
            
            else{ /* TO DO may need to call shadow_fillpage  s*/ }
        
       if(*pf){
dbg(DBG_VNREF,"lookuppage: Returning page frame with pf->pf_obj: 0x%p, pf->pf_pagenum: %d, with forwrite: %d \n",o,pagenum,forwrite);
       return 0; 
       }
       else{
dbg(DBG_VNREF,"lookuppage: Could not find the page frame \n");
        return -1;
        }
        
/*        NOT_YET_IMPLEMENTED("VM: shadow_lookuppage");*/

}

/* As per the specification in mmobj.h, fill the page frame starting
 * at address pf->pf_addr with the contents of the page identified by
 * pf->pf_obj and pf->pf_pagenum. This function handles all
 * copy-on-write magic (i.e. if there is a shadow object which has
 * data for the pf->pf_pagenum-th page then we should take that data,
 * if no such shadow object exists we need to follow the chain of
 * shadow objects all the way to the bottom object and take the data
 * for the pf->pf_pagenum-th page from the last object in the chain).
 * page should be busy while filling */
static int
shadow_fillpage(mmobj_t *o, pframe_t *pf)
{
        pframe_t *src_pf, *dest_pf;
        KASSERT(pframe_is_busy(pf));
        KASSERT(!pframe_is_pinned(pf));
	dbg(DBG_VNREF,"Fillpage: destinaiton object: 0x%p, source pf->pf_obj: 0%p, pf->pf_pagenum: %d\n",o,pf->pf_obj,pf->pf_pagenum);
        /* look for the source page frame */
        int ret = shadow_lookuppage(pf->pf_obj,pf->pf_pagenum,0,&src_pf);
        /* allocate new page for given object */
        KASSERT(ret && "FILLPAGE: Could not find the source page frame\n");
        
	ret = pframe_get(o,pf->pf_pagenum,&dest_pf); /* pframe_get also fills page if allocated new one not sure how! */
        KASSERT(ret && "FILLPAGE: Could not get the destination page frame\n");
          
        /* TO DO not sure how to copy from source to dest ! may be as follows.*/
        dest_pf->pf_addr = src_pf->pf_addr;
          
 	/* NOT_YET_IMPLEMENTED("VM: shadow_fillpage");*/
        return 0;
}

/* These next two functions are not difficult. */

static int
shadow_dirtypage(mmobj_t *o, pframe_t *pf)
{
dbg(DBG_VNREF,"Fillpage: destinaiton object: 0x%p, source pf->pf_obj: 0%p, pf->pf_pagenum: %d\n",o,pf->pf_obj,pf->pf_pagenum);
        if(pframe_is_busy(pf))
               pframe_clear_busy(pf);
        if(!(pframe_is_dirty(pf)))
               pframe_set_dirty(pf);
   /*NOT_YET_IMPLEMENTED("VM: shadow_dirtypage");*/
   if(pframe_is_dirty(pf))
        return 0;
   else return -1;
}

static int
shadow_cleanpage(mmobj_t *o, pframe_t *pf)
{
        pframe_t *src_pf, *dest_pf;
        
        int ret = pframe_get(o,pf->pf_pagenum,&src_pf);
        KASSERT(ret && "CLEANUP: Could not find the source page frame\n");
        ret = shadow_lookuppage(pf->pf_obj,pf->pf_pagenum,0,&dest_pf);
        KASSERT(ret && "CLEANUP: Could not find the destination page frame\n");
        while(pframe_is_pinned(src_pf))
                pframe_unpin(src_pf);
        while(!pframe_is_busy(src_pf))
              {        
                if(pframe_is_dirty(src_pf))
                        pframe_clear_dirty(src_pf);
                dest_pf->pf_addr = src_pf->pf_addr;
                pframe_free(src_pf);
                break;
              }
       /* NOT_YET_IMPLEMENTED("VM: shadow_cleanpage");*/
        return 0;
}
