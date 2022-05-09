#pragma once

/*
	The intention of this system is to utilize preprocessor conditional inclusions.
	By defining at compile time how you wish the ECS library to operate it will conditionally include different
	pieces of source code to the compiler. 

	This could be set by an external config header file which makes more sense as a library
	but for now, there's no need to overcomplicate - I'll define the macros here

	The following explains the options availble for alternative ECS implementations. 
	There is a check (in the preprocessor) to ensure valid numbers are used. 

	Implementations:
		1 - no refactoring at all
		2 - dead entities refactored
		3 - Full refactor

	Refactor Methods:
		1 - component copying
		2 - sparse set

	And now I have this system, I can setup systems to automatically pick the most optimized unsigned integer type for the number of entities desired,
	i.e. how many entities do you want this ECS to utilize:
		1 - 256 entities (one byte)
		2 - 65536 entities (two bytes)
		3 - 4,294,967,296 entities (four bytes)
*/
// The implementation
#define IMPL 2
// The refactor method (only relevent if implementation uses it)
#define REFAC 2
// The number of entities 
#define ECS_ENTITY_CONFIG 2

// Ensure macros have valid integral numbers
#if IMPL <= 0 || IMPL > 3 || REFAC <= 0 || REFAC > 2 || ECS_ENTITY_CONFIG <= 0 || ECS_ENTITY_CONFIG > 3
#error Invalid numbers used as macros (ECS.h)
#elif IMPL == 1 && REFAC == 2
#error Cannot use implementation 1 with sparse sets since implementation 1 doesnt refactor at all
#endif

#include <iostream>
#include <array>
#include <bitset>
#include <vector>
#include <assert.h>

using std::cout;
using std::endl;
using std::array;
using std::bitset;
using std::vector;
using std::unique_ptr;

#if ECS_ENTITY_CONFIG == 1
#define MAX_ENTITIES 256
typedef uint8_t EntityID;
#elif ECS_ENTITY_CONFIG == 2
#define MAX_ENTITIES 65536
typedef uint16_t EntityID;
#elif ECS_ENTITY_CONFIG == 3
#define MAX_ENTITIES 4294967296
typedef uint32_t EntityID;
#endif

// The max number of component types this ECS supports
#define MAX_COMPONENTS 8
typedef uint8_t CompID;		// Works for 8 components
typedef bitset<MAX_COMPONENTS> CompMask;	// Used to efficiently indicate component ownership of entities

typedef uint8_t byte;		// Used for dynamic allocation of component pools

// Extern variables
extern CompID unsetComponentID;

//namespace math
//{
//	struct Vector2
//	{
//		Vector2() = default;
//		Vector2(float x_, float y_) { x = x_; y = y_; }
//
//		Vector2 operator* (float f)
//		{
//			Vector2 output = *this;
//			output.x *= f;
//			output.y *= f;
//			return output;
//		}
//
//		Vector2& operator+= (const Vector2 &rhs)
//		{
//			x += rhs.x;
//			y += rhs.y;
//			return *this;
//		}
//
//		float x = 0, y = 0;
//	};
//};

namespace ecs
{
	struct EntityDesignation
	{
		EntityDesignation() = default;

		CompMask compMask = 0;
	};

	struct ComponentPool
	{
		ComponentPool(size_t elementSize_) :
			elementSize{ elementSize_ }	// Set element size
		{
			// Dynamically create component pool
			data = new byte[elementSize * MAX_ENTITIES];
		}
		~ComponentPool()
		{
			delete[] data;
		}

		inline void* get(size_t index)
		{
			return data + index * elementSize;
		}

		inline void copy(size_t from, size_t to)
		{
			memcpy(data + to * elementSize, data + from * elementSize, elementSize);
		}

		inline void switch_ (size_t a, size_t b)
		{
			byte *b_data = new byte[elementSize];	// Get place to store b's new data
			memcpy(b_data, get(a), elementSize);	// Store b's new data
			copy(a, b);								// Copy data from a to b
			memcpy(get(b), b_data, elementSize);	// Copy data from storage into b

			// Free memory
			delete[] b_data;
		}

		byte* data = 0;
		const size_t elementSize;
	};

#if IMPL == 3

	// The sorting group is used to sort an unordered entity array into known groups (EntityGroups)
	struct SortingGroup
	{
		SortingGroup() = default;

		vector<EntityID> indices;	// All indices of entities in the main entity array that have this comp mask
		CompMask compMask = 0;		// The components used by this group
	};

