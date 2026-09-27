#include "KodResourceWallet.h"
#include "Net/UnrealNetwork.h"

UKodResourceWallet::UKodResourceWallet()
{
	SetIsReplicatedByDefault(true);
}

bool UKodResourceWallet::CanAfford(int32 DustCost) const
{
	return DustCrystal >= DustCost;
}

bool UKodResourceWallet::TrySpend(int32 DustCost)
{
	if (!CanAfford(DustCost))
	{
		return false;
	}
	DustCrystal -= DustCost;
	return true;
}

void UKodResourceWallet::AddDust(int32 Amount)
{
	DustCrystal = FMath::Max(0, DustCrystal + Amount);
}

void UKodResourceWallet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKodResourceWallet, DustCrystal);
}

void UKodResourceWallet::OnRep_DustCrystal()
{
}
