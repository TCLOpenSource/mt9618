KERNEL_HEADERS=/lib/modules/$(shell uname -r)/build

obj-m := cxd2878.o

cxd2878-objs := cxd_dmd_i2c_device.o \
		cxd_dmd_monitor_bit_error.o \
		cxd_dmd_top.o \
		cxd_dmd_common.o \
		source/CXD2878Family_refcode/cxdref_common.o \
		source/CXD2878Family_refcode/cxdref_demod.o \
		source/CXD2878Family_refcode/cxdref_integ.o \
		source/CXD2878Family_refcode/cxdref_stopwatch_port.o \
		source/CXD2878Family_refcode/atsc/cxdref_demod_atsc.o \
		source/CXD2878Family_refcode/atsc/cxdref_demod_atsc3.o \
		source/CXD2878Family_refcode/atsc/cxdref_demod_atsc3_chbond.o \
		source/CXD2878Family_refcode/atsc/cxdref_demod_atsc3_monitor.o \
		source/CXD2878Family_refcode/atsc/cxdref_demod_atsc_monitor.o \
		source/CXD2878Family_refcode/atsc/cxdref_integ_atsc.o \
		source/CXD2878Family_refcode/atsc/cxdref_integ_atsc3.o \
		source/CXD2878Family_refcode/dvb_cable/cxdref_demod_dvbc.o \
		source/CXD2878Family_refcode/dvb_cable/cxdref_demod_dvbc_monitor.o \
		source/CXD2878Family_refcode/dvb_cable/cxdref_integ_dvbc.o \
		source/CXD2878Family_refcode/dvb_sat/cxdref_demod_dvbs_s2.o \
		source/CXD2878Family_refcode/dvb_sat/cxdref_demod_dvbs_s2_monitor.o \
		source/CXD2878Family_refcode/dvb_sat/cxdref_integ_dvbs_s2.o \
		source/CXD2878Family_refcode/dvb_terr/cxdref_demod_dvbt.o \
		source/CXD2878Family_refcode/dvb_terr/cxdref_demod_dvbt2.o \
		source/CXD2878Family_refcode/dvb_terr/cxdref_demod_dvbt2_monitor.o \
		source/CXD2878Family_refcode/dvb_terr/cxdref_demod_dvbt_monitor.o \
		source/CXD2878Family_refcode/dvb_terr/cxdref_integ_dvbt.o \
		source/CXD2878Family_refcode/dvb_terr/cxdref_integ_dvbt2.o \
		source/CXD2878Family_refcode/dvb_terr/cxdref_integ_dvbt_t2.o \
		source/CXD2878Family_refcode/i2c/cxdref_i2c.o \
		source/CXD2878Family_refcode/isdb_cable/cxdref_demod_isdbc.o \
		source/CXD2878Family_refcode/isdb_cable/cxdref_demod_isdbc_chbond.o \
		source/CXD2878Family_refcode/isdb_cable/cxdref_demod_isdbc_monitor.o \
		source/CXD2878Family_refcode/isdb_cable/cxdref_integ_isdbc.o \
		source/CXD2878Family_refcode/isdb_sat/cxdref_demod_isdbs.o \
		source/CXD2878Family_refcode/isdb_sat/cxdref_demod_isdbs3.o \
		source/CXD2878Family_refcode/isdb_sat/cxdref_demod_isdbs3_monitor.o \
		source/CXD2878Family_refcode/isdb_sat/cxdref_demod_isdbs_monitor.o \
		source/CXD2878Family_refcode/isdb_sat/cxdref_integ_isdbs.o \
		source/CXD2878Family_refcode/isdb_sat/cxdref_integ_isdbs3.o \
		source/CXD2878Family_refcode/isdb_terr/cxdref_demod_isdbt.o \
		source/CXD2878Family_refcode/isdb_terr/cxdref_demod_isdbt_monitor.o \
		source/CXD2878Family_refcode/isdb_terr/cxdref_integ_isdbt.o \
		source/CXD2878Family_refcode/j83b/cxdref_demod_j83b.o \
		source/CXD2878Family_refcode/j83b/cxdref_demod_j83b_monitor.o \
		source/CXD2878Family_refcode/j83b/cxdref_integ_j83b.o \
		source/CXD2878Family_refcode/lib/cxdref_math.o \
		source/CXD2878Family_refcode/sat_device_ctrl/cxdref_demod_sat_device_ctrl.o \
		source/CXD2878Family_refcode/sat_device_ctrl/cxdref_diseqc_command_1_0.o \
		source/CXD2878Family_refcode/sat_device_ctrl/cxdref_diseqc_command_1_1.o \
		source/CXD2878Family_refcode/sat_device_ctrl/cxdref_diseqc_command_1_2.o \
		source/CXD2878Family_refcode/sat_device_ctrl/cxdref_diseqc_command_2_0.o \
		source/CXD2878Family_refcode/sat_device_ctrl/cxdref_diseqc_command_2_2.o \
		source/CXD2878Family_refcode/sat_device_ctrl/cxdref_integ_sat_device_ctrl.o \
		source/CXD2878Family_refcode/sat_device_ctrl/cxdref_singlecable2_command.o \
		source/CXD2878Family_refcode/sat_device_ctrl/cxdref_singlecable_command.o \
		source/CXD2878Family_refcode/tuner/cxdref_tuner.o


ccflags-y += -I$(srctree)/drivers/media/dvb-frontends/cxd2878/ \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/atsc \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/dvb_cable \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/dvb_sat \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/dvb_terr \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/isdb_cable \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/isdb_sat \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/isdb_terr \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/j83b \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/lib \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/sat_device_ctrl \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/sat_device_ctrl/lnb_controller\
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/tuner \
		-I$(srctree)/drivers/media/dvb-frontends/cxd2878/source/CXD2878Family_refcode/i2c \
		-I$(srctree)/drivers/media/dvb-frontends/cfm \
		-I$(REPO_ROOT_PATH)/mtktv_drivers/ree/dvb_adapter \
		-DCXDREF_DRIVER_BUILD_OPTION_CXD2878 \
		-DCXDREF_DEMOD_SUPPORT_REMOVE_DVBC2 \
		-DCXDREF_TRACE_ENABLE \
		-DCONFIG_MEDIATEK_DVB
		
#Workaround for -Werror
ccflags-y += -Wno-c99-compat
ccflags-y += -Wno-enum-conversion
ccflags-y += -Wno-format
ccflags-y += -Wno-format-insufficient-args
ccflags-y += -Wno-int-conversion
ccflags-y += -Wno-macro-redefined
ccflags-y += -Wno-misleading-indentation
ccflags-y += -Wno-parentheses
ccflags-y += -Wno-self-assign
ccflags-y += -Wno-shift-count-overflow
ccflags-y += -Wno-sometimes-uninitialized
ccflags-y += -Wno-switch
ccflags-y += -Wno-undef
ccflags-y += -Wno-unknown-escape-sequence
ccflags-y += -Wno-unused-function
ccflags-y += -Wno-unused-variable
ccflags-y += -Wno-uninitialized
#End of workaround for -Werror		

ldflags-y += -L$(REPO_ROOT_PATH)/mtktv_drivers/ree/dvb_adapter

all:
	$(MAKE) -C $(KERNEL_HEADERS) M=$(srctree)/drivers/media/dvb-frontends/cxd2878 modules
clean:
	$(MAKE) -C $(KERNEL_HEADERS) M=$(srctree)/drivers/media/dvb-frontends/cxd2878 clean
