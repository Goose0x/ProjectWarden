#include "KodLobbyChatRailWidget.h"
#include "KodChatMessageRowWidget.h"
#include "KodLabeledButton.h"
#include "KodUI.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/EditableText.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
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

void UKodLobbyChatRailWidget::AppendMessage(const FText& Speaker, const FText& Body)
{
	FKodChatLine Line;
	Line.Speaker = Speaker;
	Line.Body = Body;
	Lines.Add(Line);
	RebuildMessages();
}

void UKodLobbyChatRailWidget::AppendLine(const FText& Line)
{
	const FString Raw = Line.ToString();
	FString Speaker;
	FString Body;
	if (Raw.Split(TEXT(": "), &Speaker, &Body))
	{
		AppendMessage(FText::FromString(Speaker), FText::FromString(Body));
	}
	else
	{
		AppendMessage(FText::GetEmpty(), Line);
	}
}

void UKodLobbyChatRailWidget::SubmitInput()
{
	if (!Text_Message)
	{
		return;
	}
	const FText Message = Text_Message->GetText();
	if (Message.IsEmpty())
	{
		return;
	}
	AppendLine(Message);
	Text_Message->SetText(FText::GetEmpty());
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
	if (Btn_Send)
	{
		Btn_Send->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyChatRailWidget::HandleSend);
	}
}

void UKodLobbyChatRailWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyGlassChrome();
	if (IsDesignTime())
	{
		SeedStampPreview();
	}
	ApplyTabChrome();
}

void UKodLobbyChatRailWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsDesignTime())
	{
		SeedStampPreview();
	}
	RebuildMessages();
}

void UKodLobbyChatRailWidget::ApplyGlassChrome()
{
	const FLinearColor Cyan = UKodUIStyleLibrary::GetCyanActive();
	const FLinearColor Quiet(Cyan.R, Cyan.G, Cyan.B, 0.55f);
	if (ChatRoot)
	{
		ChatRoot->SetBrushColor(UKodUIStyleLibrary::GetPanelBlue());
	}
	if (Tab_Lobby)
	{
		Tab_Lobby->SetStampLabel(NSLOCTEXT("KodUI", "ChatTabLobby", "LOBBY"));
	}
	if (Tab_Party)
	{
		Tab_Party->SetStampLabel(NSLOCTEXT("KodUI", "ChatTabParty", "PARTY"));
	}
	if (Btn_Send)
	{
		Btn_Send->SetShowLabel(false);
		Btn_Send->SetStampLabel(NSLOCTEXT("KodUI", "ChatSend", "Send"));
		Btn_Send->SetColorAndOpacity(Quiet);
	}
	if (Text_Message)
	{
		Text_Message->SetHintText(NSLOCTEXT("KodUI", "ChatHint", "Type message..."));
	}
	if (Underline_Lobby)
	{
		Underline_Lobby->SetColorAndOpacity(Cyan);
	}
	if (Underline_Party)
	{
		Underline_Party->SetColorAndOpacity(Cyan);
	}
}

void UKodLobbyChatRailWidget::ApplyTabChrome()
{
	const FLinearColor Active = UKodUIStyleLibrary::GetCyanActive();
	const FLinearColor Dim(Active.R, Active.G, Active.B, 0.35f);
	const bool bLobby = ActiveTab == EKodLobbyChatTab::Lobby;
	if (Tab_Lobby)
	{
		Tab_Lobby->SetColorAndOpacity(bLobby ? Active : Dim);
	}
	if (Tab_Party)
	{
		Tab_Party->SetColorAndOpacity(bLobby ? Dim : Active);
	}
	if (Underline_Lobby)
	{
		Underline_Lobby->SetVisibility(bLobby ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (Underline_Party)
	{
		Underline_Party->SetVisibility(bLobby ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
}

void UKodLobbyChatRailWidget::SeedStampPreview()
{
	if (bPreviewSeeded || Lines.Num() > 0)
	{
		return;
	}
	bPreviewSeeded = true;
	auto Add = [this](const FText& Speaker, const FText& Body)
	{
		FKodChatLine Line;
		Line.Speaker = Speaker;
		Line.Body = Body;
		Lines.Add(Line);
	};
	Add(NSLOCTEXT("KodUI", "ChatSpeaker1", "[WARD]GhostFox"), NSLOCTEXT("KodUI", "ChatBody1", "gl hf"));
	Add(NSLOCTEXT("KodUI", "ChatSpeaker2", "RailRunner"), NSLOCTEXT("KodUI", "ChatBody2", "desert rail again?"));
	Add(NSLOCTEXT("KodUI", "ChatSpeaker3", "Commander Doe [WARD]"), NSLOCTEXT("KodUI", "ChatBody3", "queueing 1v1"));
	Add(NSLOCTEXT("KodUI", "ChatSpeaker4", "NestViper"), NSLOCTEXT("KodUI", "ChatBody4", "good luck"));
}

void UKodLobbyChatRailWidget::RebuildMessages()
{
	if (!MessageList || !MessageRowClass)
	{
		return;
	}
	MessageList->ClearChildren();
	for (const FKodChatLine& Line : Lines)
	{
		if (UKodChatMessageRowWidget* Row = CreateWidget<UKodChatMessageRowWidget>(this, MessageRowClass))
		{
			MessageList->AddChild(Row);
			Row->SetMessage(Line.Speaker, Line.Body);
		}
	}
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
