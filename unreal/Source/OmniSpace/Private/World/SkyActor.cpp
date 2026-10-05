#include "World/SkyActor.h"
#include "World/OmniAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectGlobals.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OmniSpace.h"

ASkyActor::ASkyActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Dome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dome"));
	SetRootComponent(Dome);
	// The engine sky sphere has inward-facing normals/winding, made for exactly this.
	UStaticMesh* SkyMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineSky/SM_SkySphere.SM_SkySphere"));
	Dome->SetStaticMesh(SkyMesh ? SkyMesh : OmniAssets::SphereMesh());
	Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dome->SetCastShadow(false);
	Dome->bCastDynamicShadow = false;
	Dome->bAffectDistanceFieldLighting = false;
	Dome->bAffectDynamicIndirectLighting = false;
	Dome->SetReceivesDecals(false);
	// M_Sky is flagged "Is Sky" (rendered at the far plane), so the dome only has to be bigger
	// than the near plane; 100 km radius keeps the bounds sane for culling and LWC.
	// SM_SkySphere is ~1 km across at scale 1; M_Sky is "Is Sky" (far-plane depth), so size only
	// has to clear the near plane and anything parented to the ship.
	Dome->SetRelativeScale3D(FVector(1000.0));
	Dome->bVisibleInRayTracing = false;
	Dome->SetTranslucentSortPriority(-100);
}

void ASkyActor::Setup(int32 Seed, const FLinearColor& NebulaTint)
{
	const bool bDebug = FParse::Param(FCommandLine::Get(), TEXT("SkyDebug"));
	UMaterialInterface* Base = bDebug ? OmniAssets::EmissiveMaterial() : OmniAssets::SkyMaterial();
	MID = OmniAssets::MakeMID(Dome, Base, this);
	UE_LOG(LogOmniSpace, Log, TEXT("Sky: material=%s mid=%d debug=%d"), Base ? *Base->GetPathName() : TEXT("null"), MID != nullptr, bDebug);
	if (MID && bDebug) MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.6f, 0.1f, 0.6f));
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("Seed"), static_cast<float>(Seed % 1000));
		MID->SetVectorParameterValue(TEXT("Nebula"), NebulaTint);
	}
}

void ASkyActor::SetWarp(float Warp, const FVector& Dir, float Flow)
{
	if (!MID) return;
	MID->SetScalarParameterValue(TEXT("Warp"), Warp);
	MID->SetScalarParameterValue(TEXT("Flow"), Flow);
	MID->SetVectorParameterValue(TEXT("WarpDir"), FLinearColor(Dir.X, Dir.Y, Dir.Z, 0));
}
