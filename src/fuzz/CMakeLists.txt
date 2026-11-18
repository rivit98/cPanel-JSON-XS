# Cpanel::JSON::XS fuzzing harness.
#
# Configured by fuzzer/lib/build.sh, one build tree per variant under
# fuzzer/build/<variant>/, with this directory as the source dir:
#
#   cmake <repo>/src/fuzz && cmake --build . --target fuzz
#
# ── what gets built ───────────────────────────────────────────────────────────
#   fuzz      the campaign binary. MUST be built with an afl-* compiler, which
#             is what -fsanitize=fuzzer on the link line means here: afl-cc
#             swaps it for its own libAFLDriver.a. EXCLUDE_FROM_ALL, so only an
#             explicit `--target fuzz` builds it - a plain-clang build of
#             everything would link the real libFuzzer runtime behind that same
#             flag.
#   fuzz2     the RR/RF reproducer, plain compiler, asan+ubsan - or msan with
#             -DFUZZ_REPRO_SANITIZER=memory
#   fuzz_cov  coverage, plain compiler, no sanitizers
#
# fuzz2 and fuzz_cov are built -DREPRODUCER: neither has a fuzzing engine to
# supply main(), and they share the one in fuzz_utils.h - argv takes input
# files or a vc<n>/tc<n> test case name.
#
# ── the library under test ────────────────────────────────────────────────────
# Cpanel::JSON::XS is a Perl XS module. The harness embeds a perl interpreter
# (fuzz_perl.h) and links the module statically: XS.xs is run through
# ExtUtils::ParseXS at build time and XS.c (with utf8_range.c, the module's
# second object - Makefile.PL OBJECT) is compiled into the harness, so
# the module's code is instrumented exactly like the harness's. libperl itself
# is NOT rebuilt - it is the system's libperl.so, uninstrumented, so AFL's edge
# map holds Cpanel::JSON::XS and the harness and nothing of the interpreter.
#
# None of the module is in the repository: the CPAN release tarball is
# downloaded (once, into fuzzer/build/), checked and unpacked into each build
# tree by upstream.cmake - see "the release under test" below.
#
# FUZZ_PERL selects the perl whose headers and libperl are used (default: the
# perl on PATH). The MSAN variants point it at an MSAN-instrumented perl that
# build.sh builds from source: MSAN reports every read of memory an
# uninstrumented library initialised, and every SV the module touches was
# initialised by libperl.
#
# On the AFL path this file adds no instrumentation flags of its own - not the
# engine's, not the sanitizers'. Both come from the environment
# (AFL_USE_ASAN=1, AFL_LLVM_CMPLOG=1, ...), which fuzzer/lib/build.sh sets.

