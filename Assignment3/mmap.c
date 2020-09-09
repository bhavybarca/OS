//170212 BHAVY SINGH
#include <types.h>
#include <mmap.h>
#include<page.h>

struct vm_area *vm_copy(struct vm_area *head){

	struct vm_area *ptr_head, *ptr_temp_head, *ret_temp_ptr;
	struct vm_area *ret_ptr = alloc_vm_area();
	
	ptr_head = head;
	*ret_ptr = *ptr_head;
	ptr_head = head->vm_next;
	ret_temp_ptr = ret_ptr;

	while(ptr_head){
		ptr_temp_head = alloc_vm_area();
		*ptr_temp_head = *ptr_head;
		ret_temp_ptr->vm_next = ptr_temp_head;
		ptr_head = ptr_head->vm_next;
		ret_temp_ptr = ptr_temp_head;
	}
	
	return ret_ptr;
}

void copy_dealloc(struct vm_area *head){
	
	if(head == NULL)
		return;

	struct vm_area *ptr_temp, *ptr_temp1;
	ptr_temp1 = head;
	ptr_temp = head->vm_next;
	while(ptr_temp){
		ptr_temp = ptr_temp1->vm_next;
		dealloc_vm_area(ptr_temp1);
		ptr_temp1 = ptr_temp;
	}
	
}
int vm_area_ctr(struct vm_area * ptr)
{	
	int c = 0;
	while(ptr)
	{	
		ptr = ptr->vm_next;
        c++;
	}
	return c;

}
/**
 * Function will invoked whenever there is page fault. (Lazy allocation)
 * 
 * For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
 */
int vm_area_pagefault(struct exec_context *current, u64 addr, int error_code) // debug done
{   
    int ret_flt = -1;
    struct vm_area* head = current->vm_area;
    struct vm_area* list_ptr = head;

    while( !(list_ptr->vm_start<=addr&&list_ptr->vm_end>=addr) && list_ptr!=NULL){
       list_ptr =  list_ptr->vm_next;
    }

    if(list_ptr!=NULL){

    if(error_code==0x4){
        if(list_ptr->access_flags&PROT_READ || list_ptr->access_flags&PROT_WRITE){
            map_physical_page((u64)osmap(current->pgd),addr,list_ptr->access_flags,0);
            ret_flt = 1;
        }
    }

    if(error_code==0x6){
        if(list_ptr->access_flags&PROT_WRITE){
            map_physical_page((u64)osmap(current->pgd),addr,PROT_WRITE,0);
            ret_flt = 1;
        }
    }

    }
    return ret_flt;
}


/**
 * mprotect System call Implementation.
 */

/**
 * mmap system call implementation.
 */
