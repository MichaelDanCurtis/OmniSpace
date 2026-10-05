#include "World/SurfaceTile.h"
#include "World/TerrainNoise.h"
#include "World/OmniAssets.h"
#include "OmniSpace.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

using namespace OmniTerrain;

ASurfaceTile::ASurfaceTile()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	Terrain->SetupAttachment(Root);
	Terrain->bUseAsyncCooking = false;
	Terrain->SetCollisionProfileName(TEXT("BlockAll"));
	Terrain->SetMobility(EComponentMobility::Static);
	Water = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Water"));
	Water->SetupAttachment(Root);
	Water->SetStaticMesh(OmniAssets::PlaneMesh());
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetVisibility(false);
	Skirt = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Skirt"));
	Skirt->SetupAttachment(Root);
	Skirt->SetStaticMesh(OmniAssets::PlaneMesh());
	Skirt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);
	Atmosphere->SetVisibility(false);
	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetVisibility(false);
}

void ASurfaceTile::ChooseProfile()
{
	const FString& T = Body.Type;
	bAtmosphere = !Body.Atmosphere.IsEmpty();
	// gravity from the body data if present, else from radius (rocky density)
	GravityMS2 = Body.GravityG > 0 ? Body.GravityG * 9.81 : FMath::Clamp(Body.RadiusM / 6.371e6, 0.15, 1.6) * 9.81;
	if (T == TEXT("rocky") || T == TEXT("barren")) { Profile = 1; Amplitude = 650; Frequency = 0.35; }
	else if (T == TEXT("ice")) { Profile = 5; Amplitude = 420; Frequency = 0.4; }
	else if (T == TEXT("desert")) { Profile = 2; Amplitude = 140; Frequency = 0.9; }
	else if (T == TEXT("lava")) { Profile = 3; Amplitude = 260; Frequency = 0.5; }
	else if (T == TEXT("ocean")) { Profile = 4; Amplitude = 90; Frequency = 0.5; WaterM = 0; }
	else if (T == TEXT("earthlike") || T == TEXT("jungle")) { Profile = 0; Amplitude = 320; Frequency = 0.45; WaterM = -40; }
	else if (T == TEXT("toxic")) { Profile = 0; Amplitude = 220; Frequency = 0.6; }
	else { Profile = 1; Amplitude = 400; Frequency = 0.4; }
}

double ASurfaceTile::HeightAt(double X, double Y) const
{
	const double Kx = (X + SiteOffset.X) / 1000.0 * Frequency, Ky = (Y + SiteOffset.Y) / 1000.0 * Frequency;
	double H = 0;
	switch (Profile)
	{
	case 1: // mountains: ridged with a broad base
		H = Ridged(Kx, Ky, Seed, 6) * 0.9 + Fbm(Kx * 0.3, Ky * 0.3, Seed + 2, 3) * 0.6;
		H = H * Amplitude - Amplitude * 0.45;
		break;
	case 2: // dunes: long crests, soft
		{
			const double Wave = FMath::Sin((Kx * 3.1 + Fbm(Kx, Ky, Seed, 3) * 1.5) * PI) * 0.5 + 0.5;
			H = (Wave * 0.7 + Fbm(Kx * 2, Ky * 2, Seed + 5, 4) * 0.3) * Amplitude;
		}
		break;
	case 3: // lava: plateaus cut by glowing fissures
		{
			double P = Fbm(Kx, Ky, Seed, 5);
			P = FMath::RoundToDouble(P * 3.0) / 3.0 * 0.7 + P * 0.3;
			const double Crack = FMath::Pow(1.0 - FMath::Abs(ValueNoise(Kx * 4, Ky * 4, Seed + 9)), 14.0);
			H = P * Amplitude - Crack * 60.0;
		}
		break;
	case 4: // ocean: mostly below the water, rare islands
		H = (Fbm(Kx, Ky, Seed, 5) - 0.25) * Amplitude;
		break;
	case 5: // ice: rolling with sharp seracs
		H = Fbm(Kx, Ky, Seed, 5) * Amplitude * 0.6 + Ridged(Kx * 2.5, Ky * 2.5, Seed + 3, 4) * Amplitude * 0.35;
		break;
	default: // rolling hills
		H = Fbm(Kx, Ky, Seed, 6) * Amplitude;
		break;
	}
	// flatten the pad under the landing point so the ship always has somewhere to sit
	const double R = FMath::Sqrt(X * X + Y * Y);
	if (bPadEnabled && R < 500.0)
	{
		const double K = FMath::SmoothStep(120.0, 500.0, R);
		H = FMath::Lerp(HeightPad, H, K);
	}
	return H;
}

