#[=======================================================================[.rst:

FindOgg
-------

This tree carries its own libogg -- the submodule at
``third_party/miniaudio/external/ogg`` -- and that project already defines the
``Ogg::ogg`` target, so there is nothing on the system for a finder to locate.
libvorbis asks for the package through ``find_package(Ogg REQUIRED)`` and looks
it up on ``CMAKE_MODULE_PATH``, where this module sits ahead of the copy in
``vorbis/cmake`` -- the one that would go looking for an installed libogg and
fail.

Result Variables
^^^^^^^^^^^^^^^^

::

  OGG_FOUND           - True; libogg is always present in this build.
  OGG_LIBRARIES       - ``Ogg::ogg``.

#]=======================================================================]

if(NOT TARGET Ogg::ogg)
  message(FATAL_ERROR
    "Ogg::ogg is not defined. third_party/ogg has to be added to the build "
    "before anything that links libogg (libvorbis, libopusfile).")
endif()

set(Ogg_FOUND TRUE)
set(OGG_FOUND TRUE)
set(Ogg_LIBRARIES Ogg::ogg)
set(OGG_LIBRARIES Ogg::ogg)
set(Ogg_VERSION_STRING "1.3.5")
set(OGG_VERSION_STRING "1.3.5")
