cmake_minimum_required(VERSION 2.8)
project(fractal-benchmark C CXX)

# silence policy CMP0042 warning by enabling RPATH explicitly
if(APPLE AND NOT DEFINED CMAKE_MACOSX_RPATH)
  set(CMAKE_MACOSX_RPATH true)
endif()

# prohibit in-source builds
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please use "
                        "./configure to choose a build directory and "
                        "initialize the build configuration.")
endif ()

# check for g++ >= 4.7 or clang++ > = 3.2
try_run(ProgramResult
        CompilationSucceeded
        ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/get_compiler_version.cpp
        RUN_OUTPUT_VARIABLE CompilerVersion)
if (NOT CompilationSucceeded OR NOT ProgramResult EQUAL 0)
  message(FATAL_ERROR "Cannot determine compiler version")
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  if (CompilerVersion VERSION_GREATER 4.6)
    message(STATUS "Found g++ version ${CompilerVersion}")
  else ()
    message(FATAL_ERROR "g++ >= 4.7 required (found: ${CompilerVersion}.")
  endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  if (CompilerVersion VERSION_GREATER 3.1)
    message(STATUS "Found clang++ version ${CompilerVersion}")
  else ()
    message(FATAL_ERROR "clang++ >= 3.2 required (found: ${CompilerVersion}.")
  endif ()
  if (NOT CXXFLAGS_PROVIDED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  endif ()
else ()
  message(FATAL_ERROR "Your C++ compiler does not support C++11.")
endif ()

###############################################################################
#                              FRACTAL-CPPA                                   #
###############################################################################

set(FRACTAL_CPPA_VERSION_MAJOR 0)
set(FRACTAL_CPPA_VERSION_MINOR 1)
set(FRACTAL_CPPA_VERSION_PATCH 0)

# qt variables
set(FRACTAL_CPPA_HDRS)
set(FRACTAL_CPPA_UIS src/main.ui src/controller.ui)
set(FRACTAL_CPPA_RCS)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# check if the user provided CXXFLAGS on the command line
if (CMAKE_CXX_FLAGS)
    set(CXXFLAGS_PROVIDED true)
endif ()
if (CXXFLAGS_PROVIDED)
  set(CMAKE_CXX_FLAGS_DEBUG          "")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "")
  set(CMAKE_CXX_FLAGS_RELEASE        "")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
else ()
  set(CMAKE_CXX_FLAGS                "-std=c++11 -Wextra -Wall -pedantic")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O4")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif ()

if (CAF_LOG_LEVEL)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCAF_LOG_LEVEL=${CAF_LOG_LEVEL}")
endif(CAF_LOG_LEVEL)

# set build type (evaluate ENABLE_DEBUG flag)
if (ENABLE_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCAF_DEBUG")
else ()
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

find_package(Libcaf COMPONENTS core io REQUIRED)

# this command finds Qt4 libraries and sets all required variables
# note that it's Qt4, not QT4 or qt4
FIND_PACKAGE(Qt4 REQUIRED)

# add some useful macros and variables
# (QT_USE_FILE is a variable defined by FIND_PACKAGE( Qt4 ) that contains a path to CMake script)
INCLUDE(${QT_USE_FILE})

# this command will generate rules that will run rcc on all files from SAMPLE_RCS
# in result SAMPLE_RC_SRCS variable will contain paths to files produced by rcc
QT4_ADD_RESOURCES(FRACTAL_CPPA_RC_SRCS ${FRACTAL_CPPA_RCS} src/fractal.qrc)

# this will run uic on .ui files:
QT4_WRAP_UI(FRACTAL_CPPA_UI_HDRS ${FRACTAL_CPPA_UIS})

# and finally this will run moc:
QT4_WRAP_CPP(FRACTAL_CPPA_MOC_SRCS ${FRACTAL_CPPA_HDRS}
                                   include/imagelabel.h
                                   include/mainwidget.hpp
                                   include/controllerwidget.hpp)

# we need this to be able to include headers produced by uic in our code
# (CMAKE_BINARY_DIR holds a path to the build directory, while INCLUDE_DIRECTORIES() works just like INCLUDEPATH
# from qmake)
INCLUDE_DIRECTORIES(. include ${CMAKE_BINARY_DIR} ${LIBCAF_INCLUDE_DIRS})

set(SOURCES_CAF
    src/config.cpp
    src/q_byte_array_info.cpp
    src/caf-fractal.cpp
    src/imagelabel.cpp
    src/mainwidget.cpp
    src/controllerwidget.cpp
    src/config_map.cpp
    src/client.cpp
    src/server.cpp
    src/fractal_request_stream.cpp
    src/controller.cpp
    ${FRACTAL_CPPA_MOC_SRCS}
    ${FRACTAL_CPPA_RC_SRCS}
    ${FRACTAL_CPPA_UI_HDRS})

file(GLOB HEADERS "include/*.hpp")


add_executable(fractal-cppa ${SOURCES_CAF} ${HEADERS})
target_link_libraries(fractal-cppa ${CMAKE_DL_LIBS} ${LIBCAF_LIBRARIES} ${QT_LIBRARIES})

###############################################################################
#                                    MPI                                      #
###############################################################################

find_package(MPI REQUIRED)
find_package(Boost COMPONENTS mpi serialization REQUIRED)

include_directories(${MPI_INCLUDE_PATH})
include_directories(${Boost_INCLUDE_DIRS})

set(SOURCES_MPI
    src/fractal_request_stream.cpp
    src/mpi-fractal.cpp
    src/config.cpp)

add_executable(mpi-fractal ${SOURCES_MPI})
target_link_libraries(mpi-fractal ${MPI_LIBRARIES} ${Boost_LIBRARIES} ${QT_LIBRARIES})

if(MPI_COMPILE_FLAGS)
  set_target_properties(mpi-fractal PROPERTIES
    COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()

if(MPI_LINK_FLAGS)
  set_target_properties(mpi-fractal PROPERTIES
    LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()