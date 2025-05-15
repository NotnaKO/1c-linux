#pragma once

#include <linux/slab.h>
#include <linux/rbtree.h>

#define NAME_MAX_LEN 30
#define SURNAME_MAX_LEN 30
#define PHONE_MAX_LEN 15
#define EMAIL_MAX_LEN 30

typedef struct {
  char name[NAME_MAX_LEN];
  char surname[SURNAME_MAX_LEN];
  size_t age;
  char phone[PHONE_MAX_LEN];
  char email[EMAIL_MAX_LEN];
} user_data_t;

typedef struct {
  struct rb_node node;
  user_data_t user;
} user_data_node_t;

typedef struct {
  struct rb_root tree;
} book_t;

bool user_data_cmp(struct rb_node* a, const struct rb_node* b);
int user_data_finder(const void* key, const struct rb_node* node);

extern book_t book;

void init_handbook(void);

// Operations
long get_user_info(const char* surname, user_data_t* output_data);
long add_user(user_data_t* input_data);
long del_user(const char* surname);