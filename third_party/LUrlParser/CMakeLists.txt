# URL Parser
cmake_minimum_required(VERSION 3.18.0)

#--------------------------------------------------------------------------------
# Source of IMGUI
#--------------------------------------------------------------------------------

set(LURL_PARSER_SOURCE
  LUrlParser.cpp
  LUrlParser.h
)

add_library(lurl_parser ${LURL_PARSER_SOURCE})
