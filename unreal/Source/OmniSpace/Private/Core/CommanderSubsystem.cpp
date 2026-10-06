#include "Core/CommanderSubsystem.h"
#include "Galaxy/GalaxySubsystem.h"
#include "Galaxy/GalaxyCore.h"
#include "OmniSpace.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/GameInstance.h"

namespace
{
	TArray<FString> EconLst(std::initializer_list<const TCHAR*> In) { TArray<FString> Out; for (const TCHAR* S : In) Out.Add(S); return Out; }
	std::string EconUtf8(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	const TCHAR* CLIENTS[] = { TEXT("Halden Logistics"), TEXT("Meridian Survey Corps"), TEXT("Brightwater Medical"), TEXT("Ostrova Freight"), TEXT("a private client"), TEXT("the Free Worlds Relief Fund"), TEXT("Kelvar Directorate Logistics"), TEXT("Ashkar Pilgrim Office") };
	FString CapFirst(const FString& S) { if (S.IsEmpty()) return S; return S.Left(1).ToUpper() + S.Mid(1); }
}

const TArray<FCommodityDef>& UCommanderSubsystem::Commodities()
{
	static const TArray<FCommodityDef> C = {
		{ TEXT("water"),       TEXT("Purified Water"),       120,  EconLst({TEXT("Extraction"), TEXT("Refinery")}),           EconLst({TEXT("Colony"), TEXT("Agriculture"), TEXT("Tourism")}) },
		{ TEXT("food"),        TEXT("Hydroponic Food"),      260,  EconLst({TEXT("Agriculture")}),                            EconLst({TEXT("Industrial"), TEXT("Extraction"), TEXT("Military"), TEXT("Colony")}) },
		{ TEXT("ore"),         TEXT("Raw Ore"),              180,  EconLst({TEXT("Extraction")}),                             EconLst({TEXT("Refinery"), TEXT("Industrial")}) },
		{ TEXT("metals"),      TEXT("Refined Metals"),       520,  EconLst({TEXT("Refinery")}),                               EconLst({TEXT("Industrial"), TEXT("High Tech"), TEXT("Military")}) },
		{ TEXT("machinery"),   TEXT("Heavy Machinery"),      900,  EconLst({TEXT("Industrial")}),                             EconLst({TEXT("Extraction"), TEXT("Colony"), TEXT("Agriculture")}) },
		{ TEXT("electronics"), TEXT("Quantum Electronics"),  1650, EconLst({TEXT("High Tech")}),                              EconLst({TEXT("Industrial"), TEXT("Research"), TEXT("Tourism")}) },
		{ TEXT("medicine"),    TEXT("Medical Supplies"),     1100, EconLst({TEXT("High Tech"), TEXT("Research")}),            EconLst({TEXT("Colony"), TEXT("Military"), TEXT("Extraction")}) },
		{ TEXT("fuelcells"),   TEXT("Fusion Fuel Cells"),    700,  EconLst({TEXT("Refinery"), TEXT("Industrial")}),           EconLst({TEXT("Military"), TEXT("Colony"), TEXT("Research")}) },
		{ TEXT("luxury"),      TEXT("Luxury Goods"),         2400, EconLst({TEXT("Tourism"), TEXT("Agriculture")}),           EconLst({TEXT("High Tech"), TEXT("Tourism"), TEXT("Research")}) },
		{ TEXT("weapons"),     TEXT("Personal Weapons"),     1900, EconLst({TEXT("Military"), TEXT("Industrial")}),           EconLst({TEXT("Colony"), TEXT("Extraction")}) },
		{ TEXT("artifacts"),   TEXT("Xeno Artifacts"),       5200, EconLst({TEXT("Research")}),                               EconLst({TEXT("High Tech"), TEXT("Tourism")}) },
		{ TEXT("data"),        TEXT("Encrypted Data Cores"), 3100, EconLst({TEXT("Research"), TEXT("High Tech")}),            EconLst({TEXT("Military"), TEXT("Industrial")}) },
	};
	return C;
}

const FCommodityDef* UCommanderSubsystem::Commodity(const FString& Id)
{
	for (const FCommodityDef& C : Commodities()) if (Id == C.Id) return &C;
	return nullptr;
}

int32 UCommanderSubsystem::Day()
{
	return static_cast<int32>(FDateTime::UtcNow().ToUnixTimestamp() / 1800);
}

int32 UCommanderSubsystem::ScanValue(const FString& T)
{
	static const TMap<FString, int32> V = { {TEXT("rocky"), 900}, {TEXT("barren"), 500}, {TEXT("desert"), 1100}, {TEXT("lava"), 1300}, {TEXT("ice"), 1000}, {TEXT("ocean"), 6500}, {TEXT("earthlike"), 14000}, {TEXT("jungle"), 9000}, {TEXT("toxic"), 1600}, {TEXT("gas_giant"), 3200}, {TEXT("ice_giant"), 2800} };
	const int32* P = V.Find(T);
	return P ? *P : 700;
}

void UCommanderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
}