long vm_area_map_virtual(struct exec_context *current, u64 addr, int length, int prot, int flags)
{

	if(length<=0) {return -1;}
	u64 numpegs = ((u64)length / PAGE_SIZE);

	if (length % PAGE_SIZE != 0)
		numpegs++;
	u64 vm_total_size = numpegs * PAGE_SIZE;

	struct exec_context *ctx = current;
	struct vm_area *virtual_head = current->vm_area;
	struct vm_area *virtual_temp_head = virtual_head;

	long function_retval = -1;
	if (addr != NULL)
	{
		if (virtual_temp_head != NULL)
		{
			struct vm_area *prev_temp = NULL;
			int clash = 0;
			while (virtual_temp_head->vm_end <= addr)
			{ 
				prev_temp = virtual_temp_head;
				virtual_temp_head = virtual_temp_head->vm_next;
				if (virtual_temp_head == NULL)
				{
					break;
				}
			}
			if (virtual_temp_head && (addr + vm_total_size > virtual_temp_head->vm_start ) )
			{
				clash = 1;
			}

			if (clash == 1 && (flags & MAP_FIXED))
				return -1;

			if (clash == 0)
			{
				if (prev_temp == NULL)
				{
					if (virtual_temp_head != NULL  && virtual_temp_head->vm_start == vm_total_size + addr && virtual_temp_head->access_flags == prot)
					{
						virtual_temp_head->vm_start = addr;
						return addr;
					}
					else
					{	if(stats->num_vm_area==128){ return -1;}

						current->vm_area = alloc_vm_area();

						current->vm_area->vm_end = addr + vm_total_size;
						current->vm_area->vm_start = addr;
						current->vm_area->access_flags = prot;
						current->vm_area->vm_next = virtual_temp_head;
                        
						return addr;
					}
				}

				if ( prev_temp->access_flags == prot && prev_temp->vm_end == addr)
				{
					prev_temp->vm_end =  vm_total_size + addr;

					if (virtual_temp_head != NULL && virtual_temp_head->access_flags == prot && prev_temp->vm_end == virtual_temp_head->vm_start )
					{
						prev_temp->vm_end = virtual_temp_head->vm_end;
						prev_temp->vm_next = virtual_temp_head->vm_next;

						dealloc_vm_area(virtual_temp_head);
					}

					function_retval = addr;
				}
				else
				{

					if (virtual_temp_head != NULL && virtual_temp_head->vm_start == addr + vm_total_size && prot == virtual_temp_head->access_flags)
					{
						virtual_temp_head->vm_start = addr;
						function_retval = addr;
					}
					else
					{	if(stats->num_vm_area==128){ return -1;}

						prev_temp->vm_next = alloc_vm_area();

						prev_temp->vm_next->vm_end = addr + vm_total_size;
						prev_temp->vm_next->vm_start = addr;
						prev_temp->vm_next->access_flags = prot;
						prev_temp->vm_next->vm_next = virtual_temp_head;

						function_retval = addr;
					}
				}
				return function_retval;
			}

			if (virtual_temp_head != NULL)
			{
				u64 prev_addr_end = prev_temp == NULL ? MMAP_AREA_START : prev_temp->vm_end;
				u64 next_addr_start = virtual_temp_head == NULL ? MMAP_AREA_END : virtual_temp_head->vm_start;
				while (next_addr_start - prev_addr_end < vm_total_size)
				{
					prev_temp = virtual_temp_head;
					virtual_temp_head = virtual_temp_head->vm_next;
					if (virtual_temp_head == NULL)
					{
						prev_addr_end = prev_temp->vm_end;
						next_addr_start = MMAP_AREA_END;
						break;
					}
					prev_addr_end = prev_temp->vm_end;
					next_addr_start = virtual_temp_head->vm_start;
				}

				
				if (prev_temp == NULL)
				{

					if (next_addr_start - prev_addr_end == vm_total_size && prot == virtual_temp_head->access_flags)
					{
						virtual_temp_head->vm_start = prev_addr_end;
						function_retval = prev_addr_end;
					}

					else
					{
						if(stats->num_vm_area==128){ return -1;}

						current->vm_area = alloc_vm_area();

                        current->vm_area->vm_end = prev_addr_end + vm_total_size;
						current->vm_area->vm_start = prev_addr_end;
						current->vm_area->access_flags = prot;
						current->vm_area->vm_next = virtual_temp_head; 

						function_retval = prev_addr_end;
					}
				}
				
				else if (virtual_temp_head == NULL)
				{

					if (next_addr_start - prev_addr_end < vm_total_size)
					{
						function_retval = vm_area_map_virtual(current, 0, vm_total_size, prot, flags);
					}

					if (prot == prev_temp->access_flags)
					{
						function_retval = prev_temp->vm_end;
						prev_temp->vm_end = prev_temp->vm_end + vm_total_size;
					}
					else
					{	if(stats->num_vm_area==128) {return -1;}

						prev_temp->vm_next = alloc_vm_area();

						prev_temp->vm_next->vm_start = prev_temp->vm_end;
						prev_temp->vm_next->vm_end = prev_temp->vm_end + vm_total_size;
						prev_temp->vm_next->access_flags = prot;
						prev_temp->vm_next->vm_next = NULL;

						function_retval = prev_temp->vm_end;
					}
				}
				else
				{
                    if (next_addr_start - prev_addr_end == vm_total_size)
					{

						
						if (prot == prev_temp->access_flags)
						{
							function_retval = prev_temp->vm_end;
							prev_temp->vm_end = prev_temp->vm_end + vm_total_size; 

							if (prot == virtual_temp_head->access_flags)
							{
								prev_temp->vm_end = virtual_temp_head->vm_end;
								prev_temp->vm_next = virtual_temp_head->vm_next;
								dealloc_vm_area(virtual_temp_head);
							}
						}
						else
						{
							if (prot == virtual_temp_head->access_flags)
							{
								virtual_temp_head->vm_start = prev_temp->vm_end;
								function_retval = virtual_temp_head->vm_start;
							}
							else
							{	if(stats->num_vm_area==128) return -1;
								prev_temp->vm_next = alloc_vm_area();
								prev_temp->vm_next->vm_start = prev_temp->vm_end;
								prev_temp->vm_next->vm_end = prev_temp->vm_end + vm_total_size;
								prev_temp->vm_next->access_flags = prot;
								prev_temp->vm_next->vm_next = virtual_temp_head;
								function_retval = prev_temp->vm_end;
							}
						}
					}
					else if (next_addr_start - prev_addr_end > vm_total_size)
					{

						if (prot == prev_temp->access_flags)
						{
							function_retval = prev_temp->vm_end;
							prev_temp->vm_end = prev_temp->vm_end + vm_total_size; 
						}
						else
						{	if(stats->num_vm_area==128) {return -1;}
							prev_temp->vm_next = alloc_vm_area();

							prev_temp->vm_next->vm_start = prev_temp->vm_end;
							prev_temp->vm_next->vm_end = prev_temp->vm_end + vm_total_size;
							prev_temp->vm_next->access_flags = prot;
							prev_temp->vm_next->vm_next = virtual_temp_head;

							function_retval = prev_temp->vm_end;
						}
					}
					else
					{
						function_retval = vm_area_map_virtual(current, 0, length, prot, flags);
					}
				}

				//end of three cases

			}
            else{
				if (prev_temp->vm_end + vm_total_size > MMAP_AREA_END)
					function_retval = vm_area_map_virtual(current, 0, length, prot, flags);
				//error return -1?
				else
				{	if(stats->num_vm_area==128) return -1;
					prev_temp->vm_next = alloc_vm_area();
					prev_temp->vm_next->vm_start = prev_temp->vm_end;
					prev_temp->vm_next->vm_end = prev_temp->vm_end + vm_total_size;
					prev_temp->vm_next->access_flags = prot;
					prev_temp->vm_next->vm_next = NULL;
					function_retval = prev_temp->vm_end;
				}
			}
			
		}
        else
        {

			if (addr + vm_total_size > MMAP_AREA_END)
			{
				//error or alloc from start
				function_retval = vm_area_map_virtual(current, NULL, length, prot, flags);
			}
			else
			{	if(stats->num_vm_area==128) return -1;
				current->vm_area = alloc_vm_area();
				current->vm_area->vm_start = addr;
				current->vm_area->vm_end = addr + vm_total_size;
				current->vm_area->access_flags = prot;
				current->vm_area->vm_next = NULL;
				function_retval = addr;
			}
		}
		
	}
    else
    {

		if (virtual_head != NULL)
		
		{
			
			u64 prev_addr_end = MMAP_AREA_START;
			u64 next_addr_start = virtual_temp_head->vm_start;
			struct vm_area *prev_temp = NULL;

			while (next_addr_start - prev_addr_end < vm_total_size)
			{

				prev_addr_end = virtual_temp_head->vm_end;
				next_addr_start = virtual_temp_head->vm_next == NULL ? MMAP_AREA_END : virtual_temp_head->vm_next->vm_start;

				prev_temp = virtual_temp_head;
				virtual_temp_head = virtual_temp_head->vm_next;
				if (virtual_temp_head == NULL)
				{
					break;
				}
			}

			if (prev_temp == NULL)
			{
				if (virtual_temp_head->vm_start - MMAP_AREA_START == vm_total_size && prot == virtual_temp_head->access_flags)
				{
					virtual_temp_head->vm_start = MMAP_AREA_START;
					function_retval = MMAP_AREA_START;
				}

				else
				{	if(stats->num_vm_area==128) {return -1;}

					current->vm_area = alloc_vm_area();
					current->vm_area->vm_start = MMAP_AREA_START;
					current->vm_area->vm_end = MMAP_AREA_START + vm_total_size;
					current->vm_area->access_flags = prot;
					current->vm_area->vm_next = virtual_temp_head; 
					function_retval = MMAP_AREA_START;		
				}
			}

			else if (virtual_temp_head == NULL)
			{
				if (MMAP_AREA_END - prev_temp->vm_end < vm_total_size)
					return -1;
				else if (prot == prev_temp->access_flags)
				{
					function_retval = prev_temp->vm_end;
					prev_temp->vm_end = prev_temp->vm_end + vm_total_size;
				}
				else
				{	if(stats->num_vm_area==128) {return -1;}
					prev_temp->vm_next = alloc_vm_area();
					prev_temp->vm_next->vm_start = prev_temp->vm_end;
					prev_temp->vm_next->vm_end = prev_temp->vm_end + vm_total_size;
					prev_temp->vm_next->access_flags = prot;
					prev_temp->vm_next->vm_next = NULL;
					function_retval = prev_temp->vm_end;
				}
			}
			else
			{

				if (virtual_temp_head->vm_start - prev_temp->vm_end != vm_total_size)
				{
					if (prot == prev_temp->access_flags)
					{
						function_retval = prev_temp->vm_end;
						prev_temp->vm_end = prev_temp->vm_end + vm_total_size; 
					}
					else
					{	if(stats->num_vm_area==128) {return -1;}
						prev_temp->vm_next = alloc_vm_area();
						prev_temp->vm_next->vm_start = prev_temp->vm_end;
						prev_temp->vm_next->vm_end = prev_temp->vm_end + vm_total_size;
						prev_temp->vm_next->access_flags = prot;
						prev_temp->vm_next->vm_next = virtual_temp_head;
						function_retval = prev_temp->vm_end;
					}
				}
				else
				{ 
					if (prot == prev_temp->access_flags)
					{
						function_retval = prev_temp->vm_end;
						prev_temp->vm_end = prev_temp->vm_end + vm_total_size; 

						if (prot == virtual_temp_head->access_flags)
						{
							prev_temp->vm_end = virtual_temp_head->vm_end;
							prev_temp->vm_next = virtual_temp_head->vm_next;
							dealloc_vm_area(virtual_temp_head);
						}
					}
					else
					{
						if (prot == virtual_temp_head->access_flags)
						{
							virtual_temp_head->vm_start = prev_temp->vm_end;
							function_retval = virtual_temp_head->vm_start;
						}
						else
						{	if(stats->num_vm_area==128) {return -1;}
							prev_temp->vm_next = alloc_vm_area();

                            prev_temp->vm_next->vm_end = prev_temp->vm_end + vm_total_size;
							prev_temp->vm_next->vm_start = prev_temp->vm_end;
							prev_temp->vm_next->access_flags = prot;
							prev_temp->vm_next->vm_next = virtual_temp_head;
                            
							function_retval = prev_temp->vm_end;
						}
					}
				}
			}
		} 
        else
        {
			if (MMAP_AREA_START + vm_total_size > MMAP_AREA_END)
				return -1; 
			if( stats->num_vm_area==128) {return -1;}

			current->vm_area = alloc_vm_area();

            current->vm_area->vm_end = MMAP_AREA_START + vm_total_size;
			current->vm_area->vm_start = MMAP_AREA_START;
			current->vm_area->access_flags = prot;
			current->vm_area->vm_next = NULL;

			function_retval = MMAP_AREA_START;
			
		}
		
	}
	
	return function_retval;
}
/**
 * munmap system call implemenations
 */

