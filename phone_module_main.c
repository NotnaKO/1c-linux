#include "phone_book.h"


#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>


#define DEVICE_BUF_SIZE 1024
#define COMMAND_MAX_LEN 3

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Kopanov");
MODULE_DESCRIPTION("Phone Book");
MODULE_VERSION("0.1.0");

static int __init phone_book_init(void);
static void __exit phone_book_exit(void);


// Character device
dev_t dev = 0;
static struct class* dev_class;
static struct cdev mipt_cdev;


// file_operations declarations.
static int book_open(struct inode* inode, struct file* file);
static int book_release(struct inode* inode, struct file* file);
static ssize_t book_read(struct file* filp,
                         char __user* buf,
                         size_t len,
                         loff_t* off);
static ssize_t book_write(struct file* filp,
                          const char* buf,
                          size_t len,
                          loff_t* off);
static struct file_operations fops =
{
    .owner = THIS_MODULE,
    .read = book_read,
    .write = book_write,
    .open = book_open,
    .release = book_release,
};


static char device_buf[DEVICE_BUF_SIZE + 1] = {};


static int book_open(struct inode* /*inode*/, struct file* /*file*/) {
  return 0;
}

static int book_release(struct inode* /*inode*/, struct file* /*file*/) {
  return 0;
}


static ssize_t book_read(struct file* /*file*/,
                         char __user* buf,
                         size_t len,
                         loff_t* off) {
  if (len == 0) {
    return 0;
  }
  if (len > DEVICE_BUF_SIZE - *off) {
    len = DEVICE_BUF_SIZE - *off;
  }

  size_t msg_len = strlen(device_buf);
  if (*off >= msg_len) {
    return 0;
  }
  if (msg_len < len) {
    len = msg_len;
  }

  if (copy_to_user(buf, device_buf + *off, len) != 0) {
    return -EFAULT;
  }

  *off += (loff_t)len;
  return (loff_t)len;
}

void try_parse_write_command(void);


// Write can be made gradually (therefore offset if provided to write function).
// So parsing instruction may fail and we "try" to parse it.
void try_parse_write_command(void) {
  char cmd[COMMAND_MAX_LEN + 1];

  if (sscanf(device_buf, "%s", cmd) != 1) {
    pr_info("read command failed\n");
    return;
  }
  char* cur_wb = device_buf + strlen(cmd) + 1;
  if (strcmp(cmd, "add") == 0) {
    user_data_t new_user;

    if (sscanf(cur_wb,
               "%s %s %zu %s %s",
               new_user.name,
               new_user.surname,
               &new_user.age,
               new_user.phone,
               new_user.email) != 5) {
      pr_info("parse command failed\n");
      return;
    }

    if (add_user(&new_user) != 0) {
      sprintf(device_buf, "user add failed");
    }

    sprintf(device_buf, "New user inserted successfully.\n");
    return;
  }
  if (strcmp(cmd, "get") == 0) {
    char surname[SURNAME_MAX_LEN];
    if (sscanf(cur_wb, "%s", surname) != 1) {
      pr_info("read surname failed\n");
      return;
    }
    user_data_t user;
    if (get_user_info(surname, &user) != 0) {
      sprintf(device_buf, "no such user\n");
      return;
    }

    sprintf(device_buf, "name=%s, surname=%s, age=%zu, phone=%s, email=%s\n",
            user.name, user.surname, user.age, user.phone, user.email);
    return;
  }
  if (strcmp(cmd, "del") == 0) {
    char surname[SURNAME_MAX_LEN];
    if (sscanf(cur_wb, "%s", surname) != 1) {
      pr_info("read surname failed\n");
      return;
    }

    if (del_user(surname) != 0) {
      sprintf(device_buf, "User '%s' not found.\n", surname);
    }
    sprintf(device_buf, "User '%s' successfully deleted.\n", surname);
    return;
  }
}

/*
** This function will be called when we write the Device file
*/
static ssize_t book_write(struct file* filp,
                          const char __user* buf,
                          size_t len,
                          loff_t* off) {
  if (len == 0) {
    return 0;
  }
  if (*off + 1 > DEVICE_BUF_SIZE) {
    return 0;
  }
  if (*off + len + 1 > DEVICE_BUF_SIZE) {
    len = DEVICE_BUF_SIZE - *off - 1;
  }
  if (copy_from_user(device_buf + *off, buf, len) != 0) {
    return -EFAULT;
  }
  device_buf[*off + len] = '\0';

  try_parse_write_command();

  *off += (loff_t)len;
  return (loff_t)len;
}


static int __init phone_book_init(void) {
  printk(KERN_INFO "Start inserting kernel module...");

  if (alloc_chrdev_region(&dev, 0, 1, "phone_book") < 0) {
    pr_err("Fail allocate major number\n");
    return -1;
  }
  pr_info("Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));

  cdev_init(&mipt_cdev, &fops);

  if (cdev_add(&mipt_cdev, dev, 1) < 0) {
    pr_err("Cannot add the device to the system\n");
    unregister_chrdev_region(dev, 1);
    return -1;
  }

  dev_class = class_create("phone_book");
  if (IS_ERR(dev_class)) {
    pr_err("Cannot create the struct class for device\n");
    unregister_chrdev_region(dev, 1);
    return -1;
  }

  if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "phone_book"))) {
    pr_err("Cannot create the Device\n");
    class_destroy(dev_class);
    unregister_chrdev_region(dev, 1);
    return -1;
  }

  init_handbook();

  pr_info("Kernel module inserted\n");
  return 0;
}

static void __exit phone_book_exit() {
  pr_info("Start removing kernel module...\n");

  device_destroy(dev_class, dev);
  class_destroy(dev_class);
  unregister_chrdev_region(dev, 1);

  pr_info("Kernel module removed\n");
}

module_init(phone_book_init);
module_exit(phone_book_exit);