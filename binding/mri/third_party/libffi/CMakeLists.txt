# CMakeLists.txt translated from libffi.gyp

cmake_minimum_required(VERSION 3.10)

project(libffi)

# Set the default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Default target architecture
if(NOT CMAKE_SIZEOF_VOID_P)
  set(TARGET_ARCH "ia32")
elseif(CMAKE_SIZEOF_VOID_P MATCHES 8)
  set(TARGET_ARCH "x64")
else()
  set(TARGET_ARCH "ia32")
endif()

# Standalone static library equivalent
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Target: ffi ---
add_library(ffi STATIC)
set_target_properties(ffi PROPERTIES PREFIX "lib")

target_sources(ffi PRIVATE
    src/prep_cif.c
    src/types.c
    src/raw_api.c
    src/java_raw_api.c
    src/closures.c
)

target_compile_definitions(ffi PUBLIC
    PIC
    FFI_BUILDING
    HAVE_CONFIG_H
)

# Include directories
if(WIN32)
    set(OS_NAME "win")
elseif(APPLE)
    set(OS_NAME "mac")
elseif(UNIX AND NOT APPLE)
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(OS_NAME "linux")
    elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
        set(OS_NAME "freebsd")
    elseif(CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
        set(OS_NAME "openbsd")
    elseif(CMAKE_SYSTEM_NAME MATCHES "SunOS")
        set(OS_NAME "solaris")
    endif()
endif()

target_include_directories(ffi PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/${OS_NAME}/${TARGET_ARCH}"
)

# --- Platform and Architecture Specific Sources ---

# ARM
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
    target_sources(ffi PRIVATE src/arm/ffi.c)
    if(OS_NAME STREQUAL "linux")
        target_sources(ffi PRIVATE src/arm/sysv.S) # [cite: 12]
    endif()
# x86 / x64
else()
    target_sources(ffi PRIVATE src/x86/ffi.c)
    if(NOT (WIN32 AND TARGET_ARCH STREQUAL "ia32"))
         target_sources(ffi PRIVATE src/x86/ffi64.c)
    endif()

    if(APPLE) # [cite: 13]
        target_sources(ffi PRIVATE
            src/x86/darwin.S
            src/x86/darwin64.S
        )
    elseif(WIN32 OR MSVC)
        # --- Custom rule for compiling .asm files on Windows ---
        set(ASM_OBJECT_FILES "")
        if(TARGET_ARCH STREQUAL "ia32")
            set(ASSEMBLER_EXECUTABLE ml)
            set(ASSEMBLER_FLAGS "/nologo" "/safeseh")
            set(ASM_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/x86/win32.asm")
            set(OBJECT_FILE "${CMAKE_CURRENT_BINARY_DIR}/ffi/win32.obj")
        else() # target_arch=="x64"
            set(ASSEMBLER_EXECUTABLE ml64)
            set(ASSEMBLER_FLAGS "/nologo")
            set(ASM_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/x86/win64.asm")
            set(OBJECT_FILE "${CMAKE_CURRENT_BINARY_DIR}/ffi/win64.obj")
        endif()
        
        # Create output directory
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ffi")
        
        add_custom_command(
            OUTPUT ${OBJECT_FILE}
            COMMAND ${ASSEMBLER_EXECUTABLE} ${ASSEMBLER_FLAGS} /c /Fo${OBJECT_FILE} ${ASM_SOURCE_FILE}
            DEPENDS ${ASM_SOURCE_FILE}
            COMMENT "Building assembly file ${ASM_SOURCE_FILE}"
        )
        list(APPEND ASM_OBJECT_FILES ${OBJECT_FILE})
        target_sources(ffi PRIVATE ${ASM_OBJECT_FILES})
        set_source_files_properties(${ASM_OBJECT_FILES} PROPERTIES GENERATED TRUE)
        # --- End custom rule ---
        
        # Silence implicit conversion warnings [cite: 14]
        if (MSVC)
            target_compile_options(ffi PRIVATE /wd4267)
        endif()
    elseif(OS_NAME MATCHES "linux|freebsd|openbsd|solaris") # [cite: 17]
        target_sources(ffi PRIVATE
            src/x86/unix64.S
            src/x86/sysv.S
        )
    endif()
endif()

# --- General Configurations ---

target_compile_definitions(ffi PRIVATE
    $<$<CONFIG:Debug>:DEBUG _DEBUG> # [cite: 3]
    $<$<CONFIG:Release>:NDEBUG>
)

if(MSVC)
    # Static runtime library settings
    target_compile_options(ffi PRIVATE
        $<$<CONFIG:Debug>:/MTd>   # static debug [cite: 3]
        $<$<CONFIG:Release>:/MT> # static release [cite: 4]
    )
    # Generate Debug Information [cite: 5]
    target_compile_options(ffi PRIVATE /Z7)
    target_link_options(ffi PRIVATE /DEBUG)
endif()

if(WIN32)
    target_compile_definitions(ffi PRIVATE WIN32)
endif()
