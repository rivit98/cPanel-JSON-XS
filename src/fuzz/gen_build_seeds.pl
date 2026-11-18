# gen_build_seeds.pl <outdir> - write builder programs for the build group.
#
# Each file is one build_value() program (src/fuzz/main.c) WITHOUT the 4-byte
# flag prefix: ./fuzz seeds --import <outdir> --group build adds the prefixes
# and minimizes. Together the programs use every opcode, every operand table
# entry (specials, scalar refs, bless classes, big ints) and the containers
# around them, so the corpus starts with each encoder path reachable by a
# single mutation.
#
# The opcode numbers mirror enum BOP_* in main.c - keep the two in step.

use strict;
use warnings;
no warnings "portable";   # 64-bit hex literals

my $out = shift or die "usage: $0 <outdir>\n";
-d $out or mkdir $out or die "$out: $!\n";

my $i = 0;
my %op;
@op{qw(UNDEF INT8 IV UV NV NV_SPECIAL STR STR_UPGRADE STR_DECODE STR_LONG
       DUALVAR NUMIFY STRINGIFY TRUE FALSE SCALAR_REF REF BEGIN_ARRAY
       BEGIN_HASH BEGIN_TIED END KEY BLESS CODE GLOB DUP READONLY HEK
       OVERLOADED BIG_INT TYPES TYPE_CONST BIGNUM)} = (0 .. 32);

sub op  { chr $op{ $_[0] } }
sub u8  { chr($_[0] & 0xff) }
sub str { my ($o, $s) = @_; op($o) . u8(length $s) . $s }
sub key { my ($s, $utf8) = @_; op('KEY') . u8(length($s) | ($utf8 ? 0x80 : 0)) . $s }
sub u64 { pack 'Q<', $_[0] }
sub nv  { pack 'd<', $_[0] }

sub emit {
  my ($name, $prog) = @_;
  my $f = sprintf "%s/%03d_%s", $out, $i++, $name;
  open my $fh, '>:raw', $f or die "$f: $!\n";
  print $fh $prog;
}

# one value of each kind at the top level
emit("undef",   op('UNDEF'));
emit("int8",    op('INT8') . u8(-5));
emit("iv",      op('IV') . u64(-1234567890123));
emit("uv",      op('UV') . u64(0xfedcba9876543210));
emit("nv",      op('NV') . nv(3.25));
emit("special_$_", op('NV_SPECIAL') . u8($_)) for 0 .. 15;
emit("str",     str('STR', "hello \x{e9}\x00\x1f\""));
emit("upgrade", str('STR_UPGRADE', "caf\xe9"));
emit("decode",  str('STR_DECODE', "\xe2\x98\xba \xf0\x9f\x98\x80"));
emit("decode_bad", str('STR_DECODE', "\xff\xfe\xc0\x80"));
emit("long",    op('STR_LONG') . u8(200) . 'x');
emit("dualvar", op('DUALVAR') . u8(3) . "abc" . u8(7));
emit("numify",  str('STR', "12abc") . op('NUMIFY'));
emit("numify_f", str('STR', "1.5e3") . op('NUMIFY'));
emit("stringify", op('IV') . u64(42) . op('STRINGIFY'));
emit("stringify_nv", op('NV') . nv(0.1) . op('STRINGIFY'));
emit("bools",   op('BEGIN_ARRAY') . op('TRUE') . op('FALSE') . op('END'));
emit("sref_$_", op('SCALAR_REF') . u8($_)) for 0 .. 7;
emit("ref_ref", op('INT8') . u8(1) . op('REF') . op('REF'));
emit("code",    op('CODE'));
emit("glob_$_", op('GLOB') . u8($_)) for 0 .. 1;
emit("hek",     str('HEK', "key"));
emit("overloaded", op('OVERLOADED'));
emit("bigint_$_", op('BIG_INT') . u8($_)) for 0 .. 9;

# containers
emit("array",   op('BEGIN_ARRAY') . op('INT8') . u8(1) . str('STR', "a") . op('UNDEF') . op('END'));
emit("hash",    op('BEGIN_HASH') . key("b") . op('INT8') . u8(2) . key("a") . op('INT8') . u8(1)
               . key("\xe2\x98\xba", 1) . op('TRUE') . op('UNDEF') . op('END'));
emit("nested",  op('BEGIN_HASH') . key("x") . op('BEGIN_ARRAY') . op('BEGIN_HASH') . key("y")
               . op('BEGIN_ARRAY') . op('END') . op('END') . op('END') . op('END'));
