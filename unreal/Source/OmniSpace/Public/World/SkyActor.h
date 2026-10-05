// Star sky: an enormous inverted sphere with the procedural M_Sky material, centred on the
// floating origin (the ship), so it is effectively at infinity.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkyActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class OMNISPACE_API ASkyActor : public AActor
{
	GENERATED_BODY()
public:
	ASkyActor();
	void Setup(int32 Seed, const FLinearColor& NebulaTint);
	/** Frame-shift streak: 0 = normal sky, 1 = full warp; Dir = jump direction (world). */
	void SetWarp(float Warp, const FVector& Dir, float Flow);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Dome;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
};
