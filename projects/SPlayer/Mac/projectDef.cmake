#/**********************************************************\ 
# Auto-generated Mac project definition file for the
# BasicMediaPlayer project
#\**********************************************************/

# Mac template platform definition CMake file
# Included from ../CMakeLists.txt

# remember that the current source dir is the project root; this file is in Mac/
file (GLOB PLATFORM RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    Mac/[^.]*.m
    Mac/[^.]*.mm
    Mac/[^.]*.cpp
    Mac/[^.]*.h
    Mac/[^.]*.cmake
    )

# use this to add preprocessor definitions
add_definitions(
    
)

SOURCE_GROUP(Mac FILES ${PLATFORM})

file (GLOB RESOURCES
    Resources/[^.]*.png
    Resources/[^.]*.tiff
)
 
set_source_files_properties(
    ${RESOURCES}
    PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources/English.lproj"
)

source_group(Resources FILES
    ${RESOURCES}
)

set (SOURCES
    ${SOURCES}
    ${RESOURCES}
    ${PLATFORM}
    )

set(PLIST "Mac/bundle_template/Info.plist")
set(STRINGS "Mac/bundle_template/InfoPlist.strings")
set(LOCALIZED "Mac/bundle_template/Localized.r")

add_mac_plugin(${PROJECT_NAME} ${PLIST} ${STRINGS} ${LOCALIZED} SOURCES)

find_library(OPENGL_FRAMEWORK OpenGL)
# find_library(OPENAL_FRAMEWORK OpenAL)
find_library(QUARTZ_CORE_FRAMEWORK QuartzCore)
# find_library(CORE_ANIMATION_FRAMEWORK CoreAnimation)
find_library(AUDIO_TOOLBOX_FRAMEWORK AudioToolbox)
find_library(AUDIO_UNIT_FRAMEWORK AudioUnit)
find_library(VDA_FRAMEWORK VideoDecodeAcceleration)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg)
include_directories(/Developer/Extras/CoreAudio/PublicUtility)

# add library dependencies here; leave ${PLUGIN_INTERNAL_DEPS} there unless you know what you're doing!
target_link_libraries(${PROJECT_NAME}
    ${PLUGIN_INTERNAL_DEPS}
    z
    bz2
    ${AUDIO_TOOLBOX_FRAMEWORK}
	#	${CORE_ANIMATION_FRAMEWORK}
	${AUDIO_UNIT_FRAMEWORK}
	${OPENGL_FRAMEWORK}
	${QUARTZ_CORE_FRAMEWORK}
    ${VDA_FRAMEWORK}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/libxplayer.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libswscale.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libavformat.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libavcodec.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libswresample.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libavutil.a
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libffmpeg/lib/libavfilter.a
)



# Copy plugin to Plug-Ins directory:
function(releasePlugin projectName pathToPlugin releaseDirectory)
	ADD_CUSTOM_COMMAND(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND mkdir -p ${releaseDirectory}/${projectName}.plugin
		)
	ADD_CUSTOM_COMMAND( TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND cp -pr ${pathToPlugin} ${releaseDirectory}
		)
endfunction()

# Current output
set(PBIN "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${PROJECT_NAME}.plugin")

# Copy plugin to /Library/Internet Plug-Ins (all users)
releasePlugin("${PROJECT_NAME}" "${PBIN}" "/Library/Internet Plug-Ins")

#create .dmg installer
#INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/Mac/installer.cmake")