emit("deep",    op('BEGIN_ARRAY') x 40 . op('INT8') . u8(0));
emit("tied_hash",  op('BEGIN_TIED') . u8(0) . key("k") . op('INT8') . u8(1) . key("j") . op('BEGIN_ARRAY') . op('END') . op('END'));
emit("tied_array", op('BEGIN_TIED') . u8(1) . op('INT8') . u8(1) . str('STR', "s") . op('END'));
emit("dup",     op('BEGIN_ARRAY') . str('STR', "shared") . op('DUP') . op('SCALAR_REF') . u8(1) . op('DUP') . op('END'));
emit("readonly", op('BEGIN_ARRAY') . op('INT8') . u8(1) . op('READONLY') . op('END'));
emit("multi_top", op('INT8') . u8(1) . op('INT8') . u8(2));

# blessed objects: F E S H N R O U JSON::PP::Boolean JSON::XS::Boolean
# Type::ArrayOf Type::HashOf Type::AnyOf Type::Optional
for my $c (0 .. 13) {
  emit("bless_array_$c", op('BEGIN_ARRAY') . op('INT8') . u8(1) . op('END') . op('BLESS') . u8($c));
  emit("bless_hash_$c",  op('BEGIN_HASH') . key("k") . op('INT8') . u8(1) . op('END') . op('BLESS') . u8($c));
  emit("bless_sref_$c",  op('SCALAR_REF') . u8(1) . op('BLESS') . u8($c));
}
emit("objects", op('BEGIN_ARRAY')
                . op('BEGIN_ARRAY') . op('END') . op('BLESS') . u8(0)
                . op('BEGIN_HASH') . op('END') . op('BLESS') . u8(3)
                . op('SCALAR_REF') . u8(0) . op('BLESS') . u8(8)
                . op('OVERLOADED') . op('END'));

# bignums: Math::BigInt / Math::BigFloat (len | 0x80)
sub bignum { my ($s, $f) = @_; op('BIGNUM') . u8(length($s) | ($f ? 0x80 : 0)) . $s }
emit("bigint",   bignum("123456789012345678901234567890"));
emit("bigint_neg", bignum("-98765432109876543210"));
emit("bigfloat", bignum("3.14159265358979323846264338327950288", 1));
emit("bigfloat_exp", bignum("1e400", 1));
emit("bignum_nan", bignum("NaN", 1));
emit("bignum_inf", bignum("-inf"));
emit("bignum_arr", op('BEGIN_ARRAY') . bignum("1") . bignum("0.5", 1) . op('END'));

# type specs: the value, then TYPES, then the spec (Cpanel::JSON::XS::Type)
emit("type_const_$_", op('INT8') . u8(7) . op('TYPES') . op('TYPE_CONST') . u8($_)) for 0 .. 10;
emit("type_str", str('STR', "12") . op('TYPES') . op('TYPE_CONST') . u8(2));
emit("type_nv",  op('NV') . nv(1.5) . op('TYPES') . op('TYPE_CONST') . u8(1));
emit("type_array", op('BEGIN_ARRAY') . op('INT8') . u8(1) . str('STR', "x") . op('UNDEF') . op('END')
                   . op('TYPES') . op('BEGIN_ARRAY') . op('TYPE_CONST') . u8(4) . op('TYPE_CONST') . u8(2)
                   . op('TYPE_CONST') . u8(7) . op('END'));
emit("type_hash", op('BEGIN_HASH') . key("a") . op('INT8') . u8(1) . key("b") . str('STR', "2") . op('END')
                  . op('TYPES') . op('BEGIN_HASH') . key("a") . op('TYPE_CONST') . u8(4)
                  . key("b") . op('TYPE_CONST') . u8(2) . op('END'));
# arrayof: \JSON_TYPE_INT blessed into Type::ArrayOf (REF wraps the last value)
emit("type_arrayof", op('BEGIN_ARRAY') . op('INT8') . u8(1) . op('INT8') . u8(2) . op('END')
                     . op('TYPES') . op('TYPE_CONST') . u8(2) . op('REF') . op('BLESS') . u8(10));
emit("type_hashof", op('BEGIN_HASH') . key("a") . op('TRUE') . key("b") . op('FALSE') . op('END')
                    . op('TYPES') . op('TYPE_CONST') . u8(1) . op('REF') . op('BLESS') . u8(11));
emit("type_anyof", op('BEGIN_ARRAY') . op('INT8') . u8(3) . op('END')
                   . op('TYPES') . op('BEGIN_ARRAY') . op('TYPE_CONST') . u8(3)
                   . op('BEGIN_ARRAY') . op('TYPE_CONST') . u8(2) . op('END')
                   . op('UNDEF') . op('END') . op('BLESS') . u8(12));
emit("type_optional", op('BEGIN_HASH') . key("a") . op('INT8') . u8(1) . op('END')
                      . op('TYPES') . op('BEGIN_HASH') . key("a") . op('TYPE_CONST') . u8(2) . key("z")
                      . op('TYPE_CONST') . u8(4) . op('REF') . op('BLESS') . u8(13) . op('END'));