FString UCommanderSubsystem::SavePath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("OmniSpace"), TEXT("commander.json"));
}

void UCommanderSubsystem::Save()
{
	FString Json;
	if (FJsonObjectConverter::UStructToJsonObjectString(S, Json))
	{
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(SavePath()));
		FFileHelper::SaveStringToFile(Json, *SavePath());
	}
}

void UCommanderSubsystem::Load()
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *SavePath())) return;
	FCommanderSave In;
	if (FJsonObjectConverter::JsonObjectStringToUStruct(Json, &In)) S = In;
	UE_LOG(LogOmniSpace, Log, TEXT("[cmdr] %s  %d CR  %d contracts  %d data entries  %d systems visited"), *S.Commander, S.Credits, S.Contracts.Num(), S.Data.Num(), S.Visited.Num());
}

// ---------------------------------------------------------------- market
TArray<FMarketRow> UCommanderSubsystem::Market(const FStationData& St, int32 InDay)
{
	TArray<FMarketRow> Rows;
	for (const FCommodityDef& C : Commodities())
	{
		omni::galaxy::Rng Rng(omni::galaxy::hashString32(EconUtf8(FString::Printf(TEXT("%s:%s:%d"), *St.Id, C.Id, InDay))));
		const bool bProd = C.Produced.Contains(St.Economy);
		const bool bCons = C.Consumed.Contains(St.Economy);
		double F = 1.0;
		if (bProd) F = 0.68;
		if (bCons) F = 1.38;
		F *= 1.0 + (Rng.next() - 0.5) * 0.18;
		const int32 Price = FMath::RoundToInt(C.Base * F);
		FMarketRow R;
		R.Id = C.Id; R.Name = C.Name;
		R.Buy = FMath::RoundToInt(Price * 1.03);
		R.Sell = FMath::RoundToInt(Price * 0.97);
		R.Supply = bProd ? Rng.intRange(400, 2400) : bCons ? Rng.intRange(0, 60) : Rng.intRange(40, 600);
		R.Tag = bProd ? TEXT("produced") : bCons ? TEXT("consumed") : TEXT("");
		Rows.Add(R);
	}
	return Rows;
}

