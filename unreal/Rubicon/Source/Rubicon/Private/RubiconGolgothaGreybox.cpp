#include "RubiconGolgothaGreybox.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RubiconAtmosphereDriverComponent.h"
#include "RubiconElevationDirectorComponent.h"
#include "RubiconEmbodiedBody.h"
#include "RubiconEpisodeAsset.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogRubiconGreybox, Log, All);

namespace RubiconGreyboxDetail
{
	UStaticMeshComponent* MakeBox(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, const FVector& Center, const FVector& HalfExtent)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(Center);
		C->SetRelativeScale3D(HalfExtent / 50.0); // engine basic shapes are 100 cm across
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		C->SetMobility(EComponentMobility::Movable); // WorldRoot tilts during elevation
		return C;
	}

	FRubiconGazePlacement Place(const TCHAR* Id, const FVector& Loc, ERubiconGazeResolution Res, ERubiconProxyShape Shape, const FVector& Extent,
		int32 Count, const TCHAR* Basis, const FLinearColor& Color = FLinearColor(0.35f, 0.32f, 0.28f), bool bCross = false)
	{
		FRubiconGazePlacement P;
		P.TargetId = FName(Id);
		P.Location = Loc;
		P.Resolution = Res;
		P.Shape = Shape;
		P.Extent = Extent;
		P.FigureCount = Count;
		P.Basis = Basis;
		P.Color = Color;
		P.bAttachToCross = bCross;
		return P;
	}
}

TArray<FRubiconGazePlacement> ARubiconGolgothaGreybox::DefaultLayout()
{
	using namespace RubiconGreyboxDetail;
	using R = ERubiconGazeResolution;
	using S = ERubiconProxyShape;
	TArray<FRubiconGazePlacement> L;
	L.Add(Place(TEXT("crowd"), FVector(1000, 0, 0), R::Cone, S::Figures, FVector(380, 650, 85), 40,
		TEXT("Specific Gospel groups, never a people: passers-by (Mark 15:29), chief priests with scribes and elders (Matthew 27:41), rulers (Luke 23:35), the people beholding (Luke 23:35). Distance: greybox.")));
	L.Add(Place(TEXT("soldiers"), FVector(260, -260, 0), R::Cone, S::Figures, FVector(90, 120, 88), 4,
		TEXT("John 19:23 (four parts for four soldiers); Matthew 27:36 (sitting down they watched him)."), FLinearColor(0.38f, 0.22f, 0.18f)));
	L.Add(Place(TEXT("condemned_man"), FVector(0, 380, 0), R::Cone, S::Box, FVector(22, 34, 128), 1,
		TEXT("Luke 23:39-43. One on either side (John 19:18); that the penitent hangs on the right is tradition only.")));
	L.Add(Place(TEXT("other_condemned"), FVector(0, -380, 0), R::Cone, S::Box, FVector(22, 34, 128), 1,
		TEXT("Luke 23:39; Mark 15:32. Side: tradition only.")));
	L.Add(Place(TEXT("mary_and_disciple"), FVector(180, 140, 0), R::Cone, S::Figures, FVector(40, 60, 82), 2,
		TEXT("John 19:25-27: standing by the cross."), FLinearColor(0.25f, 0.28f, 0.36f)));
	L.Add(Place(TEXT("women_at_distance"), FVector(2600, -1400, 0), R::Cone, S::Figures, FVector(120, 120, 82), 4,
		TEXT("Mark 15:40-41; Matthew 27:55-56; Luke 23:49: beholding afar off.")));
	L.Add(Place(TEXT("mourners"), FVector(1500, 900, 0), R::Cone, S::Figures, FVector(150, 200, 82), 6,
		TEXT("Luke 23:27-28 (women which bewailed and lamented him), 23:48 (smote their breasts).")));
	L.Add(Place(TEXT("soldier_with_sponge"), FVector(110, -70, 0), R::Cone, S::Figures, FVector(30, 30, 88), 1,
		TEXT("Matthew 27:48; Mark 15:36; John 19:29. Close enough to reach a low cross with a reed (historical inference)."), FLinearColor(0.38f, 0.22f, 0.18f)));
	L.Add(Place(TEXT("centurion"), FVector(330, 30, 0), R::Cone, S::Figures, FVector(30, 30, 92), 1,
		TEXT("Mark 15:39: stood over against him."), FLinearColor(0.45f, 0.18f, 0.15f)));
	L.Add(Place(TEXT("titulus"), FVector(8, 0, 262), R::Cone, S::Box, FVector(2, 30, 10), 1,
		TEXT("John 19:19-20 (Hebrew, Greek and Latin); Mark 15:26. On the cross above the head."), FLinearColor(0.6f, 0.55f, 0.45f), true));
	L.Add(Place(TEXT("cross"), FVector(-18, 0, 0), R::Collision, S::Cross, FVector(9, 100, 140), 1,
		TEXT("The wood itself: John 19:17 (bearing his cross), 19:19 (the title on the cross). A low cross is historical inference (the reed, Mark 15:36). Seen from the cross along the crossbeam and the upright above the head. The only gaze target that relieves."),
		FLinearColor(0.42f, 0.3f, 0.2f), true));
	L.Add(Place(TEXT("sky"), FVector(0, 0, 100000), R::SkyAbove, S::None, FVector::ZeroVector, 1,
		TEXT("Directional: gaze above the horizon band with nothing hit.")));
	L.Add(Place(TEXT("horizon"), FVector::ZeroVector, R::Horizon, S::HillRing, FVector(250000, 0, 6000), 24,
		TEXT("Low hills about 2.5 km out; also the fallback for level gaze with nothing tagged in view.")));
	L.Add(Place(TEXT("road"), FVector(1800, 0, 0), R::Collision, S::Box, FVector(250, 20000, 1), 1,
		TEXT("Mark 15:29 'they that passed by' implies a road; Roman practice chose crowded roads ([Quintilian], Minor Declamations 274)."), FLinearColor(0.5f, 0.45f, 0.36f)));
	L.Add(Place(TEXT("city_wall"), FVector(30000, 0, 0), R::Collision, S::Box, FVector(150, 40000, 400), 1,
		TEXT("John 19:20: nigh to the city. Line and height of the wall are reconstruction (about 300 m away here)."), FLinearColor(0.62f, 0.56f, 0.45f)));
	L.Add(Place(TEXT("garden"), FVector(700, 1700, 0), R::Cone, S::Figures, FVector(220, 260, 120), 7,
		TEXT("John 19:41: in the place where he was crucified there was a garden, and a new tomb. Placement is greybox."), FLinearColor(0.22f, 0.33f, 0.2f)));
	L.Add(Place(TEXT("ground_below"), FVector(19825, 0, -20), R::GroundBelow, S::Box, FVector(20175, 20000, 10), 1,
		TEXT("Ground in front of the knoll; also the fallback for downward gaze on untagged ground (road, quarry floor)."), FLinearColor(0.42f, 0.38f, 0.31f)));
	return L;
}

