#include "KodResourceWallet.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Sim/KodResourceTypes.h"
#include "Sim/KodSimSubsystem.h"

namespace
{
	constexpr int32 KodWalletTeamId = 0;

	UKodSimSubsystem* KodWalletSim(const UActorComponent* Wallet)
	{
		const UWorld* World = Wallet ? Wallet->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	}
}

UKodResourceWallet::UKodResourceWallet()
{
	SetIsReplicatedByDefault(true);
	SupplyCap = KodSupplyHardCap;
}

void UKodResourceWallet::BeginPlay()
{
	Super::BeginPlay();
	MirrorFromSim();
}

bool UKodResourceWallet::CanAfford(int32 CashCost) const
{
	const int32 Need = FMath::Max(0, CashCost);
	if (const UKodSimSubsystem* Sim = KodWalletSim(this))
	{
		FKodResourceCost Bank;
		Sim->TryGetBank(KodWalletTeamId, Bank);
		return Bank.Jadeite >= Need;
	}
	return Cash >= Need;
}

bool UKodResourceWallet::TrySpend(int32 CashCost)
{
	const int32 Need = FMath::Max(0, CashCost);
	if (UKodSimSubsystem* Sim = KodWalletSim(this))
	{
		FKodResourceCost Cost;
		Cost.Jadeite = Need;
		if (!Sim->Spend(KodWalletTeamId, Cost))
		{
			MirrorFromSim();
			return false;
		}
		MirrorFromSim();
		return true;
	}
	if (Cash < Need)
	{
		return false;
	}
	Cash -= Need;
	return true;
}

void UKodResourceWallet::AddCash(int32 Amount)
{
	const int32 Add = FMath::Max(0, Amount);
	if (UKodSimSubsystem* Sim = KodWalletSim(this))
	{
		if (Add > 0)
		{
			FKodResourceCost DepositAmount;
			DepositAmount.Jadeite = Add;
			Sim->Deposit(KodWalletTeamId, DepositAmount);
		}
		MirrorFromSim();
		return;
	}
	Cash = FMath::Max(0, Cash + Amount);
}

void UKodResourceWallet::AddOil(int32 Amount)
{
	const int32 Add = FMath::Max(0, Amount);
	if (UKodSimSubsystem* Sim = KodWalletSim(this))
	{
		if (Add > 0)
		{
			FKodResourceCost DepositAmount;
			DepositAmount.Luminene = Add;
			Sim->Deposit(KodWalletTeamId, DepositAmount);
		}
		MirrorFromSim();
		return;
	}
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

void UKodResourceWallet::MirrorFromSim()
{
	const UKodSimSubsystem* Sim = KodWalletSim(this);
	if (!Sim)
	{
		return;
	}
	FKodResourceCost Bank;
	if (Sim->TryGetBank(KodWalletTeamId, Bank))
	{
		Cash = Bank.Jadeite;
		Oil = Bank.Luminene;
	}
	int32 Used = 0;
	int32 Cap = 0;
	if (Sim->TryGetSupply(KodWalletTeamId, Used, Cap))
	{
		Supply = Used;
		SupplyCap = Cap;
		ClampSupply();
	}
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
