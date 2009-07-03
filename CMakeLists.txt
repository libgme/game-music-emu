# CMake project definition file.
project(libgme)

# 2.6+ always assumes FATAL_ERROR, but 2.4 and below don't.
# Of course, 2.4 might work, in which case you're welcome to drop
# down the requirement, but I can't test that.
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

# Shared library defined here
add_subdirectory(gme)

# EXCLUDE_FROM_ALL adds build rules but keeps it out of default build
add_subdirectory(player EXCLUDE_FROM_ALL)
