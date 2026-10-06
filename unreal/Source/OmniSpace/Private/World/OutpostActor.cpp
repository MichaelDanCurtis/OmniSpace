#include "World/OutpostActor.h"
#include "World/SurfaceTile.h"
#include "World/OmniAssets.h"
#include "OmniSpace.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FLinearColor OpHull(0.6f, 0.62f, 0.66f);
	const FLinearColor OpHullDark(0.3f, 0.32f, 0.36f);
	const FLinearColor OpTrim(0.85f, 0.45f, 0.15f);
	const FLinearColor OpGlass(0.3f, 1.2f, 2.0f);
	const FLinearColor OpWarm(2.5f, 1.6f, 0.8f);
	const FLinearColor OpPanel(0.08f, 0.12f, 0.3f);
	const FLinearColor OpScaffold(2.4f, 1.1f, 0.2f);
	const FLinearColor OpGhostOk(0.2f, 1.6f, 0.5f);
	const FLinearColor OpGhostBad(1.8f, 0.25f, 0.2f);

	UStaticMeshComponent* OpPart(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& PosM, const FRotator& Rot, const FVector& SizeM, const FLinearColor& Color, bool bEmissive = false, float PanelScale = 1.2f)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh);
		C->SetupAttachment(Parent);
		C->SetRelativeLocation(PosM * 100.0);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(SizeM);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMobility(EComponentMobility::Movable);
		if (UMaterialInstanceDynamic* MID = OmniAssets::MakeMID(C, bEmissive ? OmniAssets::EmissiveMaterial() : OmniAssets::HullMaterial(), Owner))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			MID->SetScalarParameterValue(TEXT("PanelM"), PanelScale);
			MID->SetScalarParameterValue(TEXT("Metal"), 0.25f);
			MID->SetScalarParameterValue(TEXT("Windows"), 0.f);
		}
		C->RegisterComponent();
		return C;
	}

	/** Twelve thin beams along the edges of a box (local metres, centred at origin). */
	void OpFrame(AActor* Owner, USceneComponent* Parent, UStaticMesh* Cube, const FVector& Size, double Thick, const FLinearColor& Col)
	{
		const double hx = Size.X / 2, hy = Size.Y / 2, hz = Size.Z / 2;
		for (int32 sy = -1; sy <= 1; sy += 2) for (int32 sz = -1; sz <= 1; sz += 2)
			OpPart(Owner, Parent, Cube, FVector(0, sy * hy, hz + sz * hz), FRotator::ZeroRotator, FVector(Size.X, Thick, Thick), Col, true);
		for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sz = -1; sz <= 1; sz += 2)
			OpPart(Owner, Parent, Cube, FVector(sx * hx, 0, hz + sz * hz), FRotator::ZeroRotator, FVector(Thick, Size.Y, Thick), Col, true);
		for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sy = -1; sy <= 1; sy += 2)
			OpPart(Owner, Parent, Cube, FVector(sx * hx, sy * hy, hz), FRotator::ZeroRotator, FVector(Thick, Thick, Size.Z), Col, true);
	}
}

AOutpostActor::AOutpostActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AOutpostActor::Init(ASurfaceTile* InTile, FOutpostData* InData)
{
	Tile = InTile;
	Data = InData;
	Rebuild();
}

double AOutpostActor::FootZ(int32 GX, int32 GY, int32 W, int32 H) const
{
	if (!Tile) return 0;
	const FVector2D A = CellM(GX, GY), B = CellM(GX + W, GY + H);
	double Z = -1e9;
	for (int32 i = 0; i <= 2; i++) for (int32 j = 0; j <= 2; j++)
		Z = FMath::Max(Z, Tile->HeightAt(FMath::Lerp(A.X, B.X, i / 2.0), FMath::Lerp(A.Y, B.Y, j / 2.0)));
	return FMath::Max(Z, Tile->WaterLevel());
}

double AOutpostActor::FootSlope(int32 GX, int32 GY, int32 W, int32 H) const
{
	if (!Tile) return 0;
	const FVector2D A = CellM(GX, GY), B = CellM(GX + W, GY + H);
	double S = 0, Lo = 1e9, Hi = -1e9;
	for (int32 i = 0; i <= 2; i++) for (int32 j = 0; j <= 2; j++)
	{
		const double X = FMath::Lerp(A.X, B.X, i / 2.0), Y = FMath::Lerp(A.Y, B.Y, j / 2.0);
		S = FMath::Max(S, 1.0 - Tile->NormalAt(X, Y).Z);
		const double Z = Tile->HeightAt(X, Y);
		Lo = FMath::Min(Lo, Z); Hi = FMath::Max(Hi, Z);
	}
	// a tall step under the footprint counts as slope too
	return FMath::Max(S, (Hi - Lo) / FMath::Max(8.0, FMath::Max(W, H) * Outpost::CellM) * 0.6);
}

