// The commander: credits, cargo, contracts, exploration data, visited systems, ship wear - and
// the station economy that trades with them. Ported from the web build (economy.ts, player.ts)
// with the same seeds, so a station's prices and mission board match across both clients.
// Saved as JSON in Saved/OmniSpace/commander.json.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/StarSystemData.h"
#include "CommanderSubsystem.generated.h"

struct FCommodityDef
{
	const TCHAR* Id;
	const TCHAR* Name;
	double Base;
	TArray<FString> Produced;
	TArray<FString> Consumed;
};

USTRUCT(BlueprintType)
struct FMarketRow
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32 Buy = 0;    // what the station charges
	UPROPERTY(BlueprintReadOnly) int32 Sell = 0;   // what the station pays
	UPROPERTY(BlueprintReadOnly) int32 Supply = 0;
	UPROPERTY(BlueprintReadOnly) FString Tag;      // produced | consumed | ""
};

USTRUCT(BlueprintType)
struct FContract
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() bool bCourier = false;
	UPROPERTY() FString Title;
	UPROPERTY() FString Description;
	UPROPERTY() FString FromStation;
	UPROPERTY() FString FromSystem;
	UPROPERTY() FString DestSystem;
	UPROPERTY() FString DestSystemName;
	UPROPERTY() FString DestStation;      // empty = any station in the system
	UPROPERTY() FString DestStationName;
	UPROPERTY() FString Commodity;
	UPROPERTY() int32 Tons = 0;
	UPROPERTY() int32 Reward = 0;
	UPROPERTY() double DistanceLy = 0;
};

USTRUCT(BlueprintType)
struct FCargoItem
{
	GENERATED_BODY()
	UPROPERTY() FString Id;
	UPROPERTY() double Tons = 0;
};

USTRUCT(BlueprintType)
struct FDataEntry
{
	GENERATED_BODY()
	UPROPERTY() FString Id;       // "sys:<id>" or "scan:<sys>:<body>"
	UPROPERTY() FString Label;
	UPROPERTY() int32 Value = 0;
};

USTRUCT()
struct FCommanderSave
{
	GENERATED_BODY()
	UPROPERTY() int32 Version = 1;
	UPROPERTY() FString Commander = TEXT("Commander");
	UPROPERTY() int32 Credits = 5000;
	UPROPERTY() TArray<FCargoItem> Cargo;
	UPROPERTY() TArray<FCargoItem> MissionCargo;
	UPROPERTY() TArray<FContract> Contracts;
	UPROPERTY() TArray<FDataEntry> Data;
	UPROPERTY() FString SystemId = TEXT("kelvar");
	UPROPERTY() FString DockedAt;
	UPROPERTY() FString LastStation = TEXT("tanara-station");
	UPROPERTY() double Fuel = 16;
	UPROPERTY() double Hull = 1;
	UPROPERTY() TArray<FString> Visited;
	UPROPERTY() TArray<FString> Scanned;
	UPROPERTY() TArray<FString> Route;
	UPROPERTY() int32 Tutorial = 0;
	UPROPERTY() int32 Jumps = 0;
	UPROPERTY() double LyTravelled = 0;
	UPROPERTY() int32 SystemsDiscovered = 0;
	UPROPERTY() int32 Earned = 0;
	UPROPERTY() bool bStarterGrant = false;
};

UCLASS()
class OMNISPACE_API UCommanderSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintReadOnly, Category = "OmniSpace") FCommanderSave S;
	static constexpr double CargoCap = 32.0;

	void Save();
	void Load();
	FString SavePath() const;

	// ---- reference data
	static const TArray<FCommodityDef>& Commodities();
	static const FCommodityDef* Commodity(const FString& Id);
	static int32 Day();   // 30-minute real-time bucket, same as the web build
	static int32 ScanValue(const FString& BodyType);

	// ---- market and missions (deterministic per station and day)
	static TArray<FMarketRow> Market(const FStationData& St, int32 InDay);
	TArray<FContract> MissionBoard(const FStationData& St, const FStarSystemData& Sys, int32 InDay) const;

	// ---- cargo
	double CargoTons() const;
	double MissionTons() const;
	double TotalTons() const { return CargoTons() + MissionTons(); }
	double Held(const FString& Id) const;
	void AddCargo(const FString& Id, double Tons);
	bool TakeCargo(const FString& Id, double Tons);

	// ---- actions at a station; each returns a message for the HUD
	FString Buy(const FStationData& St, const FString& Id, int32 Tons);
	FString Sell(const FStationData& St, const FString& Id, int32 Tons);
	FString Refuel(double& ShipFuel, double FuelMax);
	FString Repair(double& ShipHull);
	FString SellData();
	FString Accept(const FContract& C);
	FString Abandon(const FString& ContractId);
	/** On docking: pay out every contract delivered here. */
	TArray<FString> CompleteContracts(const FString& StationId, const FString& SystemId);
	int32 DataValue() const;

	// ---- exploration
	/** First visit to a system: logs the discovery; returns the data value added (0 if known). */
	int32 VisitSystem(const FStarSystemData& Sys);
	/** Surface scan of a body; returns the value added (0 if already scanned). */
	int32 ScanBody(const FStarSystemData& Sys, const FBodyData& B, bool bMoon);
	bool IsScanned(const FString& SysId, const FString& BodyId) const;

	/** Jump range for the current mass. */
	double JumpRangeLy(double Fuel) const;
};
