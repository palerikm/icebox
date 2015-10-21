#
#
# ICEbox, the complete ice testing client.

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
#SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


# the RPATH to be used when installing, but only if it's not a system directory
#LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
#IF("${isSystemDir}" STREQUAL "-1")
#   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
#ENDIF("${isSystemDir}" STREQUAL "-1")

set (ADDITIONAL_LIBS "")

find_package( OpenSSL )
if( OPENSSL_FOUND )
  include_directories( ${OPENSSL_INCLUDE_DIR} )
  list(APPEND ADDITIONAL_LIBS ${OPENSSL_LIBRARIES})
endif( OPENSSL_FOUND )

add_executable ( icebox
                 icebox.c )

target_link_libraries ( icebox PRIVATE pthread
                        ${ADDITIONAL_LIBS})

include_directories ( ../include )
link_directories ( ${CHECK_LIBRARY_DIRS} )

set (crusty_files
     icebox.c)

UncrustifyDir(crusty_files)