bool AOutpostActor::FootDry(int32 GX, int32 GY, int32 W, int32 H) const
{
	if (!Tile) return true;
	const FVector2D A = CellM(GX, GY), B = CellM(GX + W, GY + H);
	for (int32 i = 0; i <= 2; i++) for (int32 j = 0; j <= 2; j++)
		if (Tile->HeightAt(FMath::Lerp(A.X, B.X, i / 2.0), FMath::Lerp(A.Y, B.Y, j / 2.0)) <= Tile->WaterLevel() + 0.5) return false;
	return true;
}

bool AOutpostActor::Validate(EModuleKind Kind, int32 GX, int32 GY, int32 Rot, FString& Why) const
{
	if (!Data || !Tile) { Why = TEXT("No site"); return false; }
	FOutpostModule M; M.Kind = Kind; M.GX = GX; M.GY = GY; M.Rot = Rot;
	const int32 W = M.FootW(), H = M.FootH();
	const FModuleSpec& S = Outpost::Spec(Kind);
	if (Kind == EModuleKind::Beacon)
	{
		if (Data->HasBeacon()) { Why = TEXT("This outpost already has its beacon"); return false; }
	}
	else if (!Data->HasBeacon()) { Why = TEXT("Place the outpost beacon first"); return false; }
	if (!FootDry(GX, GY, W, H)) { Why = TEXT("Not over water"); return false; }
	if (FootSlope(GX, GY, W, H) > Outpost::MaxSlope) { Why = TEXT("Ground too steep - find flatter terrain"); return false; }
	const FVector2D C = CellM(GX, GY) + FVector2D(W, H) * Outpost::CellM * 0.5;
	if (Kind != EModuleKind::Beacon && (C - Data->OriginM).Size() > Outpost::RangeM) { Why = FString::Printf(TEXT("Too far from the beacon (%.0f m limit)"), Outpost::RangeM); return false; }
	bool bTouch = false;
	for (const FOutpostModule& O : Data->Modules)
	{
		if (O.Overlaps(GX, GY, W, H)) { Why = TEXT("Overlaps another module"); return false; }
		if (O.Touches(GX, GY, W, H) && O.Kind != EModuleKind::Power && O.Kind != EModuleKind::Pad) bTouch = true;
	}
	if (!S.bFreeStanding && !bTouch) { Why = TEXT("Must join an existing module edge to edge"); return false; }
	Why.Empty();
	return true;
}

USceneComponent* AOutpostActor::MakeNode(const FVector& PosCm)
{
	USceneComponent* N = NewObject<USceneComponent>(this);
	N->SetupAttachment(Root);
	N->SetMobility(EComponentMobility::Movable);
	N->SetRelativeLocation(PosCm);
	N->RegisterComponent();
	return N;
}

void AOutpostActor::DestroyNode(USceneComponent* Node)
{
	if (!Node) return;
	TArray<USceneComponent*> Kids;
	Node->GetChildrenComponents(true, Kids);
	for (USceneComponent* K : Kids) if (K) K->DestroyComponent();
	Node->DestroyComponent();
}

