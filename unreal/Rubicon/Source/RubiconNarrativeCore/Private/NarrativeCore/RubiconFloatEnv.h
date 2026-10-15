// Floating-point environment for the narrative core.
//
// Parity with the TypeScript reference (V8, IEEE-754 binary64, round-to-nearest, no contraction) requires:
//   - no fused multiply-add contraction (a*b+c must round twice, as in JavaScript)
//   - no fast-math reassociation
// Include this first in every NarrativeCore .cpp. Floating-point helpers live in .cpp files (never inline in
// headers) so a translation unit compiled without these pragmas cannot supply an ODR-merged copy.

#pragma once

#if defined(_MSC_VER) && !defined(__clang__)
	#pragma float_control(precise, on)
	#pragma fp_contract(off)
#elif defined(__clang__)
	#pragma STDC FP_CONTRACT OFF
	#pragma clang fp contract(off)
#elif defined(__GNUC__)
	// GCC ignores STDC FP_CONTRACT; the standalone harness passes -ffp-contract=off (native/CMakeLists.txt).
#endif

#if defined(__FAST_MATH__)
	#error "RubiconNarrativeCore must not be compiled with -ffast-math (breaks replay parity)"
#endif
