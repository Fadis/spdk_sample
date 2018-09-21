set( LIBAIO_ROOT_DIR "" CACHE PATH "LIBAIO root directory" )
set( DETECTED_LIBAIO_ROOT_DIR "" )
if( NOT "${LIBAIO_ROOT_DIR}" STREQUAL "" )
  find_path( LIBAIO_INCLUDE_DIR libaio.h  PATHS ${LIBAIO_ROOT_DIR}/include )
  if( NOT "${LIBAIO_INCLUDE_DIR}" STREQUAL "LIBAIO_INCLUDE_DIR-NOTFOUND" )
    set( DETECTED_LIBAIO_ROOT_DIR ${LIBAIO_ROOT_DIR} )
  endif()
endif()
if( "${DETECTED_LIBAIO_ROOT_DIR}" STREQUAL "" )
  find_path( LIBAIO_INCLUDE_DIR libaio.h  PATHS /usr/include )
  if( NOT "${LIBAIO_INCLUDE_DIR}" STREQUAL "LIBAIO_INCLUDE_DIR-NOTFOUND" )
    set( DETECTED_LIBAIO_ROOT_DIR /usr )
  endif()
endif()
if( "${DETECTED_LIBAIO_ROOT_DIR}" STREQUAL "" )
  find_path( LIBAIO_INCLUDE_DIR libaio.h  PATHS /usr/local/include )
  if( NOT "${LIBAIO_INCLUDE_DIR}" STREQUAL "LIBAIO_INCLUDE_DIR-NOTFOUND" )
    set( DETECTED_LIBAIO_ROOT_DIR /usr/local )
  endif()
endif()
if( NOT "${DETECTED_LIBAIO_ROOT_DIR}" STREQUAL "" )
  find_library(LIBAIO_LIBRARY aio HINTS ${DETECTED_LIBAIO_ROOT_DIR}/lib )
endif()
find_package_handle_standard_args(libaio DEFAULT_MSG
  LIBAIO_INCLUDE_DIR
  LIBAIO_LIBRARY
)
if(LIBAIO_FOUND)
set(
  LIBAIO_LIBRARIES
  aio
)
set(LIBAIO_LIBRARY_DIRS ${DETECTED_LIBAIO_ROOT_DIR}/lib)
endif(LIBAIO_FOUND)
mark_as_advanced(
  LIBAIO_INCLUDE_DIR
  LIBAIO_LIBRARIES
  LIBAIO_LIBRARY_DIRS
  LIBAIO_LIBRARY
)

