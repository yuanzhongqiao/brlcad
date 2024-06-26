#-----------------------------------------------------------------------
# Bundled External Libraries and other 3rd party components.
#
# In general, source code here will meet one of the following criteria:
#
# 1.  A third party library where BRL-CAD has had to make local
#     modifications that are essential to our use of the library,
#     precluding the use of a system-supplied version, and the library
#     is not readily integrable into BRL-CAD's own compilation.
#
# 2.  A header only library or some other component used in the
#     compilation of multiple parts of BRL-CAD (i.e. it isn't practical
#     to locate it with the logic that uses it.)
#
# 3.  A third party component that is not widely available in the
#     the broader software ecosystem.  Since we know we will
#     always be building those components, there isn't any gain or
#     justification for the increased complexity of maintaining a
#     superbuild detect-and-build-if-needed approach to support it.
#
#-----------------------------------------------------------------------

# Clear all BRL-CAD defined CMake flags
CLEAR_BUILD_FLAGS()

# Restore CMake's original flags
RESTORE_CACHED_BUILD_FLAGS(_CMAKE_DEFAULT)

# Quiet all warnings in this directory
DISABLE_WARNINGS()

# For src/other, be liberal about standards
unset(CMAKE_C_EXTENSIONS)
unset(CMAKE_C_STANDARD_REQUIRED)
unset(CMAKE_C_STANDARD)
unset(CMAKE_CXX_EXTENSIONS)
unset(CMAKE_CXX_STANDARD_REQUIRED)
unset(CMAKE_CXX_STANDARD)

# When we need to have CMake treat includes as
# system paths to avoid warnings, we add those
# patterns to the SYS_INCLUDE_PATTERNS list
mark_as_advanced(SYS_INCLUDE_PATTERNS)

###############################################################################
# Unlike the misc/tools directory components built in src/other are part of
# the distribution, not just the compilation.  Hence we need to make sure of
# some basic compatibility between the build settings.

# We don't want to try to mix 32 and 64 bit builds so despite clearing the
# parent build flags,  set the 32/64 bit flag specifically if we have it from
# the parent build.
if(${CMAKE_WORD_SIZE} MATCHES "64BIT" AND NOT CMAKE_CL_64 AND DEFINED 64BIT_FLAG)
  ADD_NEW_FLAG(C 64BIT_FLAG ALL)
  ADD_NEW_FLAG(CXX 64BIT_FLAG ALL)
  ADD_NEW_FLAG(SHARED_LINKER 64BIT_FLAG ALL)
  ADD_NEW_FLAG(EXE_LINKER 64BIT_FLAG ALL)
endif(${CMAKE_WORD_SIZE} MATCHES "64BIT" AND NOT CMAKE_CL_64 AND DEFINED 64BIT_FLAG)
if(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT MSVC AND DEFINED 32BIT_FLAG)
  ADD_NEW_FLAG(C 32BIT_FLAG ALL)
  ADD_NEW_FLAG(CXX 32BIT_FLAG ALL)
  ADD_NEW_FLAG(SHARED_LINKER 32BIT_FLAG ALL)
  ADD_NEW_FLAG(EXE_LINKER 32BIT_FLAG ALL)
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT MSVC AND DEFINED 32BIT_FLAG)

###############################################################################

# If we're using a bundled zlib with a prefix defined, we need a definition
# flag.
# TODO - is there some target property we can use to pass this automatically?
# Hammering it at high levels is almost certainly not the right approach...
if (Z_PREFIX_STR)
  add_definitions(-DZ_PREFIX)
  add_definitions(-DZ_PREFIX_STR=${Z_PREFIX_STR})
endif (Z_PREFIX_STR)

# Set a variable that will tell build logic in subdirectories that there is a
# parent build.  This will in some cases disable attempts to define settings
# such as RPATH values that we need the parent build to manage.
set(HAVE_PARENT_BUILD 1)

# Ideally we wouldn't need this, but in a few cases we're using BRL-CAD's
# include dirs.
include_directories(
  "${BRLCAD_BINARY_DIR}/include"
  "${BRLCAD_SOURCE_DIR}/include"
  )

# Most third party items have a list calling out files for distcheck -
# these are stored in files in the dlists directory.  Ignore that
# directory for distcheck
file(GLOB dlists "*.dist")
foreach(ITEM ${dlists})
  get_filename_component(dlist ${ITEM} NAME)
  CMAKEFILES(${dlist})
endforeach(ITEM ${dlists})

