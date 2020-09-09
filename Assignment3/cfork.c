//BHAVY SINGH 170212
#include <cfork.h>
#include <page.h>
#include <mmap.h>


void cfork_copy_mm(struct exec_context *chd, struct exec_context *prnt) {

  u64 virt_add;
  void *os_addr;
  struct mm_segment *seg;
  int F = 0;
  chd->pgd = os_pfn_alloc(OS_PT_REG);
  os_addr = osmap(chd->pgd);
  bzero((char *)os_addr, PAGE_SIZE);
  seg = &prnt->mms[MM_SEG_CODE];
  for (virt_add = seg->start; virt_add < seg->next_free; virt_add += PAGE_SIZE) {
    u64 *prnt_pte = get_user_pte(prnt, virt_add, 0);
    if (prnt_pte)
      install_ptable((u64)os_addr, seg, virt_add,
                     (*prnt_pte & FLAG_MASK) >> PAGE_SHIFT);
  }
  // RODATA
  seg = &prnt->mms[MM_SEG_RODATA];
  for (virt_add = seg->start; virt_add < seg->next_free; virt_add += PAGE_SIZE) {
    u64 *prnt_pte = get_user_pte(prnt, virt_add, 0);
    if (prnt_pte)
      install_ptable((u64)os_addr, seg, virt_add,
                     (*prnt_pte & FLAG_MASK) >> PAGE_SHIFT);
  }
  // DATA 
  seg = &prnt->mms[MM_SEG_DATA];
  for (virt_add = seg->start; virt_add < seg->next_free; virt_add += PAGE_SIZE) {
    u64 *prnt_pte = get_user_pte(prnt, virt_add, 0);
    if (prnt_pte) {
        u64 pfn = install_ptable( (u64)os_addr, seg, virt_add,  (*prnt_pte & FLAG_MASK) >> PAGE_SHIFT);  // Returns the blank page
        u64* chd_pte = get_user_pte(chd,virt_add,0);

        *prnt_pte = (*prnt_pte) & (0xFFFFFFFF - 2);
        *chd_pte = (*chd_pte) & (0xFFFFFFFF - 2);
      
        struct pfn_info *chd_pfn_info = get_pfn_info(pfn);
        increment_pfn_info_refcount(chd_pfn_info);
        asm volatile ("invlpg (%0);" 
                                :: "r"(virt_add) 
                                : "memory"); 
    }
  }
  // STACK
  seg = &prnt->mms[MM_SEG_STACK];
  for (virt_add = seg->end - PAGE_SIZE; virt_add >= seg->next_free; virt_add -= PAGE_SIZE) {
    u64 *prnt_pte = get_user_pte(prnt, virt_add, 0);

    if (prnt_pte) {
      u64 pfn = install_ptable((u64)os_addr, seg, virt_add,
                               0);  // Returns the blank page
      pfn = (u64)osmap(pfn);
      memcpy((char *)pfn, (char *)(*prnt_pte & FLAG_MASK), PAGE_SIZE);
    }
  }

  struct vm_area *prnt_vmarea = prnt->vm_area;
  struct vm_area *list_head = prnt_vmarea;
  
  struct vm_area *chd_vmarea;
  struct vm_area *chd_head = NULL;
  
  while (list_head != NULL) {
    struct vm_area *new_area = alloc_vm_area();
    new_area->vm_end = list_head->vm_end;
    new_area->vm_start = list_head->vm_start;
   
    new_area->access_flags = list_head->access_flags;
    new_area->vm_next = NULL;

    chd_head->vm_next = new_area;
    chd_head = new_area;
    if (F == 0) chd_vmarea = new_area;

    for (virt_add = list_head->vm_start; virt_add < list_head->vm_end;virt_add += PAGE_SIZE) {
        u64 *prnt_pte = get_user_pte(prnt, virt_add, 0);

        if (prnt_pte) {
            *prnt_pte &= ~0x2;
            u64 pfn = map_physical_page((u64)osmap(chd->pgd), virt_add, list_head->access_flags,(*prnt_pte & FLAG_MASK) >> PAGE_SHIFT);
            
            u64* chd_pte = get_user_pte(chd,virt_add,0);
            *chd_pte &= ~0x2;
            struct pfn_info *chd_pfn_info = get_pfn_info(pfn);
            increment_pfn_info_refcount(chd_pfn_info);
            asm volatile ("invlpg (%0);" 
                                    :: "r"(virt_add) 
                                    : "memory");
        }
    }
    F = 1;
    list_head = list_head->vm_next;
  }
  chd->vm_area = chd_vmarea;
  copy_os_pts(prnt->pgd, chd->pgd);
  return;
}

/* You need to implement cfork_copy_mm which will be called from do_vfork in
 * entry.c.*/
