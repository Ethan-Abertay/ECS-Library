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
#define IMPL 1
// The refactor method (only relevent if implementation uses it)
#define REFAC 1
// The number of entities 
#define ECS_ENTITY_CONFIG 2

// Ensure macros have valid integral numbers
#if IMPL <= 0 || IMPL > 3 || REFAC <= 0 || REFAC > 2 || ECS_ENTITY_CONFIG <= 0 || ECS_ENTITY_CONFIG > 3
#error invalid numbers used as macros (ECS.h)
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

namespace math
{
	struct Vector2
	{
		Vector2() = default;
		Vector2(float x_, float y_) { x = x_; y = y_; }

		Vector2 operator* (float f)
		{
			Vector2 output = *this;
			output.x *= f;
			output.y *= f;
			return output;
		}

		Vector2& operator+= (const Vector2 &rhs)
		{
			x += rhs.x;
			y += rhs.y;
			return *this;
		}

		float x = 0, y = 0;
	};
};

namespace ecs
{
	struct EntityDesignation
	{
		EntityDesignation() {}

		//EntityDesignation(EntityID ID_, CompMask mask_ = 0)
		////EntityDesignation(CompMask mask_ = 0)
		//{
		//	//ID = ID_;
		//	compMask = mask_;
		//}

		//EntityID ID;
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

		byte* data = 0;
		const size_t elementSize;
	};
};

class ECS
{
public:
	ECS();
	~ECS();

	template<class ... T> EntityID createEntity();

	/* ----------------------- Public Functions Defined in Header----------------------- */

	template <class ... T> void initComponents();
	template<class ... T> void processSystems(float DeltaTime);

	template<class ... T> void assignComps(EntityID ID);
	template<class T> void assignComp(EntityID ID);
	template<class T> void unassignComp(EntityID ID);

	template<class ... ComponentClasses> unique_ptr<vector<EntityID>> getEntitiesWithComponents();	// This should only be use for nested looping as it may be faster than normal method
	template<class T> T* getEntitysComponent(EntityID entityID);
	// Inlining a parameter pack function may result in large function bodies which would hurt instruction cache?
	// Not super sure since I imagine compilers would just loop in instruciton cache and you can't guarantee the compiler would inline this anyway. 
	template<class ... T> inline CompMask getCompMask();	

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
	
	/* ----------------------- Protected Functions Defined in Header----------------------- */
	template<class T> static inline CompID getCompID();
	template<class T> void createComp();
};

// Function templates called from outside this class cannot be defined in the cpp for some reason. 

template <class ... T>
EntityID ECS::createEntity()
{

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
#endif

	// If any implementation failed, return this (and crash if debugging)
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
void ECS::assignComp(EntityID ID)
{
	// Set comp mask
	entities[ID].compMask.set(getCompID<T>());

	// Call default constructor to initialise variables
	T* comp = getEntitysComponent<T>(ID);
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
	return static_cast<T*>(componentPools[(int)getCompID<T>()]->get(entityID));
}

// This should only be use for nested looping as it may be faster than normal method
template<class... ComponentClasses>
unique_ptr<vector<EntityID>> ECS::getEntitiesWithComponents()
{
	// Create output
	unique_ptr<vector<EntityID>> output = std::make_unique<vector<EntityID>>();

	// Get component masks
	CompMask compMask = getCompMask<ComponentClasses ...>();

	// Get entities with these compIDs
	for (int i = 0; i < getNoOfEntities(); i++)	// Loop through entities (depends on implementation)
		if ((entities[i].compMask & compMask) == compMask)	// If this entity has all components required
			output->push_back(i);	// Add this entity's ID to the output

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
}

bool ECS::entityHasComponents(const EntityID index, const CompMask compMask)
{
	return (entities[index].compMask & compMask) == compMask;
}

