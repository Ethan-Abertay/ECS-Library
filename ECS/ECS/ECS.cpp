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


