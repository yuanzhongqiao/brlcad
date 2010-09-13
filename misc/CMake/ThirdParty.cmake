#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_OPTION upper lower)
	IF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_LIBS)
		OPTION(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} "Build the local ${upper} library." ON)
		SET(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} ON)
	ELSE(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_LIBS)
		OPTION(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} "Build the local ${upper} library." OFF)
		SET(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} OFF)
	ENDIF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_LIBS)
	IF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} OR ${CMAKE_PROJECT_NAME}_SYSTEM_LIBS_ONLY)
		SET(${upper}_FOUND "${upper}-NOTFOUND")
		SET(${upper}_LIBRARY "${upper}-NOTFOUND")
	   SET(${upper}_INCLUDE_DIR "${upper}-NOTFOUND")
		IF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
			INCLUDE(${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
		ELSE(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
			INCLUDE(${CMAKE_ROOT}/Modules/Find${upper}.cmake)
		ENDIF(EXISTS ${${CMAKE_PROJECT_NAME}_CMAKE_DIR}/Find${upper}.cmake)
		IF(${upper}_FOUND)
			IF("${upper}" MATCHES "^PNG$")
				SET(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR})
				SET(${CMAKE_PROJECT_NAME}_${upper}_INCLUDE_DIR ${PNG_INCLUDE_DIR})
				MARK_AS_ADVANCED(PNG_PNG_INCLUDE_DIR)
			ELSE("${upper}" MATCHES "^PNG$")
				SET(${CMAKE_PROJECT_NAME}_${upper}_INCLUDE_DIR ${${upper}_INCLUDE_DIR} CACHE STRING "set by THIRD_PARTY macro" FORCE)
			ENDIF("${upper}" MATCHES "^PNG$")
			SET(${CMAKE_PROJECT_NAME}_${upper}_LIBRARY ${${upper}_LIBRARY} CACHE STRING "set by THIRD_PARTY macro" FORCE)
		ELSE(${upper}_FOUND)
			IF(NOT ${CMAKE_PROJECT_NAME}_SYSTEM_LIBS_ONLY) 
				SET(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} ON)
				SET(${upper}_LIBRARY "${lower}" CACHE STRING "set by THIRD_PARTY macro" FORCE)
			   SET(${CMAKE_PROJECT_NAME}_${upper}_LIBRARY ${${upper}_LIBRARY} CACHE STRING "set by THIRD_PARTY macro" FORCE)
			ENDIF(NOT ${CMAKE_PROJECT_NAME}_SYSTEM_LIBS_ONLY) 
		ENDIF(${upper}_FOUND)
	ELSE(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} OR ${CMAKE_PROJECT_NAME}_SYSTEM_LIBS_ONLY)
		SET(${upper}_LIBRARY "${lower}" CACHE STRING "set by THIRD_PARTY macro" FORCE)
	   SET(${CMAKE_PROJECT_NAME}_${upper}_LIBRARY ${${upper}_LIBRARY} CACHE STRING "set by THIRD_PARTY macro" FORCE)
	ENDIF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} OR ${CMAKE_PROJECT_NAME}_SYSTEM_LIBS_ONLY)
	MARK_AS_ADVANCED(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
ENDMACRO(THIRD_PARTY_OPTION)

#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_INCLUDE upper lower)
  IF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
    IF(${upper}_INCLUDE_DIR)
      SET(${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SYSTEM ${${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SYSTEM} ${${upper}_INCLUDE_DIR})
    ENDIF(${upper}_INCLUDE_DIR)
  ELSE(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
    SET(${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SOURCE_TREE ${${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SOURCE_TREE}
      ${${CMAKE_PROJECT_NAME}_BINARY_DIR}/Utilities/${lower}
      ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/Utilities/${lower}
    )
  ENDIF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
ENDMACRO(THIRD_PARTY_INCLUDE)

MACRO(THIRD_PARTY_INCLUDE2 upper)
  IF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
    IF(${upper}_INCLUDE_DIR)
      SET(${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SYSTEM ${${CMAKE_PROJECT_NAME}_INCLUDE_DIRS_SYSTEM} ${${upper}_INCLUDE_DIR})
    ENDIF(${upper}_INCLUDE_DIR)
  ENDIF(NOT ${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper})
ENDMACRO(THIRD_PARTY_INCLUDE2)

#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_SUBDIR upper lower)
	IF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${upper}_SYSTEM_LIBS_ONLY)
		ADD_SUBDIRECTORY(${lower})
		SET(${upper}_INCLUDE_DIR ${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/${lower} CACHE STRING "set by THIRD_PARTY_SUBDIR macro" FORCE)
		SET(${CMAKE_PROJECT_NAME}_${upper}_INCLUDE_DIR ${${upper}_INCLUDE_DIR} CACHE STRING "set by THIRD_PARTY_SUBDIR macro" FORCE)
	ENDIF(${CMAKE_PROJECT_NAME}_BUILD_LOCAL_${upper} AND NOT ${upper}_SYSTEM_LIBS_ONLY)
	MESSAGE("BRLCAD-THIRD-PARTY_${upper}: ${${CMAKE_PROJECT_NAME}_${upper}_INCLUDE_DIR} ${${CMAKE_PROJECT_NAME}_${upper}_LIBRARY}")
ENDMACRO(THIRD_PARTY_SUBDIR)

#-----------------------------------------------------------------------------
MACRO(THIRD_PARTY_WARNING_SUPPRESS upper lang)
  IF(NOT ${upper}_WARNINGS_ALLOW)
    # MSVC uses /w to suppress warnings.  It also complains if another
    # warning level is given, so remove it.
    IF(MSVC)
      SET(${upper}_WARNINGS_BLOCKED 1)
      STRING(REGEX REPLACE "(^| )([/-])W[0-9]( |$)" " "
        CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS}")
      SET(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS} /w")
    ENDIF(MSVC)

    # Borland uses -w- to suppress warnings.
    IF(BORLAND)
      SET(${upper}_WARNINGS_BLOCKED 1)
      SET(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS} -w-")
    ENDIF(BORLAND)

    # Most compilers use -w to suppress warnings.
    IF(NOT ${upper}_WARNINGS_BLOCKED)
      SET(CMAKE_${lang}_FLAGS "${CMAKE_${lang}_FLAGS} -w")
    ENDIF(NOT ${upper}_WARNINGS_BLOCKED)
  ENDIF(NOT ${upper}_WARNINGS_ALLOW)
ENDMACRO(THIRD_PARTY_WARNING_SUPPRESS)