# If the option is available (and in a number of cases we make sure it is)
# don't install the headers as part of the BRL-CAD package.  We compile
# against these, but we may have adjusted them compared to system versions
# and we don't want them mixing with the system version for an external
# code via it including our version of the header
set(SKIP_INSTALL_HEADERS TRUE CACHE  BOOL "Don't install src/other headers" FORCE)
mark_as_advanced(SKIP_INSTALL_HEADERS)

# libutahrle Library - The directory to perform ADD_SUBDIRECTORY on
# and the include directory for utahrle are different, so override the
# macro's setting of UTAHRLE_INCLUDE_DIRS here.
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libutahrle)
  add_subdirectory(libutahrle)
  set(UTAHRLE_LIBRARY utahrle CACHE STRING "directory with rle.h header" FORCE)
  set(UTAHRLE_LIBRARIES utahrle CACHE STRING "directory with rle.h header" FORCE)
  set(UTAHRLE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "directory with rle.h header" FORCE)
  set(UTAHRLE_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "directory with rle.h header" FORCE)
endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libutahrle)
mark_as_advanced(UTAHRLE_LIBRARY)
mark_as_advanced(UTAHRLE_LIBRARIES)
mark_as_advanced(UTAHRLE_INCLUDE_DIR)
mark_as_advanced(UTAHRLE_INCLUDE_DIRS)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libutahrle)
  include("${CMAKE_CURRENT_SOURCE_DIR}/libutahrle.dist")
  CMAKEFILES_IN_DIR(libutahrle_ignore_files libutahrle)
endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libutahrle)

# OpenCV library
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/opencv)
  add_subdirectory(opencv)
endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/opencv)

# OpenNURBS Library
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/openNURBS)
  add_subdirectory(openNURBS)
  SetTargetFolder(openNURBS "Third Party Libraries")
  SetTargetFolder(openNURBS-static "Third Party Libraries")
  SetTargetFolder(openNURBS-obj "Third Party Libraries")
  set(OPENNURBS_LIBRARY openNURBS CACHE STRING "Require opennurbs libraries" FORCE)
  set(OPENNURBS_LIBRARIES openNURBS CACHE STRING "Require opennurbs libraries" FORCE)
  set(OPENNURBS_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/openNURBS" CACHE STRING "Require opennurbs header includes" FORCE)
  set(OPENNURBS_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/openNURBS" CACHE STRING "Require opennurbs header includes" FORCE)
  # openNURBS headers trigger a number of warnings - treat them the same way we
  # do the ext headers
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} openNURBS CACHE STRING "Bundled system include dirs" FORCE)
  mark_as_advanced(OPENNURBS_LIBRARY)
  mark_as_advanced(OPENNURBS_LIBRARIES)
  mark_as_advanced(OPENNURBS_INCLUDE_DIR)
  mark_as_advanced(OPENNURBS_INCLUDE_DIRS)
  get_directory_property(openNURBS_headers_orig DIRECTORY openNURBS DEFINITION OPENNURBS_HEADERS)
  set(openNURBS_headers)
  foreach(onhfile ${openNURBS_headers_orig})
    set(openNURBS_headers ${openNURBS_headers} openNURBS/${onhfile})
  endforeach(onhfile ${openNURBS_headers_orig})
  BRLCAD_MANAGE_FILES(openNURBS_headers ${INCLUDE_DIR}/openNURBS)
  include("${CMAKE_CURRENT_SOURCE_DIR}/openNURBS.dist")
  CMAKEFILES_IN_DIR(openNURBS_ignore_files openNURBS)
endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/openNURBS)


