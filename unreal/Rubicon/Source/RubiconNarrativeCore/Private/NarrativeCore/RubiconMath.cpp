#include "RubiconFloatEnv.h"
#include "NarrativeCore/RubiconMath.h"

#include <cmath>
#include <cstring>

namespace RubiconCore
{
	namespace MathDetail
	{
		inline uint32_t HighWord(double X) { uint64_t B; std::memcpy(&B, &X, 8); return static_cast<uint32_t>(B >> 32); }
		inline uint32_t LowWord(double X) { uint64_t B; std::memcpy(&B, &X, 8); return static_cast<uint32_t>(B); }
		inline double FromWords(uint32_t Hi, uint32_t Lo)
		{
			const uint64_t B = (static_cast<uint64_t>(Hi) << 32) | Lo;
			double X;
			std::memcpy(&X, &B, 8);
			return X;
		}
		inline void SetHighWord(double& X, uint32_t Hi) { X = FromWords(Hi, LowWord(X)); }
		inline void SetLowWord(double& X, uint32_t Lo) { X = FromWords(HighWord(X), Lo); }
	}

	FRngDraw RngNext(uint32_t State)
	{
		const uint32_t S = State + 0x6d2b79f5u;
		uint32_t Z = S;
		Z = (Z ^ (Z >> 15)) * (Z | 1u);
		Z ^= Z + (Z ^ (Z >> 7)) * (Z | 61u);
		const uint32_t Out = Z ^ (Z >> 14);
		return { static_cast<double>(Out) / 4294967296.0, S };
	}

	// V8 base::ieee754::exp (FreeBSD msun e_exp.c).
	double Exp(double X)
	{
		using namespace MathDetail;
		static const double One = 1.0, HalF[2] = { 0.5, -0.5 }, Huge = 1.0e+300,
			OThreshold = 7.09782712893383973096e+02, UThreshold = -7.45133219101941108420e+02,
			Ln2Hi[2] = { 6.93147180369123816490e-01, -6.93147180369123816490e-01 },
			Ln2Lo[2] = { 1.90821492927058770002e-10, -1.90821492927058770002e-10 },
			InvLn2 = 1.44269504088896338700e+00,
			P1 = 1.66666666666666019037e-01, P2 = -2.77777777770155933842e-03, P3 = 6.61375632143793436117e-05,
			P4 = -1.65339022054652515390e-06, P5 = 4.13813679705723846039e-08,
			TwoM1000 = 9.33263618503218878990e-302;

		double Hi = 0, Lo = 0, C, T, TwoPk;
		int K = 0;
		uint32_t Hx = HighWord(X);
		const int Xsb = static_cast<int>((Hx >> 31) & 1);
		Hx &= 0x7fffffff;

		if (Hx >= 0x40862E42)
		{
			if (Hx >= 0x7ff00000)
			{
				if (((Hx & 0xfffff) | LowWord(X)) != 0) return X + X;
				return Xsb == 0 ? X : 0.0;
			}
			if (X > OThreshold) return Huge * Huge;
			if (X < UThreshold) return TwoM1000 * TwoM1000;
		}

		if (Hx > 0x3fd62e42)
		{
			if (Hx < 0x3FF0A2B2)
			{
				Hi = X - Ln2Hi[Xsb];
				Lo = Ln2Lo[Xsb];
				K = 1 - Xsb - Xsb;
			}
			else
			{
				K = static_cast<int>(InvLn2 * X + HalF[Xsb]);
				T = K;
				Hi = X - T * Ln2Hi[0];
				Lo = T * Ln2Lo[0];
			}
			X = Hi - Lo;
		}
		else if (Hx < 0x3e300000)
		{
			if (Huge + X > One) return One + X;
		}
		else
		{
			K = 0;
		}

		T = X * X;
		if (K >= -1021) TwoPk = FromWords(0x3ff00000u + (static_cast<uint32_t>(K) << 20), 0);
		else TwoPk = FromWords(0x3ff00000u + (static_cast<uint32_t>(K + 1000) << 20), 0);
		C = X - T * (P1 + T * (P2 + T * (P3 + T * (P4 + T * P5))));
		if (K == 0) return One - ((X * C) / (C - 2.0) - X);
		const double Y = One - ((Lo - (X * C) / (2.0 - C)) - Hi);
		if (K >= -1021)
		{
			if (K == 1024) return Y * 2.0 * FromWords(0x7fe00000, 0);
			return Y * TwoPk;
		}
		return Y * TwoPk * TwoM1000;
	}

