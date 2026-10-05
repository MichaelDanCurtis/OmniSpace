#include "World/CelestialBodyActor.h"
#include "World/OmniAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ACelestialBodyActor::ACelestialBodyActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Tilt = CreateDefaultSubobject<USceneComponent>(TEXT("Tilt"));
	Tilt->SetupAttachment(Root);
	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	Surface->SetupAttachment(Tilt);
	Surface->SetStaticMesh(OmniAssets::SphereMesh());
	Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Surface->SetCollisionProfileName(TEXT("BlockAll"));
	Surface->bCastDynamicShadow = false;
	Rings = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rings"));
	Rings->SetupAttachment(Tilt);
	Rings->SetStaticMesh(OmniAssets::PlaneMesh());
	Rings->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rings->SetVisibility(false);
	Rings->bCastDynamicShadow = false;
}

int32 ACelestialBodyActor::TypeIndex(const FString& Type)
{
	static const TArray<FString> Types = {TEXT("rocky"), TEXT("barren"), TEXT("desert"), TEXT("lava"), TEXT("ice"), TEXT("ocean"), TEXT("earthlike"), TEXT("jungle"), TEXT("toxic"), TEXT("gas_giant"), TEXT("ice_giant")};
	int32 I = Types.IndexOfByKey(Type);
	return I < 0 ? 0 : I;
}

void ACelestialBodyActor::Setup(const FBodyData& InBody)
{
	Body = InBody;
	Tilt->SetRelativeRotation(FRotator(0, 0, FMath::RadiansToDegrees(Body.Tilt)));
	// engine sphere is 100 cm across -> scale by radius in metres * 2
	Surface->SetRelativeScale3D(FVector(Body.RadiusM * 2.0));

	SurfaceMID = OmniAssets::MakeMID(Surface, OmniAssets::PlanetMaterial(), this);
	if (SurfaceMID)
	{
		FLinearColor C0 = OmniJson::HexToLinear(Body.Palette[0]);
		FLinearColor C1 = OmniJson::HexToLinear(Body.Palette[1]);
		FLinearColor C2 = OmniJson::HexToLinear(Body.Palette[2]);
		SurfaceMID->SetVectorParameterValue(TEXT("C0"), C0);
		SurfaceMID->SetVectorParameterValue(TEXT("C1"), C1);
		SurfaceMID->SetVectorParameterValue(TEXT("C2"), C2);
		SurfaceMID->SetVectorParameterValue(TEXT("Color"), C1); // fallback material
		SurfaceMID->SetScalarParameterValue(TEXT("Type"), TypeIndex(Body.Type));
		SurfaceMID->SetScalarParameterValue(TEXT("Bands"), Body.Bands);
		SurfaceMID->SetScalarParameterValue(TEXT("Clouds"), Body.Clouds);
		SurfaceMID->SetScalarParameterValue(TEXT("Seed"), static_cast<float>((GetTypeHash(Body.Id) % 1000) / 37.0));
		SurfaceMID->SetVectorParameterValue(TEXT("Atmo"), Body.Atmosphere.IsEmpty() ? FLinearColor::Black : OmniJson::HexToLinear(Body.Atmosphere));
		SurfaceMID->SetScalarParameterValue(TEXT("HasAtmo"), Body.Atmosphere.IsEmpty() ? 0.f : 1.f);
	}
	if (Body.bHasRings)
	{
		Rings->SetVisibility(true);
		// plane mesh is 100 cm square, centred; scale to outer diameter in cm
		Rings->SetRelativeScale3D(FVector(Body.Rings.OuterM * 2.0, Body.Rings.OuterM * 2.0, 1.0));
		RingMID = OmniAssets::MakeMID(Rings, OmniAssets::RingMaterial(), this);
		if (RingMID)
		{
			RingMID->SetVectorParameterValue(TEXT("Color"), OmniJson::HexToLinear(Body.Rings.Color));
			RingMID->SetScalarParameterValue(TEXT("Opacity"), Body.Rings.Opacity);
			RingMID->SetScalarParameterValue(TEXT("Inner"), Body.Rings.InnerM / Body.Rings.OuterM);
			RingMID->SetScalarParameterValue(TEXT("Seed"), static_cast<float>((GetTypeHash(Body.Id) % 97) / 7.0));
		}
	}
}

void ACelestialBodyActor::SetRenderScale(double Scale)
{
	if (FMath::IsNearlyEqual(Scale, RenderScale)) return;
	RenderScale = Scale;
	Surface->SetRelativeScale3D(FVector(Body.RadiusM * 2.0 * Scale));
	if (Body.bHasRings) Rings->SetRelativeScale3D(FVector(Body.Rings.OuterM * 2.0 * Scale, Body.Rings.OuterM * 2.0 * Scale, 1.0));
}

void ACelestialBodyActor::UpdateUniforms(const FVector& SunWorldPos, double Time)
{
	Spin = Time * 0.004;
	Surface->SetRelativeRotation(FRotator(0, FMath::RadiansToDegrees(Spin), 0));
	if (SurfaceMID) SurfaceMID->SetVectorParameterValue(TEXT("SunPos"), FLinearColor(SunWorldPos.X, SunWorldPos.Y, SunWorldPos.Z, 0));
	if (RingMID) RingMID->SetVectorParameterValue(TEXT("SunPos"), FLinearColor(SunWorldPos.X, SunWorldPos.Y, SunWorldPos.Z, 0));
}
