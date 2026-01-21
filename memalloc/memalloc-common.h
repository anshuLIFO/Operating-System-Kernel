#ifndef __MEMALLOC_COMMON_H__
#define __MEMALLOC_COMMON_H__

/* Page table allocation helper functions defined in kmod_helper.c */
pud_t*  memalloc_pud_alloc(p4d_t* p4d, unsigned long vaddr);
pmd_t*  memalloc_pmd_alloc(pud_t* pud, unsigned long vaddr);
void    memalloc_pte_alloc(pmd_t* pmd, unsigned long vaddr);

#endif 
