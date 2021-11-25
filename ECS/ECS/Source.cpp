
#include "ECS.h"
#include <chrono>
#include <thread>

namespace c
{
	// All component default constructors MUST ensure variables are reset

	// The position component
	struct Position
	{
		Position() = default;

		math::Vector2 position;
	};

	// The component containing acceleration and velocity
	struct Translation
	{
		Translation() = default;

		math::Vector2 velocity = math::Vector2(0.f, 0.f);
		math::Vector2 acceleration = math::Vector2(0.f, 0.f);
	};
}

namespace s
{
	struct Translation
	{
		static void process(ECS& ecs, float DeltaTime)
		{
			// Get relevent entities
			auto entitiesWithComponents = ecs.getEntitiesWithComponents<c::Position, c::Translation>();

			// Loop through entities
			for (auto &entityID : *entitiesWithComponents)
			{
				// Get this entity's components
				auto *translation = ecs.getEntitysComponent<c::Translation>(entityID);
				auto *position = ecs.getEntitysComponent<c::Position>(entityID);

				// Process this component
				translation->velocity += translation->acceleration * DeltaTime;
				position->position += translation->velocity * DeltaTime;
				cout << position->position.x << endl;
			}
		}
	};
}

void initEntities(ECS& ecs)
{
	auto id = ecs.createEntity();
	ecs.assignComp<c::Position>(id);
	ecs.assignComp<c::Translation>(id);
}

int main()
{
	ECS ecs;
	ecs.initComponents<c::Position, c::Translation>();
	initEntities(ecs);

	float fakeDeltaTime = 1000.f;	// In milliseconds
	while (true)
	{
		// Fix delta time to 1 second for the loop
		ecs.processSystems<s::Translation>(1.f);

		std::this_thread::sleep_for(std::chrono::milliseconds((int)fakeDeltaTime));
	}

	return 0;
}
