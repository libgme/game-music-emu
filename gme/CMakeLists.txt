# List of source files required by libgme and any emulators
# This is not 100% accurate (Fir_Resampler for instance) but
# you'll be OK.
set(libgme_SRCS Blip_Buffer.cpp
                Classic_Emu.cpp
                Data_Reader.cpp
                Dual_Resampler.cpp
                Effects_Buffer.cpp
                Fir_Resampler.cpp
                gme.cpp
                Gme_File.cpp
                M3u_Playlist.cpp
                Multi_Buffer.cpp
                Music_Emu.cpp
                )

# static builds need to find static zlib (and static forms of other needed
# libraries.  Ensure CMake looks only for static libs if we're doing a static
# build.  See https://stackoverflow.com/a/44738756
if(NOT BUILD_SHARED_LIBS)
  if(MSVC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
  else()
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  endif()
endif()

if(GME_ZLIB)
    find_package(ZLIB QUIET)
endif()
if (USE_GME_SPC AND GME_UNRAR)
    find_package(UNRAR QUIET)
endif()

# Ay_Apu is very popular around here
if(USE_GME_AY OR USE_GME_KSS)
    list(APPEND libgme_SRCS
                Ay_Apu.cpp
        )
endif()

# so is Ym2612_Emu
if(USE_GME_VGM OR USE_GME_GYM)
    if(GME_YM2612_EMU STREQUAL "Nuked")
        add_definitions(-DVGM_YM2612_NUKED)
        list(APPEND libgme_SRCS
                    Ym2612_Nuked.cpp
            )
        message(STATUS "VGM/GYM: Nuked OPN2 emulator will be used")
    elseif(GME_YM2612_EMU STREQUAL "MAME")
        add_definitions(-DVGM_YM2612_MAME)
        list(APPEND libgme_SRCS
                    Ym2612_MAME.cpp
            )
        message(STATUS "VGM/GYM: MAME YM2612 emulator will be used")
    else()
        add_definitions(-DVGM_YM2612_GENS)
        list(APPEND libgme_SRCS
                    Ym2612_GENS.cpp
            )
        message(STATUS "VGM/GYM: GENS 2.10 emulator will be used")
    endif()
endif()

# But none are as popular as Sms_Apu
if(USE_GME_VGM OR USE_GME_GYM OR USE_GME_KSS)
    list(APPEND libgme_SRCS
                Sms_Apu.cpp
        )
endif()

if(USE_GME_AY)
    list(APPEND libgme_SRCS
              # Ay_Apu.cpp included earlier
                Ay_Cpu.cpp
                Ay_Emu.cpp
        )
endif()

if(USE_GME_GBS)
    list(APPEND libgme_SRCS
                Gb_Apu.cpp
                Gb_Cpu.cpp
                Gb_Oscs.cpp
                Gbs_Emu.cpp
        )
endif()

if(USE_GME_GYM)
    list(APPEND libgme_SRCS
              # Sms_Apu.cpp included earlier
              # Ym2612_Emu.cpp included earlier
                Gym_Emu.cpp
        )
endif()

if(USE_GME_HES)
    list(APPEND libgme_SRCS
                Hes_Apu.cpp
                Hes_Cpu.cpp
                Hes_Emu.cpp
        )
endif()

if(USE_GME_KSS)
    list(APPEND libgme_SRCS
              # Ay_Apu.cpp included earlier
              # Sms_Apu.cpp included earlier
                Kss_Cpu.cpp
                Kss_Emu.cpp
                Kss_Scc_Apu.cpp
        )
endif()

if(USE_GME_NSF OR USE_GME_NSFE)
    list(APPEND libgme_SRCS
                Nes_Apu.cpp
                Nes_Cpu.cpp
                Nes_Fme7_Apu.cpp
                Nes_Namco_Apu.cpp
                Nes_Oscs.cpp
                Nes_Vrc6_Apu.cpp
                Nes_Fds_Apu.cpp
                Nes_Vrc7_Apu.cpp
                ext/emu2413.c
                ext/panning.c
                Nsf_Emu.cpp
        )
endif()

if(USE_GME_NSFE)
    list(APPEND libgme_SRCS
                Nsfe_Emu.cpp
        )
endif()

if(USE_GME_SAP)
    list(APPEND libgme_SRCS
                Sap_Apu.cpp
                Sap_Cpu.cpp
                Sap_Emu.cpp
        )
endif()

if(USE_GME_SPC)
    list(APPEND libgme_SRCS
                Snes_Spc.cpp
                Spc_Cpu.cpp
                Spc_Dsp.cpp
                Spc_Emu.cpp
                Spc_Filter.cpp
        )
    if(GME_SPC_ISOLATED_ECHO_BUFFER)
        add_definitions(-DSPC_ISOLATED_ECHO_BUFFER)
    endif()
endif()

if(USE_GME_VGM)
    list(APPEND libgme_SRCS
              # Sms_Apu.cpp included earlier
              # Ym2612_Emu.cpp included earlier
                Vgm_Emu.cpp
                Vgm_Emu_Impl.cpp
                Ym2413_Emu.cpp
        )
endif()

# These headers are part of the generic gme interface.
set (EXPORTED_HEADERS gme.h)

# while building a macOS framework, exported headers must be in the source
# list, or the header files aren't copied to the bundle.
if(BUILD_FRAMEWORK)
    set(libgme_SRCS ${libgme_SRCS} ${EXPORTED_HEADERS})
endif()

# Add library to be compiled.
add_library(gme ${libgme_SRCS})
add_library(gme::gme ALIAS gme)

set_property(TARGET gme PROPERTY C_VISIBILITY_PRESET "hidden")
set_property(TARGET gme PROPERTY VISIBILITY_INLINES_HIDDEN TRUE)
set_property(TARGET gme PROPERTY CXX_VISIBILITY_PRESET "hidden")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_definitions(gme PRIVATE LIBGME_VISIBILITY)
endif()

# On some platforms we may need to change headers or whatnot based on whether
# we're building the library or merely using the library. The following is
# only defined when building the library to allow us to tell which is which.
set_property(TARGET gme PROPERTY DEFINE_SYMBOL BLARGG_BUILD_DLL)

target_compile_definitions(gme PRIVATE GEN_TYPES_H)
if(WORDS_BIGENDIAN)
    target_compile_definitions(gme PRIVATE BLARGG_BIG_ENDIAN=1)
else()
    target_compile_definitions(gme PRIVATE BLARGG_LITTLE_ENDIAN=1)
endif()

# Try to protect against undefined behavior from signed integer overflow
# This has caused miscompilation of code already and there are other
# potential uses; see https://bitbucket.org/mpyne/game-music-emu/issues/18/
target_compile_options(gme PRIVATE -fwrapv)

target_include_directories(gme
    PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"  # For gen_types.h
    INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

## FIXME: Properly find the C++ library !!!
set(PC_LIBS -lstdc++)

if(NOT MSVC)
    # Link with -no-undefined, if available
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "-Wl,-no-undefined")
    check_cxx_source_compiles("int main(void) { return 0;}" LINKER_SUPPORTS_NO_UNDEFINED)
    cmake_pop_check_state()
    if(LINKER_SUPPORTS_NO_UNDEFINED)
        set_property(TARGET gme APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-no-undefined")
    endif()

    # Link to libm, if necessary
    check_cxx_source_compiles("
        #include <math.h>
        int main(int argc, char** argv) {
            return (int) pow(argc, 2.5);
        }" LIBM_NOT_NEEDED)
    if(NOT LIBM_NOT_NEEDED)
        cmake_push_check_state()
        set(CMAKE_REQUIRED_LIBRARIES "-lm")
        check_cxx_source_compiles("
            #include <math.h>
            int main(int argc, char** argv) {
                return (int) pow(argc, 2.5);
            }" HAVE_LIBM)
        cmake_pop_check_state()
        if(HAVE_LIBM)
            target_link_libraries(gme PRIVATE -lm)
            list(APPEND PC_LIBS -lm) # for libgme.pc
        endif()
    endif()
endif()

if(USE_GME_SPC AND GME_UNRAR)
  if(UNRAR_FOUND)
      message(STATUS "UnRAR library located, the RSN file format will be supported")
      target_compile_definitions(gme PRIVATE RARDLL)
      if(RAR_HDR_UNRAR_H)
          target_compile_definitions(gme PRIVATE RAR_HDR_UNRAR_H)
      elseif(RAR_HDR_DLL_HPP)
          target_compile_definitions(gme PRIVATE RAR_HDR_DLL_HPP)
      endif()
      target_include_directories(gme PRIVATE ${UNRAR_INCLUDE_DIRS})
      target_link_libraries(gme PRIVATE ${UNRAR_LIBRARIES})
      # Is not to be installed though
      list(APPEND PC_LIBS -lunrar) # for libgme.pc
      if (NOT WIN32) # POSIX Threading for unRAR
          target_compile_options(gme PRIVATE -pthread)
      endif()
  else()
      message(STATUS "** UnRAR library not found, disabling support for the RSN file format")
  endif()
else()
  message(STATUS "RSN file format excluded")
endif()

if(GME_ZLIB)
  if(ZLIB_FOUND)
      message(STATUS "ZLib library located, compressed file formats will be supported")
      target_compile_definitions(gme PRIVATE HAVE_ZLIB_H)
      target_link_libraries(gme PRIVATE ZLIB::ZLIB)
      # Is not to be installed though
      list(APPEND PC_LIBS -lz) # for libgme.pc
  else()
      message(STATUS "** ZLib library not found, disabling support for compressed formats such as VGZ")
  endif()
else()
  message(STATUS "Zlib-Compressed formats excluded")
endif()

# The version is the release.  The "soversion" is the API version.  As long
# as only build fixes are performed (i.e. no backwards-incompatible changes
# to the API), the SOVERSION should be the same even when bumping up VERSION.
# The way gme.h is designed, SOVERSION should very rarely be bumped, if ever.
# Hopefully the API can stay compatible with old versions.
set_target_properties(gme
    PROPERTIES VERSION ${GME_VERSION}
               SOVERSION 0)

# macOS framework build
if(BUILD_FRAMEWORK)
    set_target_properties(gme
        PROPERTIES FRAMEWORK TRUE
                   FRAMEWORK_VERSION A
                   MACOSX_FRAMEWORK_IDENTIFIER net.mpyne.gme
                   VERSION ${GME_VERSION}
                   SOVERSION 0
                   PUBLIC_HEADER "${EXPORTED_HEADERS}")
endif()

install(TARGETS gme LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  # DLL platforms
                    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}  # DLL platforms
                    FRAMEWORK DESTINATION /Library/Frameworks)   # macOS framework


# Run during cmake phase, so this is available during make
configure_file(gme_types.h.in gen_types.h @ONLY)
string(JOIN " " PC_LIBS ${PC_LIBS})
configure_file(libgme.pc.in libgme.pc @ONLY)

install(FILES ${EXPORTED_HEADERS} DESTINATION include/gme)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libgme.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
