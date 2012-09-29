### Generic native Skirmish AI config
#
set(mySourceDirRel         "") # Common values are "" or "src"
set(additionalSources      ${CUtils_SRC})
set(additionalCompileFlags "")
set(additionalLibraries    ${LegacyCpp_AIWRAPPER_TARGET} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY})

ConfigureNativeSkirmishAI(mySourceDirRel additionalSources additionalCompileFlags additionalLibraries)