void AOutpostActor::BuildModuleVisual(USceneComponent* Node, const FOutpostModule& M, bool bGhost, bool bValid, USceneComponent*& OutRiser)
{
	OutRiser = nullptr;
	UStaticMesh* Cube = OmniAssets::CubeMesh();
	UStaticMesh* Cyl = OmniAssets::CylinderMesh();
	UStaticMesh* Sph = OmniAssets::SphereMesh();
	const FModuleSpec& S = Outpost::Spec(M.Kind);
	const int32 W = M.FootW(), H = M.FootH();
	const double FW = W * Outpost::CellM, FH = H * Outpost::CellM;   // footprint metres
	const FVector2D A = CellM(M.GX, M.GY);
	const double Z = FootZ(M.GX, M.GY, W, H);
	// node sits at the footprint centre, on the foundation level; everything below is local metres
	Node->SetRelativeLocation(FVector((A.X + FW / 2) * 100.0, (A.Y + FH / 2) * 100.0, Z * 100.0));
	Node->SetRelativeRotation(FRotator(0, M.Rot * 90.0, 0));
	// the un-rotated size: rotate 90 swaps W/H back to the spec's own frame
	const double SW = S.W * Outpost::CellM, SH = S.H * Outpost::CellM;

	if (bGhost)
	{
		const FLinearColor C = bValid ? OpGhostOk : OpGhostBad;
		OpFrame(this, Node, Cube, FVector(SW - 0.4, SH - 0.4, S.HeightM), 0.12, C);
		OpPart(this, Node, Cube, FVector(0, 0, 0.05), FRotator::ZeroRotator, FVector(SW - 0.4, SH - 0.4, 0.06), C * 0.35f, true);
		return;
	}

	// foundation slab: from the lowest ground under the footprint up to the pad level
	{
		double Lo = Z;
		const FVector2D B = CellM(M.GX + W, M.GY + H);
		for (int32 i = 0; i <= 2; i++) for (int32 j = 0; j <= 2; j++) Lo = FMath::Min(Lo, Tile ? Tile->HeightAt(FMath::Lerp(A.X, B.X, i / 2.0), FMath::Lerp(A.Y, B.Y, j / 2.0)) : Z);
		const double Depth = (Z - Lo) + 0.6;
		OpPart(this, Node, Cube, FVector(0, 0, -Depth / 2 + 0.3), FRotator::ZeroRotator, FVector(SW - 0.2, SH - 0.2, Depth), OpHullDark, false, 2.0f);
	}

	if (!M.bComplete)
	{
		// construction site: scaffold frame, a stack of delivered crates, the structure rising inside
		OpFrame(this, Node, Cube, FVector(SW - 0.6, SH - 0.6, S.HeightM), 0.15, OpScaffold);
		const double Frac = FMath::Clamp(M.Delivered.Total() / FMath::Max(1.0, S.Cost.Total()), 0.0, 1.0);
		const int32 Crates = FMath::RoundToInt(Frac * 6);
		for (int32 i = 0; i < Crates; i++)
			OpPart(this, Node, Cube, FVector(-SW / 2 + 1.2 + (i % 3) * 1.4, -SH / 2 + 1.2, 0.65 + (i / 3) * 1.1), FRotator(0, 10.0 * i, 0), FVector(1.2, 1.2, 1.0), OpTrim, false, 0.5f);
		USceneComponent* R = NewObject<USceneComponent>(this);
		R->SetupAttachment(Node);
		R->SetMobility(EComponentMobility::Movable);
		R->RegisterComponent();
		OpPart(this, R, Cube, FVector(0, 0, S.HeightM * 0.5), FRotator::ZeroRotator, FVector(SW - 1.2, SH - 1.2, S.HeightM), OpHull);
		R->SetRelativeScale3D(FVector(1, 1, FMath::Max(0.02, M.Progress)));
		R->SetRelativeLocation(FVector::ZeroVector);
		OutRiser = R;
		return;
	}

	switch (M.Kind)
	{
	case EModuleKind::Beacon:
	{
		OpPart(this, Node, Cyl, FVector(0, 0, 0.3), FRotator::ZeroRotator, FVector(3.0, 3.0, 0.6), OpHullDark, false, 2.0f);
		OpPart(this, Node, Cyl, FVector(0, 0, 3.2), FRotator::ZeroRotator, FVector(0.35, 0.35, 6.0), OpHull);
		OpPart(this, Node, Sph, FVector(0, 0, 6.4), FRotator::ZeroRotator, FVector(0.8, 0.8, 0.8), OpGlass, true);
		for (int32 i = 0; i < 3; i++)
			OpPart(this, Node, Cube, FVector(0, 0, 1.0 + i * 1.6), FRotator(0, i * 60.0, 0), FVector(2.2, 0.08, 0.08), OpTrim, true);
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(Node);
		L->SetRelativeLocation(FVector(0, 0, 640));
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(400.f);
		L->SetLightColor(FLinearColor(0.4f, 0.9f, 1.f));
		L->SetAttenuationRadius(6000.f);
		L->SetMobility(EComponentMobility::Movable);
		L->RegisterComponent();
		break;
	}
	case EModuleKind::Hab:
	case EModuleKind::Lab:
	{
		const double Hh = M.Kind == EModuleKind::Lab ? 4.0 : 3.6;
		OpPart(this, Node, Cube, FVector(0, 0, Hh / 2 + 0.6), FRotator::ZeroRotator, FVector(SW - 1.4, SH - 1.4, Hh), OpHull, false, 1.6f);
		// rounded roof: a cylinder laid along X
		OpPart(this, Node, Cyl, FVector(0, 0, Hh + 0.6), FRotator(90, 0, 0), FVector(3.0, SH - 1.4, SW - 1.6), OpHull, false, 1.6f);
		// window strips on the long sides, a door frame on each short side
		for (int32 s = -1; s <= 1; s += 2)
		{
			OpPart(this, Node, Cube, FVector(0, s * (SH - 1.4) / 2, Hh * 0.6 + 0.6), FRotator::ZeroRotator, FVector(SW - 3.0, 0.08, 0.7), OpGlass, true);
			OpPart(this, Node, Cube, FVector(s * (SW - 1.4) / 2, 0, 1.6), FRotator::ZeroRotator, FVector(0.3, 2.2, 2.6), OpHullDark, false, 0.6f);
			OpPart(this, Node, Cube, FVector(s * (SW - 1.4) / 2 + s * 0.1, 0, 1.6), FRotator::ZeroRotator, FVector(0.2, 1.6, 2.2), OpWarm, true);
		}
		// legs
		for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sy = -1; sy <= 1; sy += 2)
			OpPart(this, Node, Cyl, FVector(sx * (SW / 2 - 1.5), sy * (SH / 2 - 1.5), 0.3), FRotator::ZeroRotator, FVector(0.5, 0.5, 0.7), OpHullDark);
		if (M.Kind == EModuleKind::Lab)
		{
			OpPart(this, Node, Sph, FVector(0, 0, Hh + 1.2), FRotator::ZeroRotator, FVector(7.0, 7.0, 5.0), FLinearColor(0.75f, 0.8f, 0.9f), false, 0.8f);
			OpPart(this, Node, Cyl, FVector(SW / 2 - 2.5, SH / 2 - 2.5, Hh + 3.0), FRotator::ZeroRotator, FVector(0.25, 0.25, 5.0), OpHull);
			OpPart(this, Node, Sph, FVector(SW / 2 - 2.5, SH / 2 - 2.5, Hh + 5.6), FRotator::ZeroRotator, FVector(0.5, 0.5, 0.5), OpTrim, true);
		}
		break;
	}
	case EModuleKind::Power:
	{
		// two tilted panels on a post
		for (int32 i = -1; i <= 1; i += 2)
		{
			OpPart(this, Node, Cyl, FVector(0, i * SH * 0.25, 1.0), FRotator::ZeroRotator, FVector(0.3, 0.3, 2.0), OpHull);
			OpPart(this, Node, Cube, FVector(0, i * SH * 0.25, 2.2), FRotator(0, 0, 25), FVector(SW - 1.0, SH * 0.45, 0.12), OpPanel, false, 0.3f);
			OpPart(this, Node, Cube, FVector(0, i * SH * 0.25, 2.27), FRotator(0, 0, 25), FVector(SW - 1.4, SH * 0.45 - 0.4, 0.02), FLinearColor(0.15f, 0.3f, 0.8f), true);
		}
		OpPart(this, Node, Cube, FVector(0, 0, 0.5), FRotator::ZeroRotator, FVector(1.2, 1.2, 1.0), OpHullDark, false, 0.5f);
		break;
	}
	case EModuleKind::Storage:
	{
		for (int32 i = -1; i <= 1; i += 2)
		{
			OpPart(this, Node, Cyl, FVector(0, i * (SH / 4), 1.9), FRotator(90, 0, 0), FVector(3.0, 3.0, SW - 1.6), OpHull, false, 1.0f);
			for (int32 j = -1; j <= 1; j += 2) OpPart(this, Node, Cube, FVector(j * (SW / 2 - 2.0), i * (SH / 4), 0.6), FRotator::ZeroRotator, FVector(0.6, 3.4, 1.2), OpHullDark);
			OpPart(this, Node, Cube, FVector(0, i * (SH / 4), 3.45), FRotator::ZeroRotator, FVector(SW - 3.0, 0.3, 0.1), OpTrim, true);
		}
		break;
	}
	case EModuleKind::Corridor:
	{
		OpPart(this, Node, Cube, FVector(0, 0, 1.6 + 0.6), FRotator::ZeroRotator, FVector(SW - 0.4, 3.0, 3.0), OpHull, false, 1.0f);
		for (int32 i = -1; i <= 1; i++) OpPart(this, Node, Cube, FVector(i * 2.4, 0, 1.6 + 0.6), FRotator::ZeroRotator, FVector(0.3, 3.3, 3.3), OpHullDark);
		OpPart(this, Node, Cube, FVector(0, 0, 3.75), FRotator::ZeroRotator, FVector(SW - 1.0, 0.2, 0.08), OpGlass, true);
		break;
	}
	case EModuleKind::Pad:
	{
		OpPart(this, Node, Cyl, FVector(0, 0, 0.3), FRotator::ZeroRotator, FVector(SW - 1.0, SH - 1.0, 0.6), OpHullDark, false, 3.0f);
		for (int32 i = 0; i < 12; i++)
		{
			const double a = i / 12.0 * 2 * PI;
			OpPart(this, Node, Cube, FVector(FMath::Cos(a) * (SW / 2 - 1.2), FMath::Sin(a) * (SH / 2 - 1.2), 0.65), FRotator::ZeroRotator, FVector(0.4, 0.4, 0.1), (i % 3 == 0) ? OpWarm : OpGlass, true);
		}
		OpPart(this, Node, Cube, FVector(0, 0, 0.62), FRotator::ZeroRotator, FVector(1.0, 6.0, 0.04), OpWarm, true);
		OpPart(this, Node, Cube, FVector(0, 0, 0.62), FRotator::ZeroRotator, FVector(6.0, 1.0, 0.04), OpWarm, true);
		break;
	}
	default: break;
	}
}

