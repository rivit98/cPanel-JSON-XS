/*
 * The embedded perl interpreter: setup, the Cpanel::JSON::XS objects a test
 * case uses, the input scalar, calls into fixture.pl, and the per-test-case
 * SV leak check.
 *
 * ── one interpreter, many execs ───────────────────────────────────────────────
 * perl_setup() runs from LLVMFuzzerInitialize(), before libAFLDriver starts
 * the deferred fork server: the interpreter is constructed, Cpanel::JSON::XS is
 * loaded (its BOOT: section runs) and fixture.pl is compiled once, in the parent.
 * Every forked child inherits all of it, and the persistent loop runs
 * thousands of inputs through the same interpreter. A test case therefore
 * runs inside ENTER/SAVETMPS ... FREETMPS/LEAVE (T_RUN() in fuzz_utils.h):
 * every scalar it creates is mortal, and nothing survives into the next exec.
 *
 * ── Cpanel::JSON::XS is linked statically ─────────────────────────────────────
 * XS.c and utf8_range.c are compiled into this binary, and xs_init() registers
 * the boot function as Cpanel::JSON::XS::bootstrap - XSLoader::load() jumps to
 * an already defined bootstrap instead of looking for a shared object. XS.pm,
 * XS/Boolean.pm and XS/Type.pm are loaded unchanged from the build tree's
 * perl5/ (FUZZ_PERL5LIB, CMakeLists.txt).
 *
 * ── determinism ───────────────────────────────────────────────────────────────
 * Perl randomises its hash function per process and the key order per hash.
 * A forked child would see the parent's seed, but the per-hash perturbation
 * still makes key order - which the non-canonical encoder walks - vary
 * between two runs of one input. PERL_HASH_SEED=0 and PERL_PERTURB_KEYS=0
 * switch both off before the interpreter exists.
 *
 * ── allocation failures are not injected ──────────────────────────────────────
 * libfyaml's harness fails the nth malloc() of a test case. Perl has no
 * out-of-memory path to fuzz: every allocation goes through safesysmalloc(),
 * which on NULL prints "Out of memory!" and exits the process - by design, in
 * libperl, not in Cpanel::JSON::XS. Failing one would only measure that exit.
 */

#ifndef FUZZ_PERL_H
#define FUZZ_PERL_H

#define PERL_NO_GET_CONTEXT
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include "fuzz_flags.h"
#include "fixture_pl.h"

static PerlInterpreter *fuzz_perl;
#define dFUZZ PerlInterpreter *my_perl = fuzz_perl

EXTERN_C void boot_DynaLoader(pTHX_ CV *cv);
EXTERN_C void boot_Cpanel__JSON__XS(pTHX_ CV *cv);

static void xs_init(pTHX) {
  /* DynaLoader for any XS module the fixture's core modules pull in */
  newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, __FILE__);
  newXS("Cpanel::JSON::XS::bootstrap", boot_Cpanel__JSON__XS, __FILE__);
}

/* fixture tables, resolved once */
static AV *jxf_obj_cb, *jxf_sk_keys, *jxf_sk_cb, *jxf_sort_by, *jxf_types;
static SV *jxf_class, *jxf_subclass;

static void perl_die_setup(const char *what) {
  dFUZZ;
  fprintf(stderr, "perl_setup: %s: %s\n", what, SvPV_nolen(ERRSV));
  exit(1);
}

static AV *fixture_av(const char *name, I32 want) {
  dFUZZ;
  AV *av = get_av(name, 0);

  if (!av || av_count(av) != (Size_t)want) {
    fprintf(stderr, "perl_setup: @%s has %ld entries, fuzz_flags.h expects %d\n",
            name, av ? (long)av_count(av) : -1L, (int)want);
    exit(1);
  }
  return av;
}