int vm_area_unmap_virtual(struct exec_context *current, u64 addr, int length) // debug done
{
	
	if ( addr < MMAP_AREA_START || addr >= MMAP_AREA_END)
		return -1;

	u64 numpgs = ((u64)length / PAGE_SIZE);
	if (length % PAGE_SIZE != 0)
		numpgs++;

	struct exec_context *ctx = current;

	struct vm_area *starter = current->vm_area;
	struct vm_area *prev_starter = NULL;

	u64 end_addr = addr + (numpgs * PAGE_SIZE); 

	
	if (starter == NULL)
	{
		return 0;
	}

	while (starter->vm_end <= addr) 
	{		
		prev_starter = starter;
		starter = starter->vm_next;
		if (!starter)
			break;
	}

	if (!starter)
	{
		return 0;
	}

	while (starter->vm_start < end_addr)
	{
		if (starter->vm_start < addr)
		{
			if (starter->vm_end <= end_addr)
			
			{
				starter->vm_end = addr;
				prev_starter=starter;
				starter = starter->vm_next;
            }
            else
                {

				struct vm_area *lst = starter->vm_next;

				if(stats->num_vm_area==128) {return -1;}

				starter->vm_next = alloc_vm_area();

				starter->vm_next->vm_end = starter->vm_end;
				starter->vm_next->vm_start = end_addr;
				starter->vm_end = addr;
				starter->vm_next->access_flags = starter->access_flags;
				starter->vm_next->vm_next = lst;
				break;
			
            }
		}
        else
        {
			if (starter->vm_end > end_addr)
			{
				starter->vm_start = end_addr; 
				break;
			}
			else
			{
				
                if (!prev_starter)
				{
					current->vm_area = starter->vm_next;
					dealloc_vm_area(starter);
					
					starter=starter->vm_next;
					continue;
				}
				else
				{
					prev_starter->vm_next = starter->vm_next;
					dealloc_vm_area(starter);
					starter=starter->vm_next;
					continue;

				}				
			}
		}
		

		if (starter == NULL)
			break;
	}
	return 0;

	
}

