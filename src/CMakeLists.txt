
set (qlocalytics_MOC_HDRS
  localyticsdatabase.h
  localyticssession.h
  localyticsuploader.h
  )

qt4_wrap_cpp(qlocalytics_MOC_SRCS ${qlocalytics_MOC_HDRS})

set (qlocalytics_SRCS
  localyticsdatabase.cpp 
  localyticssession.cpp
  localyticsuploader.cpp
  )

set (qlocalytics_HEADERS
  localyticsdatabase.h
  localyticssession.h
  localyticsuploader.h
  )



add_library (qlocalytics SHARED ${qlocalytics_SRCS} ${qlocalytics_MOC_SRCS} ${qlocalytics_HEADERS})
target_link_libraries ( qlocalytics ${QT_LIBRARIES})

FIND_PACKAGE( ZLIB REQUIRED )
IF ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    target_link_libraries( qlocalytics ${ZLIB_LIBRARIES} )
ENDIF( ZLIB_FOUND )


set_target_properties(qlocalytics PROPERTIES
                                  VERSION ${QLOCALYTICS_LIB_MAJOR_VERSION}.${QLOCALYTICS_LIB_MINOR_VERSION}.${QLOCALYTICS_LIB_PATCH_VERSION}
                                  SOVERSION ${QLOCALYTICS_LIB_MAJOR_VERSION}
                                  )