# 3.24: file(ARCHIVE_EXTRACT ... TOUCH) in upstream.cmake
cmake_minimum_required(VERSION 3.24)
project(cpanel_json_xs_fuzz C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

get_filename_component(REPO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(FUZZ_PERL "perl" CACHE STRING "perl whose headers and libperl the harness embeds")
set(FUZZ_REPRO_SANITIZER "address,undefined" CACHE STRING "fuzz2's -fsanitize= list")

# ── perl configuration ────────────────────────────────────────────────────────
function(perl_query out code)
    execute_process(
        COMMAND ${FUZZ_PERL} -MConfig -MExtUtils::Embed -e "${code}"
        OUTPUT_VARIABLE _v
        RESULT_VARIABLE _rc
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${FUZZ_PERL} -e '${code}' failed (${_rc})")
    endif()
    set(${out} "${_v}" PARENT_SCOPE)
endfunction()

perl_query(PERL_VERSION_STR  "print \$Config{version}")
perl_query(PERL_CORE_DIR     "print \"\$Config{archlibexp}/CORE\"")
perl_query(PERL_CCFLAGS      "print \$Config{ccflags}")
perl_query(PERL_LIBPERL      "print \$Config{libperl}")
perl_query(PERL_LIBS         "print \$Config{perllibs}")
perl_query(PERL_LIBPTH       "print join ';', \"\$Config{archlibexp}/CORE\", split ' ', \$Config{libpth}")
perl_query(PERL_PRIVLIB      "print \$Config{privlibexp}")

# libperl by its full name: Debian ships libperl.so.5.xx without the
# libperl.so development symlink unless libperl-dev is installed, so -lperl is
# not something to rely on. A static perl (the MSAN one) has libperl.a in CORE.
find_file(PERL_LIBPERL_PATH ${PERL_LIBPERL} PATHS ${PERL_LIBPTH} /usr/lib/x86_64-linux-gnu NO_DEFAULT_PATH)
if(NOT PERL_LIBPERL_PATH)
    message(FATAL_ERROR "${PERL_LIBPERL} not found in ${PERL_LIBPTH}")
endif()
message(STATUS "perl ${PERL_VERSION_STR}: ${PERL_LIBPERL_PATH}")

separate_arguments(PERL_CCFLAGS_LIST UNIX_COMMAND "${PERL_CCFLAGS}")
# perl's own -fstack-protector-strong / -I/usr/local/include add nothing here
list(FILTER PERL_CCFLAGS_LIST EXCLUDE REGEX "^-fstack-protector")
list(FILTER PERL_CCFLAGS_LIST EXCLUDE REGEX "^-pipe$")
separate_arguments(PERL_LIBS_LIST UNIX_COMMAND "${PERL_LIBS}")

# ── Cpanel::JSON::XS: the release under test ──────────────────────────────────
# Nothing of the module is in the repository. upstream.cmake downloads the CPAN
# release once into fuzzer/build/ (every variant shares it; put it there by
# hand to build offline), checks the pinned SHA-256 and unpacks it, unpatched,
# into this build tree's upstream/ - plus perl5/, the module's .pm files where
# `use Cpanel::JSON::XS` finds them. Everything else they use is core perl
# (Exporter, XSLoader, overload, Carp, Scalar::Util, Math::BigInt).
#
# It runs at configure time (UPSTREAM_DIR/XS.pm gives the version) and again
# at build time if any file it produces has gone missing. Bumping the release
# means changing all three FUZZ_UPSTREAM_* values (or -D them), then
# reconfiguring.
set(FUZZ_UPSTREAM_VERSION "4.82" CACHE STRING "Cpanel-JSON-XS release under test")
set(FUZZ_UPSTREAM_SHA256 "ff84f5ef1d168e9dfdd4a164ed08a6a5b3bf2de57a63101cfdeb6173e22302dc"
    CACHE STRING "SHA-256 of Cpanel-JSON-XS-<FUZZ_UPSTREAM_VERSION>.tar.gz")
set(FUZZ_UPSTREAM_URL
    "https://cpan.metacpan.org/authors/id/R/RU/RURBAN/Cpanel-JSON-XS-${FUZZ_UPSTREAM_VERSION}.tar.gz"
    CACHE STRING "where the release tarball is downloaded from")
set(FUZZ_UPSTREAM_TARBALL "${REPO_DIR}/fuzzer/build/Cpanel-JSON-XS-${FUZZ_UPSTREAM_VERSION}.tar.gz"
    CACHE FILEPATH "the downloaded release, shared by every variant")

set(UPSTREAM_DIR   ${CMAKE_CURRENT_BINARY_DIR}/upstream/Cpanel-JSON-XS-${FUZZ_UPSTREAM_VERSION})
set(UPSTREAM_STAMP ${CMAKE_CURRENT_BINARY_DIR}/upstream/.Cpanel-JSON-XS-${FUZZ_UPSTREAM_VERSION}.stamp)
set(FUZZ_PERL5LIB  ${CMAKE_CURRENT_BINARY_DIR}/perl5)
set(UPSTREAM_CMD ${CMAKE_COMMAND}
    -DTARBALL=${FUZZ_UPSTREAM_TARBALL} -DURL=${FUZZ_UPSTREAM_URL} -DSHA256=${FUZZ_UPSTREAM_SHA256}
    -DDEST=${UPSTREAM_DIR} -DPERL5LIB=${FUZZ_PERL5LIB} -DSTAMP=${UPSTREAM_STAMP}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/upstream.cmake)
# what the build reads from the release; one missing file re-runs the unpack
set(UPSTREAM_FILES
    ${UPSTREAM_DIR}/XS.xs ${UPSTREAM_DIR}/typemap ${UPSTREAM_DIR}/ppport.h
    ${UPSTREAM_DIR}/utf8_range.c ${UPSTREAM_DIR}/utf8_range.h
    ${FUZZ_PERL5LIB}/Cpanel/JSON/XS.pm
    ${FUZZ_PERL5LIB}/Cpanel/JSON/XS/Boolean.pm
    ${FUZZ_PERL5LIB}/Cpanel/JSON/XS/Type.pm)

set(_need_fetch FALSE)
foreach(_f ${UPSTREAM_STAMP} ${UPSTREAM_FILES})
    if(NOT EXISTS ${_f})
        set(_need_fetch TRUE)
    endif()
endforeach()
if(_need_fetch)
    execute_process(COMMAND ${UPSTREAM_CMD} RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "fetching Cpanel-JSON-XS-${FUZZ_UPSTREAM_VERSION} failed")
    endif()
endif()
message(STATUS "Cpanel-JSON-XS ${FUZZ_UPSTREAM_VERSION}: ${UPSTREAM_DIR}")

add_custom_command(
    OUTPUT ${UPSTREAM_STAMP} ${UPSTREAM_FILES}
    COMMAND ${UPSTREAM_CMD}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/upstream.cmake
    COMMENT "unpacking Cpanel-JSON-XS-${FUZZ_UPSTREAM_VERSION}"
    VERBATIM)

file(STRINGS ${UPSTREAM_DIR}/XS.pm _ver_line REGEX "^our \\$VERSION = ")
string(REGEX REPLACE ".*'([0-9.]+)'.*" "\\1" JSON_XS_VERSION "${_ver_line}")
if(NOT JSON_XS_VERSION STREQUAL FUZZ_UPSTREAM_VERSION)
    message(FATAL_ERROR "${UPSTREAM_DIR}/XS.pm: \$VERSION '${JSON_XS_VERSION}', expected ${FUZZ_UPSTREAM_VERSION}")
endif()

# ── XS.xs -> XS.c ─────────────────────────────────────────────────────────────
# ParseXS runs in the unpacked release so the module's own ./typemap is found
# next to perl's core typemap, exactly as `perl Makefile.PL && make` would.
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/XS.c
    COMMAND ${FUZZ_PERL} -MExtUtils::ParseXS
            -e "ExtUtils::ParseXS->new->process_file(filename => 'XS.xs', output => \$ARGV[0], prototypes => 1, linenumbers => 1)"
            ${CMAKE_CURRENT_BINARY_DIR}/XS.c
    DEPENDS ${UPSTREAM_DIR}/XS.xs ${UPSTREAM_DIR}/typemap
    WORKING_DIRECTORY ${UPSTREAM_DIR}
    COMMENT "xsubpp XS.xs"
    VERBATIM)

# The perl side of the fixture (classes, callbacks, tie classes), embedded as a
# C string so a replay never depends on a file next to the binary.
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fixture_pl.h
    COMMAND ${FUZZ_PERL} ${CMAKE_CURRENT_SOURCE_DIR}/embed.pl fixture_pl
            ${CMAKE_CURRENT_SOURCE_DIR}/fixture.pl ${CMAKE_CURRENT_BINARY_DIR}/fixture_pl.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fixture.pl ${CMAKE_CURRENT_SOURCE_DIR}/embed.pl
    COMMENT "embedding fixture.pl"
    VERBATIM)

set(FUZZ_DEFS
    FUZZ_PERL5LIB="${FUZZ_PERL5LIB}"
    VERSION="${JSON_XS_VERSION}"
    XS_VERSION="${JSON_XS_VERSION}")

# Upstream's code, built as upstream builds it: its warnings are not ours to fix.
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/XS.c ${UPSTREAM_DIR}/utf8_range.c PROPERTIES COMPILE_OPTIONS -w)

