#include "frame.hpp"

#include "utils.hpp"

#include <cstring>

namespace uvpx {

Frame::Frame()
    : m_planes{0}, m_width{0}, m_height{0}, m_time(0.0), m_empty(true) {}

Frame::~Frame() {
  for (int i = 0; i < 3; ++i)
    SafeDeleteArray<unsigned char>(m_planes[i]);
}

unsigned char* Frame::plane(int id) const {
  return m_planes[id];
}

size_t Frame::width(int id) const {
  return m_width[id];
}

size_t Frame::height(int id) const {
  return m_height[id];
}

void Frame::setTime(double time) {
  m_time = time;
}

double Frame::time() const {
  return m_time;
}

void Frame::resize(int id, size_t frame_width, size_t frame_height) {
  if (m_planes[id])
    SafeDeleteArray<unsigned char>(m_planes[id]);

  m_width[id] = frame_width;
  m_height[id] = frame_height;

  size_t plane_size = frame_width * frame_height;
  m_planes[id] = new unsigned char[plane_size];

  if (id > 0)
    std::memset(m_planes[id], 128, plane_size);
  else
    std::memset(m_planes[id], 0, plane_size);
}

void Frame::copyData(Frame* dst) {
  dst->m_time = m_time;

  for (int i = 0; i < 3; ++i) {
    if (dst->m_width[i] != m_width[i] || dst->m_height[i] != m_height[i])
      dst->resize(i, m_width[i], m_height[i]);

    dst->m_empty = m_empty;
    std::memcpy(dst->m_planes[i], m_planes[i], m_width[i] * m_height[i]);
  }
}

}  // namespace uvpx