	// V8 base::ieee754::pow (fdlibm e_pow.c). V8 differs from fdlibm in one expression, reproduced here:
	//   fdlibm: r = (z*t1)/(t1-two) - (w+z*w)
	//   V8:     r = (z*t1)/((t1-two) - (w+z*w))
	// Verified bit-exact against Node 22 Math.pow on 3M inputs (native/README.md).
	double Pow(double X, double Y)
	{
		using namespace MathDetail;
		static const double Bp[] = { 1.0, 1.5 }, DpH[] = { 0.0, 5.84962487220764160156e-01 },
			DpL[] = { 0.0, 1.35003920212974897128e-08 },
			Zero = 0.0, Half = 0.5, Qrtr = 0.25, Thrd = 3.3333333333333331e-01, One = 1.0, Two = 2.0,
			Two53 = 9007199254740992.0, Huge = 1.0e300, Tiny = 1.0e-300,
			L1 = 5.99999999999994648725e-01, L2 = 4.28571428578550184252e-01, L3 = 3.33333329818377432918e-01,
			L4 = 2.72728123808534006489e-01, L5 = 2.30660745775561754067e-01, L6 = 2.06975017800338417784e-01,
			P1 = 1.66666666666666019037e-01, P2 = -2.77777777770155933842e-03, P3 = 6.61375632143793436117e-05,
			P4 = -1.65339022054652515390e-06, P5 = 4.13813679705723846039e-08,
			Lg2 = 6.93147180559945286227e-01, Lg2H = 6.93147182464599609375e-01, Lg2L = -1.90465429995776804525e-09,
			Ovt = 8.0085662595372944372e-17,
			Cp = 9.61796693925975554329e-01, CpH = 9.61796700954437255859e-01, CpL = -7.02846165095275826516e-09,
			IvLn2 = 1.44269504088896338700e+00, IvLn2H = 1.44269502162933349609e+00, IvLn2L = 1.92596299112661746887e-08;

		double Z, Ax, ZH, ZL, PH, PL, Y1, T1, T2, R, S, T, U, V, W;
		int32_t I, J, K, YIsInt, N;
		const int32_t Hx = static_cast<int32_t>(HighWord(X));
		const uint32_t Lx = LowWord(X);
		const int32_t Hy = static_cast<int32_t>(HighWord(Y));
		const uint32_t Ly = LowWord(Y);
		int32_t Ix = Hx & 0x7fffffff;
		const int32_t Iy = Hy & 0x7fffffff;

		if ((static_cast<uint32_t>(Iy) | Ly) == 0) return One;
		if (Hx == 0x3ff00000 && Lx == 0) return One;
		if (Ix > 0x7ff00000 || (Ix == 0x7ff00000 && Lx != 0) || Iy > 0x7ff00000 || (Iy == 0x7ff00000 && Ly != 0)) return X + Y;

		YIsInt = 0;
		if (Hx < 0)
		{
			if (Iy >= 0x43400000) YIsInt = 2;
			else if (Iy >= 0x3ff00000)
			{
				K = (Iy >> 20) - 0x3ff;
				if (K > 20)
				{
					const uint32_t Jj = Ly >> (52 - K);
					if ((Jj << (52 - K)) == Ly) YIsInt = 2 - static_cast<int32_t>(Jj & 1);
				}
				else if (Ly == 0)
				{
					J = Iy >> (20 - K);
					if ((J << (20 - K)) == Iy) YIsInt = 2 - (J & 1);
				}
			}
		}

		if (Ly == 0)
		{
			if (Iy == 0x7ff00000)
			{
				if (((Ix - 0x3ff00000) | static_cast<int32_t>(Lx)) == 0) return One;
				if (Ix >= 0x3ff00000) return Hy >= 0 ? Y : Zero;
				return Hy < 0 ? -Y : Zero;
			}
			if (Iy == 0x3ff00000) return Hy < 0 ? One / X : X;
			if (Hy == 0x40000000) return X * X;
			if (Hy == 0x3fe00000 && Hx >= 0) return std::sqrt(X); // sqrt is correctly rounded everywhere
		}

		Ax = std::fabs(X);
		if (Lx == 0 && (Ix == 0x7ff00000 || Ix == 0 || Ix == 0x3ff00000))
		{
			Z = Ax;
			if (Hy < 0) Z = One / Z;
			if (Hx < 0)
			{
				if (((Ix - 0x3ff00000) | YIsInt) == 0) Z = (Z - Z) / (Z - Z);
				else if (YIsInt == 1) Z = -Z;
			}
			return Z;
		}

		N = static_cast<int32_t>(static_cast<uint32_t>(Hx) >> 31) - 1;
		if ((N | YIsInt) == 0) return (X - X) / (X - X);

		S = One;
		if ((N | (YIsInt - 1)) == 0) S = -One;

		if (Iy > 0x41e00000)
		{
			if (Iy > 0x43f00000)
			{
				if (Ix <= 0x3fefffff) return Hy < 0 ? Huge * Huge : Tiny * Tiny;
				if (Ix >= 0x3ff00000) return Hy > 0 ? Huge * Huge : Tiny * Tiny;
			}
			if (Ix < 0x3fefffff) return Hy < 0 ? S * Huge * Huge : S * Tiny * Tiny;
			if (Ix > 0x3ff00000) return Hy > 0 ? S * Huge * Huge : S * Tiny * Tiny;
			T = Ax - One;
			W = (T * T) * (Half - T * (Thrd - T * Qrtr));
			U = IvLn2H * T;
			V = T * IvLn2L - W * IvLn2;
			T1 = U + V;
			SetLowWord(T1, 0);
			T2 = V - (T1 - U);
		}
		else
		{
			double Ss, S2, SH, SL, TH, TL;
			N = 0;
			if (Ix < 0x00100000)
			{
				Ax *= Two53;
				N -= 53;
				Ix = static_cast<int32_t>(HighWord(Ax));
			}
			N += (Ix >> 20) - 0x3ff;
			J = Ix & 0x000fffff;
			Ix = J | 0x3ff00000;
			if (J <= 0x3988E) K = 0;
			else if (J < 0xBB67A) K = 1;
			else { K = 0; N += 1; Ix -= 0x00100000; }
			SetHighWord(Ax, static_cast<uint32_t>(Ix));

			U = Ax - Bp[K];
			V = One / (Ax + Bp[K]);
			Ss = U * V;
			SH = Ss;
			SetLowWord(SH, 0);
			TH = Zero;
			SetHighWord(TH, static_cast<uint32_t>(((Ix >> 1) | 0x20000000) + 0x00080000 + (K << 18)));
			TL = Ax - (TH - Bp[K]);
			SL = V * ((U - SH * TH) - SH * TL);
			S2 = Ss * Ss;
			R = S2 * S2 * (L1 + S2 * (L2 + S2 * (L3 + S2 * (L4 + S2 * (L5 + S2 * L6)))));
			R += SL * (SH + Ss);
			S2 = SH * SH;
			TH = 3.0 + S2 + R;
			SetLowWord(TH, 0);
			TL = R - ((TH - 3.0) - S2);
			U = SH * TH;
			V = SL * TH + TL * Ss;
			PH = U + V;
			SetLowWord(PH, 0);
			PL = V - (PH - U);
			ZH = CpH * PH;
			ZL = CpL * PH + PL * Cp + DpL[K];
			T = static_cast<double>(N);
			T1 = (((ZH + ZL) + DpH[K]) + T);
			SetLowWord(T1, 0);
			T2 = ZL - (((T1 - T) - DpH[K]) - ZH);
		}

		Y1 = Y;
		SetLowWord(Y1, 0);
		PL = (Y - Y1) * T1 + Y * T2;
		PH = Y1 * T1;
		Z = PL + PH;
		J = static_cast<int32_t>(HighWord(Z));
		I = static_cast<int32_t>(LowWord(Z));
		if (J >= 0x40900000)
		{
			if (((J - 0x40900000) | I) != 0) return S * Huge * Huge;
			if (PL + Ovt > Z - PH) return S * Huge * Huge;
		}
		else if ((J & 0x7fffffff) >= 0x4090cc00)
		{
			if (((static_cast<uint32_t>(J) - 0xc090cc00u) | static_cast<uint32_t>(I)) != 0) return S * Tiny * Tiny;
			if (PL <= Z - PH) return S * Tiny * Tiny;
		}

		I = J & 0x7fffffff;
		K = (I >> 20) - 0x3ff;
		N = 0;
		if (I > 0x3fe00000)
		{
			N = J + (0x00100000 >> (K + 1));
			K = ((N & 0x7fffffff) >> 20) - 0x3ff;
			T = Zero;
			SetHighWord(T, static_cast<uint32_t>(N & ~(0x000fffff >> K)));
			N = ((N & 0x000fffff) | 0x00100000) >> (20 - K);
			if (J < 0) N = -N;
			PH -= T;
		}
		T = PL + PH;
		SetLowWord(T, 0);
		U = T * Lg2H;
		V = (PL - (T - PH)) * Lg2 + T * Lg2L;
		Z = U + V;
		W = V - (Z - U);
		T = Z * Z;
		T1 = Z - T * (P1 + T * (P2 + T * (P3 + T * (P4 + T * P5))));
		R = (Z * T1) / ((T1 - Two) - (W + Z * W));
		Z = One - (R - Z);
		J = static_cast<int32_t>(HighWord(Z));
		J += static_cast<int32_t>(static_cast<uint32_t>(N) << 20);
		if ((J >> 20) <= 0) Z = std::scalbn(Z, N); // exact
		else SetHighWord(Z, static_cast<uint32_t>(J));
		return S * Z;
	}

