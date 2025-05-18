#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kopanov Anton");
MODULE_DESCRIPTION("FIFO Device Driver");

#define FIFO_SIZE 4096
#define DEVICE_NAME "fifo"

static int major;
static struct class* fifo_class;
static struct cdev fifo_cdev;

struct fifo_dev {
  struct kfifo fifo;
  struct mutex lock;
  wait_queue_head_t read_queue;
  wait_queue_head_t write_queue;
};

static struct fifo_dev* fifo_device;

static int fifo_open(struct inode* /*inode*/, struct file* file) {
  file->private_data = fifo_device;
  return 0;
}

static int fifo_release(struct inode* /*inode*/, struct file* /*file*/) {
  return 0;
}

static ssize_t fifo_read(struct file* file, char __user * buf, size_t count,
                         loff_t* /*f_pos*/) {
  struct fifo_dev* dev = file->private_data;
  unsigned int copied;

  if (mutex_lock_interruptible(&dev->lock)) {
    return -1;
  }

  while (kfifo_is_empty(&dev->fifo)) {
    mutex_unlock(&dev->lock);

    if (file->f_flags & O_NONBLOCK) {
      return -EAGAIN;
    }

    if (wait_event_interruptible(dev->read_queue,
                                 !kfifo_is_empty(&dev->fifo))) {
      return -1;
    }

    if (mutex_lock_interruptible(&dev->lock)) {
      return -1;
    }
  }

  const int ret = kfifo_to_user(&dev->fifo, buf, count, &copied);
  mutex_unlock(&dev->lock);

  if (ret) {
    return ret;
  }

  wake_up_interruptible(&dev->write_queue);
  return copied;
}

static ssize_t fifo_write(struct file* filp, const char __user * buf,
                          size_t count, loff_t* /*f_pos*/) {
  struct fifo_dev* dev = filp->private_data;
  unsigned int copied;

  if (mutex_lock_interruptible(&dev->lock)) {
    return -1;
  }

  while (kfifo_avail(&dev->fifo) == 0) {
    mutex_unlock(&dev->lock);

    if (filp->f_flags & O_NONBLOCK) {
      return -EAGAIN;
    }

    if (wait_event_interruptible(dev->write_queue,
                                 kfifo_avail(&dev->fifo) > 0)) {
      return -1;
    }

    if (mutex_lock_interruptible(&dev->lock)) {
      return -1;
    }
  }

  const int ret = kfifo_from_user(&dev->fifo, buf, count, &copied);
  mutex_unlock(&dev->lock);

  if (ret) {
    return ret;
  }

  wake_up_interruptible(&dev->read_queue);
  return copied;
}

static struct file_operations fifo_fops = {
    .owner = THIS_MODULE,
    .open = fifo_open,
    .release = fifo_release,
    .read = fifo_read,
    .write = fifo_write,
    .llseek = noop_llseek
};

static int __init fifo_init(void) {
  dev_t devno;

  int ret = alloc_chrdev_region(&devno, 0, 1, DEVICE_NAME);
  if (ret < 0) {
    return ret;
  }
  major = MAJOR(devno);

  fifo_class = class_create("fifo");
  if (IS_ERR(fifo_class)) {
    unregister_chrdev_region(devno, 1);
    return PTR_ERR(fifo_class);
  }

  fifo_device = kzalloc(sizeof(struct fifo_dev), GFP_KERNEL);
  if (!fifo_device) {
    class_destroy(fifo_class);
    unregister_chrdev_region(devno, 1);
    return -ENOMEM;
  }

  ret = kfifo_alloc(&fifo_device->fifo, FIFO_SIZE, GFP_KERNEL);
  if (ret) {
    kfree(fifo_device);
    class_destroy(fifo_class);
    unregister_chrdev_region(devno, 1);
    return ret;
  }

  mutex_init(&fifo_device->lock);
  init_waitqueue_head(&fifo_device->read_queue);
  init_waitqueue_head(&fifo_device->write_queue);

  cdev_init(&fifo_cdev, &fifo_fops);
  ret = cdev_add(&fifo_cdev, devno, 1);
  if (ret) {
    kfifo_free(&fifo_device->fifo);
    kfree(fifo_device);
    class_destroy(fifo_class);
    unregister_chrdev_region(devno, 1);
    return ret;
  }

  device_create(fifo_class, NULL, devno, NULL, DEVICE_NAME);
  return 0;
}

static void __exit fifo_exit(void) {
  dev_t devno = MKDEV(major, 0);

  device_destroy(fifo_class, devno);
  class_destroy(fifo_class);
  cdev_del(&fifo_cdev);
  kfifo_free(&fifo_device->fifo);
  kfree(fifo_device);
  unregister_chrdev_region(devno, 1);
}

module_init(fifo_init);
module_exit(fifo_exit);