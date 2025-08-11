# Components - engine version
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Version automate process
#--------------------------------------------------------------------------------

# Custom target
add_custom_target(version_target ALL
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/build_version.py ${RGU_AUTOGEN_DIR}/components/version/version.h
    COMMENT "Updating URGE Git Version..."
    VERBATIM
)