	double Clamp01(double X) { return X < 0 ? 0 : X > 1 ? 1 : X; }
	double Clamp(double X, double Lo, double Hi) { return X < Lo ? Lo : X > Hi ? Hi : X; }
	double Lerp(double A, double B, double T) { return A + (B - A) * T; }

	double SmoothToward(double Current, double Target, double DtMs, double TauMs)
	{
		if (TauMs <= 0) return Target;
		return Current + (Target - Current) * (1 - Exp(-DtMs / TauMs));
	}

	double Smoothstep(double Edge0, double Edge1, double X)
	{
		const double T = Clamp01((X - Edge0) / (Edge1 - Edge0));
		return T * T * (3 - 2 * T);
	}

	std::string Fnv1a(const std::string& Utf8)
	{
		uint32_t H = 0x811c9dc5u;
		auto Mix = [&H](uint32_t Unit) { H ^= Unit; H *= 0x01000193u; };
		for (size_t I = 0; I < Utf8.size();)
		{
			const unsigned char C0 = static_cast<unsigned char>(Utf8[I]);
			uint32_t Cp;
			size_t Len;
			if (C0 < 0x80) { Cp = C0; Len = 1; }
			else if ((C0 >> 5) == 0x6 && I + 1 < Utf8.size()) { Cp = ((C0 & 0x1fu) << 6) | (Utf8[I + 1] & 0x3fu); Len = 2; }
			else if ((C0 >> 4) == 0xe && I + 2 < Utf8.size()) { Cp = ((C0 & 0x0fu) << 12) | ((Utf8[I + 1] & 0x3fu) << 6) | (Utf8[I + 2] & 0x3fu); Len = 3; }
			else if ((C0 >> 3) == 0x1e && I + 3 < Utf8.size()) { Cp = ((C0 & 0x07u) << 18) | ((Utf8[I + 1] & 0x3fu) << 12) | ((Utf8[I + 2] & 0x3fu) << 6) | (Utf8[I + 3] & 0x3fu); Len = 4; }
			else { Cp = 0xfffd; Len = 1; }
			I += Len;
			if (Cp >= 0x10000)
			{
				Cp -= 0x10000;
				Mix(0xd800u + (Cp >> 10));
				Mix(0xdc00u + (Cp & 0x3ffu));
			}
			else
			{
				Mix(Cp);
			}
		}
		static const char Hex[] = "0123456789abcdef";
		std::string Out(8, '0');
		for (int I = 7; I >= 0; --I) { Out[static_cast<size_t>(I)] = Hex[H & 0xf]; H >>= 4; }
		return Out;
	}