int vm_area_mprotect_virtual(struct exec_context *current, u64 addr, int length, int prot) //debug done
{
	
	u64 numpgs = ((u64)length / PAGE_SIZE);

	if (length % PAGE_SIZE != 0){
		numpgs++;
    }

	u64 tot_area = PAGE_SIZE * numpgs;

	struct exec_context *ctx = current;

	struct vm_area *starter = current->vm_area;

	if (starter == NULL){return -1;}

	while (starter->vm_end <= addr)
	{ 
		starter = starter->vm_next;
		if (!starter)
			break;
	}
	if ((starter == NULL) || (starter->vm_start > addr) ){return -1;}
	while (starter->vm_end <= addr)
	{
		if (starter->vm_next->vm_start != starter->vm_end)
			return -1;

		starter = starter->vm_next;
		if (!starter)
			return -1; 
	}

	starter = current->vm_area;
	
	while (starter->vm_end <= addr)
	{ 
		starter = starter->vm_next;
		if (!starter)
			return -1;
	}
	if(stats->num_vm_area==127 && prot!=starter->access_flags && starter->vm_end>addr+tot_area && starter->vm_start<addr ){
		return -1;
	}				
	if(stats->num_vm_area==128) {return -1;}

	u64 temp1=vm_area_unmap_virtual(current, addr, tot_area);

	u64 temp2=vm_area_map_virtual(current, addr, tot_area, prot, 0);
	
	return 0;
}

