// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_IOSTREAM_H_
#define CONTENT_PUBLIC_ENGINE_IOSTREAM_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:IOStream)--*/
class URGE_OBJECT(IOStream) {
 public:
  virtual ~IOStream() = default;

  /*--urge(name:IOStatus)--*/
  enum IOStatus {
    STATUS_READY,
    STATUS_ERROR,
    STATUS_EOF,
    STATUS_NOT_READY,
    STATUS_READONLY,
    STATUS_WRITEONLY,
  };

  /*--urge(name:IOWhence)--*/
  enum IOWhence {
    IOSEEK_SET,
    IOSEEK_CUR,
    IOSEEK_END,
  };

  /*--urge(name:from_filesystem)--*/
  static scoped_refptr<IOStream> FromFileSystem(
      ExecutionContext* execution_context,
      const std::string& filename,
      const std::string& mode,
      ExceptionState& exception_state);

  /*--urge(name:from_iosystem)--*/
  static scoped_refptr<IOStream> FromIOSystem(
      ExecutionContext* execution_context,
      const std::string& filename,
      ExceptionState& exception_state);

  /*--urge(name:from_memory)--*/
  static scoped_refptr<IOStream> FromMemory(ExecutionContext* execution_context,
                                            void* target_buffer,
                                            int64_t buffer_size,
                                            ExceptionState& exception_state);

  /*--urge(name:string_as_pointer)--*/
  static uint64_t StringToPointer(ExecutionContext* execution_context,
                                  const void* source,
                                  ExceptionState& exception_state);

  /*--urge(name:copy_memory_from_ptr)--*/
  static uint64_t CopyMemoryFromPtr(ExecutionContext* execution_context,
                                    void* dest,
                                    uint64_t source_ptr,
                                    uint64_t byte_size,
                                    ExceptionState& exception_state);

  /*--urge(name:copy_memory_to_ptr)--*/
  static uint64_t CopyMemoryToPtr(ExecutionContext* execution_context,
                                  uint64_t dest_ptr,
                                  const void* source,
                                  uint64_t byte_size,
                                  ExceptionState& exception_state);

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:status)--*/
  virtual IOStatus GetStatus(ExceptionState& exception_state) = 0;

  /*--urge(name:size)--*/
  virtual int64_t GetSize(ExceptionState& exception_state) = 0;

  /*--urge(name:seek)--*/
  virtual int64_t Seek(int64_t offset,
                       IOWhence whence,
                       ExceptionState& exception_state) = 0;

  /*--urge(name:tell)--*/
  virtual int64_t Tell(ExceptionState& exception_state) = 0;

  /*--urge(name:read)--*/
  virtual int64_t Read(void* buffer,
                       int64_t size,
                       ExceptionState& exception_state) = 0;

  /*--urge(name:write)--*/
  virtual int64_t Write(const void* buffer,
                        int64_t size,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:flush)--*/
  virtual bool Flush(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_IOSTREAM_H_