# Poly2Tri CDT library
# Note - this really should be embedded as part of libbg - it is simple, can be
# compiled as part of libbg with a few source adjustments, and there is an API
# for this feature exposed by libbg.  The only reason it is still maintained as
# a separate library is the fast cdt feature of libbrep, which is using this
# API directly instead of going through libbg.  Care will have to be taken when
# making that shift, and it may even be that we'll discover a need to adjust the
# libbg API in doing so to avoid performance issues.  We should eventually
# address those issues though, if for no other reason than to discourage other
# direct uses of this API instead of bg_nested_polygon_triangulate.
#
# May also want to investigate https://github.com/MetricPanda/fast-poly2tri, but
# the removal of Steiner point insertion would probably need to be addressed...
add_subdirectory(poly2tri)
include("${CMAKE_CURRENT_SOURCE_DIR}/poly2tri.dist")
CMAKEFILES_IN_DIR(poly2tri_ignore_files poly2tri)
DISTCLEAN("${CMAKE_CURRENT_SOURCE_DIR}/poly2tri/Makefile")
set(POLY2TRI_LIBRARY "poly2tri" CACHE STRING "Poly2Tri library" FORCE)
set(POLY2TRI_LIBRARIES "poly2tri" CACHE STRING "Poly2Tri library" FORCE)
set(POLY2TRI_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/poly2tri" CACHE STRING "Directory containing poly2tri header" FORCE)
set(POLY2TRI_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/poly2tri" CACHE STRING "Directory containing poly2tri header" FORCE)
SetTargetFolder(poly2tri "Third Party Libraries")
SetTargetFolder(poly2tri-static "Third Party Libraries")
mark_as_advanced(POLY2TRI_LIBRARY)
mark_as_advanced(POLY2TRI_LIBRARIES)
mark_as_advanced(POLY2TRI_INCLUDE_DIR)
mark_as_advanced(POLY2TRI_INCLUDE_DIRS)

# Manifold is a geometry library for operating on manifold triangle meshes:
# https://github.com/elalish/manifold
# (current configuration is from https://github.com/BRL-CAD/manifold RELEASE branch)
if ("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  add_subdirectory(manifold)
  include("${CMAKE_CURRENT_SOURCE_DIR}/manifold.dist")
  CMAKEFILES_IN_DIR(manifold_ignore_files manifold)
  DISTCLEAN("${CMAKE_CURRENT_SOURCE_DIR}/manifold/Makefile")
  set(MANIFOLD_LIBRARY "manifold" CACHE STRING "Manifold library" FORCE)
  set(MANIFOLD_LIBRARIES "manifold" CACHE STRING "Manifold library" FORCE)
  set(MANIFOLD_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/manifold/include;${BRLCAD_SOURCE_DIR}/src/other/manifold/meshIO/include;${BRLCAD_SOURCE_DIR}/src/other/manifold/include/manifold;${BRLCAD_SOURCE_DIR}/src/other/manifold/src/third_party/glm" CACHE STRING "Directory containing manifold headers" FORCE)
  set(MANIFOLD_INCLUDE_DIRS "${MANIFOLD_INCLUDE_DIR}" CACHE STRING "Directory containing manifold headers" FORCE)
  SetTargetFolder(manifold "Third Party Libraries")
endif("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
mark_as_advanced(MANIFOLD_LIBRARY)
mark_as_advanced(MANIFOLD_LIBRARIES)
mark_as_advanced(MANIFOLD_INCLUDE_DIR)
mark_as_advanced(MANIFOLD_INCLUDE_DIRS)

# Lightning Memory-Mapped Database (LMDB)
# Fast key-value store database from OpenLDAP Project.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lmdb")
  add_subdirectory(lmdb)
  include("${CMAKE_CURRENT_SOURCE_DIR}/lmdb.dist")
  CMAKEFILES_IN_DIR(lmdb_ignore_files lmdb)
  DISTCLEAN("${CMAKE_CURRENT_SOURCE_DIR}/lmdb/Makefile")
  set(LMDB_LIBRARY "lmdb" CACHE STRING "Lightning Memory-Mapped Database library" FORCE)
  set(LMDB_LIBRARIES "lmdb" CACHE STRING "Lightning Memory-Mapped Database library" FORCE)
  set(LMDB_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/lmdb" CACHE STRING "Directory containing lmdb header" FORCE)
  set(LMDB_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/lmdb" CACHE STRING "Directory containing lmdb header" FORCE)
  SetTargetFolder(lmdb "Third Party Libraries")
  mark_as_advanced(LMDB_LIBRARY)
  mark_as_advanced(LMDB_LIBRARIES)
  mark_as_advanced(LMDB_INCLUDE_DIR)
  mark_as_advanced(LMDB_INCLUDE_DIRS)
endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lmdb")

# Interactive Robust Mesh Booleans - slight modification of
# https://github.com/gcherchi/InteractiveAndRobustMeshBooleans - to build, place libirmb
# branch of https://github.com/starseeker/InteractiveAndRobustMeshBooleans at
# src/other/irmb and define the USE_IRMB flag in src/libged/facetize/facetize.cpp
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/irmb")
  add_subdirectory(irmb)
  include("${CMAKE_CURRENT_SOURCE_DIR}/irmb.dist")
  CMAKEFILES_IN_DIR(irmb_ignore_files irmb)
  set(IRMB_LIBRARY "irmb" CACHE STRING "Interactive Robust Mesh Booleans library" FORCE)
  set(IRMB_LIBRARIES "irmb" CACHE STRING "Interactive Robust Mesh Booleans library" FORCE)
  set(IRMB_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/irmb" CACHE STRING "Directory containing irmb header" FORCE)
  set(IRMB_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/irmb" CACHE STRING "Directory containing irmb header" FORCE)
  SetTargetFolder(irmb "Third Party Libraries")
else (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/irmb")
  set(IRMB_LIBRARIES "" CACHE STRING "Disabled" FORCE)
  set(IRMB_INCLUDE_DIRS "" CACHE STRING "Disabled" FORCE)
endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/irmb")
mark_as_advanced(IRMB_LIBRARY)
mark_as_advanced(IRMB_LIBRARIES)
mark_as_advanced(IRMB_INCLUDE_DIR)
mark_as_advanced(IRMB_INCLUDE_DIRS)

# OSMesa Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libosmesa")
  add_subdirectory(libosmesa)
  SetTargetFolder(osmesa "Third Party Libraries")
  set(OSMESA_LIBRARY osmesa CACHE STRING "Require osmesa libraries" FORCE)
  set(OSMESA_LIBRARIES osmesa CACHE STRING "Require osmesa libraries" FORCE)
  set(OSMESA_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libosmesa/include" CACHE STRING "Require osmesa header includes" FORCE)
  set(OSMESA_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/libosmesa/include" CACHE STRING "Require osmesa header includes" FORCE)
  mark_as_advanced(OSMESA_LIBRARY)
  mark_as_advanced(OSMESA_LIBRARIES)
  mark_as_advanced(OSMESA_INCLUDE_DIR)
  mark_as_advanced(OSMESA_INCLUDE_DIRS)
  include("${CMAKE_CURRENT_SOURCE_DIR}/libosmesa.dist")
  CMAKEFILES_IN_DIR(libosmesa_ignore_files libosmesa)
endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libosmesa")

if (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK)

  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tkhtml")
    # Tkhtml doesn't have an active upstream
    add_subdirectory(tkhtml)
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tkhtml/pkgIndex.tcl")
    DISTCLEAN("${CMAKE_BINARY_DIR}/lib/Tkhtml3.0")
    SetTargetFolder(Tkhtml "Third Party Libraries")
    SetTargetFolder(tkhtml_n_gen "Compilation Utilities")
  endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tkhtml")

  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tktable")
    # Tktable is a borderline case.  It does exist and is used in the Tcl/Tk
    # ecosystem, but it is has the same problem we have generally with "package
    # require" Tk package testing and there is very little upstream activity...
    add_subdirectory(tktable)
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktable/pkgIndex.tcl")
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktable/tktable_cfg.h")
    DISTCLEAN("${CMAKE_BINARY_DIR}/lib/Tktable2.10")
    SetTargetFolder(Tktable "Third Party Libraries")
    SetTargetFolder(tktable_header_gen "Compilation Utilities")
  endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tktable")

endif (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tkhtml")
  include("${CMAKE_CURRENT_SOURCE_DIR}/tkhtml.dist")
  CMAKEFILES_IN_DIR(tkhtml_ignore_files tkhtml)
endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tkhtml")

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tktable")
  include("${CMAKE_CURRENT_SOURCE_DIR}/tktable.dist")
  CMAKEFILES_IN_DIR(tktable_ignore_files tktable)
endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tktable")

# The above libraries are all compilation targets with libraries, and so have
# build outputs that must be distributed with BRL-CAD.  The following are
# either header-only libraries or built directly into executables, and as such
# may remain in place - they are needed only for building.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Eigen")
  include("${CMAKE_CURRENT_SOURCE_DIR}/Eigen.dist")
  CMAKEFILES_IN_DIR(Eigen_ignore_files Eigen)
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} Eigen CACHE STRING "Bundled system include dirs" FORCE)
endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Eigen")

# linenoise is used by applications directly
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linenoise")
  add_subdirectory(linenoise)
  SetTargetFolder(linenoise "Third Party Libraries")
  set(LINENOISE_LIBRARY linenoise CACHE STRING "Require linenoise libraries" FORCE)
  set(LINENOISE_LIBRARIES linenoise CACHE STRING "Require linenoise libraries" FORCE)
  set(LINENOISE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/linenoise" CACHE STRING "Require linenoise header includes" FORCE)
  set(LINENOISE_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/linenoise" CACHE STRING "Require linenoise header includes" FORCE)
  mark_as_advanced(LINENOISE_LIBRARY)
  mark_as_advanced(LINENOISE_LIBRARIES)
  mark_as_advanced(LINENOISE_INCLUDE_DIR)
  mark_as_advanced(LINENOISE_INCLUDE_DIRS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/linenoise.dist)
  CMAKEFILES_IN_DIR(linenoise_ignore_files linenoise)
endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linenoise")

CMAKEFILES(
  README
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