TArray<FContract> UCommanderSubsystem::MissionBoard(const FStationData& St, const FStarSystemData& Sys, int32 InDay) const
{
	TArray<FContract> Out;
	UGalaxySubsystem* Galaxy = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGalaxySubsystem>() : nullptr;
	if (!Galaxy) return Out;
	omni::galaxy::Rng Rng(omni::galaxy::hashString32(EconUtf8(FString::Printf(TEXT("%s:missions:%d"), *St.Id, InDay))));
	TArray<FStarDescriptor> Neighbors = Galaxy->StarsNear(Sys.Star.Pos, 26.0);
	Neighbors.RemoveAll([&](const FStarDescriptor& N) { return N.Id == Sys.Star.Id || N.Settlement <= 0.45 || N.Cls == TEXT("NS") || N.Cls == TEXT("L") || N.Cls == TEXT("WD") || N.Cls == TEXT("O"); });
	Neighbors.Sort([&](const FStarDescriptor& A, const FStarDescriptor& B) { return FVector::Dist(A.Pos, Sys.Star.Pos) < FVector::Dist(B.Pos, Sys.Star.Pos); });
	FStarDescriptor Home;
	if (Sys.Star.Id != TEXT("kelvar") && Galaxy->GetStar(TEXT("kelvar"), Home) && FVector::Dist(Home.Pos, Sys.Star.Pos) < 26.0) Neighbors.Insert(Home, 0);
	const int32 N = Rng.intRange(3, 5);
	TArray<const FStationData*> Local;
	for (const FStationData& O : Sys.Stations) if (O.Id != St.Id) Local.Add(&O);
	TArray<const FCommodityDef*> Cheap;
	for (const FCommodityDef& C : Commodities()) if (C.Base < 3000) Cheap.Add(&C);
	for (int32 i = 0; i < N; i++)
	{
		const bool bUseLocal = Local.Num() > 0 && Rng.chance(0.3);
		FStarDescriptor DestSys = Sys.Star;
		const FStationData* DestSt = nullptr;
		if (bUseLocal) DestSt = Local[static_cast<int32>(FMath::Floor(Rng.next() * Local.Num())) % Local.Num()];
		else if (Neighbors.Num()) { const int32 K = FMath::Min(8, Neighbors.Num()); DestSys = Neighbors[static_cast<int32>(FMath::Floor(Rng.next() * K)) % K]; }
		else if (Local.Num()) DestSt = Local[static_cast<int32>(FMath::Floor(Rng.next() * Local.Num())) % Local.Num()];
		else continue;
		const double Dist = FVector::Dist(DestSys.Pos, Sys.Star.Pos);
		const bool bCourier = Rng.chance(0.35);
		const FCommodityDef* Com = Cheap[static_cast<int32>(FMath::Floor(Rng.next() * Cheap.Num())) % Cheap.Num()];
		const int32 Tons = bCourier ? 0 : Rng.intRange(2, 12) * 2;
		const int32 Reward = FMath::RoundToInt((900 + Dist * 320 + Tons * 75 + (DestSt ? 300 : 0)) * Rng.range(0.85, 1.3) / 10.0) * 10;
		const FString Client = CLIENTS[static_cast<int32>(FMath::Floor(Rng.next() * 8)) % 8];
		const FString Where = DestSt ? FString::Printf(TEXT("%s%s"), *DestSt->Name, DestSys.Id == Sys.Star.Id ? TEXT("") : *FString::Printf(TEXT(" in %s"), *DestSys.Name)) : FString::Printf(TEXT("any station in the %s system"), *DestSys.Name);
		FContract C;
		C.Id = FString::Printf(TEXT("%s-%d-%d"), *St.Id, InDay, i);
		C.bCourier = bCourier;
		C.Title = bCourier ? FString::Printf(TEXT("Courier: sealed data to %s"), *DestSys.Name) : FString::Printf(TEXT("Deliver %dt %s"), Tons, Com->Name);
		C.Description = bCourier ? FString::Printf(TEXT("%s needs encrypted data carried to %s. No cargo space required."), *CapFirst(Client), *Where)
			: FString::Printf(TEXT("%s requests %d tonnes of %s delivered to %s. Cargo is provided."), *CapFirst(Client), Tons, Com->Name, *Where);
		C.FromStation = St.Id; C.FromSystem = Sys.Star.Id;
		C.DestSystem = DestSys.Id; C.DestSystemName = DestSys.Name;
		C.DestStation = DestSt ? DestSt->Id : TEXT(""); C.DestStationName = DestSt ? DestSt->Name : TEXT("");
		C.Commodity = bCourier ? TEXT("") : Com->Id;
		C.Tons = Tons; C.Reward = Reward; C.DistanceLy = FMath::RoundToInt(Dist * 10) / 10.0;
		Out.Add(C);
	}
	return Out;
}

// ---------------------------------------------------------------- cargo
double UCommanderSubsystem::CargoTons() const { double T = 0; for (const FCargoItem& C : S.Cargo) T += C.Tons; return T; }
double UCommanderSubsystem::MissionTons() const { double T = 0; for (const FCargoItem& C : S.MissionCargo) T += C.Tons; return T; }
double UCommanderSubsystem::Held(const FString& Id) const { for (const FCargoItem& C : S.Cargo) if (C.Id == Id) return C.Tons; return 0; }

void UCommanderSubsystem::AddCargo(const FString& Id, double Tons)
{
	for (FCargoItem& C : S.Cargo) if (C.Id == Id) { C.Tons += Tons; if (C.Tons < 1e-6) S.Cargo.RemoveAll([&](const FCargoItem& X) { return X.Id == Id; }); return; }
	if (Tons > 1e-6) { FCargoItem C; C.Id = Id; C.Tons = Tons; S.Cargo.Add(C); }
}

