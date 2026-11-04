/*
 * encinit_builtin.c -- Init_enc for a build with statically linked encodings.
 *
 * The original build has three flavours of Init_enc:
 *   - miniinit.c  for miniruby (declares the three built-in encodings only)
 *   - dmyenc.c    for a shared build (require "enc/encdb.so")
 *   - encinit.c   generated for --with-static-linked-ext
 *
 * This build links the built-in encodings and enc/encdb.c into the
 * interpreter. Init_enc runs the encoding database and, when enabled, the
 * RGSS encoding and transcoder initializers. That makes
 * the Encoding::* constants and the single byte aliases available without any
 * loadable encoding objects.
 */
#include "ruby.h"

void Init_encdb(void);
#ifdef RUBY_STATIC_RGSS_ENCODINGS
void Init_trans_single_byte(void);
void Init_trans_utf_16_32(void);
void Init_trans_japanese_sjis(void);
#endif

void
Init_enc(void)
{
    Init_encdb();
#ifdef RUBY_STATIC_RGSS_ENCODINGS
    // Each encoding has an Init_* entry point via ONIG_ENC_REGISTER. Preserve
    // Ruby's aliases/replicas from encdb, then install their actual handlers.
#define RUBY_INIT_ENCODING(name) do { \
        extern void Init_##name(void); \
        Init_##name(); \
        rb_provide("enc/" #name ".so"); \
    } while (0);
#include "encinit_rgss.h"
#undef RUBY_INIT_ENCODING
    Init_trans_single_byte();
    Init_trans_utf_16_32();
    Init_trans_japanese_sjis();
    rb_provide("enc/trans/single_byte.so");
    rb_provide("enc/trans/utf_16_32.so");
    rb_provide("enc/trans/japanese_sjis.so");
#endif
}