void AOutpostActor::RebuildModule(int32 Index)
{
	if (!Data || !Data->Modules.IsValidIndex(Index)) return;
	while (Nodes.Num() <= Index) { Nodes.Add(nullptr); Risers.Add(nullptr); VisState.Add(-1); }
	DestroyNode(Nodes[Index]);
	Nodes[Index] = MakeNode(FVector::ZeroVector);
	USceneComponent* R = nullptr;
	BuildModuleVisual(Nodes[Index], Data->Modules[Index], false, true, R);
	Risers[Index] = R;
	VisState[Index] = Data->Modules[Index].bComplete ? 1 : 0;
}

void AOutpostActor::Rebuild()
{
	for (USceneComponent* N : Nodes) DestroyNode(N);
	Nodes.Empty(); Risers.Empty(); VisState.Empty();
	if (!Data) return;
	for (int32 i = 0; i < Data->Modules.Num(); i++) RebuildModule(i);
}

void AOutpostActor::SetGhost(EModuleKind Kind, int32 GX, int32 GY, int32 Rot, bool bValid)
{
	ClearGhost();
	GhostNode = MakeNode(FVector::ZeroVector);
	FOutpostModule M; M.Kind = Kind; M.GX = GX; M.GY = GY; M.Rot = Rot;
	USceneComponent* R = nullptr;
	BuildModuleVisual(GhostNode, M, true, bValid, R);
}

