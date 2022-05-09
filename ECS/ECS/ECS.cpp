#include "ECS.h"
#include <algorithm>	// Contains std::sort

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

#if IMPL == 3

	for (auto ptr : sortingGroups)
	{
		delete ptr;
		ptr = 0;
	}
	sortingGroups.clear();

	for (auto ptr : entityGroups)
	{
		delete ptr;
		ptr = 0;
	}
	entityGroups.clear();

#endif
}

// This switches entities in the entity array and handles the switching of components (implementation and refactor dependant)
void ECS::switchEntities(EntityID a, EntityID b)
{
	if (a == b)
		return;

	// Switch all component data
	switchComponents(a, b);

	// Switch comp masks
	const auto old = entities[a].compMask;
	entities[a].compMask = entities[b].compMask;
	entities[b].compMask = old;
}

// This transfer an entity from one place to another (implementation and refactor dependant)
void ECS::transferEntity(EntityID from, EntityID to)
{
	if (from == to)
		return;

	// Transfer components
	transferComponents(from, to);

	// Set comp masks
	entities[to].compMask = entities[from].compMask;
	entities[from].compMask = 0;
}

// This Transfers all components from an entity to another and is optimized to ignore the old components
// This is handy for whenever the old entity is about to be deleted etc (but is still implemmentation specific)
void ECS::transferComponents(EntityID from, EntityID to)
{
	if (from == to)
		return;

	// Loop through each possible component this entity could have
	for (int i = 0; i < MAX_COMPONENTS; i++)
	{
		// NOTE, i is the compID which is deliberately fixed as such when the components were created with initComponents<>()

		// Check component pool exists
		if (componentPools.size() >= i)
			break;

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
		componentSparseSets[i]->at(to) = componentSparseSets[i]->at(from);	// Transfer new component location into place

		// The component availability flag is tied only to the dense component array which isn't changed here. 

#endif

	}
}

// This switches components from a and b
// This is used for refactoring purposes
void ECS::switchComponents(EntityID a, EntityID b)
{
	if (a == b)
		return;

	// Loop through each possible component this entity could have
	for (int i = 0; i < MAX_COMPONENTS; i++)
	{
		// NOTE, i is the compID which is deliberately fixed as such when the components were created with initComponents<>()

		// Check component pool exists
		if (componentPools.size() >= i)
			break;

		//// Check entity has this component
		//if (!entities[from].compMask.test(i))
		//	continue;

#if REFAC == 1

		// This method requires all component data is copied from one location to another in the components arrays
		// Directly transfer component data across
		componentPools[i]->switch_(a, b);

#elif REFAC == 2

		// This method only requires the changing of an integer in each component sparse set
		// The entityID has moved so we must tell it in the new sparse set location where it's component is (the old sparse set location's element)
		// Switching both around allows the old component to be more easily freed for future used - rather than just setting the new sparse set value 
		const auto old = componentSparseSets[i]->at(a);	// Save old component location
		componentSparseSets[i]->at(a) = componentSparseSets[i]->at(b);	// Transfer new component location into place
		componentSparseSets[i]->at(b) = old;		// Give this old entity the old (now redundant) component so it can be freed later

		// The component availability flag is tied only to the dense component array which isn't changed here. 

#endif
	}
}

void ECS::destroyEntity(EntityID entityID)
{
	// Return if entity is already dead
	if (entities[entityID].compMask == 0)
		return;

	auto finalizeDestruction = [&](EntityID index)
	{

#if REFAC == 2

		// Free availability of this entity's components
		// Loop through each possible component this entity could have
		for (int i = 0; i < MAX_COMPONENTS; i++)
		{
			// NOTE, i is the compID which is deliberately fixed as such when the components were created with initComponents<>()

			// Check entity has this component
			if (!entities[index].compMask.test(i))
				continue;

			// Get component index that this entity uses O(1)
			auto compIndex = componentSparseSets[i]->at(index);

			// Reset component availability bitset O(1)
			componentAvailabilityBitsets[i]->reset(compIndex);
		}

#endif

		// Set entity's comp mask to 0 (kills/destroys it)
		entities[index].compMask = 0;
	};

	// There is a function called switch entities but this doesn't care about the other entity so this is optimized
	auto switchDeadEntity = [&](EntityID dead, EntityID alive)
	{
		entities[dead].compMask = entities[alive].compMask;

		// Transfer component data from old to new entity (this is refactor implementation dependant also)
#if REFAC == 1

		// This doesn't care about what happens to old component, it's automatically handled
		transferComponents(alive, dead);

#elif REFAC == 2

		// REFAC 2 needs the components to be switched in order to reset component availability bitsets
		switchComponents(alive, dead);

#endif
	};

	// Decrement counter
	noOfEntities--;

	// Implementation 1 does nothing to the components in the component array as they are reset when they are assigned to an entity. 
#if IMPL == 1

	finalizeDestruction(entityID);

#elif IMPL == 2

	// This implementation must ensure all entities are at the beginning of the array. 
	// Take the entity at the end of the array and slot it into the new available space
	switchDeadEntity(entityID, noOfEntities);

	// Now kill the old entity
	finalizeDestruction(noOfEntities);

#elif IMPL == 3

	// This implementation must ensure the entity group remains contiguous
	// Move the last entity into the slot where the dead entity was and adjust entity group info accordingly

	// Find dead entity's group
	ecs::EntityGroup* group = 0;
	for (int i = 0; i < entityGroups.size(); i++)
	{
		if (entityGroups[i]->compMask == entities[entityID].compMask)
		{
			group = entityGroups[i];
			break;
		}
	}
	assert(group);

	// Up end entity into new place
	const auto aliveIndex = group->getEndIndex();
	switchDeadEntity(entityID, aliveIndex);

	// Now kill the old entity
	finalizeDestruction(aliveIndex);	// alive and dead have been switched 

	// Update group size
	group->noOfEntities--;

#endif
}