void vfork_copy_mm(struct exec_context *chd, struct exec_context *prnt) {

    struct mm_segment* prnt_seg;
    u64 ad1;

    prnt_seg = &prnt->mms[MM_SEG_STACK];
    u64 ad = prnt_seg->end - prnt_seg->next_free;
    u64 count_pfn = (prnt_seg->end - prnt_seg->next_free)/PAGE_SIZE;
    u64 area = 2*(prnt_seg->end - prnt->regs.rbp)/PAGE_SIZE;

    if(( (2*(prnt_seg->end - prnt->regs.rbp))%PAGE_SIZE)!=0){
        area++;
    }
    ad1 = prnt_seg->end - PAGE_SIZE;
    for(int i=0;i<(area-count_pfn);i++){
        u64* prnt_pte = get_user_pte(prnt,ad1,0);
        u64 pfn;
        if(prnt_pte)
            pfn = install_ptable((u64)(prnt->pgd),prnt_seg,ad1-ad,0);
        ad1 -= PAGE_SIZE;
        prnt_seg->next_free -= PAGE_SIZE;
    }
    prnt_seg = &prnt->mms[MM_SEG_STACK];
    u64 end_stack = prnt_seg->end;
    u64 offset = end_stack - prnt->regs.rbp;

    for(ad1 = (end_stack -0x8) ;ad1>= prnt->regs.rbp;ad1-=0x8)
        memcpy((char *)(ad1-offset),(char *)(ad1),0x8);

    u64 curr_address = prnt->regs.rbp;

    while(curr_address!=end_stack-0x8){
        u64 current_val = *(u64 *)(curr_address);
        *(u64 *)(curr_address-offset) -= offset;
        curr_address = current_val;
    }

    chd->regs.entry_rsp -= offset;
    chd->regs.rbp -= offset;
    prnt->state = WAITING;
    return;
}

/*You need to implement handle_cow_fault which will be called from do_page_fault
incase of a copy-on-write fault

* For valid acess. Map the physical page
 * Return 1
 *
 * For invalid access,
 * Return -1.
*/

int handle_cow_fault(struct exec_context *current, u64 cr2) {
    struct mm_segment *seg = &current->mms[MM_SEG_DATA];
    if (cr2 < seg->next_free && cr2 >= seg->start) {
        if (seg->access_flags & PROT_WRITE) {
            u64 *pte = get_user_pte(current, cr2, 0);
            u32 shif_pfn = (*pte & FLAG_MASK) >> PAGE_SHIFT;
            struct pfn_info* pfn_info = get_pfn_info(shif_pfn);
            if(get_pfn_info_refcount(pfn_info)<2){
                if(!(*pte & 0x2))
                    *pte |= 0x2;
                return 1;  
            }
            if(pte){
                u64 pfn = map_physical_page((u64)osmap(current->pgd),cr2, seg->access_flags, 0);
                pfn = (u64)osmap(pfn);
                memcpy((char *)pfn, (char *)(*pte & FLAG_MASK), PAGE_SIZE);
            }
            decrement_pfn_info_refcount(pfn_info);
            asm volatile ("invlpg (%0);" 
                                :: "r"(cr2) 
                                : "memory");   // Flush TLB
            return 1;
        }else{
            return -1;
        }

    }
    else if (cr2 < MMAP_AREA_END && cr2 >= MMAP_AREA_START) {
        struct vm_area *list_head = current->vm_area;
        while (!(list_head->vm_start <= cr2 && list_head->vm_end > cr2)){
            list_head = list_head->vm_next;
        }
        if(list_head==NULL){
            return -1;
        }
        if(list_head->access_flags & PROT_WRITE){
            u64 *pte = get_user_pte(current, cr2, 0);
            u32 shif_pfn = (*pte & FLAG_MASK) >> PAGE_SHIFT;
            struct pfn_info* pfn_info = get_pfn_info(shif_pfn);
            if(get_pfn_info_refcount(pfn_info)<2){
                if(!(*pte & 0x2)){
                    *pte |= 0x2;
                }
                return 1;  
            }
            if(pte){
                u64 pfn = map_physical_page((u64)osmap(current->pgd),cr2, list_head->access_flags, 0);
                pfn = (u64)osmap(pfn);
                memcpy((char *)pfn, (char *)(*pte & FLAG_MASK), PAGE_SIZE);
            }
            decrement_pfn_info_refcount(pfn_info);
            asm volatile ("invlpg (%0);" 
                                :: "r"(cr2) 
                                : "memory");
            return 1;
        }
        else{
            return -1;
        }
  }
  return -1;

}

void vfork_exit_handle(struct exec_context *ctx){
	struct exec_context *prnt=get_ctx_by_pid(ctx->ppid);
	struct mm_segment *seg;
    seg = &prnt->mms[MM_SEG_STACK];	
	if(prnt!=NULL && prnt->state==WAITING){
		prnt->state=READY; //checking state for returning chd
		prnt->vm_area=ctx->vm_area; 
		for(int i=0;i<MAX_MM_SEGS-1;i++){
			prnt->mms[i]=ctx->mms[i];
		}
        u64 prnt_pages=(seg->end-prnt->regs.rbp)/PAGE_SIZE;

        if(((seg->end-prnt->regs.rbp)%PAGE_SIZE) !=0) prnt_pages++;
        u64 chd_pages=(seg->end-seg->next_free)/PAGE_SIZE;
        u64 dealloc_pages=chd_pages-prnt_pages;

        for(u64 i=0;i<dealloc_pages;i++){
            do_unmap_user(ctx,seg->next_free);
            seg->next_free=seg->next_free+PAGE_SIZE;
        }
	}
  return ;
}