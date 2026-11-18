/*
 * struct flags_t and everything needed to derive one from a fuzzer-supplied
 * seed (setup_flags()), and to print one back out as a C initializer for
 * crash reproducers (flags_to_struct_string()).
 *
 * The first 4 bytes of every input are the seed; the rest is the test cases'
 * input. A seed is expanded through srand()/rand(), so one flipped bit gives
 * an entirely different configuration - the flags are a search dimension of
 * their own, not something the mutator edits field by field.
 */

#ifndef FUZZ_FLAGS_H
#define FUZZ_FLAGS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <inttypes.h>

// https://stackoverflow.com/questions/33010010/how-to-generate-random-64-bit-unsigned-integer-in-c
#define IMAX_BITS(m) ((m)/((m)%255+1) / 255%255*8 + 7-86/((m)%255+12))
#define RAND_MAX_WIDTH IMAX_BITS(RAND_MAX)
_Static_assert((RAND_MAX & (RAND_MAX + 1u)) == 0, "RAND_MAX not a Mersenne number");

static uint64_t rand64(void) {
  uint64_t r = 0;
  for (int i = 0; i < 64; i += RAND_MAX_WIDTH) {
    r <<= RAND_MAX_WIDTH;
    r ^= (unsigned) rand();
  }
  return r;
}

static bool verbose = false;

static const char *tc_filter = NULL;
static bool tc_filter_matched = false;

#define array_elements(x) (sizeof(x) / sizeof(x[0]))

/*
 * The Cpanel::JSON::XS boolean option setters, by method name - the ALIAS list
 * of `ascii` in XS.xs, minus `pretty` (a field of its own). jx_object()
 * (fuzz_perl.h) calls `$obj->name(enable)` for every one whose wanted state
 * differs from what Cpanel::JSON::XS->new starts with: allow_nonref is the one
 * that starts on (json_init(), GH #241), so it is the one switched OFF by a 0.
 *
 * `relaxed` is not one bit in XS.xs: its setter sets (or clears) relaxed,
 * allow_barekey, allow_singlequote and allow_dupkeys together, and
 * dupkeys_as_arrayref turns allow_dupkeys on. The setters run in this order,
 * so a later option overrides what relaxed implied - the order is part of the
 * configuration, keep it.
 */
static const char *jx_option_names[] = {
  "ascii",
  "latin1",
  "binary",
  "utf8",
  "indent",
  "canonical",
  "space_before",
  "space_after",
  "allow_nonref",
  "shrink",
  "allow_blessed",
  "convert_blessed",
  "relaxed",
  "allow_unknown",
  "allow_tags",
  "allow_barekey",
  "allow_singlequote",
  "allow_bignum",
  "escape_slash",
  "allow_stringify",
  "unblessed_bool",
  "allow_dupkeys",
  "require_types",
  "type_all_string",
  "dupkeys_as_arrayref",
};
#define JXO_BIT(i)           (1u << (i))
#define JXO_ALLOW_NONREF     JXO_BIT(8)
#define JXO_REQUIRE_TYPES    JXO_BIT(22)
#define JXO_TYPE_ALL_STRING  JXO_BIT(23)
#define JXO_DEFAULT          JXO_ALLOW_NONREF
#define JXO_ALL              ((1u << array_elements(jx_option_names)) - 1)
/*
 * Options that make most calls fail or flatten the output: on with p = 1/8
 * instead of 1/2. require_types croaks on any encode without a type spec;
 * type_all_string turns every scalar into a string.
 */
#define JXO_RARE             (JXO_REQUIRE_TYPES | JXO_TYPE_ALL_STRING)

/*
 * max_depth values: never above the default of 512. The default exists to
 * keep the recursive decoder and encoder off the end of the stack; a campaign
 * that raised it would only rediscover that deep recursion overflows a stack,
 * once per input shape.
 */
#define JX_MAX_DEPTH_ZERO 0x80000000u  /* calls ->max_depth(0): nothing nests */
static const uint32_t jx_max_depths[] = { JX_MAX_DEPTH_ZERO, 1, 2, 3, 4, 8, 32, 100, 512 };

static const uint32_t jx_max_sizes[] = { 1, 2, 5, 16, 64, 1000 };

/* indent_length() arguments; 16 and -1 are out of range (a warning, no change) */
static const int jx_indent_lengths[] = { 0, 1, 2, 4, 8, 15, 16, -1 };

