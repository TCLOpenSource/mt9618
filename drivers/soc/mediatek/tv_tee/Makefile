ccflags-y += -I$(srctree)/drivers/tee/optee
ccflags-y += -Wall -Werror
ifneq ($(LINUX_ONLY),true)
ccflags-y += -DSET_USER_ID=2944
endif
obj-m += mtk_tee.o
