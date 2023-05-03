
set(static_lib ${PROJECT_SOURCE_DIR}/dependency/NuRaft/build/libnuraft.a)

# # === Example replicated calculator ===
# add_executable(calc_server
#                calculator/calc_server.cxx
#                logger.cc
#                in_memory_log_store.cxx)
# add_dependencies(calc_server
#                  ${static_lib})
# target_link_libraries(calc_server
#                       ${BUILD_DIR}/${LIBRARY_OUTPUT_NAME}
#                       ${LIBRARIES})

# add_executable(echo_server
#                echo/echo_server.cxx
#                logger.cc
#                in_memory_log_store.cxx)
# add_dependencies(echo_server
#                  ${static_lib})
# target_link_libraries(echo_server
#                       ${BUILD_DIR}/${LIBRARY_OUTPUT_NAME}
#                       ${LIBRARIES})


set(LIBRARY_OUTPUT_NAME "libnuraft.a")

include_directories(${PROJECT_SOURCE_DIR}/dependency/NuRaft/include)
include_directories(${PROJECT_SOURCE_DIR}/dependency/NuRaft/include/libnuraft)
include_directories(${PROJECT_SOURCE_DIR}/examples)
include_directories(${PROJECT_SOURCE_DIR}/examples/calculator)
include_directories(${PROJECT_SOURCE_DIR}/examples/echo)

add_executable(quick_start
               quick_start.cxx
               logger.cc
               in_memory_log_store.cxx)

target_link_libraries(quick_start
  ${PROJECT_SOURCE_DIR}/dependency/NuRaft/build/libnuraft.a
  ${LIBRARIES})

