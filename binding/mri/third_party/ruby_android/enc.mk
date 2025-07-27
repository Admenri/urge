V = 0
Q1 = $(V:1=)
Q = $(Q1:0=@)
n=$(NULLCMD)
ECHO1 = $(V:1=@$n)
ECHO = $(ECHO1:0=@echo)

encsrcdir = ./enc
topdir = .
prefix = /usr/local
exec_prefix = $(prefix)
libdir = $(exec_prefix)/lib
top_srcdir = $(encsrcdir:/enc=)
srcdir = $(top_srcdir)
arch = x86_64-linux
EXTOUT = .ext
hdrdir = $(srcdir)/include
arch_hdrdir = $(EXTOUT)/include/$(arch)
ENCSODIR = $(EXTOUT)/$(arch)/enc
TRANSSODIR = $(ENCSODIR)/trans
DLEXT = so
OBJEXT = o
LIBEXT = a

BUILTIN_ENCS	= ascii.c us_ascii.c\
		  unicode.c utf_8.c

BUILTIN_TRANSES	= newline.trans

RUBY_SO_NAME = ruby
LIBRUBY = $(LIBRUBY_SO)
LIBRUBYARG_SHARED = -Wl,-R$(libdir) -L$(libdir) -l$(RUBY_SO_NAME)
LIBRUBYARG_STATIC = $(LIBRUBYARG_SHARED)

empty =
AR = ar
CC = gcc
ARFLAGS = rcu $(empty)
RANLIB = ranlib
OUTFLAG = -o $(empty)
COUTFLAG = -o $(empty)
CFLAGS = $(CCDLFLAGS) $(cflags)  -fPIC 
cflags =  $(optflags) $(debugflags) $(warnflags)
optflags = -O3 -fno-fast-math
debugflags = -ggdb3
warnflags = -Wall -Wextra -Wno-unused-parameter -Wno-parentheses -Wno-long-long -Wno-missing-field-initializers -Wunused-variable -Wpointer-arith -Wwrite-strings -Wdeclaration-after-statement -Wimplicit-function-declaration -Wdeprecated-declarations -Wno-packed-bitfield-compat
CCDLFLAGS = -fPIC
INCFLAGS = -I. -I$(arch_hdrdir) -I$(hdrdir) -I$(top_srcdir)
DEFS = 
CPPFLAGS =  $(DEFS) $(cppflags) -DONIG_ENC_REGISTER=rb_enc_register
LDFLAGS = -L. -fstack-protector -rdynamic -Wl,-export-dynamic
LDSHARED = $(CC) -shared
ldflags  = $(LDFLAGS)
dldflags = 
extdldflags = 
archflag = 
DLDFLAGS = $(ldflags) $(dldflags) $(extdldflags) $(archflag)
RUBY     = $(MINIRUBY)
TARGET_NAME_ = $(@F) # BSD make seems unable to deal with @F with substitution
TARGET_NAME = $(TARGET_NAME_:.so=)
TARGET_ENTRY = Init_$(TARGET_NAME)

WORKDIRS = $(ENCSODIR) $(TRANSSODIR) enc enc/trans

NULLCMD = :
RM = rm -f
RMDIR = rmdir --ignore-fail-on-non-empty
RMDIRS = rmdir --ignore-fail-on-non-empty -p
MAKEDIRS = /bin/mkdir -p

.SUFFIXES: .trans

all: make-workdir

make-workdir:
	$(Q)$(MAKEDIRS) $(WORKDIRS)

clean:

distclean: clean
	$(Q)$(RM) enc.mk
realclean: distclean clean-srcs

#### depend ####

.SUFFIXES: .trans .c


VPATH = $(arch_hdrdir)/ruby:$(hdrdir)/ruby:$(srcdir):$(encsrcdir)
LIBPATH =  -L. -L$(topdir)
LIBS = $(LIBRUBYARG_SHARED) -lpthread -lgmp -ldl -lcrypt -lm  $(EXTLIBS)

ENCOBJS = enc/encdb.$(OBJEXT) \
	  enc/big5.$(OBJEXT) \
	  enc/cp949.$(OBJEXT) \
	  enc/emacs_mule.$(OBJEXT) \
	  enc/euc_jp.$(OBJEXT) \
	  enc/euc_kr.$(OBJEXT) \
	  enc/euc_tw.$(OBJEXT) \
	  enc/gb2312.$(OBJEXT) \
	  enc/gb18030.$(OBJEXT) \
	  enc/gbk.$(OBJEXT) \
	  enc/iso_8859_1.$(OBJEXT) \
	  enc/iso_8859_2.$(OBJEXT) \
	  enc/iso_8859_3.$(OBJEXT) \
	  enc/iso_8859_4.$(OBJEXT) \
	  enc/iso_8859_5.$(OBJEXT) \
	  enc/iso_8859_6.$(OBJEXT) \
	  enc/iso_8859_7.$(OBJEXT) \
	  enc/iso_8859_8.$(OBJEXT) \
	  enc/iso_8859_9.$(OBJEXT) \
	  enc/iso_8859_10.$(OBJEXT) \
	  enc/iso_8859_11.$(OBJEXT) \
	  enc/iso_8859_13.$(OBJEXT) \
	  enc/iso_8859_14.$(OBJEXT) \
	  enc/iso_8859_15.$(OBJEXT) \
	  enc/iso_8859_16.$(OBJEXT) \
	  enc/koi8_r.$(OBJEXT) \
	  enc/koi8_u.$(OBJEXT) \
	  enc/shift_jis.$(OBJEXT) \
	  enc/utf_16be.$(OBJEXT) \
	  enc/utf_16le.$(OBJEXT) \
	  enc/utf_32be.$(OBJEXT) \
	  enc/utf_32le.$(OBJEXT) \
	  enc/windows_31j.$(OBJEXT) \
	  enc/windows_1251.$(OBJEXT)

ENCSOS = $(ENCSODIR)/encdb.$(DLEXT) \
	 $(ENCSODIR)/big5.$(DLEXT) \
	 $(ENCSODIR)/cp949.$(DLEXT) \
	 $(ENCSODIR)/emacs_mule.$(DLEXT) \
	 $(ENCSODIR)/euc_jp.$(DLEXT) \
	 $(ENCSODIR)/euc_kr.$(DLEXT) \
	 $(ENCSODIR)/euc_tw.$(DLEXT) \
	 $(ENCSODIR)/gb2312.$(DLEXT) \
	 $(ENCSODIR)/gb18030.$(DLEXT) \
	 $(ENCSODIR)/gbk.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_1.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_2.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_3.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_4.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_5.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_6.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_7.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_8.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_9.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_10.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_11.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_13.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_14.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_15.$(DLEXT) \
	 $(ENCSODIR)/iso_8859_16.$(DLEXT) \
	 $(ENCSODIR)/koi8_r.$(DLEXT) \
	 $(ENCSODIR)/koi8_u.$(DLEXT) \
	 $(ENCSODIR)/shift_jis.$(DLEXT) \
	 $(ENCSODIR)/utf_16be.$(DLEXT) \
	 $(ENCSODIR)/utf_16le.$(DLEXT) \
	 $(ENCSODIR)/utf_32be.$(DLEXT) \
	 $(ENCSODIR)/utf_32le.$(DLEXT) \
	 $(ENCSODIR)/windows_31j.$(DLEXT) \
	 $(ENCSODIR)/windows_1251.$(DLEXT)

