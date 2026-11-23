#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include "fuzz_utils.h"

/*
 * The test cases. Each one turns its input into Cpanel::JSON::XS calls; the calls
 * themselves are the t_* subs in fixture.pl, and the C side supplies what perl
 * code cannot spell cheaply or exactly: the two configured objects
 * (jx_object()), the input scalar in one of its representations
 * (make_input_sv()) and, for test_build, a perl structure with precise SV
 * flags (build_value()).
 *
 * Every test case runs in its own perl scope (T_RUN(), fuzz_utils.h), so it
 * creates only mortals and returns with the interpreter as it found it.
 */

/* ── json group: the input is JSON text ────────────────────────────────────── */

void test_decode(struct flags_t *flags, const char *data, size_t size) {
  dFUZZ;
  SV *dec = jx_object(flags, &flags->dec);
  SV *enc = jx_object(flags, &flags->enc);
  CHECK(dec && enc);

  SV *in = make_input_sv(flags, data, size);
  jx_call("JXF::t_decode", false, G_DISCARD, NULL, 4, dec, enc, in,
          sv_2mortal(newSVuv(FLAGS_AUX(flags, AUX_DECODE_STEPS, 6))));
out:
  return;
}

void test_decode_prefix(struct flags_t *flags, const char *data, size_t size) {
  dFUZZ;
  SV *dec = jx_object(flags, &flags->dec);
  CHECK(dec);

  SV *in = make_input_sv(flags, data, size);
  jx_call("JXF::t_decode_prefix", false, G_DISCARD, NULL, 3, dec, in,
          sv_2mortal(newSVuv(FLAGS_AUX(flags, AUX_PREFIX_N, 4))));
out:
  return;
}

void test_incr(struct flags_t *flags, const char *data, size_t size) {
  dFUZZ;
  SV *dec = jx_object(flags, &flags->dec);
  CHECK(dec);

  SV *in = make_input_sv(flags, data, size);
  jx_call("JXF::t_incr", false, G_DISCARD, NULL, 7, dec, in,
          sv_2mortal(newSVuv(1 + FLAGS_AUX(flags, AUX_INCR_CHUNK, 6))),
          boolSV(FLAGS_AUX(flags, AUX_INCR_LIST, 1)),
          boolSV(FLAGS_AUX(flags, AUX_INCR_TEXT, 1)),
          boolSV(FLAGS_AUX(flags, AUX_INCR_RESET, 1)),
          boolSV(FLAGS_AUX(flags, AUX_INCR_SWAP, 1)));
out:
  return;
}

void test_functional(struct flags_t *flags, const char *data, size_t size) {
  dFUZZ;
  SV *in = make_input_sv(flags, data, size);

  jx_call("JXF::t_functional", false, G_DISCARD, NULL, 2, in,
          sv_2mortal(newSVuv(FLAGS_AUX(flags, AUX_FUNC_MODE, 2))));
}

/* ── build group: the input is a program that builds a perl value ──────────── */

/*
 * build_value() runs the input as a small stack-machine program and returns
 * the perl value it built - the encoder's input. JSON text cannot reach most
 * of what the encoder handles: scalars that are both string and number
 * (dualvars, numified strings), UV/IV/NV edge values, Inf/NaN, strings with
 * and without the UTF8 flag, references to scalars, code and globs, blessed
 * objects, tied containers, shared hash key scalars, Math::BigInt/BigFloat
 * objects - and the encoder's type spec (Cpanel::JSON::XS::Type): after
 * BOP_TYPES, the program builds the type spec instead of the value.
 *
 * Every byte is a valid opcode (byte % BOP__COUNT); operands follow the
 * opcode, and a truncated operand reads as zeros. Values are appended to the
 * innermost open container - an array, or a hash under the key set by
 * BOP_KEY (an automatic one otherwise) - or, outside any container, to the
 * list of top-level values. Containers still open at the end are closed.
 *
 * The program never builds a reference cycle (perl's refcounting would leak
 * it, and the SV leak check would blame Cpanel::JSON::XS for the harness's own leak),
 * nor a shared container (see BOP_DUP). Nothing in it may croak: it runs
 * outside any eval, where a croak ends the process with exit status 255.
 */