ARubiconGolgothaGreybox::ARubiconGolgothaGreybox()
{
	using namespace RubiconGreyboxDetail;
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("CrossBase"));
	RootComponent = Root;
	WorldRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WorldRoot"));
	WorldRoot->SetupAttachment(Root);
	WorldRoot->SetMobility(EComponentMobility::Movable);
	CrossRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CrossRoot"));
	CrossRoot->SetupAttachment(Root);
	CrossRoot->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cube = CubeMesh.Object;
	UStaticMesh* Sphere = SphereMesh.Object;

	// Quarry edge: the knoll stands at the lip of a worked-out quarry (historical inference: the traditional site
	// lay in a disused quarry outside the wall; John 19:20 "nigh to the city", John 19:41 a garden nearby).
	QuarryFloor = MakeBox(this, WorldRoot, TEXT("QuarryFloor"), Cube, FVector(-4350, 0, -710), FVector(4000, 6000, 10));
	QuarryFace = MakeBox(this, WorldRoot, TEXT("QuarryFace"), Cube, FVector(-360, 0, -350), FVector(10, 6000, 350));
	Knoll = MakeBox(this, WorldRoot, TEXT("Knoll"), Sphere, FVector(0, 0, -150), FVector(450, 450, 170));
	// The road and the city wall are gaze targets (points of interest), placed by Layout like every other target.
	const FName GroundTag = ARubiconGazeTargetActor::TagFor(TEXT("ground_below"));
	QuarryFloor->ComponentTags.Add(GroundTag);
	Knoll->ComponentTags.Add(GroundTag);

	// The cross itself is the `cross` gaze target in Layout (attached to CrossRoot), so gaze resolves on the wood.

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-48, 140, 0));
	Sun->bAtmosphereSunLight = true;
	Sun->Intensity = 10.f;
	Sun->bUseTemperature = true;
	Sun->Temperature = 5400.f;

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;

	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	HeightFog->SetupAttachment(Root);
	HeightFog->FogDensity = 0.004f;
	HeightFog->FogHeightFalloff = 0.2f;

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;

	AtmosphereDriver = CreateDefaultSubobject<URubiconAtmosphereDriverComponent>(TEXT("AtmosphereDriver"));
	ElevationDirector = CreateDefaultSubobject<URubiconElevationDirectorComponent>(TEXT("ElevationDirector"));

	Layout = DefaultLayout();
	WitnessVantages.Add(ERubiconWitnessVantage::BelovedDisciple, FVector(150, 110, 0));
	WitnessVantages.Add(ERubiconWitnessVantage::Mary, FVector(170, 165, 0));
	WitnessVantages.Add(ERubiconWitnessVantage::Centurion, FVector(360, 45, 0));
	WitnessVantages.Add(ERubiconWitnessVantage::WomenAfar, FVector(2450, -1250, 0));
}

