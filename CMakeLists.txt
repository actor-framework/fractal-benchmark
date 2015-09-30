cmake_minimum_required(VERSION 2.8)
project(fractal-benchmark C CXX)

add_custom_target(all_benchmarks ALL)

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

################################################################################
#                                   Utility                                    #
################################################################################

macro(add_xxx_benchmark benchname dir name src_ending bin_ending out_path)
  set(OUT_FILE ${out_path}/${name}.${bin_ending})
  set(SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/${name}.${src_ending})
  set(CAF_${benchname}_SRC_FILES ${SRC_FILE} ${CAF_${benchname}_SRC_FILES})
  set(CAF_${benchname}_${name}_OUT ${OUT_FILE})
  set(CAF_${benchname}_OUT_FILES ${OUT_FILE} ${CAF_${benchname}_OUT_FILES})
  add_custom_command(OUTPUT ${OUT_FILE}
                     COMMAND ${CAF_${benchname}_COMPILER}
                             ${CAF_${benchname}_COMPILER_OPTS}
                             ${SRC_FILE}
                     DEPENDS ${SRC_FILE} ${ARGN}
                     WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
                     VERBATIM)
endmacro()

# make files show up in GUI even if not building a particular benchmark
macro(add_dummy_target benchname src_ending)
  # check all aditional dependency flags
  set(globstr "${CMAKE_CURRENT_SOURCE_DIR}/src/*.${src_ending}")
  foreach(additional_ending ${ARGN})
    set(globstr ${globstr}
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.${additional_ending}")
  endforeach()
  file(GLOB_RECURSE source_files ${globstr})
  add_custom_target(caf_${benchname}_dummy SOURCES ${source_files})
endmacro()

################################################################################
#                                    Scala                                     #
################################################################################

# search for Scala compiler if not disabled by the user
if(NOT CAF_NO_SCALA_BENCHMARKS)
  if(NOT CAF_SCALA_COMPILER)
    set(CAF_SCALA_COMPILER "scalac")
  endif()
  execute_process(COMMAND "${CAF_SCALA_COMPILER}" "-version"
                  RESULT_VARIABLE ScalacResult
                  ERROR_VARIABLE ScalacVersionStr)
  if(ScalacResult EQUAL 0)
    string(REGEX REPLACE ".*version ([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1"
           ScalacVersion "${ScalacVersionStr}")
    if(ScalacVersion VERSION_LESS 2.10)
      message(STATUS "Disable Scala benchmarks: version >= 2.10 required,"
                     "${ScalacVersion} found")
      set(CAF_NO_SCALA_BENCHMARKS yes)
    endif()
  else()
    message(STATUS "Disable Scala benchmarks: no running compiler found")
    set(CAF_NO_SCALA_BENCHMARKS yes)
  endif()
endif()
# add targets for Scala benchmarks
if(NOT CAF_NO_SCALA_BENCHMARKS)
  file(MAKE_DIRECTORY build/bin/org/caf)
  file(COPY src/Mandelbrot.class DESTINATION bin/org/caf/)
  file(COPY scala-values.txt DESTINATION bin/)
  message(STATUS "Add Scala benchmarks to build (found ${ScalacVersion})")
  set(CLASS_PATH ${EXECUTABLE_OUTPUT_PATH}/org/caf)
  set(CAF_SCALA_COMPILER_OPTS -optimize)
  macro(add_scala_benchmark name)
    add_xxx_benchmark(SCALA scala ${name} scala class ${CLASS_PATH} ${ARGN})
  endmacro()
  add_scala_benchmark(scala-requests)
  add_scala_benchmark(scala-fractal ${CAF_SCALA_scala-requests_OUT} "${EXECUTABLE_OUTPUT_PATH}/org/caf/Mandelbrot.class")
  add_custom_target(scala_benchmarks
                    SOURCES ${CAF_SCALA_SRC_FILES}
                    DEPENDS ${CAF_SCALA_OUT_FILES})
  add_dependencies(all_benchmarks scala_benchmarks)
  set(CAF_COMPILED_BENCHES "scala ${CAF_COMPILED_BENCHES}")
else()
  add_dummy_target(scala scala)
endif()

################################################################################
#                                    Erlang                                    #
################################################################################

add_custom_target(erlang_dummy SOURCES src/fractal.erl src/fractal_nif.cpp src/benchmark.erl)


###############################################################################
#                              FRACTAL-CPPA                                   #
###############################################################################

set(FRACTAL_CPPA_VERSION_MAJOR 0)
set(FRACTAL_CPPA_VERSION_MINOR 1)
set(FRACTAL_CPPA_VERSION_PATCH 0)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt4 REQUIRED QtCore QtGui)

find_package(Libcaf REQUIRED COMPONENTS core io)

file(GLOB HEADERS "include/*.hpp")

message(STATUS "Qt headers: ${QT_INCLUDES}")
message(STATUS "Qt libraries: ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY}")

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

###############################################################################
#                                    MPI                                      #
###############################################################################

find_package(MPI)
find_package(Boost COMPONENTS mpi serialization)

set(SOURCES_MPI
    src/fractal_request_stream.cpp
    src/mpi-fractal.cpp
    src/config.cpp)

if(MPI_C_FOUND AND Boost_FOUND)
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
  message(STATUS "skip MPI implementation")
  add_custom_target(mpi_dummy SOURCES ${SOURCES_MPI})
endif()

###############################################################################
#                                   misc.                                     #
###############################################################################

find_package(JNI)

if(JNI_FOUND)
  include_directories(${JNI_INCLUDE_DIRS})
  set(JNI_SOURCES src/Mandelbrot.cpp src/config.cpp)
  set(JNI_HEADERS src/org_caf_Mandelbrot.h)
  add_library(wrapper_lib SHARED ${JNI_SOURCES} ${JNI_HEADERS})
  target_link_libraries(wrapper_lib ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${JNI_LIBRARIES})
  set_target_properties(wrapper_lib PROPERTIES SOVERSION 1 VERSION 1
                        OUTPUT_NAME "fractal-benchmark")
endif()

add_custom_target(gui_dummy SOURCES src/Mandelbrot.java)