bool UCommanderSubsystem::TakeCargo(const FString& Id, double Tons)
{
	if (Held(Id) + 1e-6 < Tons) return false;
	AddCargo(Id, -Tons);
	return true;
}

// ---------------------------------------------------------------- station actions
FString UCommanderSubsystem::Buy(const FStationData& St, const FString& Id, int32 Tons)
{
	const TArray<FMarketRow> Rows = Market(St, Day());
	const FMarketRow* R = Rows.FindByPredicate([&](const FMarketRow& X) { return X.Id == Id; });
	if (!R) return TEXT("Not sold here");
	const int32 Room = FMath::FloorToInt(CargoCap - TotalTons() + 1e-6);
	Tons = FMath::Min(Tons, Room);
	if (Tons <= 0) return TEXT("Hold is full");
	Tons = FMath::Min(Tons, R->Supply > 0 ? Tons : 0);
	if (Tons <= 0) return TEXT("None in stock");
	const int32 Cost = R->Buy * Tons;
	if (Cost > S.Credits) { Tons = S.Credits / FMath::Max(1, R->Buy); if (Tons <= 0) return TEXT("Not enough credits"); }
	S.Credits -= R->Buy * Tons;
	AddCargo(Id, Tons);
	Save();
	return FString::Printf(TEXT("Bought %d t %s for %d CR"), Tons, *R->Name, R->Buy * Tons);
}

FString UCommanderSubsystem::Sell(const FStationData& St, const FString& Id, int32 Tons)
{
	const TArray<FMarketRow> Rows = Market(St, Day());
	const FMarketRow* R = Rows.FindByPredicate([&](const FMarketRow& X) { return X.Id == Id; });
	if (!R) return TEXT("Not traded here");
	Tons = FMath::Min(Tons, FMath::FloorToInt(Held(Id) + 1e-6));
	if (Tons <= 0) return TEXT("Nothing of that aboard");
	TakeCargo(Id, Tons);
	S.Credits += R->Sell * Tons;
	S.Earned += R->Sell * Tons;
	Save();
	return FString::Printf(TEXT("Sold %d t %s for %d CR"), Tons, *R->Name, R->Sell * Tons);
}

FString UCommanderSubsystem::Refuel(double& ShipFuel, double FuelMax)
{
	const double Need = FuelMax - ShipFuel;
	if (Need < 0.05) return TEXT("Tanks are full");
	const int32 Cost = FMath::CeilToInt(Need * 55);
	if (Cost > S.Credits) return FString::Printf(TEXT("Refuelling costs %d CR - not enough credits"), Cost);
	S.Credits -= Cost; ShipFuel = FuelMax; S.Fuel = FuelMax; Save();
	return FString::Printf(TEXT("Refuelled %.1f t for %d CR"), Need, Cost);
}

FString UCommanderSubsystem::Repair(double& ShipHull)
{
	const double Missing = 1.0 - ShipHull;
	if (Missing < 0.005) return TEXT("Hull is sound");
	const int32 Cost = FMath::CeilToInt(Missing * 6000);
	if (Cost > S.Credits) return FString::Printf(TEXT("Repairs cost %d CR - not enough credits"), Cost);
	S.Credits -= Cost; ShipHull = 1.0; S.Hull = 1.0; Save();
	return FString::Printf(TEXT("Hull repaired for %d CR"), Cost);
}

int32 UCommanderSubsystem::DataValue() const { int32 V = 0; for (const FDataEntry& D : S.Data) V += D.Value; return V; }

FString UCommanderSubsystem::SellData()
{
	const int32 V = DataValue();
	if (V <= 0) return TEXT("No exploration data to sell");
	S.Credits += V; S.Earned += V;
	const int32 N = S.Data.Num();
	S.Data.Empty();
	Save();
	return FString::Printf(TEXT("Sold %d exploration records for %d CR"), N, V);
}

