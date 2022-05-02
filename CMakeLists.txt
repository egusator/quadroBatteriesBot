 

PROJECT (cv_sb)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(CURL)

set( NAME_SRC
    src/main.cpp
        )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpqxx -lpq")

set(PQXX /usr/local/include/pqxx)

find_library(PQXX_LIB pqxx)
find_library(PQ_LIB pq)

file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")






if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    add_definitions(-DHAVE_CURL)
endif()
include_directories("date/include/date")


INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR}/include )

link_directories( ${CMAKE_BINARY_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

add_executable( drainer  ${NAME_SRC} )

target_compile_features(drainer PRIVATE cxx_std_17)
TARGET_LINK_libraries (drainer ${PQXX_LIB} ${PQ_LIB})

target_link_libraries(drainer /usr/local/lib/libTgBot.a ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES})
