# List of source files required by libgme
# TODO: Allow for conditional compilation of only certain emulators.
set(libgme_SRCS Ay_Apu.cpp
                Ay_Cpu.cpp
                Ay_Emu.cpp
                Blip_Buffer.cpp
                Classic_Emu.cpp
                Data_Reader.cpp
                Dual_Resampler.cpp
                Effects_Buffer.cpp
                Fir_Resampler.cpp
                Gb_Apu.cpp
                Gb_Cpu.cpp
                Gb_Oscs.cpp
                Gbs_Emu.cpp
                gme.cpp
                Gme_File.cpp
                Gym_Emu.cpp
                Hes_Apu.cpp
                Hes_Cpu.cpp
                Hes_Emu.cpp
                Kss_Cpu.cpp
                Kss_Emu.cpp
                Kss_Scc_Apu.cpp
                M3u_Playlist.cpp
                Multi_Buffer.cpp
                Music_Emu.cpp
                Nes_Apu.cpp
                Nes_Cpu.cpp
                Nes_Fme7_Apu.cpp
                Nes_Namco_Apu.cpp
                Nes_Oscs.cpp
                Nes_Vrc6_Apu.cpp
                Nsfe_Emu.cpp
                Nsf_Emu.cpp
                Sap_Apu.cpp
                Sap_Cpu.cpp
                Sap_Emu.cpp
                Sms_Apu.cpp
                Snes_Spc.cpp
                Spc_Cpu.cpp
                Spc_Dsp.cpp
                Spc_Emu.cpp
                Vgm_Emu.cpp
                Vgm_Emu_Impl.cpp
                Ym2413_Emu.cpp
                Ym2612_Emu.cpp)

# Add library to be compiled.
add_library(gme SHARED ${libgme_SRCS})

# The version is the release.  The "soversion" is the API version.  As long
# as only build fixes are performed (i.e. no changes/additions to API) the
# SOVERSION should be the same even when bumping up VERSION.
set_target_properties(gme
    PROPERTIES VERSION 0.5.3
               SOVERSION 0)

# TODO: Libsuffix for 64-bit?
install(TARGETS gme LIBRARY DESTINATION lib)
