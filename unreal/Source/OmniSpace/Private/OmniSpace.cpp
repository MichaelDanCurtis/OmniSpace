#include "OmniSpace.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogOmniSpace);

void FOmniSpaceModule::StartupModule()
{
	// Expose Shaders/ as /Project/ so Custom material nodes can #include noise.ush
	FString ShaderDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
	if (!AllShaderSourceDirectoryMappings().Contains(TEXT("/Project")))
		AddShaderSourceDirectoryMapping(TEXT("/Project"), ShaderDir);
}
void FOmniSpaceModule::ShutdownModule() {}

IMPLEMENT_PRIMARY_GAME_MODULE(FOmniSpaceModule, OmniSpace, "OmniSpace");
