# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/_deps/unity-src")
  file(MAKE_DIRECTORY "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/_deps/unity-src")
endif()
file(MAKE_DIRECTORY
  "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/_deps/unity-build"
  "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/_deps/unity-subbuild/unity-populate-prefix"
  "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/_deps/unity-subbuild/unity-populate-prefix/tmp"
  "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/_deps/unity-subbuild/unity-populate-prefix/src/unity-populate-stamp"
  "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/_deps/unity-subbuild/unity-populate-prefix/src"
  "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/_deps/unity-subbuild/unity-populate-prefix/src/unity-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/_deps/unity-subbuild/unity-populate-prefix/src/unity-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/sanyalnet/SOFTWARE-DEVELOPMENT/carrom/build_release_noasan/_deps/unity-subbuild/unity-populate-prefix/src/unity-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
