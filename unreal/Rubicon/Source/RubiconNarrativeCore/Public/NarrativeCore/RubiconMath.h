// Deterministic numeric primitives. Port of src/prng.ts plus the two transcendental functions the core uses.
//
// Exp and Pow are bit-exact ports of V8's base::ieee754::exp and base::ieee754::pow (fdlibm / FreeBSD msun
// lineage), so C++ reproduces Math.exp and Math.pow exactly on every platform. The host libm is never used
// for simulation math: glibc, MSVC and Bionic each round differently (about 8% of inputs differ from V8).

#pragma once

#include <cstdint>
#include <string>

namespace RubiconCore
{
	struct FRngDraw
	{
		double Value; // [0,1)
		uint32_t State;
	};

	/** mulberry32: single u32 state, bit-identical to src/prng.ts rngNext. */
	FRngDraw RngNext(uint32_t State);

	double Exp(double X);
	double Pow(double X, double Y);

	double Clamp01(double X);
	double Clamp(double X, double Lo, double Hi);
	double Lerp(double A, double B, double T);
	/** Exact first-order step: Current + (Target - Current) * (1 - exp(-DtMs / TauMs)). */
	double SmoothToward(double Current, double Target, double DtMs, double TauMs);
	double Smoothstep(double Edge0, double Edge1, double X);

	/**
	 * Startup guard for the floating-point environment. Recomputes Exp/Pow on inputs whose results change
	 * under FMA contraction or fast-math (clang -ffp-contract=fast ignores the core's pragmas) and compares
	 * them with V8's bit patterns. Returns false and describes the first mismatch if replay parity is broken.
	 */
	bool SelfTestDeterministicMath(std::string* OutFailure = nullptr);

	/** FNV-1a 32-bit over UTF-16 code units of a UTF-8 string, as 8 lowercase hex digits (src/prng.ts fnv1a). */
	std::string Fnv1a(const std::string& Utf8);
}