	// The entity group is what is used to allow for more efficient looping and other benefits. 
	struct EntityGroup
	{
		EntityGroup() = default;

		inline EntityID getEndIndex() { return startIndex + noOfEntities - (EntityID)1; }
		inline EntityID getNextIndex() { return startIndex + noOfEntities; };

		EntityID startIndex = 0;	// The index (in main entity array) of the first entity in this group
		EntityID noOfEntities = 0;	// The number of entities in this group
		CompMask compMask = 0;		// The components used by this group
	};

#endif
};

class ECS
{
public:
	ECS();
	~ECS();


	/* ----------------------- Public Functions Defined in Header----------------------- */
	template<class ... T> EntityID createEntity();
	template<class ... T> EntityID init_CreateEntity();
	void destroyEntity(EntityID id);
	void switchEntities(EntityID a, EntityID b);
	void transferEntity(EntityID from, EntityID to);
	bool entityIsDead(EntityID id) { return entities[id].compMask == 0; };

	template <class ... T> void initComponents();
	template<class ... T> void processSystems(float DeltaTime);
	void transferComponents(EntityID from, EntityID to);
	void switchComponents(EntityID a, EntityID b);

	template<class ... T> void assignComps(EntityID ID);
	template<class T> void assignComp(EntityID ID);
	template<class T> void unassignComp(EntityID ID);

	template<class ... ComponentClasses> unique_ptr<vector<EntityID>> getEntitiesWithComponents();	// This should only be use for nested looping as it may be faster than normal method
	template<class T> T* getEntitysComponent(EntityID entityID);
	// Inlining a parameter pack function may result in large function bodies which would hurt instruction cache?
	// Not super sure since I imagine compilers would just loop in instruciton cache and you can't guarantee the compiler would inline this anyway. 
	template<class ... T> inline CompMask getCompMask();	

#if IMPL == 3

	void performFullRefactor();
	vector<ecs::EntityGroup*>& getEntityGroups() { return entityGroups; };

#endif

	// Getters
	//array<ecs::EntityDesignation, MAX_ENTITIES>& getEntities() { return entities; };

#if IMPL == 1

	// Implementation 1 requires you check the entire entity array because alive entities may be anywhere
	EntityID getNoOfEntities() { return EntityID(-1); };	

#elif IMPL == 2 || IMPL == 3

	// Implementation 2 and 3 organize all alive entities to the begining of the array, thus return that number
	EntityID getNoOfEntities() { return noOfEntities; };

#endif

	inline bool entityHasComponents(const EntityID index, const CompMask compMask);

protected:
	// Entities
	array<ecs::EntityDesignation, MAX_ENTITIES> entities;
	EntityID noOfEntities = 0;

	// Component Pools
	vector<ecs::ComponentPool*> componentPools;	// Vector of pointers used because component pools can be very large and it's only set on init, then just read
	
#if REFAC == 2

	// Sparse set linking entity ID to it's component ID rather than them being the same ID (allows for more efficient component movement in theory)
	// The entity ID index has element which is the index to the component.
	// The index of the vector returns the sparse set array of the component pool is the same index in the component pools. i.e. index 1 in the component pools has its sparse set in index 1 here. 
	vector<array<EntityID, MAX_ENTITIES>*> componentSparseSets;

	// Unfortunately, the sparse set method needs a way to know which components in the dense array aren't in use.
	// I believe the best way to do this would be to create a dense bitset to use as a reference because it is memory efficient,
	// and doesn't modify regular runtime performance of the components 

	// A vector of bitsets corresponding to each component (dense) array. 
	// If value is 1 at bit number n, then the component in the dense array at index n is currently in use by an entity
	vector<bitset<MAX_ENTITIES>*> componentAvailabilityBitsets;	

#endif

#if IMPL == 3

	vector<ecs::SortingGroup*> sortingGroups;
	vector<ecs::EntityGroup*> entityGroups;

#endif

	/* ----------------------- Protected Functions Defined in Header----------------------- */
	template<class T> static inline CompID getCompID();
	template<class T> void createComp();
};

// Function templates called from outside this class cannot be defined in the cpp for some reason. 

