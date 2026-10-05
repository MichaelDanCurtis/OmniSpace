// Small deterministic 2D noise for landing-tile heightfields (CPU side; the terrain
// material does its own detail in HLSL). Value noise with quintic smoothing, fbm, ridged.
#pragma once

#include "CoreMinimal.h"

namespace OmniTerrain
{
	inline double Hash2(double X, double Y, double Seed)
	{
		double N = FMath::Sin(X * 127.1 + Y * 311.7 + Seed * 74.7) * 43758.5453;
		return N - FMath::Floor(N);
	}

	inline double ValueNoise(double X, double Y, double Seed)
	{
		const double Fx = FMath::Floor(X), Fy = FMath::Floor(Y);
		double Tx = X - Fx, Ty = Y - Fy;
		Tx = Tx * Tx * Tx * (Tx * (Tx * 6 - 15) + 10);
		Ty = Ty * Ty * Ty * (Ty * (Ty * 6 - 15) + 10);
		const double A = Hash2(Fx, Fy, Seed), B = Hash2(Fx + 1, Fy, Seed), C = Hash2(Fx, Fy + 1, Seed), D = Hash2(Fx + 1, Fy + 1, Seed);
		return FMath::Lerp(FMath::Lerp(A, B, Tx), FMath::Lerp(C, D, Tx), Ty) * 2.0 - 1.0; // -1..1
	}

	inline double Fbm(double X, double Y, double Seed, int32 Octaves, double Lacunarity = 2.05, double Gain = 0.5)
	{
		double Sum = 0, Amp = 0.5, Norm = 0;
		for (int32 i = 0; i < Octaves; i++)
		{
			Sum += Amp * ValueNoise(X, Y, Seed + i * 13.1);
			Norm += Amp;
			X = X * Lacunarity + 17.3;
			Y = Y * Lacunarity + 9.7;
			Amp *= Gain;
		}
		return Sum / Norm;
	}

	inline double Ridged(double X, double Y, double Seed, int32 Octaves)
	{
		double Sum = 0, Amp = 0.5, Norm = 0;
		for (int32 i = 0; i < Octaves; i++)
		{
			double N = 1.0 - FMath::Abs(ValueNoise(X, Y, Seed + i * 7.7));
			Sum += Amp * N * N;
			Norm += Amp;
			X = X * 2.1 + 3.1;
			Y = Y * 2.1 + 1.3;
			Amp *= 0.5;
		}
		return Sum / Norm; // 0..1
	}
}
