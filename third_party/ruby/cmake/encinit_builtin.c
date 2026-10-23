/*
 * encinit_builtin.c -- Init_enc for a build with statically linked encodings.
 *
 * The original build has three flavours of Init_enc:
 *   - miniinit.c  for miniruby (declares the three built-in encodings only)
 *   - dmyenc.c    for a shared build (require "enc/encdb.so")
 *   - encinit.c   generated for --with-static-linked-ext
 *
 * This build links the built-in encodings and enc/encdb.c into the
 * interpreter, so Init_enc just has to run the encoding database.  That makes
 * the Encoding::* constants and the single byte aliases available without any
 * loadable encoding objects.
 */
void Init_encdb(void);

void
Init_enc(void)
{
    Init_encdb();
}
