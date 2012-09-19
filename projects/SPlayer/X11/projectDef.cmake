#/**********************************************************\ 
# Auto-generated X11 project definition file for the
# FBTestPlugin project
#\**********************************************************/

# X11 template platform definition CMake file
# Included from ../CMakeLists.txt

# remember that the current source dir is the project root; this file is in X11/
file (GLOB PLATFORM RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    X11/*.cpp
    X11/*.h
    X11/*.cmake
    )

SOURCE_GROUP(X11 FILES ${PLATFORM})

# use this to add preprocessor definitions
add_definitions(
)

set (SOURCES
    ${SOURCES}
    ${PLATFORM}
    )

add_x11_plugin(${PROJECT_NAME} SOURCES)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../sipalsa)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg)

# add library dependencies here; leave ${PLUGIN_INTERNAL_DEPS} there unless you know what you're doing!
target_link_libraries(${PROJECT_NAME}
    ${PLUGIN_INTERNAL_DEPS}
    GL
    GLU
    X11
    Xext
    m
    dl
    z
    asound
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/libxplayer.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libavformat.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libavfilter.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libavcodec.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libswresample.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libavutil.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libswscale.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sipalsa/libsipalsa.a
    )

