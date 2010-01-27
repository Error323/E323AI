# Downloaded from: http://www-id.imag.fr/FLOWVR/manual/flowvr-suite-src/flowvr-render/cmake/
# License: GPL v2, http://www-id.imag.fr/FLOWVR/manual/flowvr-suite-src/flowvr-render/COPYING

# - Try to find CURL
# Once done this will define
#
#  CURL_FOUND - system has CURL
#  CURL_INCLUDE_DIR - the CURL include directory
#  CURL_LIBRARIES - Link these to use CURL
#  CURL_DEFINITIONS - Compiler switches required for using CURL
#


IF (CURL_INCLUDE_DIR)
	# Already in cache, be silent
	SET(CURL_FIND_QUIETLY TRUE)
ENDIF (CURL_INCLUDE_DIR)

FIND_PATH(CURL_INCLUDE_DIR NAMES curl/curl.h
  PATHS
  ${PROJECT_BINARY_DIR}/include
  ${PROJECT_SOURCE_DIR}/include
  ENV CPATH
  /usr/include
  /usr/local/include
  NO_DEFAULT_PATH
)
FIND_PATH(CURL_INCLUDE_DIR NAMES curl/curl.h)

FIND_LIBRARY(CURL_LIBRARIES NAMES CURL
  PATHS
  ${PROJECT_BINARY_DIR}/lib64
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/lib64
  ${PROJECT_SOURCE_DIR}/lib
  ENV LD_LIBRARY_PATH
  ENV LIBRARY_PATH
  /usr/lib64
  /usr/lib
  /usr/local/lib64
  /usr/local/lib
  NO_DEFAULT_PATH
)
IF(WIN32)
  FIND_LIBRARY(CURL_LIBRARIES NAMES CURL curl32)
ELSE(WIN32)
  FIND_LIBRARY(CURL_LIBRARIES NAMES CURL libcurl)
ENDIF(WIN32)

IF(CURL_INCLUDE_DIR AND CURL_LIBRARIES)
   SET(CURL_FOUND TRUE)
ENDIF(CURL_INCLUDE_DIR AND CURL_LIBRARIES)

IF(CURL_FOUND)
  IF(NOT CURL_FIND_QUIETLY)
    MESSAGE(STATUS "Found CURL: ${CURL_LIBRARIES}")
  ENDIF(NOT CURL_FIND_QUIETLY)
ELSE(CURL_FOUND)
  IF(CURL_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find CURL")
  ENDIF(CURL_FIND_REQUIRED)
ENDIF(CURL_FOUND)

# show the CURL_INCLUDE_DIR and CURL_LIBRARIES variables only in the advanced view
MARK_AS_ADVANCED(CURL_INCLUDE_DIR CURL_LIBRARIES)
