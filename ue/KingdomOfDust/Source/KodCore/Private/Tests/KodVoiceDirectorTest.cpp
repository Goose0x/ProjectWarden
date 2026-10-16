#include "Audio/KodVoiceDirector.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodVoiceDirectorTest,
	"Kod.Voice.Director",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodVoiceDirectorTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FKodVoiceDirector Voice;
	Voice.SelectCooldown = 0.f;
	Voice.AckCooldown = 0.f;
	Voice.AnnoyedCooldown = 0.f;
	Voice.AnnoyedWindow = 3.f;
	Voice.AnnoyedAfter = 4;
	const FName Marine(TEXT("Marine"));
	Voice.AddLine(Marine, FName(TEXT("Select")), FName(TEXT("S1")));
	Voice.AddLine(Marine, FName(TEXT("Select")), FName(TEXT("S2")));
	Voice.AddLine(Marine, FName(TEXT("Select")), FName(TEXT("S3")));
	Voice.AddLine(Marine, FName(TEXT("Ack")), FName(TEXT("A1")));
	Voice.AddLine(Marine, FName(TEXT("Ack")), FName(TEXT("A2")));
	Voice.AddLine(Marine, FName(TEXT("Annoyed")), FName(TEXT("P1")));
	Voice.AddLine(Marine, FName(TEXT("Annoyed")), FName(TEXT("P2")));

	TSet<FName> Heard;
	FName Previous;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FKodVoiceChoice Choice = Voice.Choose(Marine, FName(TEXT("Select")), 1, static_cast<float>(Index) * 0.1f);
		TestTrue(TEXT("select plays"), Choice.bPlay);
		TestEqual(TEXT("select category"), Choice.Category, FName(TEXT("Select")));
		TestFalse(TEXT("select does not repeat inside the bag"), Heard.Contains(Choice.Cue));
		Heard.Add(Choice.Cue);
		Previous = Choice.Cue;
	}
	TestEqual(TEXT("select bag is exhausted once"), Heard.Num(), 3);

	const FKodVoiceChoice Fourth = Voice.Choose(Marine, FName(TEXT("Select")), 1, 0.4f);
	TestTrue(TEXT("fourth select escalates"), Fourth.bPlay);
	TestEqual(TEXT("first annoyed line"), Fourth.Cue, FName(TEXT("P1")));
	const FKodVoiceChoice Fifth = Voice.Choose(Marine, FName(TEXT("Select")), 1, 0.5f);
	TestEqual(TEXT("second annoyed line"), Fifth.Cue, FName(TEXT("P2")));
	const FKodVoiceChoice Sixth = Voice.Choose(Marine, FName(TEXT("Select")), 1, 0.6f);
	TestEqual(TEXT("annoyed holds the last line"), Sixth.Cue, FName(TEXT("P2")));

	const FKodVoiceChoice AfterIdle = Voice.Choose(Marine, FName(TEXT("Select")), 1, 0.6f + 3.1f);
	TestTrue(TEXT("idle resets to select"), AfterIdle.bPlay);
	TestEqual(TEXT("select after the timeout"), AfterIdle.Category, FName(TEXT("Select")));
	TestTrue(TEXT("reshuffle does not repeat the previous select"), AfterIdle.Cue != Previous);

	FKodVoiceDirector Fallback;
	Fallback.SelectCooldown = 0.f;
	Fallback.AnnoyedAfter = 4;
	Fallback.AnnoyedWindow = 3.f;
	Fallback.AddLine(Marine, FName(TEXT("Select")), FName(TEXT("Only")));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Fallback.Choose(Marine, FName(TEXT("Select")), 7, static_cast<float>(Index));
	}
	const FKodVoiceChoice Missed = Fallback.Choose(Marine, FName(TEXT("Select")), 7, 3.f);
	TestEqual(TEXT("missing annoyed falls back to select"), Missed.Category, FName(TEXT("Select")));
	TestEqual(TEXT("fallback cue"), Missed.Cue, FName(TEXT("Only")));

	FKodVoiceDirector Busy;
	Busy.SelectCooldown = 1.5f;
	Busy.AckCooldown = 1.5f;
	Busy.AddLine(Marine, FName(TEXT("Select")), FName(TEXT("S")));
	Busy.AddLine(Marine, FName(TEXT("Ack")), FName(TEXT("M")));
	TestTrue(TEXT("first marine speaks"), Busy.Choose(Marine, FName(TEXT("Select")), 1, 0.f).bPlay);
	TestFalse(TEXT("another marine waits"), Busy.Choose(Marine, FName(TEXT("Ack")), 2, 0.2f).bPlay);
	TestTrue(TEXT("the type speaks again after the cooldown"), Busy.Choose(Marine, FName(TEXT("Ack")), 2, 1.6f).bPlay);

	FKodVoiceDirector Ack;
	Ack.AckCooldown = 0.f;
	Ack.AddLine(Marine, FName(TEXT("Ack")), FName(TEXT("M1")));
	Ack.AddLine(Marine, FName(TEXT("Ack")), FName(TEXT("M2")));
	const FName FirstAck = Ack.Choose(Marine, FName(TEXT("Ack")), 1, 0.f).Cue;
	const FName SecondAck = Ack.Choose(Marine, FName(TEXT("Ack")), 1, 0.1f).Cue;
	TestTrue(TEXT("ack lines differ"), FirstAck != SecondAck);
	Ack.AddLine(Marine, FName(TEXT("Attack")), FName(TEXT("K1")));
	Ack.AddLine(Marine, FName(TEXT("Attack")), FName(TEXT("K2")));
	const FKodVoiceChoice AttackA = Ack.Choose(Marine, FName(TEXT("Attack")), 1, 0.2f);
	const FKodVoiceChoice AttackB = Ack.Choose(Marine, FName(TEXT("Attack")), 1, 0.3f);
	TestTrue(TEXT("attack plays"), AttackA.bPlay && AttackB.bPlay);
	TestTrue(TEXT("attack does not repeat"), AttackA.Cue != AttackB.Cue);
	return true;
}
