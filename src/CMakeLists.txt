#
#
# ICEbox, the complete ice testing client.


set (ADDITIONAL_LIBS "")

find_package( OpenSSL )
if( OPENSSL_FOUND )
  include_directories( ${OPENSSL_INCLUDE_DIR} )
  list(APPEND ADDITIONAL_LIBS ${OPENSSL_LIBRARIES})
endif( OPENSSL_FOUND )

# Todo fix propper library discovery.
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
   list(APPEND ADDITIONAL_LIBS "bsd")
endif()

add_executable ( icebox
                 media.c
                 sdp.c
                 candidate_harvester.c
                 signaling.c
                 sockethelper.c
                 icebox.c )

target_link_libraries ( icebox PRIVATE icelib stunlib sockaddrutil
                        pthread
                        ${ADDITIONAL_LIBS})

set_target_properties(icebox PROPERTIES INSTALL_RPATH "${install_dir}/lib")
set_target_properties(icebox PROPERTIES BUILD_WITH_INSTALL_RPATH ON)

include_directories ( ../include )
link_directories ( ${CHECK_LIBRARY_DIRS} )

set (crusty_files
     icebox.c)

UncrustifyDir(crusty_files)
