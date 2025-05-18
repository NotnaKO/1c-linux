#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Коpanov Anton");
MODULE_DESCRIPTION(
    "Procfs mmanag module for VMA listing, page finding, and value writing");

#define PROCFS_NAME "mmanag"

static struct proc_dir_entry* proc_file_entry;

static void handle_listvma(void) {
  struct mm_struct* mm = current->mm;
  struct vm_area_struct* vma;
  int count = 0;

  pr_info("mmanag: listvma for PID %d (%s)\n", current->pid, current->comm);
  if (!mm) {
    pr_warn("mmanag: No mm_struct for current process\n");
    return;
  }
  mmap_read_lock(mm);
  VMA_ITERATOR(iter, mm, 0);
  for_each_vma(iter, vma) {
    pr_info("mmanag: VMA %d: start=0x%lx, end=0x%lx, flags=0x%lx\n",
            count++, vma->vm_start, vma->vm_end, vma->vm_flags);
  }
  mmap_read_unlock(mm);
}

static void handle_findpage(unsigned long addr) {
  struct mm_struct* mm = current->mm;

  struct page* page;

  if (!mm) {
    pr_info("No mm for current process\n");
    return;
  }

  mmap_read_lock(mm);
  if (get_user_pages_fast(addr, 1, FOLL_GET, &page) < 1) {
    pr_info("mmanag: get_user_pages_fast failed\n");
  } else {
    phys_addr_t phys = page_to_phys(page) + (addr & ~PAGE_MASK);
    pr_info("PA: %pa for VA: %lx\n", &phys, addr);
    put_page(page);
  }
  mmap_read_unlock(mm);
}


static void handle_writeval(unsigned long addr, unsigned long val) {
  unsigned long __user * uaddr = (unsigned long __user *)addr;

  if (!access_ok(uaddr, sizeof(unsigned long))) {
    pr_info("Invalid user address 0x%lx\n", addr);
    return;
  }

  bool fail = put_user(val, uaddr);
  if (fail) {
    pr_info("Failed to write 0x%lx to 0x%lx\n", val, addr);
  } else {
    pr_info("Wrote 0x%lx to 0x%lx\n", val, addr);
  }
}

static ssize_t proc_write(struct file* file, const char __user * buffer,
                          size_t count, loff_t* ppos) {
  char input[128];
  char cmd[16];
  unsigned long addr;
  unsigned long val;

  if (count >= sizeof(input)) {
    return -EINVAL;
  }

  if (copy_from_user(input, buffer, count)) {
    return -EFAULT;
  }

  input[count] = '\0';

  int n = sscanf(input, "%15s %lx %lu", cmd, &addr, &val);
  if (n < 1) {
    return -EINVAL;
  }

  if (strcmp(cmd, "listvma") == 0 && n == 1) {
    handle_listvma();
  } else if (strcmp(cmd, "findpage") == 0 && n == 2) {
    handle_findpage(addr);
  } else if (strcmp(cmd, "writeval") == 0 && n == 3) {
    handle_writeval(addr, val);
  } else {
    pr_warn("Unknown command\n");
  }

  return count;
}

static const struct proc_ops mmanag_proc_ops = {
    .proc_write = proc_write,
};

static int __init mmanag_module_init(void) {
  proc_file_entry = proc_create(PROCFS_NAME, 0666, NULL, &mmanag_proc_ops);
  if (proc_file_entry == NULL) {
    pr_err("mmanag: Could not create /proc/%s\n", PROCFS_NAME);
    return -ENOMEM;
  }

  pr_info("mmanag: /proc/%s created\n", PROCFS_NAME);
  return 0;
}

static void __exit mmanag_module_exit(void) {
  if (proc_file_entry) {
    proc_remove(proc_file_entry);
  }
  pr_info("mmanag: /proc/%s removed\n", PROCFS_NAME);
}

module_init(mmanag_module_init);
module_exit(mmanag_module_exit);