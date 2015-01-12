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

# check if the user provided CXXFLAGS on the command line
if (CMAKE_CXX_FLAGS)
  set(CXXFLAGS_PROVIDED true)
else()
  set(CMAKE_CXX_FLAGS "-std=c++11 -Wextra -Wall -pedantic -O3")
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

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt4 REQUIRED QtCore QtGui)

find_package(Libcaf COMPONENTS core io)

file(GLOB HEADERS "include/*.hpp")

if(LIBCAF_FOUND)
  # we need this to be able to include headers produced by uic in our code
  # (CMAKE_BINARY_DIR holds a path to the build directory, while INCLUDE_DIRECTORIES() works just like INCLUDEPATH
  # from qmake)
  include_directories(. include ${QT_INCLUDES} ${LIBCAF_INCLUDE_DIRS})
  set(SOURCES_CAF
      src/config.cpp
      src/q_byte_array_info.cpp
      src/caf-fractal.cpp
      src/fractal_request_stream.cpp)
  add_executable(fractal-cppa ${SOURCES_CAF} ${HEADERS})
  target_link_libraries(fractal-cppa ${CMAKE_DL_LIBS} ${LIBCAF_LIBRARIES}
                        ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY})
endif()

###############################################################################
#                                    MPI                                      #
###############################################################################

find_package(MPI)
find_package(Boost COMPONENTS mpi serialization)

set(SOURCES_MPI
    src/fractal_request_stream.cpp
    src/mpi-fractal.cpp
    src/config.cpp)

if(MPI_FOUND AND BOOST_FOUND)
  include_directories(. include ${MPI_INCLUDE_PATH} ${Boost_INCLUDE_DIRS}
                      ${QT_INCLUDES})
  add_executable(mpi-fractal ${SOURCES_MPI} ${HEADERS})
  target_link_libraries(mpi-fractal ${MPI_LIBRARIES} ${Boost_LIBRARIES}
                        ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY})
  set_target_properties(mpi-fractal PROPERTIES
                        COMPILE_FLAGS "${MPI_COMPILE_FLAGS} -O3")
  if(MPI_LINK_FLAGS)
    set_target_properties(mpi-fractal PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
  endif()
else()
  add_custom_target(mpi_dummy SOURCES ${SOURCES_MPI})
endif()