# One target owns the unpack, so building fuzz, fuzz2 and fuzz_cov together
# never runs it three times in parallel.
add_custom_target(upstream DEPENDS ${UPSTREAM_STAMP} ${UPSTREAM_FILES})

# -fno-wrapv after perl's ccflags: perl builds with -fwrapv, which makes signed
# overflow defined and so silently drops UBSAN's signed-overflow (and, on some
# clang versions, pointer-overflow) checks. Without it, decoding
# "-9223372036854775808" (-(IV)uv in decode_num) is never reported.
set(FUZZ_BASE_OPTIONS -g ${PERL_CCFLAGS_LIST} -fno-wrapv -Wno-unused-parameter -Wno-unused-function)

# fuzz_add_harness(<name> OPTIONS <...> LINK_OPTIONS <...>)
#
# One executable: the harness (main.c) and the module (XS.c, utf8_range.c),
# compiled with the same options, linked against libperl. The unpacked release
# is on the include path for XS.c's "ppport.h" and "utf8_range.h".
function(fuzz_add_harness name)
    cmake_parse_arguments(A "" "" "OPTIONS;LINK_OPTIONS" ${ARGN})

    add_executable(${name}
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_BINARY_DIR}/XS.c
        ${UPSTREAM_DIR}/utf8_range.c
        ${CMAKE_CURRENT_BINARY_DIR}/fixture_pl.h)
    add_dependencies(${name} upstream)
    target_compile_options(${name} PRIVATE ${A_OPTIONS})
    target_compile_definitions(${name} PRIVATE ${FUZZ_DEFS})
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${UPSTREAM_DIR}
        ${PERL_CORE_DIR})
    # -rdynamic: fuzz_replay_main() finds vc<n>/tc<n> through dlsym, and XS
    # extensions a static perl loads at run time need libperl's symbols
    target_link_options(${name} PRIVATE ${A_LINK_OPTIONS} -rdynamic)
    target_link_libraries(${name} PRIVATE ${PERL_LIBPERL_PATH} ${PERL_LIBS_LIST})
