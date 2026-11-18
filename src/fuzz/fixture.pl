# fixture.pl - the perl half of the harness.
#
# Embedded into the binary at build time (embed.pl -> fixture_pl.h) and
# evaluated once by perl_setup() (fuzz_perl.h), before the fork server: every
# class, callback and table below exists before the first exec, so none of it
# is created - or credited as coverage - inside one.
#
# The test cases in main.c build the Cpanel::JSON::XS objects and the input
# scalar in C (jx_object(), make_input_sv(), build_value()) and then call one
# of the JXF::t_* subs here, which is the sequence of Cpanel::JSON::XS calls a
# test case makes. Every call that can croak is inside its own eval, so one
# failing call does not skip the rest; the C side wraps the whole sub in
# G_EVAL as well, for anything that escapes (a die in a callback, say).
#
# The sizes of @obj_cb, @sk_keys, @sk_cb, @sort_by and @types are mirrored by
# the JXF_* constants in fuzz_flags.h; perl_setup() checks they agree.
#
# Nothing here keeps state across execs: the persistent loop reuses this
# interpreter for thousands of inputs, and a test case whose effect lingered
# would make the next input's coverage depend on this one.

use strict;
no warnings;

use Cpanel::JSON::XS ();
use Cpanel::JSON::XS::Type ();
use Cpanel::JSON::XS::Boolean ();
use Scalar::Util ();
# Loaded lazily by the module on first use: allow_bignum (Math::Big*), the
# UTF-16/32 BOMs (Encode). Loading them here keeps that out of the execs.
use Math::BigInt ();
use Math::BigFloat ();
BEGIN { eval { require Encode } }

# Math::BigFloat's '""' overload (bstr) writes out every digit, so a decoded
# 1e3372036854775808 asks for terabytes when 4.81 stringifies it (README,
# "Bignum hangs"): an OOM kill per input, which buries everything else in the
# crash set. Past BIGFLOAT_MAX_EXP, fall back to scientific notation - the
# form 4.82 emits - and leave the decoder's exponent handling untouched. The
# overload calls bstr as a method, so redefining it here is enough.
use constant BIGFLOAT_MAX_EXP => 10000;
{
  no warnings 'redefine';
  my $bstr = \&Math::BigFloat::bstr;
  *Math::BigFloat::bstr = sub {
    my $x = $_[0];
    return $x->bsstr
      if ref $x && $x->is_finite && abs($x->exponent) > BIGFLOAT_MAX_EXP;
    goto &$bstr;
  };
}
require Tie::Hash;
require Tie::Array;
require Tie::Scalar;

# Warnings go nowhere: a campaign has no reader for them, and a replay that
# wants them can drop this line.
$SIG{__WARN__} = sub { };

package JXF;

BEGIN { Cpanel::JSON::XS::Type->import }

# ── decoder callbacks ─────────────────────────────────────────────────────────
# filter_json_object: called with the decoded hashref; may return nothing
# (keep it), one scalar (replace it) or several (an error).
our @obj_cb = (
  sub { () },                         # keep
  sub { $_[0] },                      # replace with itself
  sub { "x" },                        # replace with a string
  sub { (1, 2) },                     # croak: more than one scalar
  sub { die "obj_cb\n" },             # die inside decode
  sub { [ sort keys %{ $_[0] } ] },   # replace with a new structure
);

# filter_json_single_key_object: keyed by the one key, called with its value.
our @sk_keys = ("", "a", "__class__", "\x{263a}", "\$type", "1");
our @sk_cb = (
  sub { () },
  sub { $_[0] },
  sub { [ $_[0], $_[0] ] },
  sub { (1, 2) },
  sub { die "sk_cb\n" },
);

# sort_by() arguments: unset, off, on, a block (ignored, but stored)
our @sort_by = (undef, 0, 1, sub { $JXF::a cmp $JXF::b });

