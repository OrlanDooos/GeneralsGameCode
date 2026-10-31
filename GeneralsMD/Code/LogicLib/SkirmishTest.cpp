/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 OrlanDooos
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// SkirmishTest.cpp: a scripted gameplay test through GeneralsLogic.h only. It starts a skirmish
// as the USA against an easy AI, has the dozer build a War Factory (and what it requires),
// produces Crusader tanks and orders them to destroy an enemy unit. A single tank does not
// survive the trip into the base of the AI, so there are a few.
//
// Usage: generalszh_skirmishtest <gameDir> [mapName] [engine arguments...]
// Without a map name it lists the maps and uses the first one for two players.

#include "GeneralsLogic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace
{

const char *const PLAYER_FACTION = "FactionAmerica";
const char *const ENEMY_FACTION = "FactionGLA";
const char *const DOZER = "AmericaVehicleDozer";
const char *const COMMAND_CENTER = "AmericaCommandCenter";
const char *const TANK = "AmericaTankCrusader";
const int TANK_COUNT = 3;

// Built in this order, each one needs the ones before it.
const char *const BUILD_ORDER[] = { "AmericaPowerPlant", "AmericaSupplyCenter", "AmericaWarFactory" };
const int BUILD_ORDER_COUNT = sizeof(BUILD_ORDER) / sizeof(BUILD_ORDER[0]);

const uint32_t MAX_FRAMES = 15 * 60 * GZ_LOGIC_FRAMES_PER_SECOND; // 15 minutes of game time

std::vector<GzObjectInfo> getObjects()
{
	std::vector<GzObjectInfo> objects(gz_get_object_count());
	objects.resize(gz_get_objects(objects.data(), static_cast<int>(objects.size())));
	return objects;
}

// Finds the living objects of the player with this template.
std::vector<uint32_t> findObjects(int player, const char *templateName, bool mustBeComplete)
{
	std::vector<uint32_t> ids;
	for (const GzObjectInfo &o : getObjects())
	{
		if (o.playerIndex != player || (o.flags & GZ_OBJECT_FLAG_DEAD) || strcmp(o.templateName, templateName) != 0)
			continue;
		if (mustBeComplete && (o.flags & GZ_OBJECT_FLAG_UNDER_CONSTRUCTION))
			continue;
		ids.push_back(o.id);
	}
	return ids;
}

// Finds a living object of the player with this template, or returns 0.
uint32_t findObject(int player, const char *templateName, bool mustBeComplete)
{
	const std::vector<uint32_t> ids = findObjects(player, templateName, mustBeComplete);
	return ids.empty() ? 0 : ids.front();
}

bool isAlive(uint32_t id, GzObjectInfo &info)
{
	return gz_get_object(id, &info) == 0 && !(info.flags & GZ_OBJECT_FLAG_DEAD);
}

double distance(const GzObjectInfo &a, const GzObjectInfo &b)
{
	return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

// The enemy unit closest to the tank, preferring infantry and vehicles over structures.
uint32_t findTarget(int localPlayer, const GzObjectInfo &tank)
{
	uint32_t best = 0;
	double bestScore = 1e30;
	for (const GzObjectInfo &o : getObjects())
	{
		if (o.playerIndex < 0 || o.playerIndex == localPlayer || (o.flags & GZ_OBJECT_FLAG_DEAD) ||
			(o.flags & GZ_OBJECT_FLAG_PROJECTILE) || o.maxHealth <= 0.0f)
			continue;
		if (!(o.flags & (GZ_OBJECT_FLAG_INFANTRY | GZ_OBJECT_FLAG_VEHICLE | GZ_OBJECT_FLAG_STRUCTURE)))
			continue;
		// Neutral objects, like the civilians and supplies, belong to player 0.
		if (o.playerIndex == 0)
			continue;
		const double penalty = (o.flags & GZ_OBJECT_FLAG_STRUCTURE) ? 100000.0 : 0.0;
		const double score = distance(o, tank) + penalty;
		if (score < bestScore)
		{
			bestScore = score;
			best = o.id;
		}
	}
	return best;
}

const char *orderResultText(int result)
{
	switch (result)
	{
	case GZ_ORDER_OK: return "ok";
	case GZ_ORDER_NOT_IN_GAME: return "not in game";
	case GZ_ORDER_NO_SUCH_OBJECT: return "no such object";
	case GZ_ORDER_NOT_YOURS: return "not yours";
	case GZ_ORDER_NO_SUCH_TEMPLATE: return "no such template";
	case GZ_ORDER_NO_PREREQUISITES: return "prerequisites missing";
	case GZ_ORDER_NOT_ENOUGH_MONEY: return "not enough money";
	case GZ_ORDER_CANNOT_MAKE: return "cannot make";
	case GZ_ORDER_ILLEGAL_LOCATION: return "illegal location";
	case GZ_ORDER_NO_SUCH_TARGET: return "no such target";
	default: return "?";
	}
}

void printTime(const char *what)
{
	const uint32_t seconds = gz_get_frame() / GZ_LOGIC_FRAMES_PER_SECOND;
	printf("[%02u:%02u] %s\n", seconds / 60, seconds % 60, what);
}

bool step(int frames)
{
	for (int i = 0; i < frames; ++i)
		if (gz_step() != GZ_STEP_OK)
			return false;
	return true;
}

std::string pickMap()
{
	const int count = gz_get_map_count();
	printf("%d skirmish maps\n", count);
	std::string first;
	for (int i = 0; i < count; ++i)
	{
		char name[260];
		int players = 0;
		if (gz_get_map(i, name, sizeof(name), &players) != 0)
			continue;
		if (i < 10)
			printf("  %s (%d players)\n", name, players);
		if (first.empty() && players == 2)
			first = name;
	}
	return first;
}

int runTest(const std::string &map)
{
	printf("Map: %s\n", map.c_str());
	const int startResult = gz_start_skirmish(map.c_str(), PLAYER_FACTION, ENEMY_FACTION, 1, 20000, 12345);
	if (startResult != 0)
	{
		printf("FAIL: gz_start_skirmish returned %d\n", startResult);
		return 1;
	}

	const int me = gz_get_local_player();
	printf("Local player %d, money %d\n", me, gz_get_player_money(me));

	const uint32_t dozer = findObject(me, DOZER, true);
	const uint32_t commandCenter = findObject(me, COMMAND_CENTER, true);
	if (dozer == 0 || commandCenter == 0)
	{
		printf("FAIL: no dozer (%u) or command center (%u) at the start\n", dozer, commandCenter);
		return 1;
	}
	GzObjectInfo cc;
	gz_get_object(commandCenter, &cc);
	printf("Dozer #%u, command center #%u at (%.0f, %.0f)\n", dozer, commandCenter, cc.x, cc.y);

	// 1. Build the structures one after the other.
	for (int b = 0; b < BUILD_ORDER_COUNT; ++b)
	{
		const char *structure = BUILD_ORDER[b];
		// Away from the command center, towards the map center, so that the base does not get crowded.
		float x = 0, y = 0;
		const float offset = 180.0f + 120.0f * b;
		int result = gz_find_build_location(dozer, structure, cc.x + offset, cc.y, &x, &y);
		if (result != GZ_ORDER_OK)
			result = gz_find_build_location(dozer, structure, cc.x - offset, cc.y, &x, &y);
		if (result == GZ_ORDER_OK)
			result = gz_order_build(dozer, structure, x, y, 0.0f);
		if (result != GZ_ORDER_OK)
		{
			printf("FAIL: cannot order %s: %s\n", structure, orderResultText(result));
			return 1;
		}
		char text[256];
		snprintf(text, sizeof(text), "Ordered %s at (%.0f, %.0f), money %d", structure, x, y, gz_get_player_money(me));
		printTime(text);

		uint32_t built = 0;
		while (built == 0)
		{
			if (!step(GZ_LOGIC_FRAMES_PER_SECOND) || gz_get_frame() > MAX_FRAMES)
			{
				printf("FAIL: %s was not finished\n", structure);
				return 1;
			}
			built = findObject(me, structure, true);
		}
		snprintf(text, sizeof(text), "%s #%u is finished", structure, built);
		printTime(text);
	}

	// 2. Produce the tanks.
	const uint32_t factory = findObject(me, BUILD_ORDER[BUILD_ORDER_COUNT - 1], true);
	for (int i = 0; i < TANK_COUNT; ++i)
	{
		const int result = gz_order_produce(factory, TANK);
		if (result != GZ_ORDER_OK)
		{
			printf("FAIL: cannot produce %s: %s\n", TANK, orderResultText(result));
			return 1;
		}
	}
	char text[256];
	snprintf(text, sizeof(text), "Ordered %d Crusaders, money %d", TANK_COUNT, gz_get_player_money(me));
	printTime(text);

	std::vector<uint32_t> tanks;
	while (static_cast<int>(tanks.size()) < TANK_COUNT)
	{
		if (!step(GZ_LOGIC_FRAMES_PER_SECOND) || gz_get_frame() > MAX_FRAMES)
		{
			printf("FAIL: the tanks were not produced\n");
			return 1;
		}
		tanks = findObjects(me, TANK, true);
	}
	// Give the last one a moment to roll out of the factory.
	step(3 * GZ_LOGIC_FRAMES_PER_SECOND);
	GzObjectInfo tankInfo;
	gz_get_object(tanks.front(), &tankInfo);
	snprintf(text, sizeof(text), "%d Crusaders are out, the first at (%.0f, %.0f)", TANK_COUNT, tankInfo.x, tankInfo.y);
	printTime(text);

	// 3. Destroy an enemy unit.
	const uint32_t target = findTarget(me, tankInfo);
	if (target == 0)
	{
		printf("FAIL: no enemy found\n");
		return 1;
	}
	GzObjectInfo targetInfo;
	gz_get_object(target, &targetInfo);
	for (uint32_t tank : tanks)
	{
		const int result = gz_order_attack(tank, target);
		if (result != GZ_ORDER_OK)
		{
			printf("FAIL: cannot order the attack: %s\n", orderResultText(result));
			return 1;
		}
	}
	snprintf(text, sizeof(text), "Attack ordered on %s #%u of player %d at (%.0f, %.0f), %.0f away, health %.0f/%.0f",
		targetInfo.templateName, target, targetInfo.playerIndex, targetInfo.x, targetInfo.y,
		distance(tankInfo, targetInfo), targetInfo.health, targetInfo.maxHealth);
	printTime(text);

	for (int seconds = 1; ; ++seconds)
	{
		if (!step(GZ_LOGIC_FRAMES_PER_SECOND) || gz_get_frame() > MAX_FRAMES)
		{
			printf("FAIL: the target was not destroyed in time\n");
			return 1;
		}

		int tanksAlive = 0;
		float tanksHealth = 0.0f;
		GzObjectInfo lead = {};
		for (uint32_t tank : tanks)
		{
			if (isAlive(tank, tankInfo))
			{
				if (tanksAlive++ == 0)
					lead = tankInfo;
				tanksHealth += tankInfo.health;
			}
		}

		if (!isAlive(target, targetInfo))
			break;

		if (tanksAlive == 0)
		{
			printTime("FAIL: all Crusaders were destroyed");
			return 1;
		}

		if (seconds % 10 == 0)
		{
			snprintf(text, sizeof(text), "  %d tanks alive, total health %.0f, lead at (%.0f, %.0f), target %.0f away, health %.0f/%.0f",
				tanksAlive, tanksHealth, lead.x, lead.y, distance(lead, targetInfo), targetInfo.health, targetInfo.maxHealth);
			printTime(text);
		}
	}

	int survivors = 0;
	for (uint32_t tank : tanks)
		if (isAlive(tank, tankInfo))
			++survivors;
	snprintf(text, sizeof(text), "PASS: the Crusaders destroyed %s #%u, %d of %d tanks survived",
		targetInfo.templateName, target, survivors, TANK_COUNT);
	printTime(text);

	// A little more, so that the replay shows the end.
	step(5 * GZ_LOGIC_FRAMES_PER_SECOND);
	return 0;
}

} // namespace

int main(int argc, char **argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	if (argc < 2)
	{
		printf("Usage: %s <gameDir> [mapName] [engine arguments...]\n", argv[0]);
		return 2;
	}

	// An argument that starts with '-' is an engine argument, otherwise it is the map.
	std::string map;
	int firstEngineArg = 2;
	if (argc > 2 && argv[2][0] != '-')
	{
		map = argv[2];
		firstEngineArg = 3;
	}

	if (gz_init(argv[1], argc - firstEngineArg, argv + firstEngineArg) != 0)
	{
		printf("FAIL: gz_init failed\n");
		return 1;
	}

	if (map.empty())
		map = pickMap();

	const int result = map.empty() ? 1 : runTest(map);

	gz_shutdown();
	return result;
}
