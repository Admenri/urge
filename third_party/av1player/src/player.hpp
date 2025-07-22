#ifndef _UVPX_PLAYER_H_
#define _UVPX_PLAYER_H_

#include "dll_defines.hpp"
#include "error_codes.hpp"
#include "frame.hpp"

struct SDL_IOStream;

namespace uvpx {

typedef void (*OnAudioDataDecoded)(void* userPtr, float* values, size_t count);
typedef void (*OnVideoFinished)(void* userPtr);
typedef void (*DebugLogFuncPtr)(const char* msg);

class VideoPlayer;

class UVPX_EXPORT Player {
 protected:
  VideoPlayer* m_videoPlayer;

 public:
  struct Config {
    int decodeThreadsCount;
    int videoDecodeBufferSize;
    int audioDecodeBufferSize;
    int frameBufferCount;
    int maxFrameDelay;

    Config()
        : decodeThreadsCount(32),
          videoDecodeBufferSize(2 * 1024 * 1024),
          audioDecodeBufferSize(4 * 1024),
          frameBufferCount(4),
          maxFrameDelay(1) {}
  };

  struct Statistics {
    int framesDecoded;
    int videoBufferSize;
    int audioBufferSize;
  };

  struct VideoInfo {
    int width;
    int height;
    float duration;
    float frameRate;
    int hasAudio;
    int audioChannels;
    int audioFrequency;
    int audioSamples;
    int decodeThreadsCount;
  };

  struct TextureInfo {
    int width;
    int height;
    void* texture;
  };

  enum class LoadResult {
    Success = 0,
    AlreadyReaded,
    FileNotExists,
    FailedParseHeader,
    FailedCreateInstance,
    FailedLoadSegment,
    FailedGetSegmentInfo,
    FailedInitializeVideoDecoder,
    FailedDecodeAudioHeader,
    FailedInitializeAudioDecoder,
    UnsupportedVideoCodec,
    NotInitialized,
    InternalError
  };

 public:
  Player(const Config& cfg);
  ~Player();

  LoadResult load(SDL_IOStream* io, int audioTrack, bool preloadFile);
  bool update(float dt);

  VideoInfo* info() const;

  Frame* lockRead();
  void unlockRead();

  void setOnAudioData(OnAudioDataDecoded func, void* userPtr);
  void setOnVideoFinished(OnVideoFinished func, void* userPtr);

  double playTime();
  float duration();

  void play();
  void pause();
  void stop();
  bool isStopped();
  bool isPaused();
  bool isPlaying();
  bool isFinished();

  static const Config& defaultConfig();
  bool readStats(Statistics* dst);

  static void setDebugLog(DebugLogFuncPtr func);
};

}  // namespace uvpx

#endif  // _UVPX_PLAYER_H_