# ── encode type specs (Cpanel::JSON::XS::Type) ────────────────────────────────
# Built once, read-only from then on: encode() only reads its type spec.
our @types = (
  JSON_TYPE_STRING,
  JSON_TYPE_INT_OR_NULL,
  JSON_TYPE_FLOAT,
  JSON_TYPE_BOOL,
  JSON_TYPE_NULL,
  json_type_arrayof(JSON_TYPE_INT),
  json_type_hashof(JSON_TYPE_STRING_OR_NULL),
  json_type_anyof(JSON_TYPE_FLOAT, [JSON_TYPE_STRING], { a => JSON_TYPE_INT }),
  json_type_null_or_anyof(json_type_arrayof(JSON_TYPE_BOOL), json_type_hashof(JSON_TYPE_FLOAT)),
  [ JSON_TYPE_INT, JSON_TYPE_STRING, json_type_arrayof(JSON_TYPE_BOOL_OR_NULL) ],
  { a => JSON_TYPE_INT, b => [JSON_TYPE_STRING], k0 => json_type_optional(JSON_TYPE_FLOAT) },
  json_type_arrayof(json_type_anyof(JSON_TYPE_STRING, json_type_arrayof(JSON_TYPE_INT), json_type_hashof(JSON_TYPE_BOOL))),
  json_type_hashof(json_type_arrayof(json_type_hashof(JSON_TYPE_INT_OR_NULL))),
  { "" => json_type_optional(JSON_TYPE_STRING), "\x{263a}" => JSON_TYPE_BOOL_OR_NULL },
  99,                                 # not a type: croaks
);

sub noop { }

# ── test case bodies ──────────────────────────────────────────────────────────
# Each one is called as JXF::t_<name>(...) from the C test case of that name.
# $steps is a bit set (FLAGS_AUX()) of optional extra calls.

# test_decode: decode, then encode what came out - with the decoder object
# itself and with the encoder - and decode that again. With the type steps,
# decode also returns the types it found, and those drive the re-encode.
sub t_decode {
  my ($dec, $enc, undef, $steps) = @_;

  # $_[2], not a copy: a copy would drop the input scalar's magic and its
  # shared-key COW state (make_input_sv(), fuzz_perl.h)
  my ($v, $types);
  if ($steps & 16) {
    $v = eval { $dec->decode($_[2], $types) };
  } else {
    $v = eval { $dec->decode($_[2]) };
  }
  return if $@;

  my $out = eval { $enc->encode($v) };
  eval { $dec->encode($v) } if $steps & 1;
  eval { $enc->decode($out) } if ($steps & 2) && defined $out;
  eval { $dec->decode($out) } if ($steps & 4) && defined $out;
  eval { Cpanel::JSON::XS::encode_json($v) } if $steps & 8;
  if (($steps & 16) && defined $types) {
    my $typed = eval { $enc->encode($v, $types) };
    eval { $dec->decode($typed) } if ($steps & 32) && defined $typed;
  }
}

# test_decode_prefix: consume the input one JSON text at a time.
sub t_decode_prefix {
  my ($dec, $in, $rounds) = @_;

  for (0 .. $rounds) {
    last unless length $in;
    my ($v, $off) = eval { $dec->decode_prefix($in) };
    last if $@ || !$off;
    substr($in, 0, $off, "");
  }
}

# test_incr: feed the input to the incremental parser in fixed-size chunks.
sub t_incr {
  my ($dec, $in, $chunk, $list, $text, $reset, $swap) = @_;

  if ($text) {
    # incr_text is an lvalue; before parsing starts it may be assigned
    eval { $dec->incr_text = substr($in, 0, $chunk) };
    substr($in, 0, $chunk, "") unless $@;
  }

  for (my $pos = 0; $pos < length $in; $pos += $chunk) {
    my $part = substr($in, $pos, $chunk);

    # the incremental parser converts its buffer when utf8-ness changes
    eval { $dec->utf8(!$dec->get_utf8) } if $swap && ($pos / $chunk) % 3 == 2;

    if ($list) {
      my @v = eval { $dec->incr_parse($part) };
      eval { $dec->incr_skip } if $@;
    } else {
      eval { $dec->incr_parse($part) };  # void context: only appends
      for (1 .. 64) {
        my $v = eval { $dec->incr_parse };
        if ($@) { eval { $dec->incr_skip }; last }
        last unless defined $v;
      }
    }
  }

  # croaks once parsing started
  eval { my $t = $dec->incr_text };
  eval { $dec->incr_reset } if $reset;
}

# test_functional: the exported functional interface (utf8), and its
# non-utf8 twin; $mode picks decode_json's optional allow_nonref argument.
sub t_functional {
  my (undef, $mode) = @_;
  my $v;

  if    ($mode == 0) { $v = eval { Cpanel::JSON::XS::decode_json($_[0]) } }
  elsif ($mode == 1) { $v = eval { Cpanel::JSON::XS::decode_json($_[0], 0) } }
  elsif ($mode == 2) { $v = eval { Cpanel::JSON::XS::decode_json($_[0], 1, my $t) } }
  else               { $v = eval { Cpanel::JSON::XS::_from_json($_[0]) } }
  return if $@;
  eval { Cpanel::JSON::XS::encode_json($v) };
  eval { Cpanel::JSON::XS::_to_json($v) } if $mode == 3;
}

