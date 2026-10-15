#include "Modules/ModuleManager.h"
#include "NarrativeCore/RubiconMath.h"

DEFINE_LOG_CATEGORY_STATIC(LogRubiconCore, Log, All);

class FRubiconNarrativeCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Replay parity (and cross-platform determinism) requires IEEE binary64 without FMA contraction or
		// fast-math in the core. Fail loudly at startup rather than diverge silently later.
		std::string Failure;
		if (!RubiconCore::SelfTestDeterministicMath(&Failure))
		{
			UE_LOG(LogRubiconCore, Error, TEXT("Rubicon narrative core floating-point self-test failed: %s. Replays will not match the TypeScript reference on this build."), UTF8_TO_TCHAR(Failure.c_str()));
		}
	}
};

IMPLEMENT_MODULE(FRubiconNarrativeCoreModule, RubiconNarrativeCore)
