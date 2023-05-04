set(EXAMPLES_SRC ${PROJECT_SOURCE_DIR}/examples)

set(BINARY_1 quick_start2)

add_executable(${BINARY_1}
  ${EXAMPLES_SRC}/quick_start.cxx
  ${EXAMPLES_SRC}/logger.cc
  ${EXAMPLES_SRC}/in_memory_log_store.cxx
)

include_directories(${EXAMPLES_SRC}/)
include_directories(${EXAMPLES_SRC}/calculator)
include_directories(${EXAMPLES_SRC}/echo)

# NOTE: ordering of libraries is important !
target_link_libraries(${BINARY_1} PUBLIC
  # ${NURAFT_LIBRARY} # will pick `.so` library ! instead of `.a` static library
  ${CMAKE_CURRENT_BINARY_DIR}/dependency/NuRaft/libnuraft.so # NURAFT_LIBRARY gives wrong path

  OpenSSL::SSL OpenSSL::Crypto
  asio::asio
  ${SSL_LIBRARY}
)

# wait for NuRaft project target binaries
add_dependencies(${BINARY_1} static_lib) 
add_dependencies(${BINARY_1} shared_lib) 

target_compile_options(${BINARY_1} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++20 -pthread -Wall -g -O0 -D_FILE_OFFSET_BITS=64 -Wextra -Wzero-as-null-pointer-constant -Wextra -Wno-unused -Wno-unused-parameter>)

install(TARGETS ${BINARY_1} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# ---------------

set(BINARY_2 calc_server)

add_executable(${BINARY_2}
  ${EXAMPLES_SRC}/calculator/calc_server.cxx
  ${EXAMPLES_SRC}/logger.cc
  ${EXAMPLES_SRC}/in_memory_log_store.cxx
)

include_directories(${EXAMPLES_SRC}/)
include_directories(${EXAMPLES_SRC}/calculator)
include_directories(${EXAMPLES_SRC}/echo)

# NOTE: ordering of libraries is important !
target_link_libraries(${BINARY_2} PUBLIC
  # ${NURAFT_LIBRARY} # will pick `.so` library ! instead of `.a` static library
  ${CMAKE_CURRENT_BINARY_DIR}/dependency/NuRaft/libnuraft.so # NURAFT_LIBRARY gives wrong path

  OpenSSL::SSL OpenSSL::Crypto
  asio::asio
  ${SSL_LIBRARY}
)

# wait for NuRaft project target binaries
add_dependencies(${BINARY_2} static_lib) 
add_dependencies(${BINARY_2} shared_lib) 

target_compile_options(${BINARY_2} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++20 -pthread -fPIC -lssl -lcrypto -lrt -fuse-ld=gold -Wall -g -O0 -D_FILE_OFFSET_BITS=64 -Wextra -Wzero-as-null-pointer-constant -Wextra -Wno-unused -Wno-unused-parameter>)

install(TARGETS ${BINARY_2} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# ---------------

set(BINARY_3 echo_server)

add_executable(${BINARY_3}
  ${EXAMPLES_SRC}/echo/echo_server.cxx
  ${EXAMPLES_SRC}/logger.cc
  ${EXAMPLES_SRC}/in_memory_log_store.cxx
)

include_directories(${EXAMPLES_SRC}/)
include_directories(${EXAMPLES_SRC}/calculator)
include_directories(${EXAMPLES_SRC}/echo)

# NOTE: ordering of libraries is important !
target_link_libraries(${BINARY_3} PUBLIC
  # ${NURAFT_LIBRARY} # will pick `.so` library ! instead of `.a` static library
  ${CMAKE_CURRENT_BINARY_DIR}/dependency/NuRaft/libnuraft.so # NURAFT_LIBRARY gives wrong path

  OpenSSL::SSL OpenSSL::Crypto
  asio::asio
  ${SSL_LIBRARY}
)

# wait for NuRaft project target binaries
add_dependencies(${BINARY_3} static_lib) 
add_dependencies(${BINARY_3} shared_lib) 

target_compile_options(${BINARY_3} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++20 -pthread -fPIC -lssl -lcrypto -lrt -fuse-ld=gold -Wall -g -O0 -D_FILE_OFFSET_BITS=64 -Wextra -Wzero-as-null-pointer-constant -Wextra -Wno-unused -Wno-unused-parameter>)

install(TARGETS ${BINARY_3} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

