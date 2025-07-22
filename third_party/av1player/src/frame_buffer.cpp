#include "frame_buffer.hpp"

#include "utils.hpp"
#include "video_player.hpp"

#include <cstring>

namespace uvpx {

FrameBuffer::FrameBuffer(VideoPlayer* parent,
                         size_t width,
                         size_t height,
                         size_t frameCount)
    : m_parent(parent),
      m_frameCount(frameCount),
      m_width(width),
      m_height(height),
      m_writeFrame(nullptr),
      m_readTime(0.0) {
  m_readFrame = new Frame();

  for (size_t i = 0; i < frameCount; i++)
    m_writeQueue.push(new Frame());
}

FrameBuffer::~FrameBuffer() {
  SafeDelete<Frame>(m_readFrame);

  while (m_writeQueue.size() > 0) {
    Frame* frame = m_writeQueue.front();
    SafeDelete<Frame>(frame);
    m_writeQueue.pop();
  }

  while (m_readQueue.size() > 0) {
    Frame* frame = m_readQueue.front();
    SafeDelete<Frame>(frame);
    m_readQueue.pop();
  }
}

Frame* FrameBuffer::lockRead() {
  m_readLock.lock();
  return m_readFrame;
}

void FrameBuffer::unlockRead() {
  m_readLock.unlock();
}

Frame* FrameBuffer::lockWrite(double time) {
  if (!m_writeQueue.size())
    return nullptr;

  m_writeFrame = m_writeQueue.front();
  m_writeQueue.pop();

  m_writeFrame->setTime(time);
  return m_writeFrame;
}

void FrameBuffer::unlockWrite() {
  m_readQueue.push(m_writeFrame);
  m_writeFrame = nullptr;
}

void FrameBuffer::reset() {
  while (m_readQueue.size() > 0) {
    m_writeQueue.push(m_readQueue.front());
    m_readQueue.pop();
  }

  m_writeFrame = nullptr;
  m_readTime = 0.0;
}

void FrameBuffer::update(double playTime, double frameTime) {
  if (m_readQueue.size() == 0)
    return;

  m_updateLock.lock();

  while (m_readQueue.size() > 0) {
    Frame* front = m_readQueue.front();

    if (std::fabs(playTime - front->time()) < frameTime / 2.0) {
      m_readLock.lock();
      front->copyData(m_readFrame);
      m_readLock.unlock();

      m_readQueue.pop();
      m_writeQueue.push(front);
    } else if (front->time() < playTime - frameTime) {
      float f = front->time();
      float n = playTime;

      // If the frame is too old, push it to write queue (discard)
      m_readQueue.pop();
      m_writeQueue.push(front);
    } else {
      break;
    }
  }

  m_updateLock.unlock();
}

bool FrameBuffer::isFull() {
  return m_writeQueue.size() == 0;
}

}  // namespace uvpx
