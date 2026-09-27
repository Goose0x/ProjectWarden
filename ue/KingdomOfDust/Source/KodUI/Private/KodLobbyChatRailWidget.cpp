#include "KodLobbyChatRailWidget.h"
#include "KodLabeledButton.h"
#include "CommonTextBlock.h"
#include "Components/EditableText.h"
#include "Style/KodUIStyle.h"

EKodLobbyChatRail UKodLobbyChatRailWidget::GetRequiredRail()
{
	return EKodLobbyChatRail::Right;
}

void UKodLobbyChatRailWidget::SetActiveTab(EKodLobbyChatTab Tab)
{
	ActiveTab = Tab;
	ApplyTabChrome();
}

void UKodLobbyChatRailWidget::AppendLine(const FText& Line)
{
	Lines.Add(Line);
	ApplyHistoryText();
}

void UKodLobbyChatRailWidget::SubmitInput()
{
	if (!ChatInput)
	{
		return;
	}
	const FText Message = ChatInput->GetText();
	if (Message.IsEmpty())
	{
		return;
	}
	AppendLine(Message);
	ChatInput->SetText(FText::GetEmpty());
	OnChatSubmitted.Broadcast(Message);
}

void UKodLobbyChatRailWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Tab_Lobby)
	{
		Tab_Lobby->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyChatRailWidget::HandleLobbyTab);
	}
	if (Tab_Party)
	{
		Tab_Party->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyChatRailWidget::HandlePartyTab);
	}
	if (ChatSend)
	{
		ChatSend->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyChatRailWidget::HandleSend);
	}
}

void UKodLobbyChatRailWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Tab_Lobby)
	{
		Tab_Lobby->SetStampLabel(NSLOCTEXT("KodUI", "ChatTabLobby", "LOBBY"));
	}
	if (Tab_Party)
	{
		Tab_Party->SetStampLabel(NSLOCTEXT("KodUI", "ChatTabParty", "PARTY"));
	}
	if (ChatSend)
	{
		ChatSend->SetShowLabel(false);
		ChatSend->SetStampLabel(NSLOCTEXT("KodUI", "ChatSend", "Send"));
		ChatSend->SetColorAndOpacity(UKodUIStyleLibrary::GetCyanActive());
	}
	if (ChatInput)
	{
		ChatInput->SetHintText(NSLOCTEXT("KodUI", "ChatHint", "Type message..."));
	}
	if (ChatHistory)
	{
		ChatHistory->SetAutoWrapText(true);
		ChatHistory->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetWhiteText()));
	}
	if (IsDesignTime() && Lines.Num() == 0)
	{
		Lines = {
			NSLOCTEXT("KodUI", "ChatLine1", "[WARD]GhostFox: gl hf"),
			NSLOCTEXT("KodUI", "ChatLine2", "RailRunner: desert rail again?"),
			NSLOCTEXT("KodUI", "ChatLine3", "Commander Doe [WARD]: queueing 1v1"),
			NSLOCTEXT("KodUI", "ChatLine4", "NestViper: good luck")
		};
	}
	ApplyTabChrome();
	ApplyHistoryText();
}

void UKodLobbyChatRailWidget::ApplyTabChrome()
{
	const FLinearColor Active = UKodUIStyleLibrary::GetCyanActive();
	const FLinearColor Dim = FLinearColor(Active.R, Active.G, Active.B, 0.35f);
	if (Tab_Lobby)
	{
		Tab_Lobby->SetColorAndOpacity(ActiveTab == EKodLobbyChatTab::Lobby ? Active : Dim);
	}
	if (Tab_Party)
	{
		Tab_Party->SetColorAndOpacity(ActiveTab == EKodLobbyChatTab::Party ? Active : Dim);
	}
}

void UKodLobbyChatRailWidget::ApplyHistoryText()
{
	if (!ChatHistory)
	{
		return;
	}
	FString Combined;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		if (Index > 0)
		{
			Combined.Append(TEXT("\n"));
		}
		Combined.Append(Lines[Index].ToString());
	}
	ChatHistory->SetText(FText::FromString(Combined));
}

void UKodLobbyChatRailWidget::HandleLobbyTab(UKodLabeledButton* Button)
{
	(void)Button;
	SetActiveTab(EKodLobbyChatTab::Lobby);
}

void UKodLobbyChatRailWidget::HandlePartyTab(UKodLabeledButton* Button)
{
	(void)Button;
	SetActiveTab(EKodLobbyChatTab::Party);
}

void UKodLobbyChatRailWidget::HandleSend(UKodLabeledButton* Button)
{
	(void)Button;
	SubmitInput();
}
