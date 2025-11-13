#------------------------------------------------
# environment config
#------------------------------------------------
## CMAKE_MINIMUM_REQUIRED (VERSION 3.14)
CMAKE_MINIMUM_REQUIRED (VERSION 3.0)
MESSAGE(STATUS "CMAKE_VERSION:${CMAKE_VERSION}")

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
# add project
#************************************************
PROJECT(QComService)

# 包含子目录头文件

# 添加子目录，这样子目录中的CMakeLists.txt才会被调用

MESSAGE(STATUS "PROJECT_NAME:${PROJECT_NAME}")
MESSAGE(STATUS "PROJECT_SOURCE_DIR:${PROJECT_SOURCE_DIR}")
MESSAGE(STATUS "PROJECT_BINARY_DIR:${PROJECT_BINARY_DIR}")

#************************************************
# add the include directories
#************************************************
SET(PROJECT_INCLUDE_PATH
    ${QNX_SYSROOT}/usr/include
    ${PROJECT_SOURCE_DIR}/inc
    ${PROJECT_SOURCE_DIR}/src/debug
)

MESSAGE(STATUS "PROJECT_INCLUDE_PATH: ${PROJECT_INCLUDE_PATH}")
INCLUDE_DIRECTORIES(${PROJECT_INCLUDE_PATH})

#************************************************
# add sub directory
#************************************************
# ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/test)


#************************************************
# add the link directories
#************************************************
LINK_DIRECTORIES(
   ${PROJECT_SOURCE_DIR}/test/libs
   ${QNX_SYSROOT}/aarch64le/lib
   ${QNX_SYSROOT}/aarch64le/usr/lib
)


#************************************************
# add executable or library
#************************************************

SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin/")
SET(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/lib/")


SET(LIBS
    ssl
    slog2
)

SET(SERVICE_NAME QComService)
ADD_EXECUTABLE(${SERVICE_NAME}
    src/debug/Debug.cpp
    src/ServiceManager.cpp
	src/main.cpp
)
TARGET_LINK_LIBRARIES(${SERVICE_NAME} -Wl,--start-group ${LIBS} -Wl,--end-group,-lsocket -Wsign-compare)

# install
INSTALL(TARGETS ${SERVICE_NAME} RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/out/usr/bin/)
