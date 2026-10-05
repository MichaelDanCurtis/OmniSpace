#include "Data/StarSystemData.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	double Num(const TSharedPtr<FJsonObject>& J, const TCHAR* Key, double Default = 0.0)
	{
		double V = Default;
		if (J.IsValid() && J->HasTypedField<EJson::Number>(Key)) J->TryGetNumberField(Key, V);
		return V;
	}
	FString Str(const TSharedPtr<FJsonObject>& J, const TCHAR* Key, const FString& Default = FString())
	{
		FString V = Default;
		if (J.IsValid() && J->HasTypedField<EJson::String>(Key)) J->TryGetStringField(Key, V);
		return V;
	}
	TArray<FString> StrArray(const TSharedPtr<FJsonObject>& J, const TCHAR* Key)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (J.IsValid() && J->TryGetArrayField(Key, Arr))
			for (const TSharedPtr<FJsonValue>& V : *Arr)
				if (V.IsValid() && V->Type == EJson::String) Out.Add(V->AsString());
		return Out;
	}
	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& J, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (J.IsValid() && J->TryGetObjectField(Key, O)) return *O;
		return nullptr;
	}
	TArray<TSharedPtr<FJsonValue>> StrValues(const TArray<FString>& In)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const FString& S : In) Out.Add(MakeShared<FJsonValueString>(S));
		return Out;
	}
}

FLinearColor OmniJson::HexToLinear(const FString& Hex, const FLinearColor& Fallback)
{
	FString H = Hex;
	H.RemoveFromStart(TEXT("#"));
	if (H.Len() != 6) return Fallback;
	FColor C = FColor::FromHex(H);
	return FLinearColor(C); // sRGB -> linear
}

FString OmniJson::LinearToHex(const FLinearColor& C)
{
	return TEXT("#") + C.ToFColor(true).ToHex().Left(6).ToLower();
}

// ---------------------------------------------------------------- star
TSharedRef<FJsonObject> FStarDescriptor::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("id"), Id);
	J->SetStringField(TEXT("name"), Name);
	J->SetStringField(TEXT("cls"), Cls);
	TArray<TSharedPtr<FJsonValue>> P{MakeShared<FJsonValueNumber>(Pos.X), MakeShared<FJsonValueNumber>(Pos.Y), MakeShared<FJsonValueNumber>(Pos.Z)};
	J->SetArrayField(TEXT("pos"), P);
	J->SetNumberField(TEXT("temperature"), Temperature);
	J->SetNumberField(TEXT("radiusSolar"), RadiusSolar);
	J->SetNumberField(TEXT("massSolar"), MassSolar);
	J->SetNumberField(TEXT("luminosity"), Luminosity);
	J->SetStringField(TEXT("color"), Color);
	J->SetNumberField(TEXT("ageGyr"), AgeGyr);
	J->SetNumberField(TEXT("settlement"), Settlement);
	J->SetStringField(TEXT("region"), Region);
	return J;
}

FStarDescriptor FStarDescriptor::FromJson(const TSharedPtr<FJsonObject>& J)
{
	FStarDescriptor S;
	if (!J.IsValid()) return S;
	S.Id = Str(J, TEXT("id"));
	S.Name = Str(J, TEXT("name"));
	S.Cls = Str(J, TEXT("cls"), TEXT("M"));
	const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
	if (J->TryGetArrayField(TEXT("pos"), P) && P->Num() == 3)
		S.Pos = FVector((*P)[0]->AsNumber(), (*P)[1]->AsNumber(), (*P)[2]->AsNumber());
	S.Temperature = Num(J, TEXT("temperature"), 5778);
	S.RadiusSolar = Num(J, TEXT("radiusSolar"), 1);
	S.MassSolar = Num(J, TEXT("massSolar"), 1);
	S.Luminosity = Num(J, TEXT("luminosity"), 1);
	S.Color = Str(J, TEXT("color"), TEXT("#fff4ea"));
	S.AgeGyr = Num(J, TEXT("ageGyr"), 4);
	S.Settlement = Num(J, TEXT("settlement"), 0);
	S.Region = Str(J, TEXT("region"));
	return S;
}

