cmake_minimum_required(VERSION 3.10)

project(
  libaudiodecoder
  VERSION 1.0
  LANGUAGES CXX)

if (APPLE)
    set(MACOSX TRUE)
endif()

SET(COMMON_SRCS
	src/audiodecoderbase.cpp
)

SET(WIN_SRCS
	src/audiodecodermediafoundation.cpp
)

SET(MAC_SRCS
	src/audiodecodercoreaudio.cpp
)

# Compose our SRCS variable based on the target OS.
SET(SRCS )
list(APPEND SRCS ${COMMON_SRCS})
if(WIN32)
	list(APPEND SRCS ${WIN_SRCS})
elseif(DARWIN)
	list(APPEND SRCS ${MAC_SRCS})
else()
	message( FATAL_ERROR "Building libaudiodecoder on this platform is not supported, sorry. Patches welcome." )
endif()


add_library(libaudiodecoder 
		  ${SRCS}
)

target_include_directories(libaudiodecoder PRIVATE include/)

if(WIN32)
	# These libraries come from the Windows SDK (Vista, 7, 10, 11+).
	target_link_libraries(libaudiodecoder PUBLIC Mf Mfplat mfreadwrite mfuuid ole32)
elseif(DARWIN)
	find_library(LIB_CF CoreFoundation)
	find_library(LIB_AT AudioToolbox)
	target_link_libraries(libaudiodecoder PUBLIC ${LIB_AT} ${LIB_CF})
else()
	message( FATAL_ERROR "You must implement linking against dependent libraries on this platform." )
endif()

# Enable parallel builds in MSVC
if(MSVC)
 target_compile_options(libaudiodecoder PRIVATE "/MP")
endif()