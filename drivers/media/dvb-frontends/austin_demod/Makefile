
############################################################
# Makefile for mtk-cfm.ko
#
#
#
#
############################################################

.PHONY: clean
clean:
	rm *.o


mtk-austin-demod-objs += halDMD_EXTERN_ATSC.o austin-demod.o

obj-m += mtk-austin-demod.o

ccflags-y += -Idrivers/media/dvb-frontends/austin_demod/

ifeq ($(IC_SERIES),merak_20)
ccflags-y += -DADD_MERAK20_ATSC3
endif