FVector ASurfaceTile::NormalAt(double X, double Y) const
{
	const double E = 4.0;
	const double Hx = HeightAt(X + E, Y) - HeightAt(X - E, Y);
	const double Hy = HeightAt(X, Y + E) - HeightAt(X, Y - E);
	return FVector(-Hx / (2 * E), -Hy / (2 * E), 1.0).GetSafeNormal();
}

void ASurfaceTile::Build(const FBodyData& InBody, const FVector2D& Site, const FLinearColor& SunColor, double SunElevationDeg)
{
	Body = InBody;
	Seed = (GetTypeHash(Body.Id) % 10007) / 37.0;
	SiteOffset = FVector2D(Site.X * 200000.0, Site.Y * 200000.0); // the site picks a window into the planet's noise
	ChooseProfile();
	// pad height: sample the raw field around the centre so the pad sits at a natural level
	bPadEnabled = false;
	double H0 = 0;
	for (int i = 0; i < 8; i++) H0 += HeightAt(FMath::Cos(i * PI / 4) * 600.0, FMath::Sin(i * PI / 4) * 600.0);
	HeightPad = FMath::Max(H0 / 8.0, WaterM + 6.0);
	bPadEnabled = true;
	BuildMesh();
	BuildSky(SunColor, SunElevationDeg);
	UE_LOG(LogOmniSpace, Log, TEXT("Surface tile for %s (%s): profile %d amp %.0f m g=%.2f atmo=%d pad=%.0f"), *Body.Name, *Body.Type, Profile, Amplitude, GravityMS2, bAtmosphere, HeightPad);
}

