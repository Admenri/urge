#ifndef BASE_EXCEPTIONS_EXCEPTION_H
#define BASE_EXCEPTIONS_EXCEPTION_H

#include <stdarg.h>
#include <stdio.h>

#include <string>

namespace base {

class Exception {
 public:
  enum Type {
    ContentError = 0,
    RendererError,
    SDLError,
    FilesystemError,
    NoFileError,
  };

  Exception(Type type, const char* format, ...) : type_(type) {
    va_list ap;
    va_start(ap, format);

    msg_.resize(512);
    vsnprintf(&msg_[0], msg_.size(), format, ap);

    va_end(ap);
  }

  Exception& operator=(const Exception) = delete;

  Type GetType() const { return type_; }
  std::string GetErrorMessage() const { return msg_; }

 private:
  Type type_;
  std::string msg_;
};

}  // namespace base

#endif  // BASE_EXCEPTIONS_EXCEPTION_H