template <class ... T>
EntityID ECS::createEntity()
{
	// If there's no capability to spawn another entity
	if (noOfEntities == EntityID(-1))
		return -1;

#if IMPL == 1
	// For this implementation (1), dead entities can be anywhere so loop through and find one to place this new entity in
	for (int i = 0; i < entities.size(); i++)
	{
		// If dead
		if (entities[i].compMask == 0)
		{
			// Use this space
			assignComps<T ...>(i);	// Assign components
			noOfEntities++;
			return i;	// Return ID
		}
	}
#elif IMPL == 2

	// This implementation has all alive entities to the begining of the array, thus it can insert a new entity in constant time
	const EntityID newID = noOfEntities++;	// Set to current value, then increment
	assignComps<T ...>(newID);
	return newID;

#elif IMPL == 3

	// This implementation needs to find the group of this entity (or create it if it doesn't exist)
	// Then to append this entity onto the end. If there's no space it needs to move other entities. 

	// Get comp mask of this new entity
	const auto compMask = getCompMask<T ...>();

	auto findGroup = [&](CompMask mask) -> ecs::EntityGroup*
	{
		for (auto* group : entityGroups)
		{
			if (group->compMask == mask)
			{
				return group;
			}
		}
		return 0;
	};

	// Find group
	ecs::EntityGroup* entityGroup = findGroup(compMask);

	auto createNewGroup = [&]() -> EntityID
	{
		// Get the entity group at the end (of vector and entity array as well)
		auto* previousGroup = entityGroups.back();

		// Create entity group
		auto* entityGroup = new ecs::EntityGroup();
		entityGroup->startIndex = previousGroup->getEndIndex();			// Set starting point
		entityGroup->compMask = compMask;				// Set comp mask
		entityGroup->noOfEntities = 1;					// Set number of entities
		entityGroups.push_back(entityGroup);			// Add entity group to the vector of groups

		// Insert entity here
		const EntityID newID = entityGroup->startIndex;	// Set to current value, then increment
		assignComps<T ...>(newID);
		return newID;
	};

	// This lambda has the trick to recursively call itself (pass itself in as a parameter)
	auto moveEntityToEndOfGroup = [&](EntityID entityToMove, auto& moveEntityToEndOfGroup)
	{
		// If entity is dead, return
		if (entities[entityToMove].compMask == 0)
			return;

		// Get group of this entity - Because it's not dead it should have a group
		auto* group = findGroup(entities[entityToMove].compMask);
		assert(group);

		const auto newIndex = group->getEndIndex() + 1;

		// See if there isn't a vacancy at the end of this group
		if (entities[newIndex].compMask != 0)
		{
			// Move that entity out the way
			moveEntityToEndOfGroup(newIndex, moveEntityToEndOfGroup);
		}

		// Transfer (not switch since it's only one alive entity) this entity to the vacany
		transferEntity(entityToMove, newIndex);
	};

	auto insertEntityAtEndOfGroup = [&]() -> EntityID
	{
		// See if there isn't a vancancy at the end of this group
		const auto newIndex = entityGroup->getNextIndex();
		if (entities[newIndex].compMask != 0)
		{
			// There is not a vacancy, the entity that is in the way must be moved to the end of its group.
			// If there is an entity in the way there just repeat until done
			moveEntityToEndOfGroup(entityGroup->getEndIndex() + 1, moveEntityToEndOfGroup);
		}

		// There is now a vacancy

		// Insert entity
		assignComps<T ...>(newIndex);

		// Update group info
		entityGroup->noOfEntities++;

		// Update global info
		noOfEntities++;

		return newIndex;
	};

	// If group hasn't been found then it doesn't exist and a new one needs to be created
	if (!entityGroup)
		return createNewGroup();
	else
		return insertEntityAtEndOfGroup();

#endif

	// If any implementation failed, return -1 (and crash if debugging)
	assert(false);
	return EntityID(-1);	// Entity wasn't created, just return max
}

// This is a function to create entities when first creating them on start up. 
// It allows for more optimized creation
template <class ... T>
EntityID ECS::init_CreateEntity()
{

#if IMPL == 1 || IMPL == 2 || IMPL == 3

	// These implementations have all alive entities to the begining of the array on initialization, thus it can insert a new entity in constant time
	const EntityID newID = noOfEntities++;	// Set to current value, then increment
	assignComps<T ...>(newID);
	return newID;

#endif

	// If any implementation failed, return -1 (and crash if debugging)
	assert(false);
	return EntityID(-1);	// Entity wasn't created, just return max
}

template <class ... T>
void ECS::initComponents()
{
	// For each template class, call this function 
	// This is a fold expression, new to C++17
	(createComp<T>(), ...);
}

