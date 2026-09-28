/* This file provides the Init_ext() entry point for a build with no
 * dynamically loadable extensions.
 *
 * The encoding/transcoder *name* databases (enc/encdb.c -> encdb.h and
 * enc/trans/transdb.c -> transdb.h) are compiled directly into the binary by
 * the CMake build, so register them here.  Init_ext() is invoked from
 * ruby.c (require_libraries) after rb_call_inits(), so the Encoding class and
 * the built-in encodings (ASCII-8BIT/UTF-8/US-ASCII) are already available and
 * this simply binds the Encoding::* constants to them.
 */
#include "ruby.h"
#include "ruby/encoding.h"

void Init_encdb(void);
void Init_transdb(void);
/* Statically linked encodings/transcoders (see ENCOBJS in CMakeLists.txt). */
void Init_utf_16le(void);
void Init_utf_16be(void);
void Init_utf_32le(void);
void Init_utf_32be(void);
void Init_utf_16_32(void);
void Init_single_byte(void);
void Init_gbk(void);   /* the GBK <-> UTF-8 transcoder (enc/trans/gbk.c) */
void Init_chinese(void);

int rb_enc_register(const char *, rb_encoding *);
extern rb_encoding OnigEncodingGBK;   /* the GBK encoding table (enc/gbk.c) */

void
Init_ext(void)
{
    Init_encdb();
    Init_transdb();
    /* Register the encodings/transcoders linked into the binary so they are
     * usable without the enc/*.so loadable modules.  Init_encdb()/Init_transdb()
     * first declared the names (as autoload stubs); registering here upgrades
     * them to fully functional encodings. */
    Init_utf_16le();
    Init_utf_16be();
    Init_utf_32le();
    Init_utf_32be();
    Init_utf_16_32();
    Init_single_byte();
    Init_gbk();
    Init_chinese();
    /* enc/gbk.c is compiled without ONIG_ENC_REGISTER (its transcoder sibling
     * owns Init_gbk), so register the GBK encoding table explicitly.  The
     * CP936 alias declared by encdb.h resolves to it. */
    rb_enc_register("GBK", &OnigEncodingGBK);
}
