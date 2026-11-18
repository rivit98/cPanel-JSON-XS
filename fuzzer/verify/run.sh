#!/bin/sh
# Runs inside the container. /reports (ro) holds reportN.md, /out gets the evidence.
#   docker run --rm --cpus=2 --ulimit nofile=1024:1024 \
#     -v $PWD/reports:/reports:ro -v $PWD/reports/evidence:/out \
#     -v $PWD/fuzzer/verify:/verify:ro cpanel-jsonxs-verify sh /verify/run.sh
set -u
OUT=/out; export OUT
V=Cpanel-JSON-XS-4.82
ASAN_SO=$(clang -print-file-name=libclang_rt.asan-x86_64.so)
export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer-19
perl /verify/extract.pl /reports/report*.md
{
  echo "== environment"; date -u; uname -srm; cat /etc/debian_version
  perl -v | sed -n 2p
  dpkg-query -W perl libcpanel-json-xs-perl
  perl -MCpanel::JSON::XS -e 'print "stock Cpanel::JSON::XS $Cpanel::JSON::XS::VERSION from $INC{q(Cpanel/JSON/XS.pm)}\n"'
  echo "CPAN tarball sha256: $(cat /src/tarball.sha256)"
  sha256sum /src/$V/XS.xs /src/$V/XS.pm
} > $OUT/environment.txt 2>&1
for pl in $OUT/report*.pl; do
  n=$(basename $pl .pl)
  # 1) stock Debian perl + stock Debian Cpanel::JSON::XS (an older release), no instrumentation
  ( cd /tmp && /usr/bin/time -f 'peak RSS: %M kB' timeout 600 perl $pl ) > $OUT/$n.stock.log 2>&1; echo "exit=$?" >> $OUT/$n.stock.log
  # resource-exhaustion reports ("no sanitizer needed"): the stock run is the evidence;
  # valgrind/ASAN on a multi-GB allocation only measures how long the tools take
  grep -q 'no sanitizer needed' /reports/$n.md && continue
  # 2) the same under valgrind
  ( cd /tmp && timeout 900 valgrind --error-exitcode=99 --track-origins=yes --leak-check=no perl $pl ) > $OUT/$n.valgrind-stock.log 2>&1; echo "exit=$?" >> $OUT/$n.valgrind-stock.log
  # 3) stock perl + pristine CPAN release built with ASAN+UBSAN
  ( cd /src/$V && LD_PRELOAD=$ASAN_SO ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
      timeout 600 perl -Mblib $pl ) > $OUT/$n.asan-ubsan.log 2>&1; echo "exit=$?" >> $OUT/$n.asan-ubsan.log
  # 4) stock perl + pristine CPAN release with -g -O0 under valgrind (source lines)
  ( cd /src/$V-dbg && timeout 900 valgrind --error-exitcode=99 --track-origins=yes --leak-check=no perl -Mblib $pl ) > $OUT/$n.valgrind-dbg.log 2>&1; echo "exit=$?" >> $OUT/$n.valgrind-dbg.log
done
