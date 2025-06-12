// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/io/iostream_impl.h"

#include "components/filesystem/io_service.h"
#include "content/context/execution_context.h"

namespace content {

scoped_refptr<IOStream> IOStream::FromFileSystem(
    ExecutionContext* execution_context,
    const base::String& filename,
    const base::String& mode,
    ExceptionState& exception_state) {
  auto* stream = SDL_IOFromFile(filename.c_str(), mode.c_str());
  if (!stream) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to open file: %s", SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<IOStreamImpl>(execution_context, stream,
                                            base::String());
}

scoped_refptr<IOStream> IOStream::FromIOSystem(
    ExecutionContext* execution_context,
    const base::String& filename,
    ExceptionState& exception_state) {
  filesystem::IOState io_state;
  auto* stream =
      execution_context->io_service->OpenReadRaw(filename.c_str(), &io_state);
  if (io_state.error_count) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to load from iosystem: %s",
                               io_state.error_message.c_str());
    return nullptr;
  }

  return base::MakeRefCounted<IOStreamImpl>(execution_context, stream,
                                            base::String());
}

scoped_refptr<IOStream> IOStream::FromMemory(
    ExecutionContext* execution_context,
    void* target_buffer,
    int64_t buffer_size,
    ExceptionState& exception_state) {
  auto* stream = SDL_IOFromConstMem(target_buffer, buffer_size);
  if (!stream) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to open file: %s", SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<IOStreamImpl>(execution_context, stream);
}

uint64_t IOStream::CopyMemoryFromPtr(void* dest,
                                     uint64_t source_ptr,
                                     uint64_t byte_size,
                                     ExceptionState& exception_state) {
  return reinterpret_cast<uint64_t>(
      SDL_memcpy(dest, reinterpret_cast<void*>(source_ptr), byte_size));
}

uint64_t IOStream::CopyMemoryToPtr(uint64_t dest_ptr,
                                   const void* source,
                                   uint64_t byte_size,
                                   ExceptionState& exception_state) {
  return reinterpret_cast<uint64_t>(
      SDL_memcpy(reinterpret_cast<void*>(dest_ptr), source, byte_size));
}

IOStreamImpl::IOStreamImpl(ExecutionContext* execution_context,
                           SDL_IOStream* stream)
    : Disposable(execution_context->disposable_parent),
      EngineObject(execution_context),
      stream_(stream) {}

IOStreamImpl::~IOStreamImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

scoped_refptr<IOStreamImpl> IOStreamImpl::From(scoped_refptr<IOStream> host) {
  return static_cast<IOStreamImpl*>(host.get());
}

void IOStreamImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool IOStreamImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

IOStream::IOStatus IOStreamImpl::GetStatus(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return STATUS_ERROR;

  return static_cast<IOStatus>(SDL_GetIOStatus(stream_));
}

int64_t IOStreamImpl::GetSize(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return SDL_GetIOSize(stream_);
}

int64_t IOStreamImpl::Seek(int64_t offset,
                           IOWhence whence,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return SDL_SeekIO(stream_, offset, static_cast<SDL_IOWhence>(whence));
}

int64_t IOStreamImpl::Tell(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return SDL_TellIO(stream_);
}

int64_t IOStreamImpl::Read(void* buffer,
                           int64_t size,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return static_cast<int64_t>(SDL_ReadIO(stream_, buffer, size));
}

int64_t IOStreamImpl::Write(const void* buffer,
                            int64_t size,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return static_cast<int64_t>(SDL_WriteIO(stream_, buffer, size));
}

bool IOStreamImpl::Flush(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return SDL_FlushIO(stream_);
}

void IOStreamImpl::OnObjectDisposed() {
  SDL_CloseIO(stream_);
  stream_ = nullptr;
}

}  // namespace content
