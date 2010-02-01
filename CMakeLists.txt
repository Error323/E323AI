### Generic native Skirmish AI config
#
FIND_PACKAGE( PkgConfig )
pkg_check_modules( CURL libcurl )
IF( NOT CURL_FOUND )
	MESSAGE( STATUS "E323AI will not be built -- required library libCURL not found" )
ELSE( NOT CURL_FOUND )
	INCLUDE_DIRECTORIES( ${CURL_INCLUDE_DIR} )
	IF(WIN32)
		ADD_DEFINITIONS( ${CURL_CFLAGS}  )
		LINK_DIRECTORIES( ${CURL_STATIC_LIBRARY_DIRS} )
		LINK_LIBRARIES( ${CURL_STATIC_LDFLAGS} )
	ELSE(WIN32)
		ADD_DEFINITIONS( ${CURL_CFLAGS} )
		LINK_DIRECTORIES( ${CURL_LIB_DIRS} )
		LINK_LIBRARIES( ${CURL_LDFLAGS} )
	ENDIF(WIN32)

	set(mySourceDirRel         "") # Common values are "" or "src"
	set(additionalSources      ${CUtils_SRC})
	set(additionalCompileFlags "")
	set(additionalLibraries    ${LegacyCpp_AIWRAPPER_TARGET} ${SDL_LIBRARY}
		${Boost_THREAD_LIBRARY} ${CURL_LIBRARIES})

	ConfigureNativeSkirmishAI(mySourceDirRel additionalSources additionalCompileFlags additionalLibraries)
ENDIF( NOT CURL_FOUND )


