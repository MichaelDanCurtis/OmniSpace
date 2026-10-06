// Outposts: prefab modules placed on a landing tile, built by delivering materials.
// Starfield-style habs on a grid, Elite-style construction sites that need commodities hauled in.
#pragma once

#include "CoreMinimal.h"
#include "OutpostData.generated.h"

/** Construction materials, in tons. Also the ship's cargo for the first pass of the economy. */
USTRUCT(BlueprintType)
struct FMaterials
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) double Alloy = 0;       // structural alloy
	UPROPERTY(BlueprintReadOnly) double Composites = 0;  // hull composites, glazing
	UPROPERTY(BlueprintReadOnly) double Electronics = 0; // power, life support, computers

	double Total() const { return Alloy + Composites + Electronics; }
	bool Covers(const FMaterials& Need) const { return Alloy >= Need.Alloy - 1e-6 && Composites >= Need.Composites - 1e-6 && Electronics >= Need.Electronics - 1e-6; }
	FMaterials Minus(const FMaterials& O) const { FMaterials R; R.Alloy = FMath::Max(0.0, Alloy - O.Alloy); R.Composites = FMath::Max(0.0, Composites - O.Composites); R.Electronics = FMath::Max(0.0, Electronics - O.Electronics); return R; }
};

UENUM(BlueprintType)
enum class EModuleKind : uint8
{
	Beacon,    // outpost marker: the first thing placed, anchors the grid
	Hab,       // living module, 2x2
	Power,     // solar array, 1x2, free-standing
	Storage,   // tanks, 2x1
	Corridor,  // 1x1 link
	Lab,       // domed research module, 2x2
	Pad,       // landing pad, 3x3, free-standing
	Count UMETA(Hidden)
};

/** Catalog entry: what a module is and what it costs. */
struct FModuleSpec
{
	EModuleKind Kind;
	const TCHAR* Name;
	int32 W, H;             // footprint in grid cells (8 m each), before rotation
	double HeightM;         // visual height
	bool bFreeStanding;     // may be placed without touching another module
	FMaterials Cost;
	double BuildSeconds;    // time from fully supplied to complete
	const TCHAR* Blurb;
};

namespace Outpost
{
	constexpr double CellM = 8.0;       // grid cell
	constexpr double RangeM = 260.0;    // modules must sit within this of the beacon
	constexpr double DeliverM = 160.0;  // the ship must land within this of the beacon to unload
	constexpr double MaxSlope = 0.12;   // 1 - N.z

	inline const FModuleSpec& Spec(EModuleKind K)
	{
		static const FModuleSpec Specs[] = {
			{ EModuleKind::Beacon,   TEXT("Outpost beacon"), 1, 1,  6.0, true,  { 1, 0, 1 },   8,  TEXT("Claims the site and anchors the build grid") },
			{ EModuleKind::Hab,      TEXT("Habitat"),        2, 2,  5.0, false, { 18, 8, 4 },  40, TEXT("Living quarters: pressurised, windows, airlock") },
			{ EModuleKind::Power,    TEXT("Solar array"),    1, 2,  3.0, true,  { 4, 2, 6 },   20, TEXT("Powers the outpost; stands alone") },
			{ EModuleKind::Storage,  TEXT("Storage tanks"),  2, 1,  4.0, false, { 10, 2, 1 },  25, TEXT("Bulk storage for fuel, water and ore") },
			{ EModuleKind::Corridor, TEXT("Corridor"),       1, 1,  3.0, false, { 3, 2, 0 },   10, TEXT("Links modules") },
			{ EModuleKind::Lab,      TEXT("Laboratory"),     2, 2,  6.0, false, { 16, 10, 12 }, 50, TEXT("Research dome: surveys, samples, the Forge's field station") },
			{ EModuleKind::Pad,      TEXT("Landing pad"),    3, 3,  0.6, true,  { 12, 0, 3 },  30, TEXT("A lit pad: the ship sets down here on return") },
		};
		return Specs[FMath::Clamp(static_cast<int32>(K), 0, static_cast<int32>(EModuleKind::Count) - 1)];
	}
}

USTRUCT(BlueprintType)
struct FOutpostModule
{
	GENERATED_BODY()
	UPROPERTY() EModuleKind Kind = EModuleKind::Hab;
	UPROPERTY() int32 GX = 0;          // grid cell of the footprint's min corner
	UPROPERTY() int32 GY = 0;
	UPROPERTY() int32 Rot = 0;         // 0..3 quarter turns
	UPROPERTY() FMaterials Delivered;
	UPROPERTY() double Progress = 0;   // 0..1 once supplied
	UPROPERTY() bool bComplete = false;

	int32 FootW() const { const FModuleSpec& S = Outpost::Spec(Kind); return (Rot % 2) ? S.H : S.W; }
	int32 FootH() const { const FModuleSpec& S = Outpost::Spec(Kind); return (Rot % 2) ? S.W : S.H; }
	bool Supplied() const { return Delivered.Covers(Outpost::Spec(Kind).Cost); }
	FMaterials Remaining() const { return Outpost::Spec(Kind).Cost.Minus(Delivered); }
	bool Overlaps(int32 X, int32 Y, int32 W, int32 H) const { return X < GX + FootW() && X + W > GX && Y < GY + FootH() && Y + H > GY; }
	bool Touches(int32 X, int32 Y, int32 W, int32 H) const
	{
		const bool bX = X < GX + FootW() && X + W > GX;
		const bool bY = Y < GY + FootH() && Y + H > GY;
		return (bX && (Y == GY + FootH() || Y + H == GY)) || (bY && (X == GX + FootW() || X + W == GX));
	}
};

USTRUCT(BlueprintType)
struct FOutpostData
{
	GENERATED_BODY()
	UPROPERTY() FString BodyId;
	UPROPERTY() FString SystemId;
	UPROPERTY() FVector2D Site = FVector2D::ZeroVector;
	UPROPERTY() FString Name;
	UPROPERTY() FVector2D OriginM = FVector2D::ZeroVector; // beacon position, tile metres: grid (0,0)
	UPROPERTY() TArray<FOutpostModule> Modules;

	bool HasBeacon() const { for (const FOutpostModule& M : Modules) if (M.Kind == EModuleKind::Beacon) return true; return false; }
	int32 Built() const { int32 N = 0; for (const FOutpostModule& M : Modules) if (M.bComplete) N++; return N; }
};

USTRUCT()
struct FOutpostSave
{
	GENERATED_BODY()
	UPROPERTY() TArray<FOutpostData> Outposts;
};
