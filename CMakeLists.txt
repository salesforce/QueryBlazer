cmake_minimum_required(VERSION 3.10)
project(QueryBlazer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/..)

find_package(Boost 1.41.0 REQUIRED COMPONENTS serialization)
add_subdirectory(third_party/pybind11)

set(FST_INCDIR "third_party/openfst-1.7.7/src/include")
set(UTF8_INCDIR "third_party/utf8")
set(THREADPOOL_INCDIR "third_party/ThreadPool")

file(GLOB FST_LIB_SRC third_party/openfst-1.7.7/src/lib/*.cc)
add_library(FST_LIB ${FST_LIB_SRC})
target_include_directories(FST_LIB PUBLIC ${FST_INCDIR})
set_property(TARGET FST_LIB PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(QBZ_LIB INTERFACE)
target_include_directories(QBZ_LIB BEFORE INTERFACE ${UTF8_INCDIR} ${Boost_INCLUDE_DIRS} ${THREADPOOL_INCDIR})
target_link_libraries(QBZ_LIB INTERFACE FST_LIB dl ${Boost_LIBRARIES} pthread)

add_executable(qbz_build_encoder src/build_encoder.cc src/mpc.h)
target_link_libraries(qbz_build_encoder QBZ_LIB)

add_executable(qbz_encode src/encode.cc)
target_link_libraries(qbz_encode QBZ_LIB)

add_executable(qbz_test_candidates src/test_candidates.cc)
target_link_libraries(qbz_test_candidates QBZ_LIB)

pybind11_add_module(queryblazer src/queryblazer.h src/queryblazer.cc)
target_link_libraries(queryblazer PRIVATE QBZ_LIB)

add_executable(qbz_build_queryblazer src/build_queryblazer.cc)
target_link_libraries(qbz_build_queryblazer QBZ_LIB)

add_executable(qbz_test_queryblazer src/test_queryblazer.cc)
target_link_libraries(qbz_test_queryblazer QBZ_LIB)

add_executable(qbz_build_mpc src/build_mpc.cc)
target_link_libraries(qbz_build_mpc QBZ_LIB)

add_executable(qbz_test_mpc src/test_mpc.cc)
target_link_libraries(qbz_test_mpc QBZ_LIB)