static void perl_setup(void) {
  static char *embedding[] = { "fuzz", "-e", "0", NULL };
  static int argc = 3;
  static char **argv = embedding;
  static char *envp_empty[] = { NULL };
  char **envp = envp_empty;

  if (fuzz_perl)
    return;

  setenv("PERL_HASH_SEED", "0", 1);
  setenv("PERL_PERTURB_KEYS", "0", 1);

  PERL_SYS_INIT3(&argc, &argv, &envp);
  fuzz_perl = perl_alloc();
  perl_construct(fuzz_perl);
  {
    dFUZZ;
    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
    PL_perl_destruct_level = 0;
    if (perl_parse(fuzz_perl, xs_init, argc, argv, NULL) != 0) {
      fprintf(stderr, "perl_setup: perl_parse failed\n");
      exit(1);
    }
    perl_run(fuzz_perl);

    eval_pv("unshift @INC, '" FUZZ_PERL5LIB "'; 1", FALSE);
    if (SvTRUE(ERRSV))
      perl_die_setup("@INC");
    eval_pv(fixture_pl, FALSE);
    if (SvTRUE(ERRSV))
      perl_die_setup("fixture.pl");

    jxf_obj_cb  = fixture_av("JXF::obj_cb",  JXF_OBJ_CB);
    jxf_sk_keys = fixture_av("JXF::sk_keys", JXF_SK_KEYS);
    jxf_sk_cb   = fixture_av("JXF::sk_cb",   JXF_SK_CB);
    jxf_sort_by = fixture_av("JXF::sort_by", JXF_SORT_BY);
    jxf_types   = fixture_av("JXF::types",   JXF_TYPES);
    jxf_class   = newSVpvs("Cpanel::JSON::XS");
    SvREADONLY_on(jxf_class);
    jxf_subclass = newSVpvs("JXF::Sub");
    SvREADONLY_on(jxf_subclass);
  }
}

/* ── calls ─────────────────────────────────────────────────────────────────── */

/*
 * Call a method or sub with G_EVAL. Returns the number of values left on the
 * stack (0 on a die); with want == G_SCALAR and a result, *out gets it,
 * owned by the caller's mortal scope.
 */
static int jx_call(const char *name, bool method, I32 want, SV **out, int nargs, ...) {
  dFUZZ;
  dSP;
  va_list ap;
  int count;

  PUSHMARK(SP);
  EXTEND(SP, nargs);
  va_start(ap, nargs);
  for (int i = 0; i < nargs; i++)
    PUSHs(va_arg(ap, SV *));
  va_end(ap);
  PUTBACK;

  count = method ? call_method(name, want | G_EVAL) : call_pv(name, want | G_EVAL);

  SPAGAIN;
  if (SvTRUE(ERRSV)) {
    if (verbose)
      fprintf(stderr, "    %s: %s", name, SvPV_nolen(ERRSV));
    SP -= count;
    count = 0;
  } else {
    /* still alive after the pop: a mortal, or owned by something longer-lived */
    if (out && count > 0)
      *out = *(SP - count + 1);
    SP -= count;
  }
  PUTBACK;
  return count;
}

#define JX_METHOD(obj, name, ...) \
  jx_call(name, true, G_DISCARD, NULL, (int)(sizeof((SV *[]){ obj, __VA_ARGS__ }) / sizeof(SV *)), obj, __VA_ARGS__)

/* ── objects ───────────────────────────────────────────────────────────────── */

static SV *av_at(AV *av, size_t i) {
  dFUZZ;
  SV **svp = av_fetch(av, (SSize_t)i, 0);
  return svp ? *svp : &PL_sv_undef;
}

static const char *jx_getter_names[] = {
  "get_ascii", "get_latin1", "get_binary", "get_utf8", "get_indent", "get_canonical",
  "get_space_before", "get_space_after", "get_allow_nonref", "get_shrink",
  "get_allow_blessed", "get_convert_blessed", "get_relaxed", "get_allow_unknown",
  "get_allow_tags", "get_allow_barekey", "get_allow_singlequote", "get_allow_bignum",
  "get_escape_slash", "get_allow_stringify", "get_unblessed_bool", "get_allow_dupkeys",
  "get_require_types", "get_type_all_string", "get_dupkeys_as_arrayref",
  "get_indent_length", "get_max_depth", "get_max_size", "get_stringify_infnan",
};