enum {
  BOP_UNDEF,         /* undef */
  BOP_INT8,          /* i8: a small IV */
  BOP_IV,            /* i64 */
  BOP_UV,            /* u64, as a UV */
  BOP_NV,            /* f64 */
  BOP_NV_SPECIAL,    /* u8: Inf, -Inf, NaN, -0.0, DBL_MAX, DBL_MIN, ... */
  BOP_STR,           /* u8 len, bytes: a byte string */
  BOP_STR_UPGRADE,   /* u8 len, bytes: utf8::upgrade'd */
  BOP_STR_DECODE,    /* u8 len, bytes: utf8::decode'd (UTF8 only when valid) */
  BOP_STR_LONG,      /* u8 n, u8 c: c repeated 16 * (n % 64) + 1 times */
  BOP_DUALVAR,       /* u8 len, bytes, i8: PV and IV that disagree */
  BOP_NUMIFY,        /* top value used as a number: pIOK/pNOK */
  BOP_STRINGIFY,     /* top value used as a string: pPOK */
  BOP_TRUE,          /* Cpanel::JSON::XS::true */
  BOP_FALSE,         /* Cpanel::JSON::XS::false */
  BOP_SCALAR_REF,    /* u8: \0, \1, \"1", \1.0, \"", \undef, \"0", \2 */
  BOP_REF,           /* replace the last value with a reference to it */
  BOP_BEGIN_ARRAY,
  BOP_BEGIN_HASH,
  BOP_BEGIN_TIED,    /* u8: a tied hash (even) or array (odd) */
  BOP_END,           /* close the innermost container */
  BOP_KEY,           /* u8 len, bytes: the next hash key; len & 0x80: UTF-8 key */
  BOP_BLESS,         /* u8: bless the last reference into a fixture class */
  BOP_CODE,          /* \&JXF::noop */
  BOP_GLOB,          /* *STDOUT, or \*STDOUT */
  BOP_DUP,           /* the last value again (shared, not copied) - not a container */
  BOP_READONLY,      /* the last value made read-only */
  BOP_HEK,           /* u8 len, bytes: a shared hash key scalar */
  BOP_OVERLOADED,    /* an object with "" / 0+ overloading */
  BOP_BIG_INT,       /* u8: IV_MIN, IV_MAX, UV_MAX, +-59000 boundaries */
  BOP_TYPES,         /* close every container; what follows builds the type spec */
  BOP_TYPE_CONST,    /* u8: a JSON_TYPE_* constant (or an invalid one) */
  BOP_BIGNUM,        /* u8 len, bytes: Math::BigInt (len & 0x80: BigFloat) from the bytes */
  BOP__COUNT
};

#define BUILD_MAX_DEPTH  64
#define BUILD_MAX_ITEMS  4096

struct build_state {
  const uint8_t *p, *end;
  /* the open containers; frame 0 is the top level, an array */
  SV *frame[BUILD_MAX_DEPTH + 1];
  SV *frame_ref[BUILD_MAX_DEPTH + 1];   /* the reference the parent holds */
  bool is_hash[BUILD_MAX_DEPTH + 1];
  SV *key;                /* pending hash key, or NULL */
  unsigned int depth;
  unsigned int auto_key;
  /*
   * The last value appended, borrowed - what BOP_DUP, BOP_BLESS and friends act
   * on. Never a container that is still open: a duplicate of that, added to
   * itself, would be a cycle.
   */
  SV *last;
  SV *types;              /* the type spec's top level once BOP_TYPES ran, or NULL */
};

static uint8_t rd_u8(struct build_state *b) {
  return b->p < b->end ? *b->p++ : 0;
}

static uint64_t rd_u64(struct build_state *b) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++)
    v |= (uint64_t)rd_u8(b) << (8 * i);
  return v;
}

