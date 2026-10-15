// Drives sun, sky and fog from the manifest's per-state atmosphere keys (FRubiconFrameOutputs.Atmosphere).
//
// The darkness from the sixth to the ninth hour is rising aerosol optical depth with falling illuminance and
// colour temperature: Mie scattering and absorption scale with AOD, the sun dims in lux, the sky light
// recaptures. Never a fade to black and never an eclipse disc. Transitions are first-order (tau seconds), with
// illuminance interpolated in log space so the eye reads it as a steady loss of light.
//
// Mapping from AOD to engine coefficients is artistic tuning (evidence: artistic-interpretation), anchored so
// the third-hour key (AOD 0.25) reproduces the engine's clear-sky defaults.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RubiconAtmosphereDriverComponent.generated.h"

UCLASS(ClassGroup = (Rubicon), meta = (BlueprintSpawnableComponent))
class RUBICON_API URubiconAtmosphereDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URubiconAtmosphereDriverComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Atmosphere", meta = (ClampMin = "0.1"))
	float TransitionTauSeconds = 8.f;

	/** AOD at which the engine's default Mie coefficients apply. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere")
	float ReferenceAerosolOpticalDepth = 0.25f;

	/** Exposure window (EV100) for HDR renderers: wide enough that the darkness reads as darkness, never as black. */
	UPROPERTY(EditAnywhere, Category = "Exposure") float MinEV100 = 8.5f;
	UPROPERTY(EditAnywhere, Category = "Exposure") float MaxEV100 = 15.f;
	UPROPERTY(EditAnywhere, Category = "Exposure") float AdaptationSpeed = 0.35f;

	/** LDR (mobile without HDR): map [MinLux, MaxLux] logarithmically onto [MinLdrIntensity, MaxLdrIntensity]. */
	UPROPERTY(EditAnywhere, Category = "LDR") float MinLux = 100.f;
	UPROPERTY(EditAnywhere, Category = "LDR") float MaxLux = 100000.f;
	UPROPERTY(EditAnywhere, Category = "LDR") float MinLdrIntensity = 0.35f;
	UPROPERTY(EditAnywhere, Category = "LDR") float MaxLdrIntensity = 10.f;

private:
	bool bInitialized = false;
	double LogLux = 0;
	double CctK = 5400;
	double Aod = 0.25;
	float DefaultMieScattering = 0.003996f;
	float DefaultMieAbsorption = 0.000444f;
	float DefaultFogDensity = 0.004f;
};
