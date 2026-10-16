#include "Data/KodMeshCatalog.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Warden/KodWardenPaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodMeshResolveTest,
	"Kod.Meshes.Resolve",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodMeshResolveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestTrue(TEXT("1000 cm fits"), FKodMeshCatalog::SpanWithin(1000.f, 1000.f));
	TestTrue(TEXT("15 percent fits"), FKodMeshCatalog::SpanWithin(1150.f, 1000.f));
	TestFalse(TEXT("20 percent does not fit"), FKodMeshCatalog::SpanWithin(1200.f, 1000.f));
	TestTrue(TEXT("fit scale lands on the footprint"), FMath::IsNearlyEqual(FKodMeshCatalog::FittedSpanCm(100.f, 1.f, 1000.f), 1000.f));
	TestTrue(TEXT("barracks footprint"), FMath::IsNearlyEqual(FKodMeshCatalog::FootprintCm(FName(KodWardenPaths::Id_Barracks)), 800.f));
	TestTrue(TEXT("depot footprint"), FMath::IsNearlyEqual(FKodMeshCatalog::FootprintCm(FName(KodWardenPaths::Id_SupplyDepot)), 450.f));
	TestTrue(TEXT("rite well footprint"), FMath::IsNearlyEqual(FKodMeshCatalog::FootprintCm(FName(TEXT("RiteWell"))), 450.f));

	const TArray<FName> Required = {
		FName(KodWardenPaths::Id_CommandCenter),
		FName(KodWardenPaths::Id_Barracks),
		FName(KodWardenPaths::Id_SupplyDepot),
		FName(KodWardenPaths::Id_Dozer),
		FName(KodWardenPaths::Id_Marine),
		FName(KodWardenPaths::Id_ScnWorker),
		FName(TEXT("ScinderHQ")),
		FName(TEXT("ScinderHost")),
		FName(TEXT("Faultburst")),
		FName(TEXT("Shardbitten")),
		FName(TEXT("RiteWell")),
		FName(TEXT("SkyReaver")),
		FName(TEXT("SkySentinel"))
	};
	const TArray<FKodMeshRow>& Rows = FKodMeshCatalog::Rows();
	for (const FName& Id : Required)
	{
		const FKodMeshRow* Row = nullptr;
		for (const FKodMeshRow& Candidate : Rows)
		{
			if (Candidate.Id == Id)
			{
				Row = &Candidate;
				break;
			}
		}
		TestNotNull(*FString::Printf(TEXT("row %s"), *Id.ToString()), Row);
		if (!Row)
		{
			continue;
		}
		const FKodMeshChoice Choice = FKodMeshCatalog::Resolve(Id);
		TestTrue(*FString::Printf(TEXT("%s has a row"), *Id.ToString()), Choice.bHasRow);
		TestFalse(*FString::Printf(TEXT("%s path"), *Id.ToString()), Choice.Path.IsEmpty());
		const FString PrimaryName = FPaths::GetBaseFilename(Row->Primary);
		const FString FallbackName = FPaths::GetBaseFilename(Row->Fallback);
		const bool bChosePrimary = !Choice.bFallback && Choice.Path.Contains(PrimaryName);
		const bool bChoseFallback = Choice.bFallback && Choice.Path.Contains(FallbackName);
		TestTrue(*FString::Printf(TEXT("%s primary or fallback"), *Id.ToString()), bChosePrimary || bChoseFallback);
		const float Expected = FKodMeshCatalog::FootprintCm(Id);
		const bool bSizedFit = Row->FitCm > 0.f
			&& (Row->Fit == EKodMeshFit::Length || Row->Fit == EKodMeshFit::Footprint || Row->Fit == EKodMeshFit::Height);
		const float FitExpected = bSizedFit ? Row->FitCm : Expected;
		if (FitExpected > 0.f && Choice.bLoaded)
		{
			TestTrue(
				*FString::Printf(TEXT("%s bounds %.0f vs %.0f"), *Id.ToString(), Choice.BoundsCm, FitExpected),
				FKodMeshCatalog::SpanWithin(Choice.BoundsCm, FitExpected));
		}
		if (Id == FName(KodWardenPaths::Id_Dozer))
		{
			TestTrue(TEXT("AMP length fit"), Row->Fit == EKodMeshFit::Length);
			TestTrue(TEXT("AMP 300 cm"), FMath::IsNearlyEqual(Row->FitCm, 300.f));
			TestTrue(TEXT("AMP min height"), FMath::IsNearlyEqual(Row->MinHeightCm, 220.f));
			if (Choice.bLoaded)
			{
				TestTrue(
					*FString::Printf(TEXT("AMP length %.0f in 280-320"), Choice.BoundsCm),
					Choice.BoundsCm >= 280.f && Choice.BoundsCm <= 320.f);
			}
		}
		if (Id == FName(TEXT("ScinderHost")))
		{
			TestTrue(TEXT("host length fit"), Row->Fit == EKodMeshFit::Length);
			TestTrue(TEXT("host 300 cm"), FMath::IsNearlyEqual(Row->FitCm, 300.f));
		}
		if (Id == FName(KodWardenPaths::Id_ScnWorker))
		{
			TestTrue(TEXT("scinder worker 260 cm"), FMath::IsNearlyEqual(Row->FitCm, 260.f));
		}
		if (Id == FName(KodWardenPaths::Id_Marine))
		{
			TestTrue(TEXT("marine 286 cm"), FMath::IsNearlyEqual(Row->FitCm, 286.f));
		}
		if (Id == FName(TEXT("Shardbitten")))
		{
			TestTrue(TEXT("shardbitten height fit"), Row->Fit == EKodMeshFit::Height);
			TestTrue(TEXT("shardbitten 260 cm"), FMath::IsNearlyEqual(Row->FitCm, 260.f));
			TestTrue(TEXT("shardbitten v7h"), Row->Primary.Contains(TEXT("unit_v7h")));
			if (Choice.bLoaded)
			{
				TestTrue(
					*FString::Printf(TEXT("shardbitten scale %.3f"), Choice.Scale),
					Choice.Scale >= 0.01f && Choice.Scale <= 20.f);
			}
		}
		if (Id == FName(KodWardenPaths::Id_Marine) || Id == FName(KodWardenPaths::Id_ScnWorker))
		{
			TestTrue(*FString::Printf(TEXT("%s height fit"), *Id.ToString()), Row->Fit == EKodMeshFit::Height);
		}
		if (Id == FName(KodWardenPaths::Id_CommandCenter))
		{
			TestTrue(TEXT("command center footprint fit"), Row->Fit == EKodMeshFit::Footprint);
			TestTrue(TEXT("command center 1000 cm"), FMath::IsNearlyEqual(Row->FitCm, 1000.f));
			TestTrue(TEXT("command center v4h primary"), Row->Primary.Contains(TEXT("CrimsonOutpost_v4h")));
			TestTrue(TEXT("command center fallback"), Row->Fallback.Contains(TEXT("SM_general")));
		}
		if (Id == FName(KodWardenPaths::Id_Barracks))
		{
			TestTrue(TEXT("barracks footprint fit"), Row->Fit == EKodMeshFit::Footprint);
			TestTrue(TEXT("barracks 800 cm"), FMath::IsNearlyEqual(Row->FitCm, 800.f));
			TestTrue(TEXT("barracks v9i primary"), Row->Primary.Contains(TEXT("Barracks_v9i")));
			TestTrue(TEXT("barracks v8g fallback"), Row->Fallback.Contains(TEXT("v8g")));
		}
	}
	TArray<FKodMeshAttach> CcAttaches;
	KodMeshCatalogCopyAttaches(FName(KodWardenPaths::Id_CommandCenter), CcAttaches);
	TestEqual(TEXT("command center attaches"), CcAttaches.Num(), 0);
	// export_h ships SOCKET_BayRamp / SOCKET_BayInterior on the body only; no separate attach meshes.
	TArray<FKodMeshAttach> BarracksAttaches;
	KodMeshCatalogCopyAttaches(FName(KodWardenPaths::Id_Barracks), BarracksAttaches);
	TestEqual(TEXT("barracks attaches"), BarracksAttaches.Num(), 1);
	if (BarracksAttaches.Num() >= 1)
	{
		TestTrue(TEXT("barracks ramp mesh"), BarracksAttaches[0].Mesh.Contains(TEXT("SK_Vanguard_Barracks_v9i_Ramp")));
		TestTrue(TEXT("barracks tries Ramp"), BarracksAttaches[0].SocketChain.Contains(FName(TEXT("Ramp"))));
		TestTrue(TEXT("barracks tries BayRamp"), BarracksAttaches[0].SocketChain.Contains(FName(TEXT("BayRamp"))));
		TestTrue(TEXT("barracks tries Door"), BarracksAttaches[0].SocketChain.Contains(FName(TEXT("Door"))));
		TestTrue(TEXT("barracks origin fallback"), BarracksAttaches[0].SocketChain.Contains(NAME_None));
		TestTrue(TEXT("barracks idle clip"), FPaths::GetBaseFilename(BarracksAttaches[0].Anim) == TEXT("IdleClosed"));
		TestTrue(TEXT("barracks open clip"), FPaths::GetBaseFilename(BarracksAttaches[0].OpenAnim) == TEXT("Open"));
		TestTrue(TEXT("barracks close clip"), FPaths::GetBaseFilename(BarracksAttaches[0].CloseAnim) == TEXT("Close"));
		TestTrue(TEXT("barracks idle open clip"), FPaths::GetBaseFilename(BarracksAttaches[0].IdleOpenAnim) == TEXT("IdleOpen"));
	}

	const FName BladeIds[] = {FName(TEXT("SCN_BLADE")), FName(TEXT("SCN_TWINBLADE"))};
	for (const FName& BladeId : BladeIds)
	{
		FKodMeshRow BladeRow;
		const bool bBlade = KodMeshCatalogFindRow(BladeId, BladeRow);
		TestTrue(*FString::Printf(TEXT("%s row"), *BladeId.ToString()), bBlade);
		if (!bBlade)
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s height"), *BladeId.ToString()), BladeRow.Fit == EKodMeshFit::Height);
		TestTrue(*FString::Printf(TEXT("%s 286 cm"), *BladeId.ToString()), FMath::IsNearlyEqual(BladeRow.FitCm, 286.f));
		TestTrue(*FString::Printf(TEXT("%s jade fang"), *BladeId.ToString()), BladeRow.Primary.Contains(TEXT("SK_JadeFang_v3a")));
	}

	IConsoleVariable* NewMeshes = IConsoleManager::Get().FindConsoleVariable(TEXT("kod.NewMeshes"));
	TestNotNull(TEXT("kod.NewMeshes"), NewMeshes);
	if (NewMeshes)
	{
		const int32 Previous = NewMeshes->GetInt();
		NewMeshes->Set(0, ECVF_SetByCode);
		const FKodMeshChoice Old = FKodMeshCatalog::Resolve(FName(KodWardenPaths::Id_CommandCenter));
		TestTrue(TEXT("toggle uses the fallback"), Old.bFallback);
		TestTrue(TEXT("fallback is the old HQ mesh"), Old.Path.Contains(TEXT("SM_general")));
		NewMeshes->Set(Previous, ECVF_SetByCode);
	}
	return true;
}
