#include "phone_book.h"

#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

book_t book = {
    .tree = RB_ROOT
};

void init_handbook() {
  book.tree = RB_ROOT;
}


long get_user_info(const char* surname, user_data_t* output_data) {
  if (output_data == NULL) {
    return EINVAL;
  }
  pr_info("user info: surname: %s\n", surname);
  struct rb_node* node = rb_find(surname, &book.tree, user_data_finder);
  if (node == NULL) {
    return ENOENT;
  }
  pr_info("node found\n");
  user_data_node_t* res = container_of(node, user_data_node_t, node);
  pr_info("user %s %s\n", res->user.name, res->user.surname);

  memcpy(output_data, &res->user, sizeof(user_data_node_t));

  return 0;
}

long add_user(user_data_t* input_data) {
  user_data_node_t* ud_impl = kmalloc(sizeof(user_data_node_t), GFP_KERNEL);

  memcpy(&ud_impl->user, input_data, sizeof(user_data_t));
  pr_info("user add: surname: %s", ud_impl->user.surname);
  rb_add(&ud_impl->node, &book.tree, user_data_cmp);
  return 0;
}

long del_user(const char* surname) {
  struct rb_node* node = rb_find(surname, &book.tree, user_data_finder);
  if (node == NULL) {
    return ENOENT;
  }
  user_data_node_t* res = container_of(node, user_data_node_t, node);
  rb_erase(&res->node, &book.tree);
  kfree(res);
  return 0;
}

bool user_data_cmp(struct rb_node* a, const struct rb_node* b) {
  user_data_node_t* a_data = container_of(a, user_data_node_t, node);
  user_data_node_t* b_data = container_of(b, user_data_node_t, node);
  pr_info("comparing %s vs %s\n", a_data->user.surname, b_data->user.surname);
  return strcmp(a_data->user.name, b_data->user.name) < 0;
}

int user_data_finder(const void* key, const struct rb_node* node) {
  user_data_node_t* user_data = container_of(node, user_data_node_t, node);
  pr_info("comparing %s vs %s\n", (char*)key, user_data->user.surname);
  return strcmp(key, user_data->user.surname);
}

// Syscalls

SYSCALL_DEFINE2(get_user, const char*, surname, user_data_t*,
                output_data) {
  return get_user_info(surname, output_data);
}

SYSCALL_DEFINE1(add_user, user_data_t*, input_data) {
  return add_user(input_data);
}

SYSCALL_DEFINE1(del_user, const char*, surname) {
  return del_user(surname);
}