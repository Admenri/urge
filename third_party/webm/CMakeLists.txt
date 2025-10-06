# Webm format
cmake_minimum_required(VERSION 3.20.0)

#--------------------------------------------------------------------------------
# Source
#--------------------------------------------------------------------------------

set(WEBM_SOURCE
  common/file_util.cc
  common/file_util.h
  common/hdr_util.cc
  common/hdr_util.h
  common/webmids.h
  mkvmuxer/mkvmuxer.cc
  mkvmuxer/mkvmuxer.h
  mkvmuxer/mkvmuxertypes.h
  mkvmuxer/mkvmuxerutil.cc
  mkvmuxer/mkvmuxerutil.h
  mkvmuxer/mkvwriter.cc
  mkvmuxer/mkvwriter.h
  mkvparser/mkvparser.cc
  mkvparser/mkvparser.h
  mkvparser/mkvreader.cc
  mkvparser/mkvreader.h
)

add_library(libwebm STATIC ${WEBM_SOURCE})
target_include_directories(libwebm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