ENCCLEANLIBS = 
ENCCLEANOBJS = 
LIBENC=enc/libenc.$(LIBEXT)

TRANSVPATH = $(srcdir)/enc/trans

TRANSCSRCS = enc/trans/big5.c \
	     enc/trans/chinese.c \
	     enc/trans/emoji.c \
	     enc/trans/emoji_iso2022_kddi.c \
	     enc/trans/emoji_sjis_docomo.c \
	     enc/trans/emoji_sjis_kddi.c \
	     enc/trans/emoji_sjis_softbank.c \
	     enc/trans/escape.c \
	     enc/trans/gb18030.c \
	     enc/trans/gbk.c \
	     enc/trans/iso2022.c \
	     enc/trans/japanese.c \
	     enc/trans/japanese_euc.c \
	     enc/trans/japanese_sjis.c \
	     enc/trans/korean.c \
	     enc/trans/single_byte.c \
	     enc/trans/utf8_mac.c \
	     enc/trans/utf_16_32.c

TRANSOBJS = enc/trans/transdb.$(OBJEXT) \
	    enc/trans/big5.$(OBJEXT) \
	    enc/trans/chinese.$(OBJEXT) \
	    enc/trans/emoji.$(OBJEXT) \
	    enc/trans/emoji_iso2022_kddi.$(OBJEXT) \
	    enc/trans/emoji_sjis_docomo.$(OBJEXT) \
	    enc/trans/emoji_sjis_kddi.$(OBJEXT) \
	    enc/trans/emoji_sjis_softbank.$(OBJEXT) \
	    enc/trans/escape.$(OBJEXT) \
	    enc/trans/gb18030.$(OBJEXT) \
	    enc/trans/gbk.$(OBJEXT) \
	    enc/trans/iso2022.$(OBJEXT) \
	    enc/trans/japanese.$(OBJEXT) \
	    enc/trans/japanese_euc.$(OBJEXT) \
	    enc/trans/japanese_sjis.$(OBJEXT) \
	    enc/trans/korean.$(OBJEXT) \
	    enc/trans/single_byte.$(OBJEXT) \
	    enc/trans/utf8_mac.$(OBJEXT) \
	    enc/trans/utf_16_32.$(OBJEXT)

TRANSSOS = $(ENCSODIR)/trans/transdb.$(DLEXT) \
	   $(ENCSODIR)/trans/big5.$(DLEXT) \
	   $(ENCSODIR)/trans/chinese.$(DLEXT) \
	   $(ENCSODIR)/trans/emoji.$(DLEXT) \
	   $(ENCSODIR)/trans/emoji_iso2022_kddi.$(DLEXT) \
	   $(ENCSODIR)/trans/emoji_sjis_docomo.$(DLEXT) \
	   $(ENCSODIR)/trans/emoji_sjis_kddi.$(DLEXT) \
	   $(ENCSODIR)/trans/emoji_sjis_softbank.$(DLEXT) \
	   $(ENCSODIR)/trans/escape.$(DLEXT) \
	   $(ENCSODIR)/trans/gb18030.$(DLEXT) \
	   $(ENCSODIR)/trans/gbk.$(DLEXT) \
	   $(ENCSODIR)/trans/iso2022.$(DLEXT) \
	   $(ENCSODIR)/trans/japanese.$(DLEXT) \
	   $(ENCSODIR)/trans/japanese_euc.$(DLEXT) \
	   $(ENCSODIR)/trans/japanese_sjis.$(DLEXT) \
	   $(ENCSODIR)/trans/korean.$(DLEXT) \
	   $(ENCSODIR)/trans/single_byte.$(DLEXT) \
	   $(ENCSODIR)/trans/utf8_mac.$(DLEXT) \
	   $(ENCSODIR)/trans/utf_16_32.$(DLEXT)

TRANSCLEANLIBS = 
TRANSCLEANOBJS = 
LIBTRANS=enc/libtrans.$(LIBEXT)

encs: all
all: enc trans
libencs: libenc libtrans
enc: $(ENCSOS)
libenc: $(LIBENC)
trans: $(TRANSSOS)
libtrans: $(LIBTRANS)

$(LIBENC): $(ENCOBJS)
	@$(RM) $@
	$(ECHO) linking statically-linked encoding library $@
	$(Q) $(AR) $(ARFLAGS)$@ $(ENCOBJS)
	@-$(RANLIB) $@ 2> /dev/null || true
$(LIBTRANS): $(TRANSOBJS)
	@$(RM) $@
	$(ECHO) linking statically-linked transcoder library $@
	$(Q) $(AR) $(ARFLAGS)$@ $(TRANSOBJS)
	@-$(RANLIB) $@ 2> /dev/null || true

srcs: $(TRANSCSRCS)

.trans.c:
	$(ECHO) generating table from $@
	$(Q)$(MINIRUBY) "$(srcdir)/tool/transcode-tblgen.rb" -vo "$@" "$<"


enc/emacs_mule.$(OBJEXT): regint.h
enc/euc_jp.$(OBJEXT): regint.h enc/jis/props.h
enc/shift_jis.$(OBJEXT): regint.h enc/jis/props.h
enc/windows_31j.$(OBJEXT): enc/shift_jis.c regint.h enc/jis/props.h

enc/trans/big5.c: enc/trans/big5.trans
enc/trans/big5.c: enc/trans/big5-hkscs-tbl.rb enc/trans/big5-uao-tbl.rb $(srcdir)/tool/transcode-tblgen.rb

enc/trans/chinese.c: enc/trans/chinese.trans
enc/trans/chinese.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/emoji.c: enc/trans/emoji.trans
enc/trans/emoji.c: enc/trans/emoji-exchange-tbl.rb $(srcdir)/tool/transcode-tblgen.rb