	bool SelfTestDeterministicMath(std::string* OutFailure)
	{
		// Values from Node 22 (V8): Math.exp(x), Math.pow(x, y). Each differs in the last bit under FMA.
		static const double ExpCases[][2] = {
			{ -36.384371775202453, 1.5793126373065763e-16 },
			{ -7.4048841185867786, 0.00060827460890226412 },
			{ -39.277324117720127, 8.7513463418463525e-18 },
			{ -8.0710165528580546, 0.00031246548188459191 },
		};
		static const double PowCases[][3] = {
			{ 0.36851182789541781, 1.6000000000000001, 0.20245210220528662 },
			{ 0.89287498267367482, 2.8527100402861834, 0.72380237963967509 },
			{ 0.49310703598894179, 1.3933334578759968, 0.37339173407877557 },
			{ 0.224730922607705, 1.5818606265820563, 0.094280302116254197 },
		};
		auto Same = [](double A, double B) { return std::memcmp(&A, &B, sizeof(double)) == 0; };
		for (const auto& C : ExpCases)
		{
			if (!Same(Exp(C[0]), C[1]))
			{
				if (OutFailure) *OutFailure = "Exp is not bit-exact (FMA contraction or fast-math enabled?)";
				return false;
			}
		}
		for (const auto& C : PowCases)
		{
			if (!Same(Pow(C[0], C[1]), C[2]))
			{
				if (OutFailure) *OutFailure = "Pow is not bit-exact (FMA contraction or fast-math enabled?)";
				return false;
			}
		}
		return true;
	}
}
