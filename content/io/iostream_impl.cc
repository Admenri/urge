// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/io/iostream_impl.h"

#include "components/filesystem/io_service.h"
#include "content/context/execution_context.h"

namespace content {

// static
scoped_refptr<IOStream> IOStream::FromFileSystem(
    ExecutionContext* execution_context,
    const std::string& filename,
    const std::string& mode,
    ExceptionState& exception_state) {
  auto* stream = SDL_IOFromFile(filename.c_str(), mode.c_str());
  if (!stream) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "failed to open file: %s", SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<IOStreamImpl>(execution_context, stream);
}

// static
scoped_refptr<IOStream> IOStream::FromIOSystem(
    ExecutionContext* execution_context,
    const std::string& filename,
    ExceptionState& exception_state) {
  filesystem::IOState io_state;
  auto* stream =
      execution_context->io_service->OpenReadRaw(filename.c_str(), &io_state);
  if (io_state.error_count) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "failed to load from iosystem: %s",
                               io_state.error_message.c_str());
    return nullptr;
  }

  if (!stream)
    return nullptr;

  return base::MakeRefCounted<IOStreamImpl>(execution_context, stream);
}

// static
scoped_refptr<IOStream> IOStream::FromMemory(
    ExecutionContext* execution_context,
    void* target_buffer,
    int64_t buffer_size,
    ExceptionState& exception_state) {
  auto* stream = SDL_IOFromMem(target_buffer, buffer_size);
  if (!stream) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "failed to open file: %s", SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<IOStreamImpl>(execution_context, stream);
}

// static
scoped_refptr<IOStream> IOStream::FromDynamicMemory(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  auto* stream = SDL_IOFromDynamicMem();
  if (!stream) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "failed to creat dynamic memory: %s",
                               SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<IOStreamImpl>(execution_context, stream);
}

// static
uint64_t IOStream::StringToPointer(ExecutionContext* execution_context,
                                   const void* source,
                                   ExceptionState& exception_state) {
  return reinterpret_cast<uint64_t>(source);
}

// static
uint64_t IOStream::CopyMemoryFromPtr(ExecutionContext* execution_context,
                                     void* dest,
                                     uint64_t source_ptr,
                                     uint64_t byte_size,
                                     ExceptionState& exception_state) {
  return reinterpret_cast<uint64_t>(
      SDL_memcpy(dest, reinterpret_cast<void*>(source_ptr), byte_size));
}

// static
uint64_t IOStream::CopyMemoryToPtr(ExecutionContext* execution_context,
                                   uint64_t dest_ptr,
                                   const void* source,
                                   uint64_t byte_size,
                                   ExceptionState& exception_state) {
  return reinterpret_cast<uint64_t>(
      SDL_memcpy(reinterpret_cast<void*>(dest_ptr), source, byte_size));
}

///////////////////////////////////////////////////////////////////////////////
// Surface Implement

IOStreamImpl::IOStreamImpl(ExecutionContext* execution_context,
                           SDL_IOStream* stream)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      stream_(stream) {
  DCHECK(stream_);
}

DISPOSABLE_DEFINITION(IOStreamImpl);

scoped_refptr<IOStreamImpl> IOStreamImpl::From(scoped_refptr<IOStream> host) {
  return static_cast<IOStreamImpl*>(host.get());
}

IOStream::IOStatus IOStreamImpl::GetStatus(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(STATUS_ERROR);

  return static_cast<IOStatus>(SDL_GetIOStatus(stream_));
}

int64_t IOStreamImpl::GetSize(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return SDL_GetIOSize(stream_);
}

int64_t IOStreamImpl::Seek(int64_t offset,
                           IOWhence whence,
                           ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return SDL_SeekIO(stream_, offset, static_cast<SDL_IOWhence>(whence));
}

int64_t IOStreamImpl::Tell(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return SDL_TellIO(stream_);
}

int64_t IOStreamImpl::Read(void* buffer,
                           int64_t size,
                           ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return static_cast<int64_t>(SDL_ReadIO(stream_, buffer, size));
}

int64_t IOStreamImpl::Write(const void* buffer,
                            int64_t size,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return static_cast<int64_t>(SDL_WriteIO(stream_, buffer, size));
}

bool IOStreamImpl::Flush(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return SDL_FlushIO(stream_);
}

void IOStreamImpl::OnObjectDisposed() {
  SDL_CloseIO(stream_);
  stream_ = nullptr;
}

}  // namespace content