void AOutpostActor::ClearGhost()
{
	if (GhostNode) { DestroyNode(GhostNode); GhostNode = nullptr; }
}

bool AOutpostActor::AdvanceConstruction(float Dt)
{
	if (!Data) return false;
	bool bFinished = false;
	for (int32 i = 0; i < Data->Modules.Num(); i++)
	{
		FOutpostModule& M = Data->Modules[i];
		if (M.bComplete || !M.Supplied()) continue;
		M.Progress = FMath::Min(1.0, M.Progress + Dt / Outpost::Spec(M.Kind).BuildSeconds);
		if (Risers.IsValidIndex(i) && Risers[i]) Risers[i]->SetRelativeScale3D(FVector(1, 1, FMath::Max(0.02, M.Progress)));
		if (M.Progress >= 1.0)
		{
			M.bComplete = true;
			RebuildModule(i);
			bFinished = true;
			UE_LOG(LogOmniSpace, Log, TEXT("[outpost] %s complete"), Outpost::Spec(M.Kind).Name);
		}
	}
	return bFinished;
}

TArray<int32> AOutpostActor::SitesNeedingMaterials() const
{
	TArray<int32> Out;
	if (!Data) return Out;
	for (int32 i = 0; i < Data->Modules.Num(); i++)
		if (!Data->Modules[i].bComplete && !Data->Modules[i].Supplied()) Out.Add(i);
	return Out;
}

void AOutpostActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// the beacon light breathes; cheap way to make the place feel alive
	if (Data && Nodes.Num() > 0 && Nodes[0])
	{
		TArray<USceneComponent*> Kids;
		Nodes[0]->GetChildrenComponents(false, Kids);
		for (USceneComponent* K : Kids)
			if (UPointLightComponent* L = Cast<UPointLightComponent>(K))
				L->SetIntensity(300.f + 150.f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.0));
	}
}