FTransform ARubiconGolgothaGreybox::GetEmbodimentEyeTransform() const
{
	// Yaw comes from the cross; the embodied player always starts facing +X and turns only their own head.
	return FTransform(FRotator(0, GetActorRotation().Yaw, 0), GetActorTransform().TransformPosition(EyePoint));
}

FTransform ARubiconGolgothaGreybox::GetWitnessFloorTransform(ERubiconWitnessVantage Vantage) const
{
	const FVector* Local = WitnessVantages.Find(Vantage);
	const FVector P = GetActorTransform().TransformPosition(Local ? *Local : FVector(150, 110, 0));
	const FVector ToCross = GetActorLocation() - P;
	return FTransform(FRotator(0, ToCross.Rotation().Yaw, 0), P);
}

void ARubiconGolgothaGreybox::BeginPlay()
{
	Super::BeginPlay();
	const int32 Spawned = EnsureGazeTargets();
	if (Spawned > 0) UE_LOG(LogRubiconGreybox, Log, TEXT("spawned %d gaze target actors (none saved in the level)"), Spawned);
}

int32 ARubiconGolgothaGreybox::EnsureGazeTargets()
{
	UWorld* World = GetWorld();
	if (!World) return 0;

	TSet<FName> Present;
	bool bHasBody = false;
	for (TActorIterator<ARubiconGazeTargetActor> It(World); It; ++It) Present.Add(It->TargetId);
	for (TActorIterator<ARubiconEmbodiedBody> It(World); It; ++It) bHasBody = true;

	int32 Spawned = 0;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FRubiconGazePlacement& P : Layout)
	{
		if (Present.Contains(P.TargetId)) continue;
		USceneComponent* Parent = P.bAttachToCross ? CrossRoot.Get() : WorldRoot.Get();
		const FTransform T = FTransform(P.Rotation, P.Location) * Parent->GetComponentTransform();
		// Deferred so the proxy settings are in place before OnConstruction builds the proxies.
		ARubiconGazeTargetActor* A = World->SpawnActorDeferred<ARubiconGazeTargetActor>(ARubiconGazeTargetActor::StaticClass(), T, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!A) continue;
		A->TargetId = P.TargetId;
		A->Resolution = P.Resolution;
		A->ProxyShape = P.Shape;
		A->ProxyExtent = P.Extent;
		A->FigureCount = P.FigureCount;
		A->ProxyColor = P.Color;
#if WITH_EDITOR
		A->SetActorLabel(FString::Printf(TEXT("Gaze_%s"), *P.TargetId.ToString()));
#endif
		A->FinishSpawning(T);
		A->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);
		++Spawned;
	}

	if (!bHasBody)
	{
		ARubiconEmbodiedBody* Body = World->SpawnActor<ARubiconEmbodiedBody>(ARubiconEmbodiedBody::StaticClass(), CrossRoot->GetComponentTransform(), Params);
		if (Body)
		{
			Body->EyePoint = EyePoint;
			Body->RerunConstructionScripts();
			Body->AttachToComponent(CrossRoot, FAttachmentTransformRules::KeepWorldTransform);
#if WITH_EDITOR
			Body->SetActorLabel(TEXT("EmbodiedBody"));
#endif
			++Spawned;
		}
	}
	return Spawned;
}

TArray<FName> ARubiconGolgothaGreybox::FindMissingGazeTargets(const URubiconEpisodeAsset* Episode) const
{
	TArray<FName> Missing;
	if (!Episode || !GetWorld()) return Missing;
	TSet<FName> Tagged;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		for (const FName& Tag : It->Tags) Tagged.Add(Tag);
	}
	for (const FRubiconGazeTargetInfo& G : Episode->GazeTargets)
	{
		if (!Tagged.Contains(G.Tag)) Missing.Add(G.Id);
	}
	return Missing;
}