enc/trans/emoji_iso2022_kddi.c: enc/trans/emoji_iso2022_kddi.trans
enc/trans/emoji_iso2022_kddi.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/emoji_sjis_docomo.c: enc/trans/emoji_sjis_docomo.trans
enc/trans/emoji_sjis_docomo.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/emoji_sjis_kddi.c: enc/trans/emoji_sjis_kddi.trans
enc/trans/emoji_sjis_kddi.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/emoji_sjis_softbank.c: enc/trans/emoji_sjis_softbank.trans
enc/trans/emoji_sjis_softbank.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/escape.c: enc/trans/escape.trans
enc/trans/escape.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/gb18030.c: enc/trans/gb18030.trans
enc/trans/gb18030.c: enc/trans/gb18030-tbl.rb $(srcdir)/tool/transcode-tblgen.rb

enc/trans/gbk.c: enc/trans/gbk.trans
enc/trans/gbk.c: enc/trans/gbk-tbl.rb $(srcdir)/tool/transcode-tblgen.rb

enc/trans/iso2022.c: enc/trans/iso2022.trans
enc/trans/iso2022.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/japanese.c: enc/trans/japanese.trans
enc/trans/japanese.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/japanese_euc.c: enc/trans/japanese_euc.trans
enc/trans/japanese_euc.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/japanese_sjis.c: enc/trans/japanese_sjis.trans
enc/trans/japanese_sjis.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/korean.c: enc/trans/korean.trans
enc/trans/korean.c: enc/trans/euckr-tbl.rb enc/trans/cp949-tbl.rb $(srcdir)/tool/transcode-tblgen.rb

enc/trans/single_byte.c: enc/trans/single_byte.trans
enc/trans/single_byte.c:  $(srcdir)/tool/transcode-tblgen.rb

enc/trans/utf8_mac.c: enc/trans/utf8_mac.trans
enc/trans/utf8_mac.c: enc/trans/utf8_mac-tbl.rb $(srcdir)/tool/transcode-tblgen.rb

enc/trans/utf_16_32.c: enc/trans/utf_16_32.trans
enc/trans/utf_16_32.c:  $(srcdir)/tool/transcode-tblgen.rb

$(ENCSODIR)/encdb.$(DLEXT): enc/encdb.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/encdb.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/big5.$(DLEXT): enc/big5.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/big5.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/cp949.$(DLEXT): enc/cp949.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/cp949.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/emacs_mule.$(DLEXT): enc/emacs_mule.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/emacs_mule.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/euc_jp.$(DLEXT): enc/euc_jp.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/euc_jp.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/euc_kr.$(DLEXT): enc/euc_kr.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/euc_kr.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/euc_tw.$(DLEXT): enc/euc_tw.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/euc_tw.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/gb2312.$(DLEXT): enc/gb2312.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/gb2312.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/gb18030.$(DLEXT): enc/gb18030.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/gb18030.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/gbk.$(DLEXT): enc/gbk.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/gbk.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_1.$(DLEXT): enc/iso_8859_1.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_1.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_2.$(DLEXT): enc/iso_8859_2.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_2.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_3.$(DLEXT): enc/iso_8859_3.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_3.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_4.$(DLEXT): enc/iso_8859_4.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_4.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_5.$(DLEXT): enc/iso_8859_5.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_5.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_6.$(DLEXT): enc/iso_8859_6.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_6.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_7.$(DLEXT): enc/iso_8859_7.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_7.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_8.$(DLEXT): enc/iso_8859_8.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_8.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_9.$(DLEXT): enc/iso_8859_9.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_9.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_10.$(DLEXT): enc/iso_8859_10.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_10.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_11.$(DLEXT): enc/iso_8859_11.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_11.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_13.$(DLEXT): enc/iso_8859_13.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_13.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_14.$(DLEXT): enc/iso_8859_14.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_14.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_15.$(DLEXT): enc/iso_8859_15.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_15.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/iso_8859_16.$(DLEXT): enc/iso_8859_16.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/iso_8859_16.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/koi8_r.$(DLEXT): enc/koi8_r.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/koi8_r.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/koi8_u.$(DLEXT): enc/koi8_u.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/koi8_u.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/shift_jis.$(DLEXT): enc/shift_jis.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/shift_jis.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/utf_16be.$(DLEXT): enc/utf_16be.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/utf_16be.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/utf_16le.$(DLEXT): enc/utf_16le.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/utf_16le.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/utf_32be.$(DLEXT): enc/utf_32be.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/utf_32be.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/utf_32le.$(DLEXT): enc/utf_32le.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/utf_32le.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/windows_31j.$(DLEXT): enc/windows_31j.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/windows_31j.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(ENCSODIR)/windows_1251.$(DLEXT): enc/windows_1251.$(OBJEXT)
	$(ECHO) linking encoding $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/windows_1251.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/transdb.$(DLEXT): enc/trans/transdb.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/transdb.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/big5.$(DLEXT): enc/trans/big5.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/big5.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/chinese.$(DLEXT): enc/trans/chinese.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/chinese.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/emoji.$(DLEXT): enc/trans/emoji.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/emoji.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/emoji_iso2022_kddi.$(DLEXT): enc/trans/emoji_iso2022_kddi.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/emoji_iso2022_kddi.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/emoji_sjis_docomo.$(DLEXT): enc/trans/emoji_sjis_docomo.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/emoji_sjis_docomo.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/emoji_sjis_kddi.$(DLEXT): enc/trans/emoji_sjis_kddi.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/emoji_sjis_kddi.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/emoji_sjis_softbank.$(DLEXT): enc/trans/emoji_sjis_softbank.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/emoji_sjis_softbank.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/escape.$(DLEXT): enc/trans/escape.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/escape.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/gb18030.$(DLEXT): enc/trans/gb18030.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/gb18030.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/gbk.$(DLEXT): enc/trans/gbk.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/gbk.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/iso2022.$(DLEXT): enc/trans/iso2022.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/iso2022.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/japanese.$(DLEXT): enc/trans/japanese.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/japanese.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/japanese_euc.$(DLEXT): enc/trans/japanese_euc.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/japanese_euc.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/japanese_sjis.$(DLEXT): enc/trans/japanese_sjis.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/japanese_sjis.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/korean.$(DLEXT): enc/trans/korean.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/korean.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/single_byte.$(DLEXT): enc/trans/single_byte.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/single_byte.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/utf8_mac.$(DLEXT): enc/trans/utf8_mac.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/utf8_mac.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

$(TRANSSODIR)/utf_16_32.$(DLEXT): enc/trans/utf_16_32.$(OBJEXT)
	$(ECHO) linking transcoder $(@F)
	$(Q)$(MAKEDIRS) "$(@D)"
	$(Q)$(LDSHARED) -o $@ enc/trans/utf_16_32.$(OBJEXT) $(LIBPATH) $(DLDFLAGS) $(LOCAL_LIBS) $(LIBS)

