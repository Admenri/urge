LOCAL_PATH := $(call my-dir)

### dav1d

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/android \
    $(LOCAL_PATH)/android/$(TARGET_ARCH_ABI) \
    $(LOCAL_PATH)/include \

LOCAL_SRC_FILES :=  \
    src/cdf.c \
    src/cpu.c \
    src/data.c \
    src/decode.c \
    src/dequant_tables.c \
    src/getbits.c \
    src/intra_edge.c \
    src/itx_1d.c \
    src/lf_mask.c \
    src/lib.c \
    src/log.c \
    src/mem.c \
    src/msac.c \
    src/obu.c \
    src/picture.c \
    src/qm.c \
    src/ref.c \
    src/refmvs.c \
    src/scan.c \
    src/tables.c \
    src/thread_task.c \
    src/warpmv.c \
    src/wedge.c \

LOCAL_MODULE := dav1d

include $(BUILD_STATIC_LIBRARY)


### dav1d-8bit

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/android \
    $(LOCAL_PATH)/android/$(TARGET_ARCH_ABI) \
    $(LOCAL_PATH)/include \

LOCAL_SRC_FILES := \
    src/cdef_apply_tmpl.c \
    src/cdef_tmpl.c \
    src/fg_apply_tmpl.c \
    src/filmgrain_tmpl.c \
    src/ipred_prepare_tmpl.c \
    src/ipred_tmpl.c \
    src/itx_tmpl.c \
    src/lf_apply_tmpl.c \
    src/loopfilter_tmpl.c \
    src/looprestoration_tmpl.c \
    src/lr_apply_tmpl.c \
    src/mc_tmpl.c \
    src/recon_tmpl.c \

LOCAL_CFLAGS := -DBITDEPTH=8

LOCAL_MODULE := dav1d-8bit

include $(BUILD_STATIC_LIBRARY)


### dav1d-16bit

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/android \
    $(LOCAL_PATH)/android/$(TARGET_ARCH_ABI) \
    $(LOCAL_PATH)/include \

LOCAL_SRC_FILES :=  \
    src/cdef_apply_tmpl.c \
    src/cdef_tmpl.c \
    src/fg_apply_tmpl.c \
    src/filmgrain_tmpl.c \
    src/ipred_prepare_tmpl.c \
    src/ipred_tmpl.c \
    src/itx_tmpl.c \
    src/lf_apply_tmpl.c \
    src/loopfilter_tmpl.c \
    src/looprestoration_tmpl.c \
    src/lr_apply_tmpl.c \
    src/mc_tmpl.c \
    src/recon_tmpl.c \

LOCAL_CFLAGS := -DBITDEPTH=16

LOCAL_MODULE := dav1d-16bit

include $(BUILD_STATIC_LIBRARY)
