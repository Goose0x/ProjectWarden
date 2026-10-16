#include "Data/KodMeshCatalog.h"
#include "Data/KodUnitDefinition.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodUnitSheet.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "Warden/KodWardenPaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodHideOxDefinitionTest,
	"Kod.Unit.HideOxDefinition",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodHideOxDefinitionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FKodUnitCsvRow Row;
	const FName HideId(KodWardenPaths::Id_ScnHideOx);
	TestTrue(TEXT("sheet row"), FKodUnitSheet::Find(HideId, Row, FString(), false));
	UKodUnitDefinition* Unit = UKodSlice0Bootstrap::ResolveUnit(HideId, GetTransientPackage());
	TestNotNull(TEXT("definition"), Unit);
	if (!Unit)
	{
		return false;
	}
	TestEqual(TEXT("definition supply comes from the sheet"), Unit->SupplyProvided, Row.SupplyProvided);
	TestEqual(TEXT("definition supply cost comes from the sheet"), Unit->Cost.Supply, Row.SupplyCost);
	FKodMeshRow MeshRow;
	TestTrue(TEXT("mesh row"), KodMeshCatalogFindRow(HideId, MeshRow));
	TestFalse(TEXT("idle graze path"), MeshRow.GrazeAnim.IsEmpty());
	TestFalse(TEXT("walk path"), MeshRow.WalkAnim.IsEmpty());
	TestFalse(TEXT("death path"), MeshRow.DeathAnim.IsEmpty());
	TestFalse(TEXT("definition graze"), Unit->IdleAnim.IsNull());
	TestFalse(TEXT("definition walk"), Unit->WalkAnim.IsNull());
	TestFalse(TEXT("definition death"), Unit->DeathAnim.IsNull());
	return true;
}
