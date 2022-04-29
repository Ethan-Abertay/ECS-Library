#include "ECS.h"
#include <assert.h>

// Extern setting
CompID unsetComponentID = 0;

ECS::ECS()
{

}

ECS::~ECS()
{
	for (auto ptr : componentPools)
	{
		delete ptr;
		ptr = 0;
	}
	componentPools.clear();
}

// Transfers component data from an entity to another entity
// Depending on refactor implementation it may invalidate the original entity's component access so consider it unusable
void ECS::transferComponents(EntityID from, EntityID to)
{
#if REFAC == 1
	// Directly transfer component data across
	for (int i = 0; i < entities[from].compMask.size(); i++)
	{
		// Check entity has this component
		if (!entities[from].compMask[i])
			continue;

		componentPools[i]->copy(from, to);
	}

#elif REFAC == 2

#endif
}

