#include "KodTechTree.h"

bool UKodTechTree::ArePrerequisitesMet(FGameplayTag NodeTag, const FGameplayTagContainer& Unlocked) const
{
	for (const FKodTechNode& Node : Nodes)
	{
		if (Node.NodeTag == NodeTag)
		{
			return Unlocked.HasAll(Node.Prerequisites);
		}
	}
	return false;
}