long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags) //debug done
{
	
	
	if(length%PAGE_SIZE!=0)
         length = length + PAGE_SIZE - length%PAGE_SIZE ;
	
	struct vm_area * temp_vm_area_copy;
	u64 returnd_addr;

	int vm_area_count = vm_area_ctr(current->vm_area);

	if(vm_area_count <127)
	{
		temp_vm_area_copy = NULL;	
	}
	else
	{
		temp_vm_area_copy = vm_copy(current->vm_area);
	}
   
	returnd_addr = vm_area_map_virtual(current,addr,length,prot,flags);
     vm_area_count = vm_area_ctr(current->vm_area);
	if( (vm_area_count  > 128) && (returnd_addr != -1))
	{
		copy_dealloc(current->vm_area);
		current->vm_area = temp_vm_area_copy;
		return -1;
	}
	else
	{
		copy_dealloc(temp_vm_area_copy);	
	}
	if(returnd_addr != -1 && flags & MAP_POPULATE)
	{
		for(int i = 0; i<length/PAGE_SIZE;i++)
		{
			unsigned long basestrt = (u64) current->pgd << PAGE_SHIFT;
			u64 pfn =  map_physical_page(basestrt,returnd_addr + i*PAGE_SIZE,prot,0);
		}
	}
	return returnd_addr;
}
int vm_area_unmap(struct exec_context *current, u64 addr, int length) //debug done
{
	if(length % PAGE_SIZE != 0)
		length = length + PAGE_SIZE - length%PAGE_SIZE ;

		
	struct vm_area * vm_temp_copy;
	
	int vm_area_count = vm_area_ctr(current->vm_area);
	if(vm_area_count <127)
	{
		vm_temp_copy = NULL;	
	}
	else
	{
		vm_temp_copy = vm_copy(current->vm_area);
	}
	
	int val_func = vm_area_unmap_virtual(current,addr,length);

	
	vm_area_count = vm_area_ctr(current->vm_area);
	if(val_func == 0 && vm_area_count  > 128)
	{
		copy_dealloc(current->vm_area);
		current->vm_area = vm_temp_copy;
		return -1;
	}
	else
	{
		copy_dealloc(vm_temp_copy);	
	}
	

	if(val_func == 0)
	{
	
		for(int i = 0; i<length/PAGE_SIZE; i++)
		{
			u64 str = addr + i*PAGE_SIZE;
			u64 *ptep =  get_user_pte(current, str, 0);
      
			if(ptep)
			{	
				struct pfn_info * pge_inf;
				pge_inf = get_pfn_info((*ptep & FLAG_MASK) >> PAGE_SHIFT);
				
				if(pge_inf->refcount == 1)
				{
					os_pfn_free(USER_REG,(*ptep & FLAG_MASK) >> PAGE_SHIFT);
					*ptep = *ptep & 0;
					asm volatile ("invlpg (%0);" 
								  :: "r"(str) 
								  : "memory");   // Flush TLB
				}
				else
				{

					decrement_pfn_info_refcount(pge_inf);
					*ptep = *ptep & 0;
					asm volatile ("invlpg (%0);" 
								  :: "r"(str) 
								  : "memory");   // Flush TLB
				}

			}
		
		}

		return val_func;
		
	}	
	
}
int vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot) //debug done
{
	if(length%PAGE_SIZE!=0)
         length = length + PAGE_SIZE - length%PAGE_SIZE ;
	struct vm_area * vm_temp;
	int num_area = vm_area_ctr(current->vm_area);
	if(!((num_area <127))){vm_temp = vm_copy(current->vm_area);
	}	
	
	else{vm_temp = NULL;}

	
	int vl_func =  vm_area_mprotect_virtual(current,addr,length,prot);
	
	num_area = vm_area_ctr(current->vm_area);
	if(num_area  > 128 && (vl_func == 0))
	{
		copy_dealloc(current->vm_area);
		current->vm_area = vm_temp;
		return -1;
	}
	else
	{
		copy_dealloc(vm_temp);	
	}
	

	if(vl_func == 0)
	{
			unsigned long pt_base = (u64) current->pgd << PAGE_SHIFT;
			for(int i =0;i<length/PAGE_SIZE;i++)
			{
				unsigned long srt = addr + i*PAGE_SIZE;
                struct pfn_info * pg_info;
				u64 *ptep =  get_user_pte(current, srt, 0);
				if(!ptep)
					continue;
				pg_info = get_pfn_info((*ptep & FLAG_MASK) >> PAGE_SHIFT);
				if((prot & PROT_WRITE)  && (pg_info->refcount == 1))
				{
					*ptep = (0x2 | *ptep);
				}
				else if(pg_info->refcount == 1)
				{
					*ptep = (~(0x2) & *ptep);	
				}
                else if((*ptep & MM_WR) && (!(prot & PROT_WRITE)))
				{
					u64 pfn = map_physical_page(pt_base,srt,MM_RD,0);
					pfn = (u64)osmap(pfn);
					memcpy((char *)pfn, (char *)(*ptep & FLAG_MASK), PAGE_SIZE);
					decrement_pfn_info_refcount(pg_info);
				}
				else if((!(*ptep & MM_WR))&&(prot & PROT_WRITE))
				{

					u64 pfn = map_physical_page(pt_base,srt,MM_WR|MM_RD,0);
					pfn = (u64)osmap(pfn);
					memcpy((char *)pfn, (char *)(*ptep & FLAG_MASK), PAGE_SIZE);
					decrement_pfn_info_refcount(pg_info);
				}
				

					asm volatile ("invlpg (%0);" 
								  :: "r"(srt) 
								  : "memory");   // Flush TLB
			}

		}
		return vl_func;

}