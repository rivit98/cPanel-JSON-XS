# embed.pl <name> <in> <out> - write <in> as `static const char <name>[]`, a C
# string with its length known to sizeof() (src/fuzz/CMakeLists.txt).
use strict;
my ($name, $in, $out) = @ARGV;
open my $fh, '<:raw', $in or die "$in: $!";
my $s = do { local $/; <$fh> };
open my $oh, '>:raw', $out or die "$out: $!";
print $oh "/* generated from $in - do not edit */\nstatic const char ${name}[] =\n";
for my $line (split /(?<=\n)/, $s) {
  (my $e = $line) =~ s/([\\"])/\\$1/g;
  $e =~ s/\n/\\n/g;
  print $oh "  \"$e\"\n";
}
print $oh ";\n";
