option(GME_UNRAR "Enable RAR file format (optional, requires UnRAR library)" ON)
option(GME_ARCHIVE "Enable ZIP file format (optional, requires LibArchive library)" ON)

find_package(SDL2)

if (GME_UNRAR)
    find_package(UNRAR QUIET)
endif()

if(GME_ARCHIVE)
    find_package(LibArchive QUIET)
    find_package(ZLIB QUIET)
endif()

set(player_SRCS Audio_Scope.cpp
                Audio_Scope.h
                Music_Player.cpp
                Music_Player.h
                Archive_Reader.cpp
                Archive_Reader.h
                player.cpp)

if(SDL2_FOUND)
    message(STATUS "SDL2 library located, player demo is available to be built in the /player directory")

    add_executable(gme_player ${player_SRCS})
    target_include_directories(gme_player PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/gme
    )
    set_property(TARGET gme_player PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET gme_player PROPERTY CXX_STANDARD 11)
    target_link_libraries(gme_player PRIVATE ${SDL2_LIBRARIES} gme::gme)
    # Is not to be installed though

    if(GME_UNRAR)
      if(UNRAR_FOUND)
          message(STATUS "UnRAR library located, player demo will support the RAR file format")
          target_compile_definitions(gme_player PRIVATE RARDLL)
          if(RAR_HDR_UNRAR_H)
              target_compile_definitions(gme_player PRIVATE RAR_HDR_UNRAR_H)
          elseif(RAR_HDR_DLL_HPP)
              target_compile_definitions(gme_player PRIVATE RAR_HDR_DLL_HPP)
          endif()
          target_include_directories(gme_player PRIVATE ${UNRAR_INCLUDE_DIRS})
          target_link_libraries(gme_player PRIVATE ${UNRAR_LIBRARIES})
          # Is not to be installed though
          list(APPEND PC_LIBS -lunrar) # for libgme.pc
          if (NOT WIN32) # POSIX Threading for unRAR
              target_compile_options(gme_player PRIVATE -pthread)
          endif()
      else()
          message(STATUS "** UnRAR library not found, player demo will not support the RAR file format")
      endif()
    else()
      message(STATUS "RAR file format excluded")
    endif()

    if(GME_ARCHIVE)
      if(LibArchive_FOUND)
          message(STATUS "LibArchive library located, player demo will support the ZIP file format")
          target_compile_definitions(gme_player PRIVATE HAVE_LIBARCHIVE)
          target_link_libraries(gme_player PRIVATE ${LibArchive_LIBRARIES})
          # Is not to be installed though
          list(APPEND PC_LIBS -larchive) # for libgme.pc
          if(ZLIB_FOUND)
            target_compile_definitions(gme_player PRIVATE HAVE_ZLIB_H)
            target_link_libraries(gme_player PRIVATE ZLIB::ZLIB)
            list(APPEND PC_LIBS -lz)
          endif()
      else()
          message(STATUS "** LibArchive library not found, player demo will not support zip files")
      endif()
    else()
      message(STATUS "Zip extraction not supported")
    endif()

else()
    message(STATUS "** SDL library not found, disabling player demo build")
endif()
