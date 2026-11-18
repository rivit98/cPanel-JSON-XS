# extract the first ```perl block of each report into /out/reportN.pl
for my $f (@ARGV) {
  open my $in, '<', $f or die; local $/; my $t = <$in>;
  my ($code) = $t =~ /```perl\n(.*?)```/s or die "no perl block in $f";
  (my $n = $f) =~ s{.*/|\.md$}{}g;
  open my $o, '>', "$ENV{OUT}/$n.pl" or die; print $o $code;
}
