#include "ECS.h"

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

EntityID ECS::createEntity()
{
	// This is a temp system of setting entity IDs

	entities[noOfEntities] = ecs::EntityDesignation(noOfEntities);
	noOfEntities++;
	return noOfEntities - 1;
}