/* the option bits a setter changes besides its own (the `ascii` ALIAS PPCODE in XS.xs) */
static uint32_t jx_option_effect(size_t i, bool enable) {
  const char *name = jx_option_names[i];
  uint32_t bit = 1u << i;

  if (!strcmp(name, "relaxed")) {
    /* SET_RELAXED: relaxed | allow_barekey | allow_singlequote | allow_dupkeys */
    for (size_t j = 0; j < array_elements(jx_option_names); j++)
      if (!strcmp(jx_option_names[j], "allow_barekey") ||
          !strcmp(jx_option_names[j], "allow_singlequote") ||
          !strcmp(jx_option_names[j], "allow_dupkeys"))
        bit |= 1u << j;
  } else if (!strcmp(name, "dupkeys_as_arrayref") && enable) {
    for (size_t j = 0; j < array_elements(jx_option_names); j++)
      if (!strcmp(jx_option_names[j], "allow_dupkeys"))
        bit |= 1u << j;
  }
  return bit;
}

/*
 * Cpanel::JSON::XS->new (or JXF::Sub->new, a subclass), configured by @c
 * through its setters - the XS glue (ALIAS dispatch, typemap checks) is part
 * of what is fuzzed, so nothing here pokes the JSON struct directly. Returns a
 * mortal reference, or NULL.
 *
 * Every setter whose option is not yet in the wanted state is called, in
 * jx_option_names order, tracking the bits each call changes as a side
 * effect (relaxed, dupkeys_as_arrayref), so the object ends up with exactly
 * c->options - except that dupkeys_as_arrayref implies allow_dupkeys.
 */
static SV *jx_object(const struct flags_t *flags, const struct jx_cfg_t *c) {
  dFUZZ;
  SV *obj = NULL;
  uint32_t have = JXO_DEFAULT;

  if (jx_call("new", true, G_SCALAR, &obj, 1, c->subclass ? jxf_subclass : jxf_class) != 1 ||
      !obj || !SvROK(obj))
    return NULL;

  if (c->pretty) {
    JX_METHOD(obj, "pretty", &PL_sv_yes);
    for (size_t i = 0; i < array_elements(jx_option_names); i++)
      if (!strcmp(jx_option_names[i], "indent") ||
          !strcmp(jx_option_names[i], "space_before") ||
          !strcmp(jx_option_names[i], "space_after"))
        have |= 1u << i;
  }

  for (size_t i = 0; i < array_elements(jx_option_names); i++) {
    uint32_t bit = 1u << i;
    bool want = c->options & bit;

    if (want == !!(have & bit) && !(bit == JXO_ALLOW_NONREF && c->nonref_set))
      continue;
    JX_METHOD(obj, jx_option_names[i], want ? &PL_sv_yes : &PL_sv_no);
    if (want)
      have |= jx_option_effect(i, true);
    else
      have &= ~jx_option_effect(i, false);
  }

  if (c->max_depth)
    JX_METHOD(obj, "max_depth", sv_2mortal(newSVuv(c->max_depth == JX_MAX_DEPTH_ZERO ? 0 : c->max_depth)));
  if (c->max_size)
    JX_METHOD(obj, "max_size", sv_2mortal(newSVuv(c->max_size)));
  if (c->indent_length)
    JX_METHOD(obj, "indent_length", sv_2mortal(newSViv(jx_indent_lengths[c->indent_length - 1])));
  if (c->infnan)
    JX_METHOD(obj, "stringify_infnan", sv_2mortal(newSViv((IV)c->infnan - 1)));
  if (c->sort_by)
    JX_METHOD(obj, "sort_by", av_at(jxf_sort_by, c->sort_by - 1));

  if (c->filter_object)
    JX_METHOD(obj, "filter_json_object", av_at(jxf_obj_cb, c->filter_object - 1));
  for (unsigned int i = 0; i < JXF_SK_KEYS; i++)
    if (c->filter_sk_keys & (1u << i))
      JX_METHOD(obj, "filter_json_single_key_object", av_at(jxf_sk_keys, i),
                av_at(jxf_sk_cb, (c->filter_sk_cb + i) % JXF_SK_CB));

  {
    unsigned int g = FLAGS_AUX(flags, AUX_GETTER, 5) % array_elements(jx_getter_names);
    jx_call(jx_getter_names[g], true, G_DISCARD, NULL, 1, obj);
  }
  return obj;
}

