// Copyright 2026 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#pragma once

#include <cstring>
#include <istream>
#include <streambuf>
#include <vector>

#include "SDL3/SDL_iostream.h"

namespace filesystem {

class SDLIOStreamBuf : public std::streambuf {
 public:
  static constexpr size_t DEFAULT_BUFFER_SIZE = 2 << 11;

  SDLIOStreamBuf(SDL_IOStream* io,
                 bool autoClose = false,
                 size_t bufferSize = DEFAULT_BUFFER_SIZE)
      : m_io(io),
        m_autoClose(autoClose),
        m_readBuffer(bufferSize),
        m_writeBuffer(bufferSize) {
    setg(m_readBuffer.data(), m_readBuffer.data(), m_readBuffer.data());

    setp(m_writeBuffer.data(), m_writeBuffer.data() + m_writeBuffer.size());
  }

  ~SDLIOStreamBuf() override {
    sync();

    if (m_autoClose && m_io) {
      SDL_CloseIO(m_io);
      m_io = nullptr;
    }
  }

  SDL_IOStream* sdl_stream() const { return m_io; }

 protected:
  int_type underflow() override {
    if (!m_io)
      return traits_type::eof();

    if (gptr() < egptr())
      return traits_type::to_int_type(*gptr());

    const size_t readBytes =
        SDL_ReadIO(m_io, m_readBuffer.data(), m_readBuffer.size());

    if (readBytes == 0)
      return traits_type::eof();

    setg(m_readBuffer.data(), m_readBuffer.data(),
         m_readBuffer.data() + readBytes);

    return traits_type::to_int_type(*gptr());
  }

  int_type overflow(int_type ch) override {
    if (!m_io)
      return traits_type::eof();

    if (FlushWriteBuffer() == -1)
      return traits_type::eof();

    if (!traits_type::eq_int_type(ch, traits_type::eof())) {
      *pptr() = traits_type::to_char_type(ch);
      pbump(1);
    }

    return traits_type::not_eof(ch);
  }

  int sync() override { return FlushWriteBuffer(); }

  pos_type seekoff(off_type off,
                   std::ios_base::seekdir dir,
                   std::ios_base::openmode which) override {
    if (!m_io)
      return pos_type(off_type(-1));

    sync();

    SDL_IOWhence base = SDL_IO_SEEK_SET;

    switch (dir) {
      case std::ios_base::beg:
        base = SDL_IO_SEEK_SET;
        break;

      case std::ios_base::cur:
        base = SDL_IO_SEEK_CUR;
        break;

      case std::ios_base::end:
        base = SDL_IO_SEEK_END;
        break;

      default:
        return pos_type(off_type(-1));
    }

    const Sint64 result = SDL_SeekIO(m_io, off, base);

    if (result < 0)
      return pos_type(off_type(-1));

    setg(m_readBuffer.data(), m_readBuffer.data(), m_readBuffer.data());

    return pos_type(result);
  }

  pos_type seekpos(pos_type pos, std::ios_base::openmode which) override {
    return seekoff(off_type(pos), std::ios_base::beg, which);
  }

 private:
  int FlushWriteBuffer() {
    const ptrdiff_t size = pptr() - pbase();

    if (size <= 0)
      return 0;

    const size_t written =
        SDL_WriteIO(m_io, m_writeBuffer.data(), static_cast<size_t>(size));

    if (written != static_cast<size_t>(size))
      return -1;

    setp(m_writeBuffer.data(), m_writeBuffer.data() + m_writeBuffer.size());

    return 0;
  }

 private:
  SDL_IOStream* m_io = nullptr;
  bool m_autoClose = false;

  std::vector<char> m_readBuffer;
  std::vector<char> m_writeBuffer;
};

}  // namespace filesystem