FLinearColor FStarDescriptor::LinearColor() const
{
	return OmniJson::HexToLinear(Color, FLinearColor(1, 0.95, 0.9));
}

// ---------------------------------------------------------------- system
static FBodyData ParseBody(const TSharedPtr<FJsonObject>& J)
{
	FBodyData B;
	B.Id = Str(J, TEXT("id"));
	B.Name = Str(J, TEXT("name"));
	B.Kind = Str(J, TEXT("kind"), TEXT("planet"));
	B.Type = Str(J, TEXT("type"), TEXT("rocky"));
	B.Parent = Str(J, TEXT("parent"), TEXT("star"));
	B.OrbitM = Num(J, TEXT("orbitM"));
	B.Phase = Num(J, TEXT("phase"));
	B.Inclination = Num(J, TEXT("inclination"));
	B.RadiusM = FMath::Max(1.0, Num(J, TEXT("radiusM"), 1e6));
	B.Palette = StrArray(J, TEXT("palette"));
	while (B.Palette.Num() < 3) B.Palette.Add(TEXT("#888888"));
	B.Atmosphere = Str(J, TEXT("atmosphere"));
	B.Clouds = Num(J, TEXT("clouds"));
	B.Bands = Num(J, TEXT("bands"));
	B.Tilt = Num(J, TEXT("tilt"));
	if (TSharedPtr<FJsonObject> R = Obj(J, TEXT("rings")))
	{
		B.bHasRings = true;
		B.Rings.InnerM = Num(R, TEXT("innerM"));
		B.Rings.OuterM = Num(R, TEXT("outerM"));
		B.Rings.Color = Str(R, TEXT("color"), TEXT("#d0ccc4"));
		B.Rings.Opacity = Num(R, TEXT("opacity"), 0.7);
	}
	B.TemperatureK = Num(J, TEXT("temperatureK"));
	B.GravityG = Num(J, TEXT("gravityG"));
	B.Description = Str(J, TEXT("description"));
	return B;
}

static FStationData ParseStation(const TSharedPtr<FJsonObject>& J)
{
	FStationData S;
	S.Id = Str(J, TEXT("id"));
	S.Name = Str(J, TEXT("name"));
	S.Kind = Str(J, TEXT("kind"), TEXT("outpost"));
	S.Parent = Str(J, TEXT("parent"));
	S.OrbitM = Num(J, TEXT("orbitM"));
	S.Phase = Num(J, TEXT("phase"));
	S.Inclination = Num(J, TEXT("inclination"));
	S.Faction = Str(J, TEXT("faction"));
	S.Economy = Str(J, TEXT("economy"));
	S.Services = StrArray(J, TEXT("services"));
	S.Tagline = Str(J, TEXT("tagline"));
	S.Description = Str(J, TEXT("description"));
	return S;
}

