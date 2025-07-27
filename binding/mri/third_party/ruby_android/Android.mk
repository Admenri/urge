LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


LOCAL_MODULE:= ruby
LOCAL_CFLAGS:= -DRUBY_EXPORT -pthread

ifeq ($(TARGET_ARCH_ABI), armeabi)
	LOCAL_CFLAGS += -DARCH_32BIT
else ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
	LOCAL_CFLAGS += -DARCH_32BIT
else ifeq ($(TARGET_ARCH_ABI), x86)
	LOCAL_CFLAGS += -DARCH_32BIT
else ifeq ($(TARGET_ARCH_ABI), mips)
	LOCAL_CFLAGS += -DARCH_32BIT
endif

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)
LOCAL_SRC_FILES := \
	$(LOCAL_PATH)/main.c \
	$(LOCAL_PATH)/dmydln.c \
	$(LOCAL_PATH)/miniinit.c \
	$(LOCAL_PATH)/miniprelude.c \
	$(LOCAL_PATH)/array.c \
	$(LOCAL_PATH)/bignum.c \
	$(LOCAL_PATH)/class.c \
	$(LOCAL_PATH)/compar.c \
	$(LOCAL_PATH)/complex.c \
	$(LOCAL_PATH)/dir.c \
	$(LOCAL_PATH)/dln_find.c \
	$(LOCAL_PATH)/encoding.c \
	$(LOCAL_PATH)/enum.c \
	$(LOCAL_PATH)/enumerator.c \
	$(LOCAL_PATH)/error.c \
	$(LOCAL_PATH)/eval.c \
	$(LOCAL_PATH)/load.c \
	$(LOCAL_PATH)/proc.c \
	$(LOCAL_PATH)/file.c \
	$(LOCAL_PATH)/gc.c \
	$(LOCAL_PATH)/hash.c \
	$(LOCAL_PATH)/inits.c \
	$(LOCAL_PATH)/io.c \
	$(LOCAL_PATH)/marshal.c \
	$(LOCAL_PATH)/math.c \
	$(LOCAL_PATH)/node.c \
	$(LOCAL_PATH)/numeric.c \
	$(LOCAL_PATH)/object.c \
	$(LOCAL_PATH)/pack.c \
	$(LOCAL_PATH)/parse.c \
	$(LOCAL_PATH)/process.c \
	$(LOCAL_PATH)/random.c \
	$(LOCAL_PATH)/range.c \
	$(LOCAL_PATH)/rational.c \
	$(LOCAL_PATH)/re.c \
	$(LOCAL_PATH)/regcomp.c \
	$(LOCAL_PATH)/regenc.c \
	$(LOCAL_PATH)/regerror.c \
	$(LOCAL_PATH)/regexec.c \
	$(LOCAL_PATH)/regparse.c \
	$(LOCAL_PATH)/regsyntax.c \
	$(LOCAL_PATH)/ruby.c \
	$(LOCAL_PATH)/safe.c \
	$(LOCAL_PATH)/signal.c \
	$(LOCAL_PATH)/sprintf.c \
	$(LOCAL_PATH)/st.c \
	$(LOCAL_PATH)/strftime.c \
	$(LOCAL_PATH)/string.c \
	$(LOCAL_PATH)/struct.c \
	$(LOCAL_PATH)/symbol.c \
	$(LOCAL_PATH)/time.c \
	$(LOCAL_PATH)/transcode.c \
	$(LOCAL_PATH)/util.c \
	$(LOCAL_PATH)/variable.c \
	$(LOCAL_PATH)/version.c \
	$(LOCAL_PATH)/compile.c \
	$(LOCAL_PATH)/debug.c \
	$(LOCAL_PATH)/iseq.c \
	$(LOCAL_PATH)/vm.c \
	$(LOCAL_PATH)/vm_dump.c \
	$(LOCAL_PATH)/vm_backtrace.c \
	$(LOCAL_PATH)/vm_trace.c \
	$(LOCAL_PATH)/thread.c \
	$(LOCAL_PATH)/cont.c \
	$(LOCAL_PATH)/enc/ascii.c \
	$(LOCAL_PATH)/enc/us_ascii.c \
	$(LOCAL_PATH)/enc/unicode.c \
	$(LOCAL_PATH)/enc/utf_8.c \
	$(LOCAL_PATH)/newline.c \
	$(LOCAL_PATH)/missing/setproctitle.c \
	$(LOCAL_PATH)/missing/strlcat.c \
	$(LOCAL_PATH)/missing/strlcpy.c \
	$(LOCAL_PATH)/missing/crypt.c \
	$(LOCAL_PATH)/addr2line.c \
	$(LOCAL_PATH)/dmyext.c \
	$(LOCAL_PATH)/dmyenc.c \
	$(LOCAL_PATH)/ext/zlib/zlib.c

ifeq ($(TARGET_ARCH_ABI), armeabi)
	LOCAL_CFLAGS += -DMY_32BIT
else ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
	LOCAL_CFLAGS += -DMY_32BIT
else ifeq ($(TARGET_ARCH_ABI), x86)
	LOCAL_CFLAGS += -DMY_32BIT
else ifeq ($(TARGET_ARCH_ABI), mips)
	LOCAL_CFLAGS += -DMY_32BIT
endif

#LOCAL_LDLIBS := -ldl -lm -lz
include $(BUILD_STATIC_LIBRARY)