/* [p, p + len) clamped to what is left */
static const char *rd_bytes(struct build_state *b, size_t *len) {
  const char *s = (const char *)b->p;
  size_t left = (size_t)(b->end - b->p);

  if (*len > left)
    *len = left;
  b->p += *len;
  return s;
}

/* Append @sv (a new reference the container takes over) to the innermost frame. */
static void build_add(struct build_state *b, SV *sv) {
  dFUZZ;
  SV *c = b->frame[b->depth];

  if (b->is_hash[b->depth]) {
    SV *key = b->key;
    if (!key)
      key = sv_2mortal(newSVpvf("k%u", b->auto_key++));
    b->key = NULL;
    if (!hv_store_ent((HV *)c, key, sv, 0)) {
      /* a tied hash stores a copy through STORE and keeps nothing of @sv */
      SvREFCNT_dec(sv);
      b->last = NULL;
      return;
    }
  } else {
    /* av_store, not av_push: on a tied array av_push neither stores nor
     * frees the value, av_store returns NULL and leaves it to the caller */
    if (!av_store((AV *)c, av_top_index((AV *)c) + 1, sv)) {
      SvREFCNT_dec(sv);
      b->last = NULL;
      return;
    }
  }
  b->last = sv;
}

static SV *fixture_ref(const char *sub) {
  SV *ref = NULL;

  if (jx_call(sub, false, G_SCALAR, &ref, 0) != 1 || !ref || !SvROK(ref))
    return NULL;
  return ref;
}

static const char *bless_classes[] = {
  "F", "E", "S", "H", "N", "R", "O", "U",
  /* boolean classes: scalar references only, as their overloads expect */
  "JSON::PP::Boolean", "JSON::XS::Boolean",
  /* the type spec classes (Cpanel::JSON::XS::Type) */
  "Cpanel::JSON::XS::Type::ArrayOf", "Cpanel::JSON::XS::Type::HashOf",
  "Cpanel::JSON::XS::Type::AnyOf", "Cpanel::JSON::XS::Type::Optional",
  /* not "Cpanel::JSON::XS": its DESTROY takes any referent for a JSON struct */
};
#define BLESS_BOOL_FIRST 8
#define BLESS_BOOL_LAST  9

/* JSON_TYPE_* of XS.xs; the last one is no type at all */
static const IV type_consts[] = {
  0x0000, 0x0001, 0x0002, 0x0003, 0x0004,
  0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 5,
};

static SV *build_root(struct flags_t *flags, AV *top) {
  dFUZZ;
  /* one top-level value as it is, several as an array */
  if (av_count(top) == 1 && !FLAGS_AUX(flags, AUX_BUILD_ROOT, 1))
    return *av_fetch(top, 0, 0);   /* the very SV, not a copy: its flags are the point */
  return sv_2mortal(newRV_inc((SV *)top));
}

