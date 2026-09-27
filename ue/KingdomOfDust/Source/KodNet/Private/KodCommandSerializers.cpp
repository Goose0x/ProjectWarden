#include "KodCommandSerializers.h"

bool UKodCommandSerializers::SerializeCommandToBytes(const FKodCommand& Command, TArray<uint8>& OutBytes)
{
	(void)Command;
	OutBytes.Reset();
	// TODO: net / replay binary layout
	return false;
}

bool UKodCommandSerializers::DeserializeCommandFromBytes(const TArray<uint8>& Bytes, FKodCommand& OutCommand)
{
	(void)Bytes;
	OutCommand = FKodCommand();
	return false;
}
