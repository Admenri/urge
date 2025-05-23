/**********************************************************************
  iso8859_6.c -  Oniguruma (regular expression library)
**********************************************************************/
/*-
 * Copyright (c) 2002-2007  K.Kosako  <sndgk393 AT ybb DOT ne DOT jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "regenc.h"

#define ENC_IS_ISO_8859_6_CTYPE(code,ctype) \
  ((EncISO_8859_6_CtypeTable[code] & CTYPE_TO_BIT(ctype)) != 0)

static const unsigned short EncISO_8859_6_CtypeTable[256] = {
  0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008,
  0x4008, 0x420c, 0x4209, 0x4208, 0x4208, 0x4208, 0x4008, 0x4008,
  0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008,
  0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008,
  0x4284, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0,
  0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0,
  0x78b0, 0x78b0, 0x78b0, 0x78b0, 0x78b0, 0x78b0, 0x78b0, 0x78b0,
  0x78b0, 0x78b0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0,
  0x41a0, 0x7ca2, 0x7ca2, 0x7ca2, 0x7ca2, 0x7ca2, 0x7ca2, 0x74a2,
  0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2,
  0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2,
  0x74a2, 0x74a2, 0x74a2, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x51a0,
  0x41a0, 0x78e2, 0x78e2, 0x78e2, 0x78e2, 0x78e2, 0x78e2, 0x70e2,
  0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2,
  0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2,
  0x70e2, 0x70e2, 0x70e2, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x4008,
  0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
  0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
  0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
  0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
  0x0284, 0x0000, 0x0000, 0x0000, 0x00a0, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x01a0, 0x01a0, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x01a0, 0x0000, 0x0000, 0x0000, 0x01a0,
  0x0000, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2,
  0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2,
  0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2,
  0x30a2, 0x30a2, 0x30a2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2,
  0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2, 0x30a2,
  0x30a2, 0x30a2, 0x30a2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static int
is_code_ctype(OnigCodePoint code, unsigned int ctype, OnigEncoding enc ARG_UNUSED)
{
  if (code < 256)
    return ENC_IS_ISO_8859_6_CTYPE(code, ctype);
  else
    return FALSE;
}

OnigEncodingDefine(iso_8859_6, ISO_8859_6) = {
  onigenc_single_byte_mbc_enc_len,
  "ISO-8859-6",  /* name */
  1,             /* max enc length */
  1,             /* min enc length */
  onigenc_is_mbc_newline_0x0a,
  onigenc_single_byte_mbc_to_code,
  onigenc_single_byte_code_to_mbclen,
  onigenc_single_byte_code_to_mbc,
  onigenc_ascii_mbc_case_fold,
  onigenc_ascii_apply_all_case_fold,
  onigenc_ascii_get_case_fold_codes_by_str,
  onigenc_minimum_property_name_to_ctype,
  is_code_ctype,
  onigenc_not_support_get_ctype_code_range,
  onigenc_single_byte_left_adjust_char_head,
  onigenc_always_true_is_allowed_reverse_match,
  0,
  ONIGENC_FLAG_NONE,
};
ENC_ALIAS("ISO8859-6", "ISO-8859-6")

/*
 * Name: windows-1256
 * MIBenum: 2256
 * Link: http://www.iana.org/assignments/character-sets
 * Link: http://www.microsoft.com/globaldev/reference/sbcs/1256.mspx
 * Link: http://en.wikipedia.org/wiki/Windows-1256
 */
ENC_REPLICATE("Windows-1256", "ISO-8859-6")
ENC_ALIAS("CP1256", "Windows-1256")