enc/encdb.$(OBJEXT): $(encsrcdir)/encdb.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/encdb.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/encdb.c

enc/big5.$(OBJEXT): $(encsrcdir)/big5.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/big5.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/big5.c

enc/cp949.$(OBJEXT): $(encsrcdir)/cp949.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/cp949.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/cp949.c

enc/emacs_mule.$(OBJEXT): $(encsrcdir)/emacs_mule.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/emacs_mule.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/emacs_mule.c

enc/euc_jp.$(OBJEXT): $(encsrcdir)/euc_jp.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/euc_jp.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/euc_jp.c

enc/euc_kr.$(OBJEXT): $(encsrcdir)/euc_kr.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/euc_kr.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/euc_kr.c

enc/euc_tw.$(OBJEXT): $(encsrcdir)/euc_tw.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/euc_tw.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/euc_tw.c

enc/gb2312.$(OBJEXT): $(encsrcdir)/gb2312.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/gb2312.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/gb2312.c

enc/gb18030.$(OBJEXT): $(encsrcdir)/gb18030.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/gb18030.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/gb18030.c

enc/gbk.$(OBJEXT): $(encsrcdir)/gbk.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/gbk.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/gbk.c

enc/iso_8859_1.$(OBJEXT): $(encsrcdir)/iso_8859_1.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_1.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_1.c

enc/iso_8859_2.$(OBJEXT): $(encsrcdir)/iso_8859_2.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_2.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_2.c

enc/iso_8859_3.$(OBJEXT): $(encsrcdir)/iso_8859_3.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_3.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_3.c

enc/iso_8859_4.$(OBJEXT): $(encsrcdir)/iso_8859_4.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_4.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_4.c

enc/iso_8859_5.$(OBJEXT): $(encsrcdir)/iso_8859_5.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_5.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_5.c

enc/iso_8859_6.$(OBJEXT): $(encsrcdir)/iso_8859_6.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_6.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_6.c

enc/iso_8859_7.$(OBJEXT): $(encsrcdir)/iso_8859_7.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_7.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_7.c

enc/iso_8859_8.$(OBJEXT): $(encsrcdir)/iso_8859_8.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_8.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_8.c

enc/iso_8859_9.$(OBJEXT): $(encsrcdir)/iso_8859_9.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_9.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_9.c

enc/iso_8859_10.$(OBJEXT): $(encsrcdir)/iso_8859_10.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_10.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_10.c

enc/iso_8859_11.$(OBJEXT): $(encsrcdir)/iso_8859_11.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_11.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_11.c

enc/iso_8859_13.$(OBJEXT): $(encsrcdir)/iso_8859_13.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_13.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_13.c

enc/iso_8859_14.$(OBJEXT): $(encsrcdir)/iso_8859_14.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_14.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_14.c

enc/iso_8859_15.$(OBJEXT): $(encsrcdir)/iso_8859_15.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_15.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_15.c

enc/iso_8859_16.$(OBJEXT): $(encsrcdir)/iso_8859_16.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/iso_8859_16.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/iso_8859_16.c

enc/koi8_r.$(OBJEXT): $(encsrcdir)/koi8_r.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/koi8_r.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/koi8_r.c

enc/koi8_u.$(OBJEXT): $(encsrcdir)/koi8_u.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/koi8_u.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/koi8_u.c

enc/shift_jis.$(OBJEXT): $(encsrcdir)/shift_jis.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/shift_jis.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/shift_jis.c

enc/utf_16be.$(OBJEXT): $(encsrcdir)/utf_16be.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/utf_16be.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/utf_16be.c

enc/utf_16le.$(OBJEXT): $(encsrcdir)/utf_16le.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/utf_16le.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/utf_16le.c

enc/utf_32be.$(OBJEXT): $(encsrcdir)/utf_32be.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/utf_32be.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/utf_32be.c

enc/utf_32le.$(OBJEXT): $(encsrcdir)/utf_32le.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/utf_32le.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/utf_32le.c

enc/windows_31j.$(OBJEXT): $(encsrcdir)/windows_31j.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/windows_31j.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/windows_31j.c

enc/windows_1251.$(OBJEXT): $(encsrcdir)/windows_1251.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/windows_1251.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/windows_1251.c

enc/trans/transdb.$(OBJEXT): $(encsrcdir)/trans/transdb.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/transdb.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/transdb.c

enc/trans/big5.$(OBJEXT): $(encsrcdir)/trans/big5.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/big5.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/big5.c

enc/trans/chinese.$(OBJEXT): $(encsrcdir)/trans/chinese.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/chinese.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/chinese.c

enc/trans/emoji.$(OBJEXT): $(encsrcdir)/trans/emoji.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/emoji.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/emoji.c

enc/trans/emoji_iso2022_kddi.$(OBJEXT): $(encsrcdir)/trans/emoji_iso2022_kddi.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/emoji_iso2022_kddi.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/emoji_iso2022_kddi.c

enc/trans/emoji_sjis_docomo.$(OBJEXT): $(encsrcdir)/trans/emoji_sjis_docomo.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/emoji_sjis_docomo.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/emoji_sjis_docomo.c

enc/trans/emoji_sjis_kddi.$(OBJEXT): $(encsrcdir)/trans/emoji_sjis_kddi.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/emoji_sjis_kddi.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/emoji_sjis_kddi.c

enc/trans/emoji_sjis_softbank.$(OBJEXT): $(encsrcdir)/trans/emoji_sjis_softbank.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/emoji_sjis_softbank.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/emoji_sjis_softbank.c

enc/trans/escape.$(OBJEXT): $(encsrcdir)/trans/escape.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/escape.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/escape.c

enc/trans/gb18030.$(OBJEXT): $(encsrcdir)/trans/gb18030.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/gb18030.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/gb18030.c

enc/trans/gbk.$(OBJEXT): $(encsrcdir)/trans/gbk.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/gbk.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/gbk.c

enc/trans/iso2022.$(OBJEXT): $(encsrcdir)/trans/iso2022.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/iso2022.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/iso2022.c

enc/trans/japanese.$(OBJEXT): $(encsrcdir)/trans/japanese.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/japanese.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/japanese.c

enc/trans/japanese_euc.$(OBJEXT): $(encsrcdir)/trans/japanese_euc.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/japanese_euc.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/japanese_euc.c

enc/trans/japanese_sjis.$(OBJEXT): $(encsrcdir)/trans/japanese_sjis.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/japanese_sjis.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/japanese_sjis.c

enc/trans/korean.$(OBJEXT): $(encsrcdir)/trans/korean.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/korean.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/korean.c

enc/trans/single_byte.$(OBJEXT): $(encsrcdir)/trans/single_byte.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/single_byte.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/single_byte.c

