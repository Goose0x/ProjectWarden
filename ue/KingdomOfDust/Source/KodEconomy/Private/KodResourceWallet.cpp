#include "KodResourceWallet.h"
#include "Net/UnrealNetwork.h"

UKodResourceWallet::UKodResourceWallet()
{
	SetIsReplicatedByDefault(true);
	SupplyCap = KodSupplyHardCap;
}

bool UKodResourceWallet::CanAfford(int32 CashCost) const
{
	return Cash >= CashCost;
}

bool UKodResourceWallet::TrySpend(int32 CashCost)
{
	if (!CanAfford(CashCost))
	{
		return false;
	}
	Cash -= CashCost;
	return true;
}

void UKodResourceWallet::AddCash(int32 Amount)
{
	Cash = FMath::Max(0, Cash + Amount);
}

void UKodResourceWallet::AddOil(int32 Amount)
{
	Oil = FMath::Max(0, Oil + Amount);
}

void UKodResourceWallet::SetSupplyCount(int32 Current, int32 Cap)
{
	SupplyCap = Cap;
	Supply = Current;
	ClampSupply();
}

void UKodResourceWallet::ClampSupply()
{
	SupplyCap = FMath::Clamp(SupplyCap, 0, KodSupplyHardCap);
	Supply = FMath::Clamp(Supply, 0, SupplyCap);
}

void UKodResourceWallet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKodResourceWallet, Cash);
	DOREPLIFETIME(UKodResourceWallet, Oil);
	DOREPLIFETIME(UKodResourceWallet, Supply);
	DOREPLIFETIME(UKodResourceWallet, SupplyCap);
}

void UKodResourceWallet::OnRep_Cash()
{
}

void UKodResourceWallet::OnRep_Oil()
{
}

void UKodResourceWallet::OnRep_Supply()
{
	ClampSupply();
}
