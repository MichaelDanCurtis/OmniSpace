#include "World/StarActor.h"
#include "World/OmniAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AStarActor::AStarActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	SetRootComponent(Surface);
	Surface->SetStaticMesh(OmniAssets::SphereMesh());
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->bCastDynamicShadow = false;
}

void AStarActor::Setup(const FStarDescriptor& InStar)
{
	Star = InStar;
	RadiusM = FMath::Max(Star.RadiusSolar * 695700000.0, 8000.0);
	Surface->SetRelativeScale3D(FVector(RadiusM * 2.0));
	MID = OmniAssets::MakeMID(Surface, OmniAssets::StarMaterial(), this);
	if (MID)
	{
		FLinearColor C = Star.LinearColor();
		MID->SetVectorParameterValue(TEXT("Color"), C * 20.0f);
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), C * 20.0f);
		MID->SetScalarParameterValue(TEXT("Seed"), static_cast<float>(GetTypeHash(Star.Id) % 100));
	}
}

void AStarActor::SetRenderScale(double Scale, double /*DistanceM*/)
{
	Surface->SetRelativeScale3D(FVector(RadiusM * 2.0 * Scale));
}

void AStarActor::UpdateUniforms(double Time)
{
	if (MID) MID->SetScalarParameterValue(TEXT("Time"), static_cast<float>(Time));
}
