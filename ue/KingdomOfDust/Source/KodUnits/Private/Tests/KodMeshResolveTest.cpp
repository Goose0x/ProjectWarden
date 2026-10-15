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
	TestTrue(TEXT("barracks footprint"), FMath::IsNearlyEqual(FKodMeshCatalog::FootprintCm(FName(KodWardenPaths::Id_Barracks)), 600.f));
	TestTrue(TEXT("depot footprint"), FMath::IsNearlyEqual(FKodMeshCatalog::FootprintCm(FName(KodWardenPaths::Id_SupplyDepot)), 400.f));

	const TArray<FName> Required = {
		FName(KodWardenPaths::Id_CommandCenter),
		FName(KodWardenPaths::Id_Barracks),
		FName(KodWardenPaths::Id_SupplyDepot),
		FName(KodWardenPaths::Id_Dozer),
		FName(KodWardenPaths::Id_ScnWorker),
		FName(TEXT("ScinderHQ")),
		FName(TEXT("ScinderHost")),
		FName(TEXT("Faultburst")),
		FName(TEXT("Shardbitten"))
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
		if (Expected > 0.f && Choice.bLoaded)
		{
			TestTrue(
				*FString::Printf(TEXT("%s bounds %.0f vs %.0f"), *Id.ToString(), Choice.BoundsCm, Expected),
				FKodMeshCatalog::SpanWithin(Choice.BoundsCm, Expected));
		}
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
