// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "app/platform/android.h"

#include <jni.h>

#include <string>

#include "SDL3/SDL_system.h"

#include "core/common/exception.h"
#include "core/common/logger.h"

namespace platform {
namespace android {

namespace {

/* Everything the call below makes a local reference to is released in one go.
   A JNI local reference lives until the native method that created it returns
   -- and this runs on the SDL thread, which never returns to Java, so without
   a frame the references would accumulate for the lifetime of the game. */
class LocalFrame {
 public:
  explicit LocalFrame(JNIEnv* env) : env_(env) {
    pushed_ = env_->PushLocalFrame(16) == 0;
  }

  ~LocalFrame() {
    if (pushed_)
      env_->PopLocalFrame(nullptr);
  }

  LocalFrame(const LocalFrame&) = delete;
  LocalFrame& operator=(const LocalFrame&) = delete;

  bool pushed() const { return pushed_; }

 private:
  JNIEnv* env_ = nullptr;
  bool pushed_ = false;
};

}  // namespace

std::string PrepareStorageRoot(urge::ExceptionState& exception_state) {
  JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
  if (!env || !activity) {
    exception_state.Throw(urge::ExceptionState::kIOError,
                          "the Android activity is not reachable from the "
                          "engine thread.");
    return {};
  }

  LocalFrame frame(env);
  if (!frame.pushed()) {
    exception_state.Throw(urge::ExceptionState::kIOError,
                          "out of JNI local references.");
    return {};
  }

  std::string root;

  /* The activity's own class rather than `FindClass`: on a thread that was
     attached by native code, `FindClass` searches the system class loader,
     which cannot see the application's classes.  The object in hand can. */
  jclass activity_class = env->GetObjectClass(activity);
  jmethodID ensure_bundle =
      activity_class ? env->GetStaticMethodID(
                           activity_class, "ensureBundle",
                           "(Landroid/app/Activity;)Ljava/lang/String;")
                     : nullptr;

  if (!ensure_bundle) {
    env->ExceptionClear();
    exception_state.Throw(
        urge::ExceptionState::kIOError,
        "URGEActivity.ensureBundle(Activity) is missing from the APK.");
    return {};
  }

  auto result = static_cast<jstring>(env->CallStaticObjectMethod(
      activity_class, ensure_bundle, activity));

  if (env->ExceptionCheck()) {
    /* The Java side logs its own reason with the same tag; this puts the
       stack trace in logcat too, which is where a report from a device has to
       be read. */
    env->ExceptionDescribe();
    env->ExceptionClear();
    exception_state.Throw(urge::ExceptionState::kIOError,
                          "unpacking the game bundle failed; see logcat.");
    return {};
  }

  if (!result) {
    exception_state.Throw(urge::ExceptionState::kIOError,
                          "the game bundle could not be unpacked.");
    return {};
  }

  const char* text = env->GetStringUTFChars(result, nullptr);
  if (!text) {
    exception_state.Throw(urge::ExceptionState::kIOError,
                          "the game directory name could not be read.");
    return {};
  }

  root = text;
  env->ReleaseStringUTFChars(result, text);

  LOGGER_INFO("[Android] game directory: {}", root);
  return root;
}

}  // namespace android
}  // namespace platform