endfunction()

# ── fuzz ──────────────────────────────────────────────────────────────────────
fuzz_add_harness(fuzz
    OPTIONS      ${FUZZ_BASE_OPTIONS}
    LINK_OPTIONS -fsanitize=fuzzer)
set_target_properties(fuzz PROPERTIES EXCLUDE_FROM_ALL TRUE)

# ── fuzz2 ─────────────────────────────────────────────────────────────────────
set(FUZZ_SANITIZER_OPTIONS -fsanitize=${FUZZ_REPRO_SANITIZER} -fno-omit-frame-pointer -fno-sanitize-recover=all)
if(FUZZ_REPRO_SANITIZER STREQUAL "memory")
    list(APPEND FUZZ_SANITIZER_OPTIONS -fsanitize-memory-track-origins=2)
endif()
fuzz_add_harness(fuzz2
    OPTIONS      ${FUZZ_BASE_OPTIONS} ${FUZZ_SANITIZER_OPTIONS} -O0 -DREPRODUCER
    LINK_OPTIONS ${FUZZ_SANITIZER_OPTIONS})

# ── fuzz_cov ──────────────────────────────────────────────────────────────────
set(COV_C_FLAGS -fprofile-instr-generate -fcoverage-mapping)
fuzz_add_harness(fuzz_cov
    OPTIONS      ${FUZZ_BASE_OPTIONS} ${COV_C_FLAGS} -DREPRODUCER
    LINK_OPTIONS ${COV_C_FLAGS})