enc/trans/utf8_mac.$(OBJEXT): $(encsrcdir)/trans/utf8_mac.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/utf8_mac.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/utf8_mac.c

enc/trans/utf_16_32.$(OBJEXT): $(encsrcdir)/trans/utf_16_32.c
	-$(Q)$(MAKEDIRS) "$(@D)"
	$(ECHO) compiling $(encsrcdir)/trans/utf_16_32.c
	$(Q)$(CC) $(INCFLAGS) $(CPPFLAGS) $(CFLAGS) $(COUTFLAG)$@ -c $(encsrcdir)/trans/utf_16_32.c


enc/encdb.$(OBJEXT): encdb.h
enc/trans/transdb.$(OBJEXT): transdb.h

clean:
	$(Q)$(RM) $(ENCSOS)
	$(Q)$(RM) $(LIBENC)
	$(Q)$(RM) $(ENCOBJS)
	$(Q)$(RM) $(ENCCLEANOBJS)
	$(Q)$(RM) $(ENCCLEANLIBS)
	$(Q)$(RM) $(TRANSSOS)
	$(Q)$(RM) $(LIBTRANS)
	$(Q)$(RM) $(TRANSOBJS)
	$(Q)$(RM) $(TRANSCLEANOBJS)
	$(Q)$(RM) $(TRANSCLEANLIBS)
	-$(Q)$(RMDIR) enc/trans 2> /dev/null || true
	-$(Q)$(RMDIR) enc 2> /dev/null || true
	-$(Q)$(RMDIR) $(TRANSSODIR) 2> /dev/null || true
	-$(Q)$(RMDIR) $(ENCSODIR) 2> /dev/null || true

clean-srcs:
	$(Q)$(RM) $(TRANSCSRCS)
	-$(Q)$(RMDIR) enc/trans 2> /dev/null || true
	$(Q)$(RM) enc/unicode/name2ctype.h
	$(Q)$(RM) enc/jis/props.h
	-$(Q)$(RMDIR) enc/unicode 2> /dev/null || true
	-$(Q)$(RMDIR) enc/props 2> /dev/null || true
	-$(Q)$(RMDIR) enc 2> /dev/null || true



