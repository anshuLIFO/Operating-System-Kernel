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

/* File IO-related headers */
#include <linux/fs.h>
#include <linux/bio.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>

/* Sleep and timer headers */
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/sched/types.h>
#include <linux/pci.h>

#include "../common.h"
#include "memalloc-common.h"

/* Simple licensing stuff */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("student");
MODULE_DESCRIPTION("Project 4, CSE 330 Fall 2025");
MODULE_VERSION("0.01");

/* Calls which start and stop the ioctl teardown */
bool memalloc_ioctl_init(void);
void memalloc_ioctl_teardown(void);

/* Project 2 Solution Variable/Struct Declarations */
#define MAX_PAGES           4096
#define MAX_ALLOCATIONS     100

#if defined(CONFIG_X86_64)
    #define PAGE_PERMS_RW       PAGE_SHARED
    #define PAGE_PERMS_R        PAGE_READONLY
#else
    #define PAGE_PERMS_RW       __pgprot(_PAGE_DEFAULT | PTE_USER | PTE_NG | PTE_PXN | PTE_UXN | PTE_WRITE)
    #define PAGE_PERMS_R        __pgprot(_PAGE_DEFAULT | PTE_USER | PTE_NG | PTE_PXN | PTE_UXN | PTE_RDONLY)
#endif

// Project 4 structs - GLOBAL
struct alloc_info           alloc_req;
struct free_info            free_req;

// global counters to be used for test 6 and 7
static int total_pages_allocated = 0;
static int total_alloc_requests = 0;

//          PROJECT 4 STARTS HERE
/* Virtual device and driver structures */ 
#define DEVICE_NAME "memalloc"
static dev_t dev;                        // Device number
static struct class* memalloc_class;    // Device class (for /dev entry)
static struct cdev memalloc_cdev;       // Char device structure


/* Init and Exit functions */
static int __init memalloc_module_init(void) {
    printk("Hello from the memalloc module!\n");

     // we need to initialize the ioctl system and virtual device in init function
    if (!memalloc_ioctl_init()) {
        printk("Error: IOCTL init failed.\n");
        return -1;
    }
    return 0;
}

static void __exit memalloc_module_exit(void) {
    /* Teardown IOCTL */
    memalloc_ioctl_teardown(); // this function will bring down the current ioctl
    printk("Goodbye from the memalloc module!\n");
}

/* IOCTL handler for vmod. */
static long memalloc_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
    switch (cmd)
    {
    case ALLOCATE:  // if instruction is allocate

        // we need to copy alloc_info given to us into kernel memory
        // we use the copy from user command, with exception handling.
        // if it is successful, 0 is returned, and we skip the if loop.
        //              TASK 1 of Project 4
        if (copy_from_user(&alloc_req, (void*)arg, sizeof(struct alloc_info))) {
            printk("Error: Failed to copy alloc_info from user.\n");
            return -EFAULT;
        }
        printk("IOCTL: alloc(%lx, %d, %d)\n", alloc_req.vaddr, alloc_req.num_pages, alloc_req.write);
        
        // checking number of allocations and pages input by user
        // test6 and test7 functionality (TASK 4 of Project 4)
        if (total_pages_allocated + alloc_req.num_pages > MAX_PAGES) { // if current total pages + user entered number of pages > set threshold
            printk("MAX Limit of pages: too many pages allocated (%d)\n", total_pages_allocated);
            return -2;
        }
        if (total_alloc_requests >= MAX_ALLOCATIONS) {  // if current total allocations + user entered number of allocations > set threshold
            printk("MAX Limit of allocations: too many alloc requests (%d)\n", total_alloc_requests);
            return -3;
        }

        //          TASK 2 AND TASK 3 of Project 4
        struct mm_struct *mm1 = current->mm;            // struct that stores current memory management details

        // we need a for loop that will run the number of times the user has input for pages
        for (int i = 0; i < alloc_req.num_pages; i++) {
            unsigned long vaddr = alloc_req.vaddr + i * PAGE_SIZE;          // virtual address of the current page
            // printk("DEBUG [4.%d] Working on vaddr = 0x%lx\n", i, vaddr);  // print debugging

            // Following the hierarchial structure in the diagram and from pagewalk.c & helper.c :
            // 1. PGD
            pgd_t *pgd = pgd_offset(mm1, vaddr);    // first assign the pgd
            if (pgd_none(*pgd) || pgd_bad(*pgd)) {
                return -EFAULT;           // we do not use pgd_alloc because pgd must exist for a page for sure, if it is not, then simply exit with error.
            }

            // 2. P4D
            p4d_t *p4d = p4d_offset(pgd, vaddr);
            if (p4d_none(*p4d) || p4d_bad(*p4d)) {
                memalloc_pud_alloc(p4d, vaddr);         // following the step down into the next level, we call pud_alloc
                p4d = p4d_offset(pgd, vaddr);           // then we re-fetch and assign p4d using the previous level as an argument.
            }

            // 3. PUD
            pud_t *pud = pud_offset(p4d, vaddr);
            if (pud_none(*pud)) {
                memalloc_pmd_alloc(pud, vaddr);         // following the step down into the next level, we call pmd_alloc
                pud = pud_offset(p4d, vaddr);           // then we re-fetch and assign pud using the previous level as an argument.
            }

            // 4. PMD
            pmd_t *pmd = pmd_offset(pud, vaddr);
            if (pmd_none(*pmd)) {
                memalloc_pte_alloc(pmd, vaddr);     // following the step down into the next level, we call pte_alloc
                pmd = pmd_offset(pud, vaddr);       // then we re-fetch and assign pmd using the previous level as an argument.
            }

            // 5. PTE
            pte_t *pte = pte_offset_kernel(pmd, vaddr);     // using offset_kernel as used in helper.c
            if (!pte) {                                     // if pte is pointing to garbage, we return error
                return -EFAULT;
            }
            
            // Case 1: No page mapped — need to allocate one
            if (pte_none(*pte)) {
                gfp_t gfp = GFP_KERNEL_ACCOUNT;     // as used in helper.c
                void* page = (void*) get_zeroed_page(gfp);  // we create a zeroed page to be allocated

                if (!page) {        // if the created page was not successful
                    // printk("DEBUG [12.%d] get_zeroed_page failed\n", i);  print debugging
                    return -ENOMEM;
                }

                // we access the physical address of the zeroed page
                unsigned long paddr = __pa(page);
                // then we set the permissions - read, write, and user permissions (like in helper.c)
                pgprot_t perms = alloc_req.write ? PAGE_PERMS_RW : PAGE_PERMS_R;

                // finally, we allocate the PTE using set_pte with the permissions, address and pte
                set_pte_at(current->mm, vaddr, pte, pfn_pte((paddr >> PAGE_SHIFT), perms));
            }

            // Case 2: Page is already mapped
            else {
                // Case 2.1 : Page is already present, so we simply say we can not allocate here, and return with an error.
                if (pte_present(*pte)) {
                    return -1;
                }
                // Case 2.2 : A case that is not needed in this project is when a page is already present but not marked.
            }
            
        }

        // we reach here means allocation was successful, so we update global counters
        total_pages_allocated += alloc_req.num_pages;
        total_alloc_requests++;
        
        // printk("DEBUG [15] Allocation done.\n");
        break;

    case FREE:  // if instruction is FREE
        // we need to copy free_info into kernel memory
        if (copy_from_user(&free_req, (void*)arg, sizeof(struct free_info))) {
            printk("Error: Failed to copy free_info from user.\n");
            return -EFAULT;
        }

        // Debug print: log free request details
        printk("IOCTL: free(%lx)\n", free_req.vaddr);
        break;

    default:
        // Unknown command
        printk("Error: incorrect IOCTL command.\n");
        return -EINVAL;
    }
    return 0;
}

