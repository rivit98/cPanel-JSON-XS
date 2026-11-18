# Fetch and unpack the Cpanel::JSON::XS release under test.
#
#   cmake -DTARBALL=<path> -DURL=<url> -DSHA256=<hex> -DDEST=<dir> -DPERL5LIB=<dir> -DSTAMP=<file> -P upstream.cmake
#
# Run by CMakeLists.txt at configure time (so a missing network fails early)
# and again at build time whenever an output is missing. None of the module's
# files live in the repository: the release tarball is downloaded once into
# TARBALL (fuzzer/build/, shared by every variant), checked against the pinned
# SHA-256, and unpacked into DEST, the variant's own build tree - untouched, so
# the sources stay byte-identical to the CPAN release.
#
# PERL5LIB gets the module's .pm files in the layout `use Cpanel::JSON::XS`
# expects (Cpanel/JSON/XS.pm, Cpanel/JSON/XS/{Boolean,Type}.pm).

foreach(_v TARBALL URL SHA256 DEST PERL5LIB STAMP)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "upstream.cmake: -D${_v}= missing")
    endif()
endforeach()

function(_sha_ok out file)
    set(${out} FALSE PARENT_SCOPE)
    if(EXISTS "${file}")
        file(SHA256 "${file}" _h)
        string(TOLOWER "${SHA256}" _want)
        if("${_h}" STREQUAL "${_want}")
            set(${out} TRUE PARENT_SCOPE)
        endif()
    endif()
endfunction()

_sha_ok(_ok "${TARBALL}")
if(NOT _ok)
    if(EXISTS "${TARBALL}")
        message(FATAL_ERROR "${TARBALL}: SHA-256 mismatch (expected ${SHA256}) - delete it to download again")
    endif()
    message(STATUS "downloading ${URL}")
    get_filename_component(_dir "${TARBALL}" DIRECTORY)
    file(MAKE_DIRECTORY "${_dir}")
    # a private .part name: two variants configured at once must not see each
    # other's half-written file
    string(RANDOM LENGTH 8 _rnd)
    set(_part "${TARBALL}.${_rnd}.part")
    file(DOWNLOAD "${URL}" "${_part}" STATUS _st TLS_VERIFY ON)
    list(GET _st 0 _rc)
    _sha_ok(_ok "${_part}")
    if(NOT _rc EQUAL 0 OR NOT _ok)
        file(REMOVE "${_part}")
        message(FATAL_ERROR "download of ${URL} failed (${_st}) or its SHA-256 is not ${SHA256}; "
                            "to build offline, put the release at ${TARBALL}")
    endif()
    file(RENAME "${_part}" "${TARBALL}")
endif()

# Unpack next to DEST and swap it in, so DEST is never half-populated. TOUCH:
# the tarball's 2025 mtimes would be older than this script and make every
# build re-run the extraction.
set(_tmp "${DEST}.tmp")
file(REMOVE_RECURSE "${_tmp}" "${DEST}")
file(ARCHIVE_EXTRACT INPUT "${TARBALL}" DESTINATION "${_tmp}" TOUCH)
file(GLOB _top LIST_DIRECTORIES true "${_tmp}/*")
list(LENGTH _top _n)
if(NOT _n EQUAL 1 OR NOT IS_DIRECTORY "${_top}")
    message(FATAL_ERROR "${TARBALL}: expected one top-level directory, got: ${_top}")
endif()
file(RENAME "${_top}" "${DEST}")
file(REMOVE_RECURSE "${_tmp}")

file(REMOVE_RECURSE "${PERL5LIB}/Cpanel")
file(MAKE_DIRECTORY "${PERL5LIB}/Cpanel/JSON/XS")
file(COPY_FILE "${DEST}/XS.pm" "${PERL5LIB}/Cpanel/JSON/XS.pm")
foreach(_pm Boolean.pm Type.pm)
    file(COPY_FILE "${DEST}/XS/${_pm}" "${PERL5LIB}/Cpanel/JSON/XS/${_pm}")
endforeach()

file(TOUCH "${STAMP}")
