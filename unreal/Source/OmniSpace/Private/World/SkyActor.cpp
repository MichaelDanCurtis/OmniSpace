#include "World/SkyActor.h"
#include "World/OmniAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

ASkyActor::ASkyActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Dome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dome"));
	SetRootComponent(Dome);
	Dome->SetStaticMesh(OmniAssets::SphereMesh());
	Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dome->SetCastShadow(false);
	Dome->bCastDynamicShadow = false;
	Dome->bAffectDistanceFieldLighting = false;
	Dome->bAffectDynamicIndirectLighting = false;
	Dome->SetReceivesDecals(false);
	// Larger than anything the director places (far bodies are compressed to < ~2e9 m).
	Dome->SetRelativeScale3D(FVector(1.0e11)); // sphere mesh is 100 cm across -> 5e10 m radius
	Dome->bVisibleInRayTracing = false;
	Dome->SetTranslucentSortPriority(-100);
}

void ASkyActor::Setup(int32 Seed, const FLinearColor& NebulaTint)
{
	MID = OmniAssets::MakeMID(Dome, OmniAssets::SkyMaterial(), this);
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("Seed"), static_cast<float>(Seed % 1000));
		MID->SetVectorParameterValue(TEXT("Nebula"), NebulaTint);
	}
}
