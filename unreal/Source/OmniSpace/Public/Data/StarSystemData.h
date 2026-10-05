// Mirrors the StarSystem JSON contract (client/src/game/types.ts and
// server/omnispace_server/schema.py). Distances are metres, angles radians.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "StarSystemData.generated.h"

USTRUCT(BlueprintType)
struct FStarDescriptor
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Cls;
	/** Galactic position in light years. */
	UPROPERTY(BlueprintReadOnly) FVector Pos = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) double Temperature = 5778;
	UPROPERTY(BlueprintReadOnly) double RadiusSolar = 1;
	UPROPERTY(BlueprintReadOnly) double MassSolar = 1;
	UPROPERTY(BlueprintReadOnly) double Luminosity = 1;
	UPROPERTY(BlueprintReadOnly) FString Color = TEXT("#fff4ea");
	UPROPERTY(BlueprintReadOnly) double AgeGyr = 4;
	UPROPERTY(BlueprintReadOnly) double Settlement = 0;
	UPROPERTY(BlueprintReadOnly) FString Region;

	TSharedRef<FJsonObject> ToJson() const;
	static FStarDescriptor FromJson(const TSharedPtr<FJsonObject>& J);
	FLinearColor LinearColor() const;
};

USTRUCT(BlueprintType)
struct FRingsData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) double InnerM = 0;
	UPROPERTY(BlueprintReadOnly) double OuterM = 0;
	UPROPERTY(BlueprintReadOnly) FString Color = TEXT("#d0ccc4");
	UPROPERTY(BlueprintReadOnly) double Opacity = 0.7;
};

USTRUCT(BlueprintType)
struct FBodyData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Kind;   // planet | moon
	UPROPERTY(BlueprintReadOnly) FString Type;   // rocky, gas_giant, ...
	UPROPERTY(BlueprintReadOnly) FString Parent; // "star" or body id
	UPROPERTY(BlueprintReadOnly) double OrbitM = 0;
	UPROPERTY(BlueprintReadOnly) double Phase = 0;
	UPROPERTY(BlueprintReadOnly) double Inclination = 0;
	UPROPERTY(BlueprintReadOnly) double RadiusM = 1;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Palette;
	UPROPERTY(BlueprintReadOnly) FString Atmosphere; // hex or empty
	UPROPERTY(BlueprintReadOnly) double Clouds = 0;
	UPROPERTY(BlueprintReadOnly) double Bands = 0;
	UPROPERTY(BlueprintReadOnly) double Tilt = 0;
	UPROPERTY(BlueprintReadOnly) bool bHasRings = false;
	UPROPERTY(BlueprintReadOnly) FRingsData Rings;
	UPROPERTY(BlueprintReadOnly) double TemperatureK = 0;
	UPROPERTY(BlueprintReadOnly) double GravityG = 0;
	UPROPERTY(BlueprintReadOnly) FString Description;

	bool IsGiant() const { return Type == TEXT("gas_giant") || Type == TEXT("ice_giant"); }
};

USTRUCT(BlueprintType)
struct FStationData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Kind; // spire | ring | outpost
	UPROPERTY(BlueprintReadOnly) FString Parent;
	UPROPERTY(BlueprintReadOnly) double OrbitM = 0;
	UPROPERTY(BlueprintReadOnly) double Phase = 0;
	UPROPERTY(BlueprintReadOnly) double Inclination = 0;
	UPROPERTY(BlueprintReadOnly) FString Faction;
	UPROPERTY(BlueprintReadOnly) FString Economy;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Services;
	UPROPERTY(BlueprintReadOnly) FString Tagline;
	UPROPERTY(BlueprintReadOnly) FString Description;
};

USTRUCT(BlueprintType)
struct FBeltData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Parent;
	UPROPERTY(BlueprintReadOnly) double RadiusM = 0;
	UPROPERTY(BlueprintReadOnly) double WidthM = 0;
	UPROPERTY(BlueprintReadOnly) FString Composition;
};

USTRUCT(BlueprintType)
struct FPoiData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Kind;
	UPROPERTY(BlueprintReadOnly) FString Parent;
	UPROPERTY(BlueprintReadOnly) double OffsetM = 0;
	UPROPERTY(BlueprintReadOnly) double Phase = 0;
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) int32 Reward = 0;
};

USTRUCT(BlueprintType)
struct FSystemProfile
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) FString Lore;
	UPROPERTY(BlueprintReadOnly) FString Faction = TEXT("Unclaimed");
	UPROPERTY(BlueprintReadOnly) FString Government = TEXT("None");
	UPROPERTY(BlueprintReadOnly) FString Economy = TEXT("None");
	UPROPERTY(BlueprintReadOnly) FString Security = TEXT("none");
	UPROPERTY(BlueprintReadOnly) int64 Population = 0;
	UPROPERTY(BlueprintReadOnly) int32 Danger = 0;
};

USTRUCT(BlueprintType)
struct FStarSystemData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Source; // ai | procedural | authored
	UPROPERTY(BlueprintReadOnly) FString Model;
	UPROPERTY(BlueprintReadOnly) FString DiscoveredBy;
	UPROPERTY(BlueprintReadOnly) FStarDescriptor Star;
	UPROPERTY(BlueprintReadOnly) FString StarFlavor;
	UPROPERTY(BlueprintReadOnly) FSystemProfile Profile;
	UPROPERTY(BlueprintReadOnly) TArray<FBodyData> Bodies;
	UPROPERTY(BlueprintReadOnly) TArray<FStationData> Stations;
	UPROPERTY(BlueprintReadOnly) TArray<FBeltData> Belts;
	UPROPERTY(BlueprintReadOnly) TArray<FPoiData> Pois;

	static bool FromJson(const TSharedPtr<FJsonObject>& J, FStarSystemData& Out);
	static bool FromJsonString(const FString& Text, FStarSystemData& Out);
	TSharedRef<FJsonObject> ToJson() const;

	const FBodyData* FindBody(const FString& BodyId) const;
	const FStationData* FindStation(const FString& StationId) const;
};

namespace OmniJson
{
	FLinearColor HexToLinear(const FString& Hex, const FLinearColor& Fallback = FLinearColor::Gray);
	FString LinearToHex(const FLinearColor& C);
}