/* *types gets the type spec when the program built one, else stays NULL */
static SV *build_value(struct flags_t *flags, const char *data, size_t size, SV **types) {
  dFUZZ;
  struct build_state b = {
    .p = (const uint8_t *)data,
    .end = (const uint8_t *)data + size,
  };
  unsigned int items = 0;
  SV *root;

  AV *values = (AV *)sv_2mortal((SV *)newAV());
  b.frame[0] = (SV *)values;

  while (b.p < b.end && items++ < BUILD_MAX_ITEMS) {
    uint8_t op = rd_u8(&b) % BOP__COUNT;
    SV *sv = NULL;

    switch (op) {
    case BOP_UNDEF:  sv = newSV(0); break;
    case BOP_INT8:   sv = newSViv((int8_t)rd_u8(&b)); break;
    case BOP_IV:     sv = newSViv((IV)(int64_t)rd_u64(&b)); break;
    case BOP_UV:     sv = newSVuv((UV)rd_u64(&b)); break;
    case BOP_NV: {
      uint64_t bits = rd_u64(&b);
      double d;
      memcpy(&d, &bits, sizeof(d));
      sv = newSVnv(d);
      break;
    }
    case BOP_NV_SPECIAL: {
      static const double specials[] = {
        INFINITY, -INFINITY, NAN, -0.0, DBL_MAX, DBL_MIN, -DBL_MAX, 1e-320,
        0.1, 1e21, 1e-7, 123456789012345678.0, 9007199254740993.0, 0.5, -1.5, 1e300,
      };
      sv = newSVnv(specials[rd_u8(&b) % array_elements(specials)]);
      break;
    }
    case BOP_STR:
    case BOP_STR_UPGRADE:
    case BOP_STR_DECODE:
    case BOP_HEK: {
      size_t len = rd_u8(&b);
      const char *s = rd_bytes(&b, &len);
      sv = newSVpvn(s, len);
      if (op == BOP_STR_UPGRADE)
        sv_utf8_upgrade(sv);
      else if (op == BOP_STR_DECODE)
        sv_utf8_decode(sv);
      else if (op == BOP_HEK) {
        HV *hv = (HV *)sv_2mortal((SV *)newHV());
        HE *he;
        hv_store_ent(hv, sv, newSV(0), 0);
        hv_iterinit(hv);
        he = hv_iternext(hv);
        SvREFCNT_dec(sv);
        sv = he ? newSVhek(HeKEY_hek(he)) : newSV(0);
      }
      break;
    }
    case BOP_STR_LONG: {
      size_t n = 16 * (size_t)(rd_u8(&b) % 64) + 1;
      sv = newSV(n + 1);
      SvPOK_only(sv);
      memset(SvPVX(sv), rd_u8(&b), n);
      SvCUR_set(sv, n);
      *SvEND(sv) = '\0';
      break;
    }
    case BOP_DUALVAR: {
      size_t len = rd_u8(&b);
      const char *s = rd_bytes(&b, &len);
      sv = newSVpvn(s, len);
      SvUPGRADE(sv, SVt_PVIV);
      SvIV_set(sv, (int8_t)rd_u8(&b));
      SvIOK_on(sv);
      break;
    }
    case BOP_NUMIFY:
      if (b.last && !SvROK(b.last)) {
        (void)SvNV(b.last);
        (void)SvIV(b.last);
      }
      continue;
    case BOP_STRINGIFY:
      if (b.last && !SvROK(b.last))
        (void)SvPV_nolen(b.last);
      continue;
    case BOP_TRUE:
    case BOP_FALSE:
      sv = newSVsv(get_sv(op == BOP_TRUE ? "Cpanel::JSON::XS::true" : "Cpanel::JSON::XS::false", GV_ADD));
      break;
    case BOP_SCALAR_REF: {
      SV *t;
      switch (rd_u8(&b) % 8) {
      case 0:  t = newSViv(0); break;
      case 1:  t = newSViv(1); break;
      case 2:  t = newSVpvs("1"); break;
      case 3:  t = newSVnv(1.0); break;
      case 4:  t = newSVpvs(""); break;
      case 5:  t = newSV(0); break;
      case 6:  t = newSVpvs("0"); break;
      default: t = newSViv(2); break;
      }
      sv = newRV_noinc(t);
      break;
    }
    case BOP_REF: {
      /* the last value of the innermost frame, wrapped in place */
      SV *c = b.frame[b.depth];
      if (!b.last || b.is_hash[b.depth] || SvRMAGICAL(c) || av_count((AV *)c) == 0)
        continue;
      SV *inner = av_pop((AV *)c);   /* our reference, now owned by the RV */
      sv = newRV_noinc(inner);
      break;
    }
    case BOP_BEGIN_ARRAY:
    case BOP_BEGIN_HASH:
    case BOP_BEGIN_TIED: {
      SV *ref;
      bool hash;
      if (b.depth >= BUILD_MAX_DEPTH)
        continue;
      if (op == BOP_BEGIN_TIED) {
        hash = (rd_u8(&b) & 1) == 0;
        ref = fixture_ref(hash ? "JXF::tie_hash" : "JXF::tie_array");
        if (!ref)
          continue;
        ref = newSVsv(ref);
      } else {
        hash = op == BOP_BEGIN_HASH;
        ref = newRV_noinc(hash ? (SV *)newHV() : (SV *)newAV());
      }
      /*
       * The container is held by a mortal reference of its own while it is
       * open: stored into a tied parent, @ref is only copied by STORE and
       * build_add() frees it, so SvRV(ref) cannot be read afterwards.
       */
      SV *inner = sv_2mortal(SvREFCNT_inc_simple_NN(SvRV(ref)));
      SV *held = sv_2mortal(newRV_inc(inner));
      build_add(&b, ref);
      b.last = NULL;
      b.depth++;
      b.frame[b.depth] = inner;
      b.frame_ref[b.depth] = held;
      b.is_hash[b.depth] = hash;
      b.key = NULL;
      continue;
    }
    case BOP_END:
      if (b.depth > 0) {
        b.last = b.frame_ref[b.depth];
        b.depth--;
        b.key = NULL;
      }
      continue;
    case BOP_KEY: {
      uint8_t l = rd_u8(&b);
      size_t len = l & 0x7f;
      const char *s = rd_bytes(&b, &len);
      b.key = sv_2mortal(newSVpvn(s, len));
      if (l & 0x80)
        sv_utf8_decode(b.key);
      continue;
    }
    case BOP_BLESS: {
      unsigned int ci = rd_u8(&b) % array_elements(bless_classes);
      /* never a shared referent: blessing \&JXF::noop, *STDOUT or the
       * Cpanel::JSON::XS booleans would outlive the exec */
      if (b.last && SvROK(b.last) && !SvREADONLY(SvRV(b.last)) &&
          SvTYPE(SvRV(b.last)) != SVt_PVCV && SvTYPE(SvRV(b.last)) != SVt_PVGV &&
          !(ci >= BLESS_BOOL_FIRST && ci <= BLESS_BOOL_LAST && SvTYPE(SvRV(b.last)) >= SVt_PVAV))
        sv_bless(b.last, gv_stashpv(bless_classes[ci], GV_ADD));
      continue;
    }
    case BOP_CODE:
      sv = newRV_inc((SV *)get_cv("JXF::noop", 0));
      break;
    case BOP_GLOB: {
      GV *gv = gv_fetchpvs("STDOUT", 0, SVt_PVGV);
      sv = rd_u8(&b) & 1 ? newRV_inc((SV *)gv) : newSVsv((SV *)gv);
      break;
    }
    case BOP_DUP:
      /*
       * Never an array or hash: sharing containers nests a DAG that doubles
       * the encoded size per level - a billion-laughs input that only
       * measures how long a 2^n-byte encode takes.
       */
      if (b.last && !(SvROK(b.last) && (SvTYPE(SvRV(b.last)) == SVt_PVAV ||
                                        SvTYPE(SvRV(b.last)) == SVt_PVHV)))
        sv = SvREFCNT_inc_simple_NN(b.last);
      break;
    case BOP_READONLY:
      if (b.last)
        SvREADONLY_on(b.last);
      continue;
    case BOP_OVERLOADED:
      sv = sv_bless(newRV_noinc((SV *)newHV()), gv_stashpvs("O", GV_ADD));
      break;
    case BOP_BIG_INT:
      switch (rd_u8(&b) % 10) {
      case 0: sv = newSViv(IV_MIN); break;
      case 1: sv = newSViv(IV_MAX); break;
      case 2: sv = newSVuv(UV_MAX); break;
      case 3: sv = newSViv(59000); break;
      case 4: sv = newSViv(59001); break;
      case 5: sv = newSViv(-59000); break;
      case 6: sv = newSViv(-59001); break;
      case 7: sv = newSVuv((UV)IV_MAX + 1); break;
      case 8: sv = newSViv(-1); break;
      default: sv = newSVuv(59000); break;
      }
      break;
    case BOP_TYPES:
      if (b.types)
        continue;
      b.depth = 0;
      b.key = NULL;
      b.last = NULL;
      b.types = sv_2mortal((SV *)newAV());
      b.frame[0] = b.types;
      continue;
    case BOP_TYPE_CONST:
      sv = newSViv(type_consts[rd_u8(&b) % array_elements(type_consts)]);
      break;
    case BOP_BIGNUM: {
      uint8_t l = rd_u8(&b);
      size_t len = l & 0x3f;
      const char *s = rd_bytes(&b, &len);
      SV *n = NULL;
      if (jx_call("JXF::bignum", false, G_SCALAR, &n, 2, boolSV(l & 0x80),
                  sv_2mortal(newSVpvn(s, len))) == 1 && n && SvROK(n))
        sv = newSVsv(n);
      break;
    }
    }

    if (sv)
      build_add(&b, sv);
  }

  root = build_root(flags, values);
  *types = b.types ? build_root(flags, (AV *)b.types) : NULL;
  return root;
}

