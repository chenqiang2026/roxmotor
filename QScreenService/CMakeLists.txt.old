#------------------------------------------------
# environment config
#------------------------------------------------

IF ("$ENV{QNX_HOST}" STREQUAL "")
    MESSAGE(FATAL_ERROR "QNX_HOST environment variable is not set")
ENDIF()

IF ("$ENV{QNX_TARGET}" STREQUAL "")
    MESSAGE(FATAL_ERROR "QNX_TARGET environment variable is not set")
ENDIF()

IF ("$ENV{QNX_SYSROOT}" STREQUAL "")
    MESSAGE(STATUS "QNX_SYSROOT environment variable is not set")
ENDIF()

OPTION(TARGET_ON_BOARD "determine where to run(TARGET_BOARD or QNX_x86)" ON)
IF (${TARGET_ON_BOARD})
    MESSAGE(STATUS "TARGET_ON_BOARD IS ON, run on the target board")
    SET(ARCH gcc_ntoaarch64le)
    SET(QCC_NTOARCH aarch64le)
ELSE()
    MESSAGE(STATUS "TARGET_ON_BOARD IS OFF, run on the QNX-x86 system")
    SET(ARCH gcc_ntox86_64)
    SET(QCC_NTOARCH x86_64)
ENDIF()

SET(QNX_HOST "$ENV{QNX_HOST}")
SET(QNX_TARGET "$ENV{QNX_TARGET}")
#SET(QNX_SYSROOT "$ENV{QNX_SYSROOT}")
SET(QNX_SYSROOT "$ENV{QNX_TARGET}")

MESSAGE(STATUS "QNX_HOST:${QNX_HOST}")
MESSAGE(STATUS "QNX_TARGET:${QNX_TARGET}")
MESSAGE(STATUS "ARCH:${ARCH}")
MESSAGE(STATUS "QCC_NTOARCH:${QCC_NTOARCH}")
MESSAGE(STATUS "QNX_SYSROOT:${QNX_SYSROOT}")

#************************************************
# set compiler and compiler's options
#************************************************
SET(CMAKE_C_COMPILER "${QNX_HOST}/usr/bin/qcc")
SET(CMAKE_C_COMPILER_TARGET "${ARCH}")
SET(CMAKE_CXX_COMPILER ${QNX_HOST}/usr/bin/q++)
SET(CMAKE_CXX_COMPILER_TARGET "${ARCH}")
SET(CMAKE_LINKER       "${QNX_HOST}/usr/bin/nto${QCC_NTOARCH}-ld"     CACHE PATH "QNX linker program" FORCE)
SET(CMAKE_C_FLAGS "$ENV{CMAKE_C_FLAGS} -Wall -g -fPIC -fmessage-length=0 -fno-builtin -O0 -g2 -ggdb")
SET(CMAKE_CXX_FLAGS "$ENV{CMAKE_CXX_FLAGS} -Wall -g -std=c++11 -fPIC -DENABLE_QNX -D_GLIBCXX_USE_CXX11_ABI=0 -U__STRICT_ANSI__ -lang-c++ -g2 -ggdb")

MESSAGE(STATUS "CMAKE_C_COMPILER ${CMAKE_C_COMPILER}")
MESSAGE(STATUS "CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER}")
MESSAGE(STATUS "CMAKE_C_COMPILER ${CMAKE_C_COMPILER}")
MESSAGE(STATUS "CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER}")
MESSAGE(STATUS "CMAKE_LINKER ${CMAKE_LINKER}")
MESSAGE(STATUS "CMAKE_C_FLAGS:${CMAKE_C_FLAGS}")
MESSAGE(STATUS "CMAKE_CXX_FLAGS:${CMAKE_CXX_FLAGS}")

#************************************************
# set project options
#************************************************
cmake_minimum_required(VERSION 3.14)
MESSAGE(STATUS "CMAKE_VERSION:${CMAKE_VERSION}")

project(screenService LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
aux_source_directory(src SOURCES)
# aux_source_directory(source SOURCES)

# define target binary
add_executable(${PROJECT_NAME} ${SOURCES})

# add include headers
target_include_directories(${PROJECT_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)

# add link libs
target_link_libraries(${PROJECT_NAME}
PRIVATE
    i2c_handler
    ${QNX_SYSROOT}/aarch64le/lib
    ${QNX_SYSROOT}/aarch64le/usr/lib
)

#============= CMake Install ============
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