# AUTOGENERATED DEPENDENCIES START
enc/big5.$(OBJEXT): $(top_srcdir)/regenc.h
enc/big5.$(OBJEXT): config.h
enc/big5.$(OBJEXT): defines.h
enc/big5.$(OBJEXT): enc/big5.c
enc/big5.$(OBJEXT): missing.h
enc/big5.$(OBJEXT): oniguruma.h
enc/cp949.$(OBJEXT): $(top_srcdir)/regenc.h
enc/cp949.$(OBJEXT): config.h
enc/cp949.$(OBJEXT): defines.h
enc/cp949.$(OBJEXT): enc/cp949.c
enc/cp949.$(OBJEXT): missing.h
enc/cp949.$(OBJEXT): oniguruma.h
enc/emacs_mule.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/emacs_mule.$(OBJEXT): $(top_srcdir)/regenc.h
enc/emacs_mule.$(OBJEXT): $(top_srcdir)/regint.h
enc/emacs_mule.$(OBJEXT): config.h
enc/emacs_mule.$(OBJEXT): defines.h
enc/emacs_mule.$(OBJEXT): enc/emacs_mule.c
enc/emacs_mule.$(OBJEXT): intern.h
enc/emacs_mule.$(OBJEXT): missing.h
enc/emacs_mule.$(OBJEXT): oniguruma.h
enc/emacs_mule.$(OBJEXT): st.h
enc/emacs_mule.$(OBJEXT): subst.h
enc/encdb.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/encdb.$(OBJEXT): $(top_srcdir)/include/ruby.h
enc/encdb.$(OBJEXT): $(top_srcdir)/internal.h
enc/encdb.$(OBJEXT): config.h
enc/encdb.$(OBJEXT): defines.h
enc/encdb.$(OBJEXT): enc/encdb.c
enc/encdb.$(OBJEXT): encdb.h
enc/encdb.$(OBJEXT): encoding.h
enc/encdb.$(OBJEXT): intern.h
enc/encdb.$(OBJEXT): io.h
enc/encdb.$(OBJEXT): missing.h
enc/encdb.$(OBJEXT): oniguruma.h
enc/encdb.$(OBJEXT): st.h
enc/encdb.$(OBJEXT): subst.h
enc/euc_jp.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/euc_jp.$(OBJEXT): $(top_srcdir)/regenc.h
enc/euc_jp.$(OBJEXT): $(top_srcdir)/regint.h
enc/euc_jp.$(OBJEXT): config.h
enc/euc_jp.$(OBJEXT): defines.h
enc/euc_jp.$(OBJEXT): enc/euc_jp.c
enc/euc_jp.$(OBJEXT): enc/jis/props.h
enc/euc_jp.$(OBJEXT): enc/jis/props.kwd
enc/euc_jp.$(OBJEXT): intern.h
enc/euc_jp.$(OBJEXT): missing.h
enc/euc_jp.$(OBJEXT): oniguruma.h
enc/euc_jp.$(OBJEXT): st.h
enc/euc_jp.$(OBJEXT): subst.h
enc/euc_kr.$(OBJEXT): $(top_srcdir)/regenc.h
enc/euc_kr.$(OBJEXT): config.h
enc/euc_kr.$(OBJEXT): defines.h
enc/euc_kr.$(OBJEXT): enc/euc_kr.c
enc/euc_kr.$(OBJEXT): missing.h
enc/euc_kr.$(OBJEXT): oniguruma.h
enc/euc_tw.$(OBJEXT): $(top_srcdir)/regenc.h
enc/euc_tw.$(OBJEXT): config.h
enc/euc_tw.$(OBJEXT): defines.h
enc/euc_tw.$(OBJEXT): enc/euc_tw.c
enc/euc_tw.$(OBJEXT): missing.h
enc/euc_tw.$(OBJEXT): oniguruma.h
enc/gb18030.$(OBJEXT): $(top_srcdir)/regenc.h
enc/gb18030.$(OBJEXT): config.h
enc/gb18030.$(OBJEXT): defines.h
enc/gb18030.$(OBJEXT): enc/gb18030.c
enc/gb18030.$(OBJEXT): missing.h
enc/gb18030.$(OBJEXT): oniguruma.h
enc/gb2312.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/gb2312.$(OBJEXT): $(top_srcdir)/regenc.h
enc/gb2312.$(OBJEXT): config.h
enc/gb2312.$(OBJEXT): defines.h
enc/gb2312.$(OBJEXT): enc/gb2312.c
enc/gb2312.$(OBJEXT): encoding.h
enc/gb2312.$(OBJEXT): intern.h
enc/gb2312.$(OBJEXT): missing.h
enc/gb2312.$(OBJEXT): oniguruma.h
enc/gb2312.$(OBJEXT): st.h
enc/gb2312.$(OBJEXT): subst.h
enc/gbk.$(OBJEXT): $(top_srcdir)/regenc.h
enc/gbk.$(OBJEXT): config.h
enc/gbk.$(OBJEXT): defines.h
enc/gbk.$(OBJEXT): enc/gbk.c
enc/gbk.$(OBJEXT): missing.h
enc/gbk.$(OBJEXT): oniguruma.h
enc/iso_8859_1.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_1.$(OBJEXT): config.h
enc/iso_8859_1.$(OBJEXT): defines.h
enc/iso_8859_1.$(OBJEXT): enc/iso_8859_1.c
enc/iso_8859_1.$(OBJEXT): missing.h
enc/iso_8859_1.$(OBJEXT): oniguruma.h
enc/iso_8859_10.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_10.$(OBJEXT): config.h
enc/iso_8859_10.$(OBJEXT): defines.h
enc/iso_8859_10.$(OBJEXT): enc/iso_8859_10.c
enc/iso_8859_10.$(OBJEXT): missing.h
enc/iso_8859_10.$(OBJEXT): oniguruma.h
enc/iso_8859_11.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_11.$(OBJEXT): config.h
enc/iso_8859_11.$(OBJEXT): defines.h
enc/iso_8859_11.$(OBJEXT): enc/iso_8859_11.c
enc/iso_8859_11.$(OBJEXT): missing.h
enc/iso_8859_11.$(OBJEXT): oniguruma.h
enc/iso_8859_13.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_13.$(OBJEXT): config.h
enc/iso_8859_13.$(OBJEXT): defines.h
enc/iso_8859_13.$(OBJEXT): enc/iso_8859_13.c
enc/iso_8859_13.$(OBJEXT): missing.h
enc/iso_8859_13.$(OBJEXT): oniguruma.h
enc/iso_8859_14.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_14.$(OBJEXT): config.h
enc/iso_8859_14.$(OBJEXT): defines.h
enc/iso_8859_14.$(OBJEXT): enc/iso_8859_14.c
enc/iso_8859_14.$(OBJEXT): missing.h
enc/iso_8859_14.$(OBJEXT): oniguruma.h
enc/iso_8859_15.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_15.$(OBJEXT): config.h
enc/iso_8859_15.$(OBJEXT): defines.h
enc/iso_8859_15.$(OBJEXT): enc/iso_8859_15.c
enc/iso_8859_15.$(OBJEXT): missing.h
enc/iso_8859_15.$(OBJEXT): oniguruma.h
enc/iso_8859_16.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_16.$(OBJEXT): config.h
enc/iso_8859_16.$(OBJEXT): defines.h
enc/iso_8859_16.$(OBJEXT): enc/iso_8859_16.c
enc/iso_8859_16.$(OBJEXT): missing.h
enc/iso_8859_16.$(OBJEXT): oniguruma.h
enc/iso_8859_2.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_2.$(OBJEXT): config.h
enc/iso_8859_2.$(OBJEXT): defines.h
enc/iso_8859_2.$(OBJEXT): enc/iso_8859_2.c
enc/iso_8859_2.$(OBJEXT): missing.h
enc/iso_8859_2.$(OBJEXT): oniguruma.h
enc/iso_8859_3.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_3.$(OBJEXT): config.h
enc/iso_8859_3.$(OBJEXT): defines.h
enc/iso_8859_3.$(OBJEXT): enc/iso_8859_3.c
enc/iso_8859_3.$(OBJEXT): missing.h
enc/iso_8859_3.$(OBJEXT): oniguruma.h
enc/iso_8859_4.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_4.$(OBJEXT): config.h
enc/iso_8859_4.$(OBJEXT): defines.h
enc/iso_8859_4.$(OBJEXT): enc/iso_8859_4.c
enc/iso_8859_4.$(OBJEXT): missing.h
enc/iso_8859_4.$(OBJEXT): oniguruma.h
enc/iso_8859_5.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_5.$(OBJEXT): config.h
enc/iso_8859_5.$(OBJEXT): defines.h
enc/iso_8859_5.$(OBJEXT): enc/iso_8859_5.c
enc/iso_8859_5.$(OBJEXT): missing.h
enc/iso_8859_5.$(OBJEXT): oniguruma.h
enc/iso_8859_6.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_6.$(OBJEXT): config.h
enc/iso_8859_6.$(OBJEXT): defines.h
enc/iso_8859_6.$(OBJEXT): enc/iso_8859_6.c
enc/iso_8859_6.$(OBJEXT): missing.h
enc/iso_8859_6.$(OBJEXT): oniguruma.h
enc/iso_8859_7.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_7.$(OBJEXT): config.h
enc/iso_8859_7.$(OBJEXT): defines.h
enc/iso_8859_7.$(OBJEXT): enc/iso_8859_7.c
enc/iso_8859_7.$(OBJEXT): missing.h
enc/iso_8859_7.$(OBJEXT): oniguruma.h
enc/iso_8859_8.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_8.$(OBJEXT): config.h
enc/iso_8859_8.$(OBJEXT): defines.h
enc/iso_8859_8.$(OBJEXT): enc/iso_8859_8.c
enc/iso_8859_8.$(OBJEXT): missing.h
enc/iso_8859_8.$(OBJEXT): oniguruma.h
enc/iso_8859_9.$(OBJEXT): $(top_srcdir)/regenc.h
enc/iso_8859_9.$(OBJEXT): config.h
enc/iso_8859_9.$(OBJEXT): defines.h
enc/iso_8859_9.$(OBJEXT): enc/iso_8859_9.c
enc/iso_8859_9.$(OBJEXT): missing.h
enc/iso_8859_9.$(OBJEXT): oniguruma.h
enc/koi8_r.$(OBJEXT): $(top_srcdir)/regenc.h
enc/koi8_r.$(OBJEXT): config.h
enc/koi8_r.$(OBJEXT): defines.h
enc/koi8_r.$(OBJEXT): enc/koi8_r.c
enc/koi8_r.$(OBJEXT): missing.h
enc/koi8_r.$(OBJEXT): oniguruma.h
enc/koi8_u.$(OBJEXT): $(top_srcdir)/regenc.h
enc/koi8_u.$(OBJEXT): config.h
enc/koi8_u.$(OBJEXT): defines.h
enc/koi8_u.$(OBJEXT): enc/koi8_u.c
enc/koi8_u.$(OBJEXT): missing.h
enc/koi8_u.$(OBJEXT): oniguruma.h
enc/shift_jis.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/shift_jis.$(OBJEXT): $(top_srcdir)/regenc.h
enc/shift_jis.$(OBJEXT): $(top_srcdir)/regint.h
enc/shift_jis.$(OBJEXT): config.h
enc/shift_jis.$(OBJEXT): defines.h
enc/shift_jis.$(OBJEXT): enc/jis/props.h
enc/shift_jis.$(OBJEXT): enc/jis/props.kwd
enc/shift_jis.$(OBJEXT): enc/shift_jis.c
enc/shift_jis.$(OBJEXT): intern.h
enc/shift_jis.$(OBJEXT): missing.h
enc/shift_jis.$(OBJEXT): oniguruma.h
enc/shift_jis.$(OBJEXT): st.h
enc/shift_jis.$(OBJEXT): subst.h
enc/trans/big5.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/big5.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/big5.$(OBJEXT): config.h
enc/trans/big5.$(OBJEXT): defines.h
enc/trans/big5.$(OBJEXT): enc/trans/big5.c
enc/trans/big5.$(OBJEXT): intern.h
enc/trans/big5.$(OBJEXT): missing.h
enc/trans/big5.$(OBJEXT): st.h
enc/trans/big5.$(OBJEXT): subst.h
enc/trans/chinese.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/chinese.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/chinese.$(OBJEXT): config.h
enc/trans/chinese.$(OBJEXT): defines.h
enc/trans/chinese.$(OBJEXT): enc/trans/chinese.c
enc/trans/chinese.$(OBJEXT): intern.h
enc/trans/chinese.$(OBJEXT): missing.h
enc/trans/chinese.$(OBJEXT): st.h
enc/trans/chinese.$(OBJEXT): subst.h
enc/trans/emoji.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/emoji.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/emoji.$(OBJEXT): config.h
enc/trans/emoji.$(OBJEXT): defines.h
enc/trans/emoji.$(OBJEXT): enc/trans/emoji.c
enc/trans/emoji.$(OBJEXT): intern.h
enc/trans/emoji.$(OBJEXT): missing.h
enc/trans/emoji.$(OBJEXT): st.h
enc/trans/emoji.$(OBJEXT): subst.h
enc/trans/emoji_iso2022_kddi.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/emoji_iso2022_kddi.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/emoji_iso2022_kddi.$(OBJEXT): config.h
enc/trans/emoji_iso2022_kddi.$(OBJEXT): defines.h
enc/trans/emoji_iso2022_kddi.$(OBJEXT): enc/trans/emoji_iso2022_kddi.c
enc/trans/emoji_iso2022_kddi.$(OBJEXT): intern.h
enc/trans/emoji_iso2022_kddi.$(OBJEXT): missing.h
enc/trans/emoji_iso2022_kddi.$(OBJEXT): st.h
enc/trans/emoji_iso2022_kddi.$(OBJEXT): subst.h
enc/trans/emoji_sjis_docomo.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/emoji_sjis_docomo.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/emoji_sjis_docomo.$(OBJEXT): config.h
enc/trans/emoji_sjis_docomo.$(OBJEXT): defines.h
enc/trans/emoji_sjis_docomo.$(OBJEXT): enc/trans/emoji_sjis_docomo.c
enc/trans/emoji_sjis_docomo.$(OBJEXT): intern.h
enc/trans/emoji_sjis_docomo.$(OBJEXT): missing.h
enc/trans/emoji_sjis_docomo.$(OBJEXT): st.h
enc/trans/emoji_sjis_docomo.$(OBJEXT): subst.h
enc/trans/emoji_sjis_kddi.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/emoji_sjis_kddi.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/emoji_sjis_kddi.$(OBJEXT): config.h
enc/trans/emoji_sjis_kddi.$(OBJEXT): defines.h
enc/trans/emoji_sjis_kddi.$(OBJEXT): enc/trans/emoji_sjis_kddi.c
enc/trans/emoji_sjis_kddi.$(OBJEXT): intern.h
enc/trans/emoji_sjis_kddi.$(OBJEXT): missing.h
enc/trans/emoji_sjis_kddi.$(OBJEXT): st.h
enc/trans/emoji_sjis_kddi.$(OBJEXT): subst.h
enc/trans/emoji_sjis_softbank.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/emoji_sjis_softbank.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/emoji_sjis_softbank.$(OBJEXT): config.h
enc/trans/emoji_sjis_softbank.$(OBJEXT): defines.h
enc/trans/emoji_sjis_softbank.$(OBJEXT): enc/trans/emoji_sjis_softbank.c
enc/trans/emoji_sjis_softbank.$(OBJEXT): intern.h
enc/trans/emoji_sjis_softbank.$(OBJEXT): missing.h
enc/trans/emoji_sjis_softbank.$(OBJEXT): st.h
enc/trans/emoji_sjis_softbank.$(OBJEXT): subst.h
enc/trans/escape.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/escape.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/escape.$(OBJEXT): config.h
enc/trans/escape.$(OBJEXT): defines.h
enc/trans/escape.$(OBJEXT): enc/trans/escape.c
enc/trans/escape.$(OBJEXT): intern.h
enc/trans/escape.$(OBJEXT): missing.h
enc/trans/escape.$(OBJEXT): st.h
enc/trans/escape.$(OBJEXT): subst.h
enc/trans/gb18030.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/gb18030.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/gb18030.$(OBJEXT): config.h
enc/trans/gb18030.$(OBJEXT): defines.h
enc/trans/gb18030.$(OBJEXT): enc/trans/gb18030.c
enc/trans/gb18030.$(OBJEXT): intern.h
enc/trans/gb18030.$(OBJEXT): missing.h
enc/trans/gb18030.$(OBJEXT): st.h
enc/trans/gb18030.$(OBJEXT): subst.h
enc/trans/gbk.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/gbk.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/gbk.$(OBJEXT): config.h
enc/trans/gbk.$(OBJEXT): defines.h
enc/trans/gbk.$(OBJEXT): enc/trans/gbk.c
enc/trans/gbk.$(OBJEXT): intern.h
enc/trans/gbk.$(OBJEXT): missing.h
enc/trans/gbk.$(OBJEXT): st.h
enc/trans/gbk.$(OBJEXT): subst.h
enc/trans/iso2022.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/iso2022.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/iso2022.$(OBJEXT): config.h
enc/trans/iso2022.$(OBJEXT): defines.h
enc/trans/iso2022.$(OBJEXT): enc/trans/iso2022.c
enc/trans/iso2022.$(OBJEXT): intern.h
enc/trans/iso2022.$(OBJEXT): missing.h
enc/trans/iso2022.$(OBJEXT): st.h
enc/trans/iso2022.$(OBJEXT): subst.h
enc/trans/japanese.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/japanese.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/japanese.$(OBJEXT): config.h
enc/trans/japanese.$(OBJEXT): defines.h
enc/trans/japanese.$(OBJEXT): enc/trans/japanese.c
enc/trans/japanese.$(OBJEXT): intern.h
enc/trans/japanese.$(OBJEXT): missing.h
enc/trans/japanese.$(OBJEXT): st.h
enc/trans/japanese.$(OBJEXT): subst.h
enc/trans/japanese_euc.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/japanese_euc.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/japanese_euc.$(OBJEXT): config.h
enc/trans/japanese_euc.$(OBJEXT): defines.h
enc/trans/japanese_euc.$(OBJEXT): enc/trans/japanese_euc.c
enc/trans/japanese_euc.$(OBJEXT): intern.h
enc/trans/japanese_euc.$(OBJEXT): missing.h
enc/trans/japanese_euc.$(OBJEXT): st.h
enc/trans/japanese_euc.$(OBJEXT): subst.h
enc/trans/japanese_sjis.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/japanese_sjis.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/japanese_sjis.$(OBJEXT): config.h
enc/trans/japanese_sjis.$(OBJEXT): defines.h
enc/trans/japanese_sjis.$(OBJEXT): enc/trans/japanese_sjis.c
enc/trans/japanese_sjis.$(OBJEXT): intern.h
enc/trans/japanese_sjis.$(OBJEXT): missing.h
enc/trans/japanese_sjis.$(OBJEXT): st.h
enc/trans/japanese_sjis.$(OBJEXT): subst.h
enc/trans/korean.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/korean.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/korean.$(OBJEXT): config.h
enc/trans/korean.$(OBJEXT): defines.h
enc/trans/korean.$(OBJEXT): enc/trans/korean.c
enc/trans/korean.$(OBJEXT): intern.h
enc/trans/korean.$(OBJEXT): missing.h
enc/trans/korean.$(OBJEXT): st.h
enc/trans/korean.$(OBJEXT): subst.h
enc/trans/single_byte.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/single_byte.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/single_byte.$(OBJEXT): config.h
enc/trans/single_byte.$(OBJEXT): defines.h
enc/trans/single_byte.$(OBJEXT): enc/trans/single_byte.c
enc/trans/single_byte.$(OBJEXT): intern.h
enc/trans/single_byte.$(OBJEXT): missing.h
enc/trans/single_byte.$(OBJEXT): st.h
enc/trans/single_byte.$(OBJEXT): subst.h
enc/trans/transdb.$(OBJEXT): enc/trans/transdb.c
enc/trans/transdb.$(OBJEXT): transdb.h
enc/trans/utf8_mac.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/utf8_mac.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/utf8_mac.$(OBJEXT): config.h
enc/trans/utf8_mac.$(OBJEXT): defines.h
enc/trans/utf8_mac.$(OBJEXT): enc/trans/utf8_mac.c
enc/trans/utf8_mac.$(OBJEXT): intern.h
enc/trans/utf8_mac.$(OBJEXT): missing.h
enc/trans/utf8_mac.$(OBJEXT): st.h
enc/trans/utf8_mac.$(OBJEXT): subst.h
enc/trans/utf_16_32.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/trans/utf_16_32.$(OBJEXT): $(top_srcdir)/transcode_data.h
enc/trans/utf_16_32.$(OBJEXT): config.h
enc/trans/utf_16_32.$(OBJEXT): defines.h
enc/trans/utf_16_32.$(OBJEXT): enc/trans/utf_16_32.c
enc/trans/utf_16_32.$(OBJEXT): intern.h
enc/trans/utf_16_32.$(OBJEXT): missing.h
enc/trans/utf_16_32.$(OBJEXT): st.h
enc/trans/utf_16_32.$(OBJEXT): subst.h
enc/utf_16be.$(OBJEXT): $(top_srcdir)/regenc.h
enc/utf_16be.$(OBJEXT): config.h
enc/utf_16be.$(OBJEXT): defines.h
enc/utf_16be.$(OBJEXT): enc/utf_16be.c
enc/utf_16be.$(OBJEXT): missing.h
enc/utf_16be.$(OBJEXT): oniguruma.h
enc/utf_16le.$(OBJEXT): $(top_srcdir)/regenc.h
enc/utf_16le.$(OBJEXT): config.h
enc/utf_16le.$(OBJEXT): defines.h
enc/utf_16le.$(OBJEXT): enc/utf_16le.c
enc/utf_16le.$(OBJEXT): missing.h
enc/utf_16le.$(OBJEXT): oniguruma.h
enc/utf_32be.$(OBJEXT): $(top_srcdir)/regenc.h
enc/utf_32be.$(OBJEXT): config.h
enc/utf_32be.$(OBJEXT): defines.h
enc/utf_32be.$(OBJEXT): enc/utf_32be.c
enc/utf_32be.$(OBJEXT): missing.h
enc/utf_32be.$(OBJEXT): oniguruma.h
enc/utf_32le.$(OBJEXT): $(top_srcdir)/regenc.h
enc/utf_32le.$(OBJEXT): config.h
enc/utf_32le.$(OBJEXT): defines.h
enc/utf_32le.$(OBJEXT): enc/utf_32le.c
enc/utf_32le.$(OBJEXT): missing.h
enc/utf_32le.$(OBJEXT): oniguruma.h
enc/windows_1251.$(OBJEXT): $(top_srcdir)/regenc.h
enc/windows_1251.$(OBJEXT): config.h
enc/windows_1251.$(OBJEXT): defines.h
enc/windows_1251.$(OBJEXT): enc/windows_1251.c
enc/windows_1251.$(OBJEXT): missing.h
enc/windows_1251.$(OBJEXT): oniguruma.h
enc/windows_31j.$(OBJEXT): $(hdrdir)/ruby/ruby.h
enc/windows_31j.$(OBJEXT): $(top_srcdir)/regenc.h
enc/windows_31j.$(OBJEXT): $(top_srcdir)/regint.h
enc/windows_31j.$(OBJEXT): config.h
enc/windows_31j.$(OBJEXT): defines.h
enc/windows_31j.$(OBJEXT): enc/jis/props.h
enc/windows_31j.$(OBJEXT): enc/jis/props.kwd
enc/windows_31j.$(OBJEXT): enc/shift_jis.c
enc/windows_31j.$(OBJEXT): enc/windows_31j.c
enc/windows_31j.$(OBJEXT): intern.h
enc/windows_31j.$(OBJEXT): missing.h
enc/windows_31j.$(OBJEXT): oniguruma.h
enc/windows_31j.$(OBJEXT): st.h
enc/windows_31j.$(OBJEXT): subst.h
# AUTOGENERATED DEPENDENCIES END