bool FStarSystemData::FromJson(const TSharedPtr<FJsonObject>& J, FStarSystemData& Out)
{
	if (!J.IsValid()) return false;
	Out = FStarSystemData();
	Out.Id = Str(J, TEXT("id"));
	Out.Name = Str(J, TEXT("name"));
	Out.Source = Str(J, TEXT("source"), TEXT("procedural"));
	Out.Model = Str(J, TEXT("model"));
	Out.DiscoveredBy = Str(J, TEXT("discoveredBy"));
	Out.Star = FStarDescriptor::FromJson(Obj(J, TEXT("star")));
	Out.StarFlavor = Str(J, TEXT("starFlavor"));
	if (TSharedPtr<FJsonObject> P = Obj(J, TEXT("profile")))
	{
		Out.Profile.Description = Str(P, TEXT("description"));
		Out.Profile.Lore = Str(P, TEXT("lore"));
		Out.Profile.Faction = Str(P, TEXT("faction"), TEXT("Unclaimed"));
		Out.Profile.Government = Str(P, TEXT("government"), TEXT("None"));
		Out.Profile.Economy = Str(P, TEXT("economy"), TEXT("None"));
		Out.Profile.Security = Str(P, TEXT("security"), TEXT("none"));
		Out.Profile.Population = static_cast<int64>(Num(P, TEXT("population")));
		Out.Profile.Danger = static_cast<int32>(Num(P, TEXT("danger")));
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (J->TryGetArrayField(TEXT("bodies"), Arr))
		for (const auto& V : *Arr) if (V->Type == EJson::Object) Out.Bodies.Add(ParseBody(V->AsObject()));
	if (J->TryGetArrayField(TEXT("stations"), Arr))
		for (const auto& V : *Arr) if (V->Type == EJson::Object) Out.Stations.Add(ParseStation(V->AsObject()));
	if (J->TryGetArrayField(TEXT("belts"), Arr))
		for (const auto& V : *Arr)
			if (V->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> B = V->AsObject();
				FBeltData D;
				D.Id = Str(B, TEXT("id"));
				D.Name = Str(B, TEXT("name"));
				D.Parent = Str(B, TEXT("parent"), TEXT("star"));
				D.RadiusM = Num(B, TEXT("radiusM"));
				D.WidthM = Num(B, TEXT("widthM"));
				D.Composition = Str(B, TEXT("composition"), TEXT("rocky"));
				Out.Belts.Add(D);
			}
	if (J->TryGetArrayField(TEXT("pois"), Arr))
		for (const auto& V : *Arr)
			if (V->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> P = V->AsObject();
				FPoiData D;
				D.Id = Str(P, TEXT("id"));
				D.Name = Str(P, TEXT("name"));
				D.Kind = Str(P, TEXT("kind"), TEXT("signal"));
				D.Parent = Str(P, TEXT("parent"));
				D.OffsetM = Num(P, TEXT("offsetM"));
				D.Phase = Num(P, TEXT("phase"));
				D.Description = Str(P, TEXT("description"));
				D.Reward = static_cast<int32>(Num(P, TEXT("reward")));
				Out.Pois.Add(D);
			}
	return !Out.Id.IsEmpty();
}

bool FStarSystemData::FromJsonString(const FString& Text, FStarSystemData& Out)
{
	TSharedPtr<FJsonObject> J;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, J) || !J.IsValid()) return false;
	return FromJson(J, Out);
}

