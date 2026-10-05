#include "Core/OmniGameInstance.h"
#include "OmniSpace.h"

void UOmniGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogOmniSpace, Log, TEXT("OmniSpace game instance ready"));
}
