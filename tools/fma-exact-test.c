// The module's fma() (module-template/host_fma.c) against the FMA3 instruction: every result's bits
// and every MXCSR flag, in all four rounding modes, with and without denormals-are-zero / flush-to-zero.
// Single-precision-shaped operands (the exact path), any doubles, edge exponents, zeros, denormals,
// infinities and NaNs. Needs an FMA3 CPU for the reference; MODERNGEKKO_NO_FMA3=1 tests the no-FMA3 path.
//
//   gcc -O2 -mfma -fno-builtin-fma -c tools/fma-exact-test.c -o build/fma-exact-test.o
//   clang -O2 -fno-builtin-fma -c lib/ModernGekko/vendor/dolphin/module-template/host_fma.c -o build/host_fma.o
//   gcc build/fma-exact-test.o build/host_fma.o -o build/fma-exact-test -ldl
//   MODERNGEKKO_NO_FMA3=1 build/fma-exact-test [CASES]   (exit 0: no difference)
//
// Build this file with GCC: clang moves _mm_getcsr across the reference's intrinsic (no FENV_ACCESS for
// intrinsics), so the reference's flags come out wrong. host_fma.o can come from either compiler.

#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double fma(double, double, double);

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint64_t next(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
}
static double from_bits(uint64_t u)
{
    double d;
    memcpy(&d, &u, 8);
    return d;
}
static uint64_t bits_of(double d)
{
    uint64_t u;
    memcpy(&u, &d, 8);
    return u;
}
static double reference(double a, double b, double c)
{
    return _mm_cvtsd_f64(_mm_fmadd_sd(_mm_set_sd(a), _mm_set_sd(b), _mm_set_sd(c)));
}
// random sign, biased exponent in [emin, emax], `significant` significant bits
static double shaped(int significant, int emin, int emax)
{
    uint64_t mantissa = next() & ((1ull << 52) - 1);
    mantissa &= ~((1ull << (53 - significant)) - 1);
    const int exponent = emin + (int)(next() % (uint64_t)(emax - emin + 1));
    return from_bits((next() & 1ull) << 63 | (uint64_t)exponent << 52 | mantissa);
}
static double special(void)
{
    static const uint64_t values[] = {
        0, 1ull << 63, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000000ull,
        0x7ff4000000000001ull, 0x0000000000000001ull, 0x000fffffffffffffull, 0x0010000000000000ull,
        0x7fefffffffffffffull, 0x3ff0000000000000ull,
        0x3810000000000000ull,  // FLT_MIN
        0x47efffffe0000000ull,  // FLT_MAX
        0x36a0000000000000ull,  // the smallest float denormal
    };
    return from_bits(values[next() % (sizeof values / sizeof values[0])]);
}
// multiplier: the 25-bit rounded ps_madd multiplier; otherwise a 24-bit float
static double operand(int multiplier)
{
    switch (next() % 8)
    {
    case 0: return special();
    case 1: return from_bits(next());
    case 2: return shaped(53, 1, 2046);
    case 3: return shaped(26, 1, 2046);  // the exact path's shape at any exponent (its range limits)
    default:
        return multiplier ? shaped(25, 1023 - 126, 1023 + 127) : shaped(24, 1023 - 149, 1023 + 127);
    }
}

int main(int argc, char** argv)
{
    const long cases = argc > 1 ? atol(argv[1]) : 10000000;
    static const unsigned rounding[] = {0x0000, 0x2000, 0x4000, 0x6000};  // MXCSR RC: near, down, up, zero
    long total_bad = 0;
    for (int daz = 0; daz < 2; daz++)
    {
        for (int mode = 0; mode < 4; mode++)
        {
            long bad = 0;
            for (long i = 0; i < cases; i++)
            {
                const double a = operand(0), b = operand(1);
                const double c = (next() & 3) ? operand(0) : shaped(53, 1, 2046);
                const unsigned csr = (_mm_getcsr() & ~0xe07fu) | rounding[mode] | (daz ? 0x8040u : 0);
                _mm_setcsr(csr);
                const double want = reference(a, b, c);
                const unsigned want_flags = _mm_getcsr() & 0x3f;
                _mm_setcsr(csr);
                const double got = fma(a, b, c);
                const unsigned got_flags = _mm_getcsr() & 0x3f;
                _mm_setcsr(csr & ~0x3fu);
                // A NaN result may be either NaN input (it depends on the instruction's operand order,
                // the C runtime's too); the callers replace it with the PowerPC's choice
                // (ppc_fma, DolRecomp's preferPairedFMANaN), so only "a NaN" has to match.
                const int both_nan = want != want && got != got;
                if ((bits_of(want) != bits_of(got) && !both_nan) || want_flags != got_flags)
                {
                    if (bad < 5)
                        printf("  a=%016llx b=%016llx c=%016llx: %016llx flags %02x, got %016llx flags %02x\n",
                               (unsigned long long)bits_of(a), (unsigned long long)bits_of(b),
                               (unsigned long long)bits_of(c), (unsigned long long)bits_of(want), want_flags,
                               (unsigned long long)bits_of(got), got_flags);
                    bad++;
                }
            }
            printf("daz/ftz %d, rounding %d: %ld of %ld differ\n", daz, mode, bad, cases);
            total_bad += bad;
        }
    }
    printf("%ld differ\n", total_bad);
    return total_bad != 0;
}
