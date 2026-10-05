#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/StarSystemData.h"
#include "StarActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class OMNISPACE_API AStarActor : public AActor
{
	GENERATED_BODY()
public:
	AStarActor();
	void Setup(const FStarDescriptor& InStar);
	void SetRenderScale(double Scale, double DistanceM);
	void UpdateUniforms(double Time);

	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") FStarDescriptor Star;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Surface;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
	double RadiusM = 7e8;
};