template<class ... T>
void ECS::processSystems(float DeltaTime)
{
	(T::process(*this, DeltaTime), ...);
}

template<class T>
void ECS::assignComp(EntityID entityID)
{
	// Set comp mask
	entities[entityID].compMask.set(getCompID<T>());

#if REFAC == 1

	// This method just uses the same index as the entity, thus we can just initalize the component and be done. 

#elif REFAC == 2

	// This method utilizes the sparse set to get the component index from the entity index

	// Get component ID (in the array of arrays (components) which array is our component?)
	const auto compID = getCompID<T>();

	// Get reference to the component availability bitset to find first available space
	auto* availabilityBitset = componentAvailabilityBitsets[compID];

	// Get reference to sparse set to assign the component to this entity
	auto* sparseSet = componentSparseSets[compID];

	// Find available spot (this is linear time and much slower than REFAC 1's constant time here)
	for (int i = 0; i < MAX_ENTITIES; i++)
	{
		// If this sparse set has a vacanncy
		if (!availabilityBitset->test(i))
		{
			// Set the bit
			availabilityBitset->set(i);

			// Assign component to entity in sparse set
			(*sparseSet)[entityID] = i;

			break;
		}
	}

#endif

	// Call default constructor to initialise variables in the component
	T* comp = getEntitysComponent<T>(entityID);
	*comp = T();

}

template<class ... T>
void ECS::assignComps(EntityID ID)
{
	(assignComp<T>(ID), ...);
}

template<class T>
void ECS::unassignComp(EntityID ID)
{
	entities[ID].compMask.set(getCompID<T>(), false);
}

template<class T>
T* ECS::getEntitysComponent(EntityID entityID)
{
#if REFAC == 1

	// Components are indexed in the component pool by the same index used to get the entity in the entity array (the entityID)
	return static_cast<T*>(componentPools[getCompID<T>()]->get(entityID));

#elif REFAC == 2

	// There is now a sparse set inbetween the entity array and the component array. 
	// To get the index to the component array the entityID is used in the sparse set, the element is the index in the component array
	const EntityID compIndex = componentSparseSets[getCompID<T>()]->at(entityID);
	return static_cast<T*>(componentPools[getCompID<T>()]->get(compIndex));

#endif
}

// This should only be use for nested looping as it may be faster than normal method
template<class... ComponentClasses>
unique_ptr<vector<EntityID>> ECS::getEntitiesWithComponents()
{
	// Create output
	unique_ptr<vector<EntityID>> output = std::make_unique<vector<EntityID>>();

	// Get component masks
	CompMask compMask = getCompMask<ComponentClasses ...>();

#if IMPL < 3

	// Get entities with these compIDs
	for (int i = 0; i < getNoOfEntities(); i++)	// Loop through entities (depends on implementation)
		if ((entities[i].compMask & compMask) == compMask)	// If this entity has all components required
			output->push_back(i);	// Add this entity's ID to the output

#elif IMPL == 3

	for (auto group : entityGroups)
		for (int i = group->startIndex; i < group->getNextIndex(); i++)
			output->push_back(i);

#endif

	return output;
}

template<class ... T>
CompMask ECS::getCompMask()
{
	CompMask output;

	// Unpack parameter pack
	int ids[] = { getCompID<T>() ... };

	// Set the component mask bits
	for (int i = 0; i < sizeof...(T); i++)
		output.set(ids[i]);

	// Return comp mask
	return output;
}

template<class T> 
static CompID ECS::getCompID()
{
	static CompID output = unsetComponentID++;	// Set to current value and increment for next comp
	return output;
}

template<class T>
void ECS::createComp()
{
	// Create new component pool and add it the pools
	componentPools.push_back(new ecs::ComponentPool(sizeof(T)));

	// This set's the new component's ID which is the index to this pool in the vector of pools
	// This works as long as you create all component pools initially (don't get a comp's ID before creating it's pool or the indexes will mess up)
	getCompID<T>();

#if REFAC == 2

	// Setup sparse set
	componentSparseSets.push_back(new array<EntityID, MAX_ENTITIES>());

	// Create indicator bitset
	componentAvailabilityBitsets.push_back(new bitset<MAX_ENTITIES>());	// They are automatically all set to 0	

#endif
}

bool ECS::entityHasComponents(const EntityID index, const CompMask compMask)
{
	return (entities[index].compMask & compMask) == compMask;
}

