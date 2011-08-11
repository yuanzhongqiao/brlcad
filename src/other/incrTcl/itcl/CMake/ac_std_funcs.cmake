# Standard tests and wrappers for CMake functions

INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckIncludeFileCXX)
INCLUDE(CheckTypeSize)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckStructHasMember)
INCLUDE(CheckCSourceCompiles)
INCLUDE(ResolveCompilerPaths)

SET(CMAKE_TEST_SRCS_DIR "NOTFOUND")
FOREACH($candidate_dir ${CMAKE_MODULE_PATH})
	IF(NOT CMAKE_TEST_SRCS_DIR)
		IF(EXISTS "${candidate_dir}/test_sources" AND IS_DIRECTORY "${candidate_dir}/test_sources")
			SET(CMAKE_TEST_SRCS_DIR ${candidate_dir}/test_sources)
		ENDIF(EXISTS "${candidate_dir}/test_sources" AND IS_DIRECTORY "${candidate_dir}/test_sources")
	ENDIF(NOT CMAKE_TEST_SRCS_DIR)
ENDFOREACH($candidate_dir ${CMAKE_MODULE_PATH})

INCLUDE(CheckPrototypeExists)
INCLUDE(CheckCSourceRuns)
INCLUDE(CheckCFileRuns)

MACRO(CHECK_BASENAME_D)
	SET(BASENAME_SRC "
	#include <libgen.h>
	int main(int argc, char *argv[]) {
	(void)basename(argv[0]);
	return 0;
	}")
	CHECK_C_SOURCE_RUNS("${BASENAME_SRC}" HAVE_BASENAME)
	IF(HAVE_BASENAME)
	  add_definitions(-DHAVE_BASENAME=1)
	ENDIF(HAVE_BASENAME)
ENDMACRO(CHECK_BASENAME_D)

MACRO(CHECK_DIRNAME_D)
	SET(DIRNAME_SRC "
	#include <libgen.h>
	int main(int argc, char *argv[]) {
	(void)dirname(argv[0]);
	return 0;
	}")
	CHECK_C_SOURCE_RUNS("${DIRNAME_SRC}" HAVE_DIRNAME)
	IF(HAVE_DIRNAME)
		add_definitions(-DHAVE_DIRNAME=1)
	ENDIF(HAVE_DIRNAME)
ENDMACRO(CHECK_DIRNAME_D)

# Based on AC_HEADER_STDC - using the source code for ctype
# checking found in the generated configure file
MACRO(CMAKE_HEADER_STDC_D)
  CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
  IF(HAVE_STDLIB_H)
  add_definitions(-DHAVE_STDLIB_H=1)
  ENDIF(HAVE_STDLIB_H)
  CHECK_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)
  CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
  IF(HAVE_STRING_H)
  add_definitions(-DHAVE_STRING_H=1)
  ENDIF(HAVE_STRING_H)
  CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
  IF(HAVE_STRINGS_H)
  add_definitions(-DHAVE_STRINGS_H=1)
  ENDIF(HAVE_STRINGS_H)
  CHECK_INCLUDE_FILE(float.h HAVE_FLOAT_H)
  CHECK_PROTOTYPE_EXISTS(memchr string.h HAVE_STRING_H_MEMCHR)
  CHECK_PROTOTYPE_EXISTS(free stdlib.h HAVE_STDLIB_H_FREE)
  CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/ctypes_test.c WORKING_CTYPE_MACROS)
  IF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
	 add_definitions(-DSTDC_HEADERS=1)
  ENDIF(HAVE_STDLIB_H AND HAVE_STDARG_H AND HAVE_STRING_H AND HAVE_FLOAT_H AND WORKING_CTYPE_MACROS)
ENDMACRO(CMAKE_HEADER_STDC_D)

# Based on AC_HEADER_SYS_WAIT
MACRO(CMAKE_HEADER_SYS_WAIT_D)
  CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/sys_wait_test.c WORKING_SYS_WAIT)
  IF(WORKING_SYS_WAIT)
	 add_definitions(-DHAVE_SYS_WAIT_H=1)
  ENDIF(WORKING_SYS_WAIT)
ENDMACRO(CMAKE_HEADER_SYS_WAIT_D)

# Based on AC_FUNC_ALLOCA
MACRO(CMAKE_ALLOCA_D)
	CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/alloca_header_test.c WORKING_ALLOCA_H)
	IF(WORKING_ALLOCA_H)
		add_definitions(-DHAVE_ALLOCA_H=1)
	ENDIF(WORKING_ALLOCA_H)
	CHECK_C_FILE_RUNS(${CMAKE_TEST_SRCS_DIR}/alloca_test.c WORKING_ALLOCA)
	IF(WORKING_ALLOCA)
		add_definitions(-DHAVE_ALLOCA=1)
	ENDIF(WORKING_ALLOCA)
ENDMACRO(CMAKE_ALLOCA_D)