TSharedRef<FJsonObject> FStarSystemData::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("id"), Id);
	J->SetStringField(TEXT("name"), Name);
	J->SetStringField(TEXT("source"), Source);
	if (!Model.IsEmpty()) J->SetStringField(TEXT("model"), Model);
	if (!DiscoveredBy.IsEmpty()) J->SetStringField(TEXT("discoveredBy"), DiscoveredBy);
	J->SetObjectField(TEXT("star"), Star.ToJson());
	J->SetStringField(TEXT("starFlavor"), StarFlavor);
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("description"), Profile.Description);
	P->SetStringField(TEXT("lore"), Profile.Lore);
	P->SetStringField(TEXT("faction"), Profile.Faction);
	P->SetStringField(TEXT("government"), Profile.Government);
	P->SetStringField(TEXT("economy"), Profile.Economy);
	P->SetStringField(TEXT("security"), Profile.Security);
	P->SetNumberField(TEXT("population"), static_cast<double>(Profile.Population));
	P->SetNumberField(TEXT("danger"), Profile.Danger);
	J->SetObjectField(TEXT("profile"), P);
	// Suffixed to avoid shadowing the members of the same name (-Wshadow is an error).
	TArray<TSharedPtr<FJsonValue>> BodiesJson, StationsJson, BeltsJson, PoisJson;
	for (const FBodyData& B : this->Bodies)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), B.Id);
		O->SetStringField(TEXT("name"), B.Name);
		O->SetStringField(TEXT("kind"), B.Kind);
		O->SetStringField(TEXT("type"), B.Type);
		O->SetStringField(TEXT("parent"), B.Parent);
		O->SetNumberField(TEXT("orbitM"), B.OrbitM);
		O->SetNumberField(TEXT("phase"), B.Phase);
		O->SetNumberField(TEXT("inclination"), B.Inclination);
		O->SetNumberField(TEXT("radiusM"), B.RadiusM);
		O->SetArrayField(TEXT("palette"), StrValues(B.Palette));
		if (B.Atmosphere.IsEmpty()) O->SetField(TEXT("atmosphere"), MakeShared<FJsonValueNull>());
		else O->SetStringField(TEXT("atmosphere"), B.Atmosphere);
		O->SetNumberField(TEXT("clouds"), B.Clouds);
		O->SetNumberField(TEXT("bands"), B.Bands);
		O->SetNumberField(TEXT("tilt"), B.Tilt);
		if (B.bHasRings)
		{
			TSharedRef<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetNumberField(TEXT("innerM"), B.Rings.InnerM);
			R->SetNumberField(TEXT("outerM"), B.Rings.OuterM);
			R->SetStringField(TEXT("color"), B.Rings.Color);
			R->SetNumberField(TEXT("opacity"), B.Rings.Opacity);
			O->SetObjectField(TEXT("rings"), R);
		}
		else O->SetField(TEXT("rings"), MakeShared<FJsonValueNull>());
		O->SetNumberField(TEXT("temperatureK"), B.TemperatureK);
		O->SetNumberField(TEXT("gravityG"), B.GravityG);
		O->SetStringField(TEXT("description"), B.Description);
		BodiesJson.Add(MakeShared<FJsonValueObject>(O));
	}
	for (const FStationData& S : this->Stations)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), S.Id);
		O->SetStringField(TEXT("name"), S.Name);
		O->SetStringField(TEXT("kind"), S.Kind);
		O->SetStringField(TEXT("parent"), S.Parent);
		O->SetNumberField(TEXT("orbitM"), S.OrbitM);
		O->SetNumberField(TEXT("phase"), S.Phase);
		O->SetNumberField(TEXT("inclination"), S.Inclination);
		O->SetStringField(TEXT("faction"), S.Faction);
		O->SetStringField(TEXT("economy"), S.Economy);
		O->SetArrayField(TEXT("services"), StrValues(S.Services));
		O->SetStringField(TEXT("tagline"), S.Tagline);
		O->SetStringField(TEXT("description"), S.Description);
		StationsJson.Add(MakeShared<FJsonValueObject>(O));
	}
	for (const FBeltData& B : this->Belts)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), B.Id);
		O->SetStringField(TEXT("name"), B.Name);
		O->SetStringField(TEXT("parent"), B.Parent);
		O->SetNumberField(TEXT("radiusM"), B.RadiusM);
		O->SetNumberField(TEXT("widthM"), B.WidthM);
		O->SetStringField(TEXT("composition"), B.Composition);
		BeltsJson.Add(MakeShared<FJsonValueObject>(O));
	}
	for (const FPoiData& Pz : this->Pois)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), Pz.Id);
		O->SetStringField(TEXT("name"), Pz.Name);
		O->SetStringField(TEXT("kind"), Pz.Kind);
		O->SetStringField(TEXT("parent"), Pz.Parent);
		O->SetNumberField(TEXT("offsetM"), Pz.OffsetM);
		O->SetNumberField(TEXT("phase"), Pz.Phase);
		O->SetStringField(TEXT("description"), Pz.Description);
		O->SetNumberField(TEXT("reward"), Pz.Reward);
		PoisJson.Add(MakeShared<FJsonValueObject>(O));
	}
	J->SetArrayField(TEXT("bodies"), BodiesJson);
	J->SetArrayField(TEXT("stations"), StationsJson);
	J->SetArrayField(TEXT("belts"), BeltsJson);
	J->SetArrayField(TEXT("pois"), PoisJson);
	return J;
}

const FBodyData* FStarSystemData::FindBody(const FString& BodyId) const
{
	return Bodies.FindByPredicate([&](const FBodyData& B) { return B.Id == BodyId; });
}

const FStationData* FStarSystemData::FindStation(const FString& StationId) const
{
	return Stations.FindByPredicate([&](const FStationData& S) { return S.Id == StationId; });
}
