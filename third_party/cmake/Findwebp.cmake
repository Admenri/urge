#[=======================================================================[.rst:

Findwebp
--------

This tree carries its own libwebp -- the submodule at
``third_party/SDL_image/external/libwebp`` -- so the library is never looked up
on the system: the project that compiles it has already defined the ``webp``,
``webpdemux`` and ``libwebpmux`` targets.  SDL_image reaches WEBP through
``find_package(webp)`` whenever it is *not* building a vendored copy of the
library itself, and this module answers that call by aliasing the in-tree
targets under the names it expects.

Result Variables
^^^^^^^^^^^^^^^^

::

  webp_FOUND          - True when every target SDL_image links against exists.

Imported Targets
^^^^^^^^^^^^^^^^

``WebP::webp``, ``WebP::webpdemux``, ``WebP::libwebpmux``

#]=======================================================================]

set(webp_FOUND FALSE)

if(TARGET webp AND TARGET webpdemux AND TARGET libwebpmux)
  if(NOT TARGET WebP::webp)
    add_library(WebP::webp ALIAS webp)
  endif()
  if(NOT TARGET WebP::webpdemux)
    add_library(WebP::webpdemux ALIAS webpdemux)
  endif()
  if(NOT TARGET WebP::libwebpmux)
    add_library(WebP::libwebpmux ALIAS libwebpmux)
  endif()
  set(webp_FOUND TRUE)
endif()
