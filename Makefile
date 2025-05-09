obj-m += phone_module.o
phone_module-y := phone_book.o phone_module_main.o

KERNEL_DIR := /home/anton/linux_course/linux-6.14.5
MODULE_DIR := $(shell pwd)
ROOT_DIR := /home/anton/linux_course/vroot


default:
	make -C $(KERNEL_DIR) M=$(MODULE_DIR) modules
install: default
	INSTALL_MOD_PATH=$(ROOT_DIR) make -C $(KERNEL_DIR) M=$(MODULE_DIR) modules_install
clean:
	make -C $(KERNEL_DIR) M=$(MODULE_DIR) clean