void ASurfaceTile::BuildMesh()
{
	// Two resolutions: a fine centre (3 km at 15 m) and a coarse outer tile (12 km at 60 m) - the
	// landing pad and anything you drive to is sharp; the hills on the horizon don't need to be.
	struct FGrid { double Size; int32 Cells; double Inner; };
	const FGrid Grids[] = {{3000.0, 200, 0.0}, {TileSizeM, 200, 3000.0}};
	int32 Section = 0;
	for (const FGrid& G : Grids)
	{
		TArray<FVector> V;
		TArray<int32> I;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> C;
		TArray<FProcMeshTangent> Tn;
		const int32 Nv = G.Cells + 1;
		V.Reserve(Nv * Nv); N.Reserve(Nv * Nv); UV.Reserve(Nv * Nv); C.Reserve(Nv * Nv); Tn.Reserve(Nv * Nv);
		const double Step = G.Size / G.Cells;
		for (int32 y = 0; y < Nv; y++)
			for (int32 x = 0; x < Nv; x++)
			{
				const double Px = -G.Size / 2 + x * Step, Py = -G.Size / 2 + y * Step;
				const double H = HeightAt(Px, Py);
				V.Add(FVector(Px, Py, H) * 100.0);
				N.Add(NormalAt(Px, Py));
				UV.Add(FVector2D(Px / 1000.0, Py / 1000.0));
				C.Add(FLinearColor(0, 0, 0, 1));
				Tn.Add(FProcMeshTangent(1, 0, 0));
			}
		for (int32 y = 0; y < G.Cells; y++)
			for (int32 x = 0; x < G.Cells; x++)
			{
				// skip cells covered by the fine grid
				const double Cx = -G.Size / 2 + (x + 0.5) * Step, Cy = -G.Size / 2 + (y + 0.5) * Step;
				if (G.Inner > 0 && FMath::Abs(Cx) < G.Inner / 2 && FMath::Abs(Cy) < G.Inner / 2) continue;
				const int32 A = y * Nv + x, B = A + 1, Cc = A + Nv, D = Cc + 1;
				I.Add(A); I.Add(Cc); I.Add(B);
				I.Add(B); I.Add(Cc); I.Add(D);
			}
		Terrain->CreateMeshSection_LinearColor(Section, V, I, N, UV, C, Tn, true);
		Section++;
	}
	TerrainMID = OmniAssets::MakeMID(Terrain, OmniAssets::MaterialOr(TEXT("/Game/Materials/M_Terrain.M_Terrain"), TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), this);
	if (TerrainMID)
	{
		const FLinearColor C0 = OmniJson::HexToLinear(Body.Palette.Num() > 0 ? Body.Palette[0] : TEXT("#5b524a"));
		const FLinearColor C1 = OmniJson::HexToLinear(Body.Palette.Num() > 1 ? Body.Palette[1] : TEXT("#8a7d6e"));
		const FLinearColor C2 = OmniJson::HexToLinear(Body.Palette.Num() > 2 ? Body.Palette[2] : TEXT("#b8ab98"));
		TerrainMID->SetVectorParameterValue(TEXT("C0"), C0);
		TerrainMID->SetVectorParameterValue(TEXT("C1"), C1);
		TerrainMID->SetVectorParameterValue(TEXT("C2"), C2);
		TerrainMID->SetScalarParameterValue(TEXT("Profile"), static_cast<float>(Profile));
		TerrainMID->SetScalarParameterValue(TEXT("Seed"), static_cast<float>(Seed));
		TerrainMID->SetScalarParameterValue(TEXT("Snow"), Profile == 5 ? 1.f : (Profile == 1 ? 0.5f : 0.f));
		TerrainMID->SetScalarParameterValue(TEXT("Glow"), Profile == 3 ? 1.f : 0.f);
		if (Terrain->GetNumSections() > 1) Terrain->SetMaterial(1, TerrainMID);
	}
	for (int32 s = 0; s < Terrain->GetNumSections(); s++) Terrain->SetMaterial(s, TerrainMID);
	// skirt: a huge flat disc at the lowest level with the same material so the edge reads as distant plains
	Skirt->SetRelativeLocation(FVector(0, 0, (HeightPad - Amplitude * 0.7) * 100.0));
	Skirt->SetRelativeScale3D(FVector(2500, 2500, 1)); // 250 km
	Skirt->SetMaterial(0, TerrainMID);
	// water
	if (WaterM > -1e8)
	{
		Water->SetVisibility(true);
		Water->SetRelativeLocation(FVector(0, 0, WaterM * 100.0));
		Water->SetRelativeScale3D(FVector(2500, 2500, 1));
		WaterMID = OmniAssets::MakeMID(Water, OmniAssets::MaterialOr(TEXT("/Game/Materials/M_Water.M_Water"), TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), this);
		if (WaterMID)
		{
			const FLinearColor Deep = OmniJson::HexToLinear(Body.Palette.Num() > 0 ? Body.Palette[0] : TEXT("#0b2a4a"));
			WaterMID->SetVectorParameterValue(TEXT("Color"), Body.Type == TEXT("ocean") || Body.Type == TEXT("earthlike") ? Deep : FLinearColor(0.2f, 0.25f, 0.1f));
		}
	}
}

void ASurfaceTile::BuildSky(const FLinearColor& SunColor, double SunElevationDeg)
{
	if (bAtmosphere)
	{
		const FLinearColor A = OmniJson::HexToLinear(Body.Atmosphere);
		Atmosphere->SetVisibility(true);
		Atmosphere->SetRayleighScattering(FLinearColor(A.R * 0.6f + 0.1f, A.G * 0.6f + 0.2f, A.B * 0.7f + 0.3f));
		Atmosphere->SetRayleighScatteringScale(Body.Type == TEXT("toxic") ? 0.08f : 0.033f);
		Atmosphere->SetMieScatteringScale(Body.Type == TEXT("desert") ? 0.01f : 0.004f);
		Atmosphere->SetAtmosphereHeight(Body.Type == TEXT("toxic") ? 120.f : 60.f);
		Fog->SetVisibility(true);
		Fog->SetFogDensity(Body.Type == TEXT("toxic") ? 0.02f : Body.Type == TEXT("jungle") ? 0.012f : 0.004f);
		Fog->SetFogHeightFalloff(0.08f);
		Fog->SetFogInscatteringColor(FLinearColor(A.R, A.G, A.B) * 0.8f);
		Fog->SetStartDistance(200.f);
	}
	else
	{
		Atmosphere->SetVisibility(false);
		Fog->SetVisibility(false);
	}
}
