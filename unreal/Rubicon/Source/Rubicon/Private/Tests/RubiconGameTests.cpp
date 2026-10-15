// Automation tests for the engine adapter's pure logic: Rubicon.Game.*

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RubiconConvert.h"
#include "RubiconElevationDirectorComponent.h"
#include "RubiconEmbodiedBody.h"
#include "RubiconEmbodiedPawn.h"
#include "RubiconGazeResolverComponent.h"
#include "RubiconGolgothaGreybox.h"
#include "NarrativeCore/RubiconContent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubiconTwoBoneIKTest, "Rubicon.Game.TwoBoneIK",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubiconTwoBoneIKTest::RunTest(const FString& Parameters)
{
	const FVector Root(0, 0, 0), Target(30, 20, 10), Pole(0, 0, -1);
	const FRubiconTwoBoneSolution S = ARubiconEmbodiedBody::SolveTwoBone(Root, Target, 30, 27, Pole);
	TestTrue(TEXT("reachable target is reached"), S.bReached);
	TestTrue(TEXT("end effector on target"), S.End.Equals(Target, 1e-6));
	TestEqual(TEXT("upper bone length preserved"), (S.Joint - S.Root).Size(), 30.0, 1e-6);
	TestEqual(TEXT("lower bone length preserved"), (S.End - S.Joint).Size(), 27.0, 1e-6);
	TestTrue(TEXT("joint bends toward the pole"), FVector::DotProduct(S.Joint - (Root + Target) * 0.5, Pole) > 0);
	const FRubiconTwoBoneSolution Far = ARubiconEmbodiedBody::SolveTwoBone(Root, FVector(100, 0, 0), 30, 27, Pole);
	TestFalse(TEXT("unreachable target reported"), Far.bReached);
	TestTrue(TEXT("unreachable target clamps to full extension"), (Far.End - Root).Size() < 57.0 && (Far.End - Root).Size() > 56.9);

	// The default body pose must be fully reachable (no stretched limbs).
	const ARubiconEmbodiedBody* B = GetDefault<ARubiconEmbodiedBody>();
	const FVector ShoulderL = B->EyePoint + FVector(-12, -B->ShoulderHalfWidth, -B->ShoulderDrop);
	const FVector HipL = B->EyePoint + FVector(-6, -10, -B->ShoulderDrop - B->TorsoLength);
	TestTrue(TEXT("default arm pose reachable"), ARubiconEmbodiedBody::SolveTwoBone(ShoulderL, B->LeftWristOnBeam, B->UpperArm, B->Forearm, Pole).bReached);
	TestTrue(TEXT("default leg pose reachable"), ARubiconEmbodiedBody::SolveTwoBone(HipL, B->LeftFoot, B->Thigh, B->Shin, FVector(1, 0, 0)).bReached);
	TestTrue(TEXT("greybox eye point matches the body"), GetDefault<ARubiconGolgothaGreybox>()->EyePoint.Equals(B->EyePoint));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubiconHeadClampTest, "Rubicon.Game.HeadClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubiconHeadClampTest::RunTest(const FString& Parameters)
{
	const FVector Center(10, 0, 150);
	TestTrue(TEXT("inside the radius: no correction"), ARubiconEmbodiedPawn::ClampCorrection(Center + FVector(5, 3, 0), Center, 12).IsNearlyZero());
	const FVector Head = Center + FVector(30, 0, 0);
	const FVector Corr = ARubiconEmbodiedPawn::ClampCorrection(Head, Center, 12);
	TestTrue(TEXT("excess is removed exactly"), Corr.Equals(FVector(18, 0, 0), 1e-9));
	// Eye in rig space = -Center - Corr + Head: always within the radius of the anchor.
	TestTrue(TEXT("eye stays within radius"), ((-Center - Corr + Head)).Size() <= 12 + 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubiconHeadSpeedTest, "Rubicon.Game.HeadSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubiconHeadSpeedTest::RunTest(const FString& Parameters)
{
	// Exertion input: measured in tracking space, so it is the player's own motion only.
	const FQuat A = FRotator(0, 0, 0).Quaternion();
	const FQuat B = FRotator(0, 9, 0).Quaternion();
	const FVector2D Turn = ARubiconEmbodiedPawn::HeadSpeed(A, FVector::ZeroVector, B, FVector::ZeroVector, 0.1);
	TestTrue(TEXT("9 degrees in 0.1 s is 90 deg/s"), FMath::IsNearlyEqual(Turn.X, 90.0, 1e-6) && FMath::IsNearlyZero(Turn.Y));
	const FVector2D Move = ARubiconEmbodiedPawn::HeadSpeed(A, FVector(1, 2, 3), A, FVector(1, 2, 3) + FVector(0, 3, 4), 0.5);
	TestTrue(TEXT("5 cm in 0.5 s is 10 cm/s"), FMath::IsNearlyZero(Move.X, 1e-6) && FMath::IsNearlyEqual(Move.Y, 10.0, 1e-9));
	TestTrue(TEXT("zero time step gives zero"), ARubiconEmbodiedPawn::HeadSpeed(A, FVector::ZeroVector, B, FVector(5, 0, 0), 0.0).IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubiconGazeLogicTest, "Rubicon.Game.GazeLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubiconGazeLogicTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("tag parse"), URubiconGazeResolverComponent::IdFromTag(TEXT("Gaze.Target.crowd")), FName(TEXT("crowd")));
	TestEqual(TEXT("non-gaze tag"), URubiconGazeResolverComponent::IdFromTag(TEXT("Other")), FName());
	const FName Sky(TEXT("sky")), Horizon(TEXT("horizon")), Ground(TEXT("ground_below"));
	TestEqual(TEXT("up is sky"), URubiconGazeResolverComponent::ResolveByPitch(30, 6, Sky, Horizon, Ground), Sky);
	TestEqual(TEXT("level is horizon"), URubiconGazeResolverComponent::ResolveByPitch(2, 6, Sky, Horizon, Ground), Horizon);
	TestEqual(TEXT("down is ground"), URubiconGazeResolverComponent::ResolveByPitch(-20, 6, Sky, Horizon, Ground), Ground);
	TestTrue(TEXT("ray through sphere"), URubiconGazeResolverComponent::ConeAngleToSphere(FVector::ZeroVector, FVector::ForwardVector, FSphere(FVector(1000, 0, 0), 50)) <= 0);
	TestTrue(TEXT("ray 10 deg off a small sphere"), URubiconGazeResolverComponent::ConeAngleToSphere(FVector::ZeroVector, FVector::ForwardVector,
		FSphere(FVector(1000, 176.3, 0), 10)) > 8);

	FRubiconGazeHysteresis H;
	TestEqual(TEXT("starts empty"), H.Update(NAME_None, 10, 150), FName());
	for (int I = 0; I < 14; ++I) H.Update(TEXT("crowd"), 10, 150);
	TestEqual(TEXT("not yet switched at 140 ms"), H.Current, FName());
	H.Update(TEXT("crowd"), 10, 150);
	TestEqual(TEXT("switched at 150 ms"), H.Current, FName(TEXT("crowd")));
	for (int I = 0; I < 10; ++I) H.Update(TEXT("horizon"), 10, 150);
	H.Update(TEXT("crowd"), 10, 150);
	for (int I = 0; I < 10; ++I) H.Update(TEXT("horizon"), 10, 150);
	TestEqual(TEXT("flicker does not switch"), H.Current, FName(TEXT("crowd")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubiconElevationPoseTest, "Rubicon.Game.ElevationPose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubiconElevationPoseTest::RunTest(const FString& Parameters)
{
	const FRubiconElevationPose P = FRubiconElevationPose::Make(14, 70);
	// The embodied world transform is the exact inverse of the cross motion seen in Witness.
	const FTransform Cross = P.WitnessCrossTransform();
	const FTransform World = P.EmbodiedWorldTransform();
	const FVector Probe(1234, -56, 78);
	TestTrue(TEXT("world transform inverts cross motion"), Cross.InverseTransformPosition(Probe).Equals(World.TransformPosition(Probe), 1e-6));
	TestTrue(TEXT("upright pose is identity"), FRubiconElevationPose().EmbodiedWorldTransform().Equals(FTransform::Identity));

	FRubiconFrameOutputs F;
	F.bValid = true;
	F.State = ERubiconTimelineState::Threshold;
	TestEqual(TEXT("threshold holds the start pose"), URubiconElevationDirectorComponent::TargetPose(F, P).LeanBackDeg, 14.0);
	F.State = ERubiconTimelineState::Elevation;
	F.Phase = ERubiconPhase::Raising;
	F.PhaseProgress = 0.5;
	TestEqual(TEXT("halfway (smoothstep)"), URubiconElevationDirectorComponent::TargetPose(F, P).LeanBackDeg, 7.0, 1e-9);
	F.Phase = ERubiconPhase::Settled;
	TestEqual(TEXT("settled is upright"), URubiconElevationDirectorComponent::TargetPose(F, P).LowerCm, 0.0);
	F.State = ERubiconTimelineState::Forgiveness;
	TestEqual(TEXT("upright after elevation"), URubiconElevationDirectorComponent::TargetPose(F, P).LeanBackDeg, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubiconLayoutCoverageTest, "Rubicon.Game.GazeLayoutCoversManifest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRubiconLayoutCoverageTest::RunTest(const FString& Parameters)
{
	const FString Content = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("content")));
	FString ManifestJson, LocaleJson;
	if (!FFileHelper::LoadFileToString(ManifestJson, *FPaths::Combine(Content, TEXT("passion.episode.json"))) ||
		!FFileHelper::LoadFileToString(LocaleJson, *FPaths::Combine(Content, TEXT("locale.en.json"))))
	{
		AddError(TEXT("cannot read repository content"));
		return false;
	}
	RubiconCore::FManifest M;
	RubiconCore::FLocale L;
	std::vector<std::string> Errors;
	if (!RubiconCore::LoadEpisode(RubiconConvert::ToUtf8(ManifestJson), RubiconConvert::ToUtf8(LocaleJson), M, L, Errors))
	{
		AddError(TEXT("manifest failed to load"));
		return false;
	}
	TSet<FName> Placed;
	for (const FRubiconGazePlacement& P : ARubiconGolgothaGreybox::DefaultLayout())
	{
		Placed.Add(P.TargetId);
		if (P.TargetId == TEXT("cross"))
		{
			// The wood the player hangs on answers gaze directly and moves with the cross in Witness.
			TestTrue(TEXT("cross is attached to the cross"), P.bAttachToCross);
			TestTrue(TEXT("cross resolves by collision"), P.Resolution == ERubiconGazeResolution::Collision);
			TestTrue(TEXT("cross proxy shape"), P.Shape == ERubiconProxyShape::Cross);
		}
	}
	Placed.Add(TEXT("own_hands")); // ARubiconEmbodiedBody
	for (const RubiconCore::FGazeTargetDef& G : M.GazeTargets)
	{
		const FName Id(*RubiconConvert::ToFString(G.Id));
		TestTrue(FString::Printf(TEXT("gaze target %s has a tagged actor"), *Id.ToString()), Placed.Contains(Id));
	}
	for (const FName& Id : Placed)
	{
		TestTrue(FString::Printf(TEXT("placement %s exists in the manifest"), *Id.ToString()), M.FindGazeTarget(RubiconConvert::ToUtf8(Id.ToString())) >= 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
