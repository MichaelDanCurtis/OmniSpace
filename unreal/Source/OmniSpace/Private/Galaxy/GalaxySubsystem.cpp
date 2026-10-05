#include "Galaxy/GalaxySubsystem.h"
#include "Galaxy/GalaxyCore.h"

using namespace omni::galaxy;

void UGalaxySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	delete Galaxy;
	Galaxy = new omni::galaxy::Galaxy();
}

void UGalaxySubsystem::Deinitialize()
{
	delete Galaxy;
	Galaxy = nullptr;
	Super::Deinitialize();
}

FStarDescriptor UGalaxySubsystem::Convert(const Star& S)
{
	FStarDescriptor D;
	D.Id = UTF8_TO_TCHAR(S.id.c_str());
	D.Name = UTF8_TO_TCHAR(S.name.c_str());
	D.Cls = className(S.cls);
	D.Pos = FVector(S.pos[0], S.pos[1], S.pos[2]);
	D.Temperature = S.temperature;
	D.RadiusSolar = S.radiusSolar;
	D.MassSolar = S.massSolar;
	D.Luminosity = S.luminosity;
	D.Color = UTF8_TO_TCHAR(S.color.c_str());
	D.AgeGyr = S.ageGyr;
	D.Settlement = S.settlement;
	D.Region = UTF8_TO_TCHAR(S.region.c_str());
	return D;
}

bool UGalaxySubsystem::GetStar(const FString& Id, FStarDescriptor& Out)
{
	const Star* S = Galaxy->get(TCHAR_TO_UTF8(*Id));
	if (!S) return false;
	Out = Convert(*S);
	return true;
}

TArray<FStarDescriptor> UGalaxySubsystem::StarsNear(const FVector& PosLy, double RadiusLy)
{
	double P[3] = {PosLy.X, PosLy.Y, PosLy.Z};
	TArray<FStarDescriptor> Out;
	for (const Star* S : Galaxy->nearby(P, RadiusLy)) Out.Add(Convert(*S));
	return Out;
}

TArray<FString> UGalaxySubsystem::PlotRoute(const FString& FromId, const FString& ToId, double JumpRangeLy)
{
	TArray<FString> Out;
	const Star* A = Galaxy->get(TCHAR_TO_UTF8(*FromId));
	const Star* B = Galaxy->get(TCHAR_TO_UTF8(*ToId));
	if (!A || !B) return Out;
	for (const Star* S : Galaxy->route(*A, *B, JumpRangeLy)) Out.Add(UTF8_TO_TCHAR(S->id.c_str()));
	return Out;
}

double UGalaxySubsystem::DistanceLy(const FStarDescriptor& A, const FStarDescriptor& B)
{
	return FVector::Dist(A.Pos, B.Pos);
}

FString UGalaxySubsystem::ClassLabel(const FString& Cls)
{
	static const TMap<FString, FString> Labels = {
		{TEXT("O"), TEXT("O-type blue supergiant")}, {TEXT("B"), TEXT("B-type blue-white star")}, {TEXT("A"), TEXT("A-type white star")},
		{TEXT("F"), TEXT("F-type yellow-white star")}, {TEXT("G"), TEXT("G-type yellow star")}, {TEXT("K"), TEXT("K-type orange dwarf")},
		{TEXT("M"), TEXT("M-type red dwarf")}, {TEXT("L"), TEXT("L-type brown dwarf")}, {TEXT("WD"), TEXT("White dwarf")}, {TEXT("NS"), TEXT("Neutron star")},
	};
	const FString* L = Labels.Find(Cls);
	return L ? *L : Cls;
}

bool UGalaxySubsystem::IsScoopable(const FString& Cls)
{
	return Cls != TEXT("L") && Cls != TEXT("WD") && Cls != TEXT("NS");
}
