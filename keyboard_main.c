#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anton Kopanov");
MODULE_DESCRIPTION("Module to count characters typed on a keyboard.");
MODULE_VERSION("0.1.0");

static atomic_t char_count = ATOMIC_INIT(0);
static struct timer_list count_timer;

static void count_characters(struct timer_list* timer) {
  int cnt = atomic_xchg(&char_count, 0);
  pr_info("Characters typed in the last minute: %d\n", cnt);
  mod_timer(&count_timer, jiffies + 60 * HZ);
}

static irqreturn_t keyboard_handler(int irq, void* dev_id) {
  atomic_inc(&char_count);
  pr_info("Keyboard interrupt\n");
  return IRQ_NONE;
}

static int __init keyboard_counter_init(void) {
  const int ret = request_irq(1, keyboard_handler, IRQF_SHARED,
                              "keyboard_counter", (void*)keyboard_handler);
  if (ret) {
    pr_err("Failed to register keyboard handler\n Error: %d", ret);
    return ret;
  }

  timer_setup(&count_timer, count_characters, 0);
  mod_timer(&count_timer, jiffies + 60 * HZ);

  pr_info("Keyboard counter module loaded\n");
  return 0;
}

static void __exit keyboard_counter_exit(void) {
  del_timer(&count_timer);
  free_irq(1, (void*)keyboard_handler);
  pr_info("Keyboard counter module unloaded\n");
}

module_init(keyboard_counter_init);
module_exit(keyboard_counter_exit);