#if IMPL == 3

void ECS::performFullRefactor()
{
	// First ensure these are clear
	for (auto ptr : sortingGroups)
	{
		delete ptr;
		ptr = 0;
	}
	sortingGroups.clear();
	for (auto ptr : entityGroups)
	{
		delete ptr;
		ptr = 0;
	}
	entityGroups.clear();

	// Search through entire entity array and generate sorting groups such that every entity belongs to one (and only one)
	for (EntityID i = 0; i < getNoOfEntities(); i++)
	{
		// Get entity comp mask
		const auto& entityCompMask = entities[i].compMask;

		// Ignore dead entities
		// Technically, because we are on implementation 3 (to run this function in the first place) the no of entities returns only alive entities
		// Therefore we should just assert that they are indeed alive
		assert(entityCompMask != 0);

		// Find the sorting group this entity belongs to
		bool bFoundSortingGroup = false;
		for (auto *group : sortingGroups)
		{
			// If this is the sorting group for this entity
			if (group->compMask == entities[i].compMask)
			{
				// Set flag true
				bFoundSortingGroup = true;

				// Add index
				group->indices.push_back(i);

				// Break
				break;
			}
		}

		// If this entity found a group, continue
		if (bFoundSortingGroup)
			continue;

		// This entity doesn't belong to a group

		// Create new sorting group
		auto newGroup = new ecs::SortingGroup();	// Create raw pointer
		newGroup->compMask = entities[i].compMask;	// Set new group's comp mask
		newGroup->indices.push_back(i);				// Add this entity's index to the new group's indices
		sortingGroups.push_back(newGroup);			// Add new group to vector (this handles garbage collection)
	}

	// Sorting groups are now generated

	// Sort the sorting groups by size in decreasing order
	// Sort lambda
	auto sortingLambda = [](ecs::SortingGroup* a, ecs::SortingGroup* b) -> bool
	{
		return a->indices.size() > b->indices.size();
	};
	// Sort algorithm
	std::sort(sortingGroups.begin(), sortingGroups.end(), sortingLambda);

	// Reformat entity array into correct configuration

	// Organize the entities in the array to line up with their groups and create EntityGroups to indicate this
	for (int i = 0; i < sortingGroups.size(); i++)
	{
		// Get starting index for this entity group
		EntityID startingIndex = 0;
		if (i != 0)
			startingIndex = entityGroups[i - 1]->startIndex + entityGroups[i-1]->noOfEntities;	// Next group starts right at the end of previous group

		// Create entity group for this sorting group
		auto* entityGroup = new ecs::EntityGroup();
		entityGroup->startIndex = startingIndex;						// Set starting point
		entityGroup->compMask = sortingGroups[i]->compMask;				// Set comp mask
		entityGroup->noOfEntities = sortingGroups[i]->indices.size();	// Set number of entities
		entityGroups.push_back(entityGroup);							// Add entity group to the vector of groups

		// Now place the entities into the correct places - as defined by the entity group
		const vector<EntityID>& indices = sortingGroups[i]->indices;
		for (int j = 0; j < indices.size(); j++)
		{
			// Find sorting group of entity to be moved out the way
			bool bDone = false;
			for (int k = 0; k < sortingGroups.size(); k++)
			{
				// If this is the correct sorting group
				if (entities[startingIndex + j].compMask == sortingGroups[k]->compMask)
				{
					// Update the index to its new location
					for (int w = 0; w < sortingGroups[k]->indices.size(); w++)
					{
						// If we found the index we need to switch
						if (sortingGroups[k]->indices[w] == startingIndex + j)
						{
							// Switch the index
							sortingGroups[k]->indices[w] = indices[j];
							bDone = true;
							break;
						}
					}
				}
				if (bDone)
					break;
			}

			// Switch entities
			switchEntities(startingIndex + j, indices[j]);
		}
	}
}

#endif