/* Required file ops. */
static struct file_operations fops = 
{
    .owner          = THIS_MODULE,
    .unlocked_ioctl = memalloc_ioctl,
};

/* Initialize the module for IOCTL commands and create /dev/memalloc */
bool memalloc_ioctl_init(void) {
    // first we have to allocate a device number (dev)
    if (alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME) < 0) {
        printk("Error: Could not allocate chrdev region.\n");
        return false;
    }

    // second, we initialize the cdev structure with fops
    // here we want to Register a character device with the kernel and,
    // make sure that file operations are bound to the /dev/
    cdev_init(&memalloc_cdev, &fops);

    // third, we add the char device to the kernel
    // we want to tell the kernel that our virtual device will handle operations on 'dev'
    if (cdev_add(&memalloc_cdev, dev, 1) < 0) {
        printk("Error: Could not add cdev.\n");
        unregister_chrdev_region(dev, 1); // if registration fails, we have to free the device number.
        return false;
    }

    // fourth, we create a device class which will be later used for creating device nodes.
    // again, handling excpetions at every step so we can debug easily.
    memalloc_class = class_create(DEVICE_NAME);
    if (IS_ERR(memalloc_class)) {
        printk("Error: Could not create class.\n");
        
        // since it failed, we remove previously registered cdev and release dev number.
        cdev_del(&memalloc_cdev);
        unregister_chrdev_region(dev, 1);
        return false;
    }

    // lastly, we create the device file: /dev/memalloc
    // this joins the class we created with our device number
    if (IS_ERR(device_create(memalloc_class, NULL, dev, NULL, DEVICE_NAME))) {
        printk("Error: Could not create device file.\n");
        // if fails, we remove class, delete cdev, release dev number.
        class_destroy(memalloc_class);
        cdev_del(&memalloc_cdev);
        unregister_chrdev_region(dev, 1);
        return false;
    }

    // if no errors till this point.
    printk("Device /dev/memalloc created successfully.\n");
    return true;
}

/* Destroy the IOCTL device cleanly */
void memalloc_ioctl_teardown(void) {
    // device file
    device_destroy(memalloc_class, dev);

    // device class
    class_destroy(memalloc_class);

    // cdev structure
    cdev_del(&memalloc_cdev);

    //device number
    unregister_chrdev_region(dev, 1);

    printk("Device /dev/memalloc removed.\n");
}


module_init(memalloc_module_init);
module_exit(memalloc_module_exit);