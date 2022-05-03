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

#if REFAC == 2

	for (auto ptr : componentSparseSets)
	{
		delete ptr;
		ptr = 0;
	}
	componentSparseSets.clear();

	for (auto ptr : componentAvailabilityBitsets)
	{
		delete ptr;
		ptr = 0;
	}
	componentAvailabilityBitsets.clear();

#endif
}

// Transfers component data from an entity to another entity
// Depending on refactor implementation it may invalidate the original entity's component access so consider it unusable unless reassigned 
void ECS::transferComponents(EntityID from, EntityID to)
{
	// Loop through each possible component this entity could have
	for (int i = 0; i < MAX_COMPONENTS; i++)
	{
		// NOTE, i is the compID which is deliberately fixed as such when the components were created with initComponents<>()

		// Check entity has this component
		if (!entities[from].compMask.test(i))
			continue;

#if REFAC == 1

		// This method requires all component data is copied from one location to another in the components arrays
		// Directly transfer component data across
		componentPools[i]->copy(from, to);

#elif REFAC == 2

		// This method only requires the changing of an integer in each component sparse set
		// The entityID has moved so we must tell it in the new sparse set location where it's component is (the old sparse set location's element)
		// Switching both around allows the old component to be more easily freed for future used - rather than just setting the new sparse set value 
		const auto old = componentSparseSets[i]->at(to);	// Save old component location
		componentSparseSets[i]->at(to) = componentSparseSets[i]->at(from);	// Transfer new component location into place
		componentSparseSets[i]->at(from) = old;		// Give this old entity the old (now redundant) component so it can be freed later



		// The component availability flag is tied only to the dense component array which isn't changed here. 

#endif

	}
}