# test_encode: encode a built perl structure, then decode the result.
# $types is the type spec: built by the program (BOP_TYPES), one of @types,
# or undef.
sub t_encode {
  my ($enc, $dec, $v, $steps, $types) = @_;

  my $out = eval { $enc->encode($v, $types) };
  eval { $dec->decode($out) } if ($steps & 1) && defined $out;
  eval { $enc->decode($out) } if ($steps & 2) && defined $out;
  eval { Cpanel::JSON::XS::encode_json($v) } if $steps & 4;
  eval { $dec->encode($v) } if $steps & 8;
  if ($steps & 16) {
    # encode_to: the same encoder, writing to a PerlIO handle
    if (open my $fh, '>', \my $buf) {
      eval { $enc->encode_to($fh, $v, $types) };
      close $fh;
    }
  }
  if (($steps & 32) && defined $out) {
    # the types decode finds, fed back to the encoder
    my $found;
    my $v2 = eval { $dec->decode($out, $found) };
    eval { $enc->encode($v2, $found) } unless $@;
  }
}

# ── helpers for the C side ────────────────────────────────────────────────────
sub tie_hash   { tie my %h, 'JXF::TH'; \%h }
sub tie_array  { tie my @a, 'JXF::TA'; \@a }
sub tie_scalar { tie my $s, 'JXF::TS'; $s = $_[0]; \$s }

# Math::BigInt / Math::BigFloat objects from a digit string; undef for junk.
# No exponent beyond 9999 (decimal e or binary p, underscores allowed): new()
# and the stringification the encoder calls expand it digit by digit, so
# "1e12122222222" only measures Math::BigInt (OOMs and timeouts, never the module).
sub bignum {
  my ($float, $s) = @_;
  return undef if ($s =~ tr/_ \t\n\r\f//dr) =~ /[eEpP][+-]?0*\d{5}/;
  my $n = eval { $float ? Math::BigFloat->new($s) : Math::BigInt->new($s) };
  $n;
}

package JXF::TH; our @ISA = ('Tie::StdHash');
package JXF::TA; our @ISA = ('Tie::StdArray');
package JXF::TS; our @ISA = ('Tie::StdScalar');

# a subclass, as `package My::JSON; use parent 'Cpanel::JSON::XS'` would be:
# the typemap's sv_derived_from() path instead of the stash comparison
package JXF::Sub; our @ISA = ('Cpanel::JSON::XS');

# ── classes for allow_tags (THAW/FREEZE) and convert_blessed (TO_JSON) ────────
# One-letter names, so a tag the mutator writes - ("F")[...] - has a chance to
# name one. json.dict carries them too.

# well-behaved
package F;
sub THAW    { my ($class, $fmt, @v) = @_; bless [@v], $class }
sub FREEZE  { @{ $_[0] } }
sub TO_JSON { [ @{ $_[0] } ] }

# every method dies
package E;
sub THAW    { die "THAW\n" }
sub FREEZE  { die "FREEZE\n" }
sub TO_JSON { die "TO_JSON\n" }

# returns the object it was given (an error the module checks for), or nothing
package S;
sub THAW    { () }
sub FREEZE  { $_[0] }
sub TO_JSON { $_[0] }

# hash-based; THAW with an odd number of values builds an odd hash
package H;
sub THAW    { my $class = shift; shift; bless {@_}, $class }
sub FREEZE  { %{ $_[0] } }
sub TO_JSON { +{ %{ $_[0] } } }

# returns nothing at all / an undefined value
package N;
sub THAW    { return }
sub FREEZE  { return }
sub TO_JSON { undef }

# returns other objects and references
package R;
sub THAW    { bless \(my $s = $_[2]), 'R' }
sub FREEZE  { (bless([], 'F'), \1, \"x") }
# another class's object - never its own: a TO_JSON that returns a new
# object of its own class recurses until the stack ends, in any serialiser
sub TO_JSON { bless [1], 'F' }

# overloaded, and TO_JSON returning the stringification
package O;
use overload '""' => sub { "O" }, '0+' => sub { 42 }, 'bool' => sub { 1 }, fallback => 1;
sub THAW    { bless {}, 'O' }
sub FREEZE  { ("$_[0]", 0 + $_[0]) }
sub TO_JSON { "$_[0]" }

# no methods at all
package U;

1;
