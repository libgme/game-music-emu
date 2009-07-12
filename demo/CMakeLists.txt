# Rules for building the demo.  Doesn't use an installed gme if you've already
# installed the project so if you're copying these rules you probably don't
# even need these next two lines if you're building against system-installed
# gme.
include_directories(${CMAKE_SOURCE_DIR}/gme ${CMAKE_SOURCE_DIR})
link_directories(${CMAKE_BINARY_DIR}/gme)

add_executable(demo Wave_Writer.cpp basics.c)

target_link_libraries(demo gme)