void test_encode(struct flags_t *flags, const char *data, size_t size) {
  dFUZZ;
  SV *enc = jx_object(flags, &flags->enc);
  SV *dec = jx_object(flags, &flags->dec);
  CHECK(enc && dec);

  SV *types = NULL;
  SV *v = build_value(flags, data, size, &types);
  if (!types)
    types = jx_typespec(FLAGS_AUX(flags, AUX_TYPESPEC, 4));
  jx_call("JXF::t_encode", false, G_DISCARD, NULL, 5, enc, dec, v,
          sv_2mortal(newSVuv(FLAGS_AUX(flags, AUX_ENCODE_STEPS, 6))), types);
out:
  return;
}

/* ── groups ────────────────────────────────────────────────────────────────── */

/* JSON text: decode, decode_prefix, the incremental parser, the functional interface */
void test_json(struct flags_t *flags, const char *data, size_t size) {
  T(test_decode);
  T(test_decode_prefix);
  T(test_incr);
  T(test_functional);
}

/* a builder program: the encoder, on perl values JSON text cannot express */
void test_build(struct flags_t *flags, const char *data, size_t size) {
  T(test_encode);
}

/*
 * One throwaway exec per group before the fork server starts. Every
 * first-time path then belongs to no exec at all: the TC_SKIP()/G() name
 * caches, perl's method caches and lazily created globals, libc's stdio
 * buffers and locale data. Without it the first input of every child took
 * edges no later one did.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void warmup_exec(void)
{
  static const uint8_t json[] = { 0, 0, 0, 0, '{', '"', 'a', '"', ':', '[', '1', ',', 't', 'r', 'u', 'e', ']', '}' };
  static const uint8_t build[] = { 0, 0, 0, 0, BOP_BEGIN_HASH, BOP_KEY, 1, 'a', BOP_INT8, 1, BOP_TRUE, BOP_END };

  LLVMFuzzerTestOneInput(json, sizeof(json));
  LLVMFuzzerTestOneInput(build, sizeof(build));
}

/*
 * Called by libAFLDriver once, before __afl_manual_init() starts the deferred
 * fork server, so the interpreter, Cpanel::JSON::XS's BOOT: section and fixture.pl
 * all run once in the parent instead of once per forked child.
 */
