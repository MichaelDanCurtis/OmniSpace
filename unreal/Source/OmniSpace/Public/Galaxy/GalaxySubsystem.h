// Game-instance wrapper around the pure GalaxyCore star map.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/StarSystemData.h"
#include "GalaxySubsystem.generated.h"

namespace omni::galaxy { class Galaxy; struct Star; }

UCLASS()
class OMNISPACE_API UGalaxySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Star by id ("kelvar" or "sx.sy.sz.i"). Returns false when unknown. */
	UFUNCTION(BlueprintCallable, Category = "OmniSpace|Galaxy")
	bool GetStar(const FString& Id, FStarDescriptor& Out);

	UFUNCTION(BlueprintCallable, Category = "OmniSpace|Galaxy")
	TArray<FStarDescriptor> StarsNear(const FVector& PosLy, double RadiusLy);

	/** Route as star ids (including both ends); empty when none within range. */
	UFUNCTION(BlueprintCallable, Category = "OmniSpace|Galaxy")
	TArray<FString> PlotRoute(const FString& FromId, const FString& ToId, double JumpRangeLy);

	UFUNCTION(BlueprintPure, Category = "OmniSpace|Galaxy")
	static double DistanceLy(const FStarDescriptor& A, const FStarDescriptor& B);

	UFUNCTION(BlueprintPure, Category = "OmniSpace|Galaxy")
	static FString ClassLabel(const FString& Cls);

	UFUNCTION(BlueprintPure, Category = "OmniSpace|Galaxy")
	static bool IsScoopable(const FString& Cls);

	static FStarDescriptor Convert(const omni::galaxy::Star& S);

private:
	TUniquePtr<omni::galaxy::Galaxy> Galaxy;
};