FString UCommanderSubsystem::Accept(const FContract& C)
{
	if (S.Contracts.Num() >= 6) return TEXT("Contract limit reached (6)");
	for (const FContract& X : S.Contracts) if (X.Id == C.Id) return TEXT("Already accepted");
	if (!C.bCourier && TotalTons() + C.Tons > CargoCap + 1e-6) return FString::Printf(TEXT("Needs %d t of free cargo space"), C.Tons);
	S.Contracts.Add(C);
	if (!C.bCourier) { FCargoItem M; M.Id = C.Commodity; M.Tons = C.Tons; S.MissionCargo.Add(M); }
	Save();
	return FString::Printf(TEXT("Contract accepted: %s  (%d CR)"), *C.Title, C.Reward);
}

FString UCommanderSubsystem::Abandon(const FString& ContractId)
{
	const int32 I = S.Contracts.IndexOfByPredicate([&](const FContract& X) { return X.Id == ContractId; });
	if (I < 0) return TEXT("No such contract");
	const FContract C = S.Contracts[I];
	const int32 Fee = FMath::RoundToInt(C.Reward * 0.1);
	S.Credits = FMath::Max(0, S.Credits - Fee);
	S.Contracts.RemoveAt(I);
	if (!C.bCourier) { const int32 J = S.MissionCargo.IndexOfByPredicate([&](const FCargoItem& X) { return X.Id == C.Commodity && FMath::IsNearlyEqual(X.Tons, C.Tons); }); if (J >= 0) S.MissionCargo.RemoveAt(J); }
	Save();
	return FString::Printf(TEXT("Contract abandoned - %d CR fee"), Fee);
}

TArray<FString> UCommanderSubsystem::CompleteContracts(const FString& StationId, const FString& SystemId)
{
	TArray<FString> Out;
	for (int32 i = S.Contracts.Num() - 1; i >= 0; i--)
	{
		const FContract& C = S.Contracts[i];
		if (C.DestSystem != SystemId) continue;
		if (!C.DestStation.IsEmpty() && C.DestStation != StationId) continue;
		S.Credits += C.Reward; S.Earned += C.Reward;
		if (!C.bCourier) { const int32 J = S.MissionCargo.IndexOfByPredicate([&](const FCargoItem& X) { return X.Id == C.Commodity && FMath::IsNearlyEqual(X.Tons, C.Tons); }); if (J >= 0) S.MissionCargo.RemoveAt(J); }
		Out.Add(FString::Printf(TEXT("Contract complete: %s  +%d CR"), *C.Title, C.Reward));
		S.Contracts.RemoveAt(i);
	}
	if (Out.Num()) Save();
	return Out;
}

// ---------------------------------------------------------------- exploration
int32 UCommanderSubsystem::VisitSystem(const FStarSystemData& Sys)
{
	if (S.Visited.Contains(Sys.Id)) return 0;
	S.Visited.Add(Sys.Id);
	S.SystemsDiscovered++;
	const bool bAi = Sys.Source == TEXT("ai");
	int32 V = 500 + 180 * Sys.Bodies.Num() + (bAi ? 400 : 0);
	if (Sys.DiscoveredBy == S.Commander && bAi) V = FMath::RoundToInt(V * 1.5); // first discovery bonus
	FDataEntry D; D.Id = TEXT("sys:") + Sys.Id; D.Label = FString::Printf(TEXT("%s - system survey"), *Sys.Name); D.Value = V;
	S.Data.Add(D);
	Save();
	return V;
}

bool UCommanderSubsystem::IsScanned(const FString& SysId, const FString& BodyId) const
{
	return S.Scanned.Contains(SysId + TEXT(":") + BodyId);
}

int32 UCommanderSubsystem::ScanBody(const FStarSystemData& Sys, const FBodyData& B, bool bMoon)
{
	const FString Key = Sys.Id + TEXT(":") + B.Id;
	if (S.Scanned.Contains(Key)) return 0;
	S.Scanned.Add(Key);
	double V = ScanValue(B.Type);
	if (bMoon) V *= 0.6;
	if (Sys.DiscoveredBy == S.Commander && Sys.Source == TEXT("ai")) V *= 1.5;
	FDataEntry D; D.Id = TEXT("scan:") + Key; D.Label = FString::Printf(TEXT("%s - surface scan (%s)"), *B.Name, *B.Type); D.Value = FMath::RoundToInt(V);
	S.Data.Add(D);
	Save();
	return D.Value;
}

double UCommanderSubsystem::JumpRangeLy(double Fuel) const
{
	return 18.0 * (62.0 + 16.0) / (62.0 + TotalTons() + Fuel) * 0.94 + 0.9;
}