/* @JXF::types[i - 1], or undef for 0: an encode type spec (fixture.pl) */
static SV *jx_typespec(unsigned int i) {
  dFUZZ;
  return i ? av_at(jxf_types, (i - 1) % JXF_TYPES) : &PL_sv_undef;
}

/* ── the input scalar ──────────────────────────────────────────────────────── */

/* A mortal scalar holding @data the way flags->input_kind says (fuzz_flags.h). */
static SV *make_input_sv(const struct flags_t *flags, const char *data, size_t size) {
  dFUZZ;
  SV *sv = sv_2mortal(newSVpvn(data, size));

  switch (flags->input_kind) {
  case 0:
    break;
  case 1:
    sv_utf8_upgrade(sv);
    break;
  case 2:
    sv_utf8_decode(sv);
    break;
  case 3: {
    SV *ref = NULL;
    if (jx_call("JXF::tie_scalar", false, G_SCALAR, &ref, 1, sv) == 1 && ref && SvROK(ref))
      sv = SvRV(ref);   /* the tied scalar itself, kept alive by the mortal ref */
    break;
  }
  case 4: {
    /* a hash key scalar: SvIsCOW_shared_hash, which decode() has to copy, not modify in place */
    HV *hv = (HV *)sv_2mortal((SV *)newHV());
    HE *he;
    hv_store(hv, data, (I32)(size > I32_MAX ? I32_MAX : size), newSV(0), 0);
    hv_iterinit(hv);
    he = hv_iternext(hv);
    if (he)
      sv = sv_2mortal(newSVhek(HeKEY_hek(he)));
    break;
  }
  case 5:
    (void)SvNV(sv);   /* numeric use: pIOK/pNOK on top of the PV */
    (void)SvIV(sv);
    break;
  }
  return sv;
}

/* ── per-test-case SV leak check ───────────────────────────────────────────── */

/*
 * Perl's analogue of __AFL_LEAK_CHECK(). The SVs a leak loses are not lost
 * to LSAN: they sit in perl's SV arenas, which stay reachable, so a refcount
 * leak in Cpanel::JSON::XS never shows up as a malloc() leak. What does show it is
 * PL_sv_count, perl's live-SV counter (the one behind "Scalars leaked: N" at
 * destruction): after FREETMPS/LEAVE a test case should leave it where it
 * found it.
 *
 * A first run can legitimately leave SVs behind - perl fills method caches
 * and the like on first use - so a test case that grew the count runs again,
 * up to SV_LEAK_RERUNS times, and only growth on every rerun is a leak. Then:
 * a report on stderr and abort(), which AFL saves as a crash.
 *
 * One rerun is not enough here. With a JSON_TYPE_INT/FLOAT/STRING type spec
 * the encoder numifies or stringifies a reference - its heap address - so the
 * output text, and what decoding it again touches for the first time
 * (Math::BigInt/BigFloat's lazily built state under allow_bignum, say), can
 * change between two runs of one input. A real leak grows the count on every
 * run; a cache filled late does not grow it three times in a row.
 *
 * On in the lsan variant (FUZZ_SV_LEAK_CHECK, build.sh) and wherever
 * FUZZ_SV_LEAK_CHECK=1 is in the environment - triage replays included.
 */
static bool sv_leak_check;
#define SV_LEAK_RERUNS 3

static inline I32 sv_count_now(void) {
  dFUZZ;
  return (I32)PL_sv_count;
}

#endif /* FUZZ_PERL_H */
