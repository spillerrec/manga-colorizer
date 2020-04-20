cmake_minimum_required(VERSION 3.0)
project(manga-colorizer)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall") # Turn on warnings

#Reduce executable size
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -fmerge-constants -fno-ident")

# Qt compilers setup
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SOURCE_CORE
	src/main.cpp
	)


add_executable(manga-colorizer ${SOURCE_CORE})

# Set-up libraries
find_package(Qt5Widgets REQUIRED)

target_link_libraries(manga-colorizer Qt5::Widgets -lsvgren -lpapki -lsvgdom)


# Enable C++14 features
set_property(TARGET manga-colorizer PROPERTY CXX_STANDARD 17)
set_property(TARGET manga-colorizer PROPERTY CXX_STANDARD_REQUIRED ON)