/*
 * Memory cap for the uninstrumented builds (fast, laf, cmplog, cov):
 * RLIMIT_AS, set here so the fork server and every child inherit it. perl
 * treats a NULL from malloc as "Out of memory!" and exits - a clean exit, not
 * a crash. afl-fuzz -m cannot do this: SAND oracles (-w) copy the lane's limit,
 * and the sanitizer runtimes reserve terabytes of address space (shadow,
 * allocator region) at startup. A sanitizer runtime is detected by its
 * allocator interface; those builds are capped by max_allocation_size_mb in
 * ASAN/MSAN/LSAN_OPTIONS instead (fuzzer/lib/common.sh).
 * FUZZ_MEM_MB overrides the default; 0 disables the cap.
 */
#define FUZZ_MEM_MB_DEFAULT 2048
extern size_t __sanitizer_get_current_allocated_bytes(void) __attribute__((weak));

static void set_mem_limit(void) {
  if (__sanitizer_get_current_allocated_bytes)
    return;
  const char *env = getenv("FUZZ_MEM_MB");
  unsigned long mb = env ? strtoul(env, NULL, 10) : FUZZ_MEM_MB_DEFAULT;
  if (!mb)
    return;
  struct rlimit rl = { .rlim_cur = (rlim_t)mb << 20, .rlim_max = (rlim_t)mb << 20 };
  if (setrlimit(RLIMIT_AS, &rl))
    perror("setrlimit(RLIMIT_AS)");
}

