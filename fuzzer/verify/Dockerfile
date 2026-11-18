# Independent verification of reports/*.md on a stock distro perl.
#   docker build -t cpanel-jsonxs-verify -f fuzzer/verify/Dockerfile .
# (build context: the repository root, for the release tarball)
FROM debian:trixie
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      perl libcpanel-json-xs-perl valgrind time clang make ca-certificates libc6-dbg gcc llvm-19 libclang-rt-19-dev \
 && rm -rf /var/lib/apt/lists/*
# Pristine CPAN tarball (checked in at the repository root, SHA-256 pinned),
# built with ASAN+UBSAN against the same stock perl, and once more -O0 for valgrind.
WORKDIR /src
COPY Cpanel-JSON-XS-4.82.tar.gz /src/
RUN echo "ff84f5ef1d168e9dfdd4a164ed08a6a5b3bf2de57a63101cfdeb6173e22302dc  Cpanel-JSON-XS-4.82.tar.gz" | sha256sum -c - \
 && sha256sum Cpanel-JSON-XS-4.82.tar.gz | tee /src/tarball.sha256 \
 && tar xzf Cpanel-JSON-XS-4.82.tar.gz && cp -a Cpanel-JSON-XS-4.82 Cpanel-JSON-XS-4.82-dbg \
 && cd Cpanel-JSON-XS-4.82 \
 && perl Makefile.PL CC=clang LD=clang \
      OPTIMIZE='-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -fno-wrapv' \
      LDDLFLAGS="$(perl -MConfig -e 'print $Config{lddlflags}') -fsanitize=address,undefined" \
 && make \
 && cd ../Cpanel-JSON-XS-4.82-dbg && perl Makefile.PL OPTIMIZE='-g -O0' && make
WORKDIR /work