/* One Cpanel::JSON::XS object's configuration. A test case gets two: dec and enc. */
struct jx_cfg_t {
  uint32_t options;        /* bit i: jx_option_names[i] on */
  uint32_t max_depth;      /* 0 = untouched, JX_MAX_DEPTH_ZERO = max_depth(0) */
  uint32_t max_size;       /* 0 = untouched */
  unsigned int filter_object;   /* 0 = none, else 1 + index into @JXF::obj_cb */
  unsigned int filter_sk_keys;  /* bit i: filter_json_single_key_object($JXF::sk_keys[i], ...) */
  unsigned int filter_sk_cb;    /* index into @JXF::sk_cb, rotated per key */
  unsigned int sort_by;         /* 0 = none, else 1 + index into @JXF::sort_by */
  unsigned int indent_length;   /* 0 = untouched, else 1 + index into jx_indent_lengths */
  unsigned int infnan;          /* 0 = untouched, else stringify_infnan(infnan - 1); 5 croaks */
  bool pretty;                  /* ->pretty before the options above */
  bool nonref_set;              /* allow_nonref called even when already in that state (F_NONREF_SET) */
  bool subclass;                /* JXF::Sub->new, a subclass, instead of Cpanel::JSON::XS->new */
};

struct flags_t {
  struct jx_cfg_t dec;
  struct jx_cfg_t enc;
  /*
   * How the input string becomes a perl scalar (make_input_sv(), fuzz_perl.h):
   *   0 bytes                    a plain non-UTF8 PV
   *   1 upgraded                 utf8::upgrade - every byte one character
   *   2 decoded                  utf8::decode - UTF8 flag only if valid UTF-8
   *   3 tied                     a tied scalar returning the bytes (magic)
   *   4 shared hash key          a COW shared-HEK scalar (a hash key)
   *   5 number then string       pIOK/pNOK set by numeric use, then the PV
   */
  unsigned int input_kind;
  /* Raw bits for the choices that have no field of their own; each consumer takes its own range (FLAGS_AUX()). */
  uint64_t aux;
};
#define INPUT_KINDS 6

/* the consumers' bit ranges in flags_t.aux - append, never move one */
#define AUX_INCR_CHUNK      0   /* 6: incr_parse chunk size - 1 */
#define AUX_INCR_LIST       6   /* 1: incr_parse in list context */
#define AUX_INCR_TEXT       7   /* 1: assign incr_text before parsing */
#define AUX_INCR_RESET      8   /* 1: incr_reset at the end */
#define AUX_INCR_SWAP       9   /* 1: switch utf8 between chunks */
#define AUX_PREFIX_N       10   /* 4: decode_prefix rounds */
#define AUX_DECODE_STEPS   14   /* 6: extra steps of test_decode (fixture.pl) */
#define AUX_GETTER         20   /* 5: which get_* is called */
#define AUX_BUILD_ROOT     25   /* 1: test_build root: wrap all top-level values */
#define AUX_ENCODE_STEPS   26   /* 6: extra steps of test_encode (fixture.pl) */
#define AUX_FUNC_MODE      32   /* 2: test_functional: decode_json's allow_nonref argument */
#define AUX_TYPESPEC       34   /* 4: encode type spec: 0 = none, else 1 + index into @JXF::types */

/* bits [shift, shift + width) of flags_t.aux */
#define FLAGS_AUX(flags, shift, width) \
  ((unsigned int)(((flags)->aux >> (shift)) & ((1ull << (width)) - 1)))

static const char *input_kind_desc[INPUT_KINDS] = {
  "bytes", "upgraded", "decoded", "tied", "shared hash key", "number then string",
};

/* sizes of the perl-side tables in fixture.pl - perl_setup() checks them */
#define JXF_OBJ_CB   6
#define JXF_SK_KEYS  6
#define JXF_SK_CB    5
#define JXF_SORT_BY  4
#define JXF_TYPES    15

static void setup_cfg(struct jx_cfg_t *c) {
  uint64_t r = rand64();

  /* options: each on with p = 1/2, the rare ones 1/8, allow_nonref 7/8 */
  c->options = (uint32_t)(rand64() & JXO_ALL);
  c->options &= ~JXO_RARE | (uint32_t)(rand64() & rand64());
  if ((r & 7) != 0)
    c->options |= JXO_ALLOW_NONREF;
  r >>= 3;

  /* 1 in 4 sets a max_depth */
  c->max_depth = (r & 3) ? 0 : jx_max_depths[(r >> 2) % array_elements(jx_max_depths)];
  r >>= 8;
  /* 1 in 16 sets a max_size */
  c->max_size = (r & 15) ? 0 : jx_max_sizes[(r >> 4) % array_elements(jx_max_sizes)];
  r >>= 8;
  /* 1 in 4 installs an object filter, 1 in 4 single-key filters */
  c->filter_object = (r & 3) ? 0 : 1 + (unsigned int)((r >> 2) % JXF_OBJ_CB);
  r >>= 8;
  c->filter_sk_keys = (r & 3) ? 0 : (unsigned int)((r >> 2) & ((1u << JXF_SK_KEYS) - 1));
  r >>= 8;
  c->filter_sk_cb = (unsigned int)(r % JXF_SK_CB);
  r >>= 4;
  /* 1 in 8 calls sort_by */
  c->sort_by = (r & 7) ? 0 : 1 + (unsigned int)((r >> 3) % JXF_SORT_BY);
  r >>= 5;
  c->pretty = (r & 7) == 0;
  r >>= 3;
  /* 1 in 4 sets an indent_length */
  c->indent_length = (r & 3) ? 0 : 1 + (unsigned int)((r >> 2) % array_elements(jx_indent_lengths));
  r >>= 5;
  /* 1 in 4 calls stringify_infnan, mode 0..3 or the invalid 4 */
  c->infnan = (r & 3) ? 0 : 1 + (unsigned int)((r >> 2) % 5);
  r >>= 5;
  c->nonref_set = (r & 3) == 0;
  r >>= 2;
  c->subclass = (r & 7) == 0;
}

