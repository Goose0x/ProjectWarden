# Economy

Playtest #1 wallets. Spend and store only. This folder does not start a gather loop.

- `DA_Resource_Cash` — primary spend wallet (`Kod.Resource.Cash`)
- `DA_Resource_Oil` — second wallet stub (`Kod.Resource.Oil`)

Supply hard cap is `KodSupplyHardCap` (200) on `UKodResourceWallet`.

World gather nodes are not renamed to Cash. `UKodGatherComponent` stays a generic gather stub and is not a Dust node. Dust Storm (`Kod.Power.DustStorm`, `NS_DustStorm`) is a power, not a wallet. Purple-black / orange-red Dust node art is not part of this rename. Escalate to PM if a sheet still calls the spend crystal Dust.