int LLVMFuzzerInitialize(int *argc, char ***argv) {
  (void)argc;
  (void)argv;

  set_mem_limit();
  perl_setup();
  warmup_exec();
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size <= sizeof(uint32_t)) return 0;

  struct flags_t _flags = {0};
  struct flags_t *flags = &_flags;
  uint32_t seed;
  memcpy(&seed, data, sizeof(seed));
  setup_flags(seed, flags);
  if (verbose)
    print_flags(flags);
  data += sizeof(uint32_t);
  size -= sizeof(uint32_t);

  G(test_json);
  G(test_build);

  if (__builtin_expect(tc_filter != NULL, 0) && !tc_filter_matched) {
    fprintf(stderr, "TC=%s does not name any test case\n", tc_filter);
    exit(1);
  }

  /*
   * Per-input malloc() leak check, in the AFL_USE_LSAN build only: afl-cc
   * defines __AFL_LEAK_CHECK() there (a recoverable LSAN pass, _exit(23) on a
   * leak, which AFL saves as a crash) and nowhere else. Perl-level leaks are
   * the SV leak check's (fuzz_perl.h): a leaked SV stays reachable from perl's
   * arenas, so LSAN only ever sees leaked buffers that nothing points to.
   */
#ifdef __AFL_LEAK_CHECK
  __AFL_LEAK_CHECK();
#endif

  return 0;
}

#if defined REPRODUCER


#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)


#define RR(nr,artifact_path) R(nr,artifact_path,read_artifact_raw)
#define RA(nr,artifact_path) R(nr,artifact_path,read_artifact_raw)

#define R(nr,artifact_path,read_artifact_func) \
int CONCAT(vc, nr)(void) { \
  char *data = NULL; \
  size_t n = read_artifact_func(artifact_path, &data); \
  if (n > sizeof(uint32_t) && data) { \
    struct flags_t _flags = {0}; \
    struct flags_t *flags = &_flags; \
    uint32_t seed; \
    memcpy(&seed, data, sizeof(seed)); \
    setup_flags(seed, flags); \
    char* flags_t_str = flags_to_struct_string(flags); \
    printf("RF(%d,\ntest_func,\n(&(struct flags_t)%s),\n\"", nr, flags_t_str); \
    size_t data_size = n - sizeof(uint32_t); \
    char *abuf = malloc(data_size * 4 + 1); \
    sprintf_artifact_as_hexstr(abuf, data_size * 4 + 1, data + sizeof(uint32_t), data_size); \
    printf("%s\",\n%zu\n)\n", abuf, data_size); \
    printf("\n\n"); \
    fflush(stdout); \
    free(abuf); \
    LLVMFuzzerTestOneInput((const uint8_t *)data, n); \
    munmap(data, n); \
  } \
  return n > 0; \
}

/* runs one test case through T_RUN(): its perl scope and, with
 * FUZZ_SV_LEAK_CHECK=1, the SV leak check */
#define RF(nr,func,flags,data,n) \
int CONCAT(tc, nr)(void) { \
  struct flags_t *_rf_flags = (flags); \
  T_RUN(func, func(_rf_flags, data, n)); \
  return 0; \
}

/* ── RA()/RF() pairs, one per reportN.md (see .claude/CLAUDE.md) ───────────── */

int main(int argc, char **argv) {
  return fuzz_replay_main(argc, argv);
}

#endif /* REPRODUCER */