static void setup_flags(uint32_t seed, struct flags_t *flags) {
  srand(seed);
  setup_cfg(&flags->dec);
  setup_cfg(&flags->enc);
  flags->input_kind = (unsigned int)(rand64() % INPUT_KINDS);
  /*
   * Drawn last on purpose. Every rand64() here consumes from one srand(seed)
   * stream, so inserting a draw anywhere above would shift every field after
   * it and silently re-derive a different flag set for every artifact already
   * on disk - the RR() reproducers included. Append new draws after this one.
   */
  flags->aux = rand64();
}

/* ── printing ──────────────────────────────────────────────────────────────── */

static void append_fmt(char **p, size_t *left, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void append_fmt(char **p, size_t *left, const char *fmt, ...) {
  va_list ap;
  int n;

  if (*left == 0)
    return;
  va_start(ap, fmt);
  n = vsnprintf(*p, *left, fmt, ap);
  va_end(ap);
  if (n < 0)
    return;
  if ((size_t)n >= *left)
    n = (int)*left - 1;
  *p += n;
  *left -= (size_t)n;
}

static void append_options(char **p, size_t *left, uint32_t options) {
  bool any = false;

  for (size_t i = 0; i < array_elements(jx_option_names); i++) {
    if (!(options & (1u << i)))
      continue;
    append_fmt(p, left, "%s%s", any ? " | " : "", jx_option_names[i]);
    any = true;
  }
  if (!any)
    append_fmt(p, left, "none");
}

static void print_cfg(const char *name, const struct jx_cfg_t *c) {
  char buf[1024], *p = buf;
  size_t left = sizeof(buf);

  append_options(&p, &left, c->options);
  printf("%s.options: 0x%x %s\n", name, c->options, buf);
  printf("%s.max_depth: %s%u  max_size: %u  pretty: %d\n", name,
         c->max_depth == JX_MAX_DEPTH_ZERO ? "(0) " : "",
         c->max_depth == JX_MAX_DEPTH_ZERO ? 0 : c->max_depth, c->max_size, c->pretty);
  printf("%s.filter_object: %u  filter_sk_keys: 0x%x  filter_sk_cb: %u  sort_by: %u\n", name,
         c->filter_object, c->filter_sk_keys, c->filter_sk_cb, c->sort_by);
  printf("%s.indent_length: %u  infnan: %u  nonref_set: %d  subclass: %d\n", name,
         c->indent_length, c->infnan, c->nonref_set, c->subclass);
}

static void print_flags(const struct flags_t *flags) {
  print_cfg("dec", &flags->dec);
  print_cfg("enc", &flags->enc);
  printf("input_kind: %u %s\n", flags->input_kind, input_kind_desc[flags->input_kind]);
  printf("aux: 0x%" PRIx64 "\n", flags->aux);
}

static void append_cfg_struct(char **p, size_t *left, const char *name, const struct jx_cfg_t *c) {
  append_fmt(p, left, "\n  .%s = { .options = 0x%x /* ", name, c->options);
  append_options(p, left, c->options);
  append_fmt(p, left, " */, .max_depth = 0x%x, .max_size = %u, .filter_object = %u,"
             " .filter_sk_keys = 0x%x, .filter_sk_cb = %u, .sort_by = %u, .indent_length = %u,"
             " .infnan = %u, .pretty = %d, .nonref_set = %d, .subclass = %d },",
             c->max_depth, c->max_size, c->filter_object, c->filter_sk_keys,
             c->filter_sk_cb, c->sort_by, c->indent_length, c->infnan, c->pretty,
             c->nonref_set, c->subclass);
}

/* A C initializer for the RF() reproducer; static buffer, valid until the next call. */
static char *flags_to_struct_string(const struct flags_t *flags) {
  static char buf[4096];
  char *p = buf;
  size_t left = sizeof(buf);

  append_fmt(&p, &left, "{");
  append_cfg_struct(&p, &left, "dec", &flags->dec);
  append_cfg_struct(&p, &left, "enc", &flags->enc);
  append_fmt(&p, &left, "\n  .input_kind = %u /* %s */,", flags->input_kind, input_kind_desc[flags->input_kind]);
  append_fmt(&p, &left, "\n  .aux = 0x%" PRIx64 "ull\n}", flags->aux);
  return buf;
}

#endif /* FUZZ_FLAGS_H */
