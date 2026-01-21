/* General headers */
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/skbuff.h>
#include <linux/freezer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/highmem.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <linux/vmalloc.h>
#include <asm/pgalloc.h>

#include "memalloc-common.h"

/* Project Functions */
pud_t* memalloc_pud_alloc(p4d_t* p4d, unsigned long vaddr) {
    gfp_t gfp = GFP_KERNEL_ACCOUNT;
    pud_t* pud = (pud_t*) get_zeroed_page(gfp);
    if (!pud) {
        printk("Error: Failed to allocate PUD.\n");
        return NULL;
    }

#if defined(CONFIG_X86_64)
    WRITE_ONCE(*p4d, __p4d(_PAGE_TABLE | __pa(pud)));
#else 
    p4dval_t p4dval = P4D_TYPE_TABLE | P4D_TABLE_PXN;
    phys_addr_t pud_pa = __pa(pud);
    p4d_t p4dt = __p4d(__phys_to_p4d_val(pud_pa) | p4dval);
	WRITE_ONCE(*p4d, p4dt);
	dsb(ishst);
	isb();
#endif

    return pud;
}

pmd_t* memalloc_pmd_alloc(pud_t* pud, unsigned long vaddr) {
    gfp_t gfp = GFP_KERNEL_ACCOUNT;
    pmd_t* pmd = (pmd_t*) get_zeroed_page(gfp);
    if (!pmd) {
        printk("Error: Failed to allocate PMD.\n");
        return NULL;
    }

#if defined(CONFIG_X86_64)
    set_pud(pud, __pud(_PAGE_TABLE | __pa(pmd)));
#else 
    pudval_t pudval = PUD_TYPE_TABLE | PUD_TABLE_PXN;
    phys_addr_t pmd_pa = __pa(pmd);
    pud_t pudt = __pud(__phys_to_pud_val(pmd_pa) | pudval);
	WRITE_ONCE(*pud, pudt);
	if (pud_valid(pudt)) {
		dsb(ishst);
		isb();
	}
#endif

    return pmd;
}

void memalloc_pte_alloc(pmd_t* pmd, unsigned long vaddr) {
    gfp_t gfp = GFP_PGTABLE_USER;
    struct ptdesc* pte = (struct ptdesc*) pagetable_alloc(gfp, 0);
    if (!pte) {
        printk("Error: Failed to allocate PTE.\n");
        return;
    }

    pgtable_t pt = ptdesc_page(pte);
#if defined(CONFIG_X86_64)
    pmd_populate(current->mm, pmd, pt);
#else 
    pmdval_t pmdval = PMD_TYPE_TABLE | PMD_TABLE_PXN;
    phys_addr_t pt_pa = page_to_phys(pt);
    pmd_t pmdt = __pmd(__phys_to_pmd_val(pt_pa) | pmdval);
	WRITE_ONCE(*pmd, pmdt);
	if (pmd_valid(pmdt)) {
		dsb(ishst);
		isb();
	}
#endif
}
