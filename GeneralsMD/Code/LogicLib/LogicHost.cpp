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

// LogicHost.cpp: a console program that drives the game logic library through GeneralsLogic.h
// only, the way another engine would. It plays a replay and prints the world now and then.
//
// Usage: generalszh_logichost <gameDir> <replayName> [engine arguments...]

#include "GeneralsLogic.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>

static void printWorld()
{
	const int count = gz_get_object_count();
	std::vector<GzObjectInfo> objects(count);
	const int filled = gz_get_objects(objects.data(), count);

	int perPlayer[16] = {};
	for (int i = 0; i < filled; ++i)
	{
		const int player = objects[i].playerIndex;
		if (player >= 0 && player < 16)
			++perPlayer[player];
	}

	printf("Frame %u: %d objects, per player:", gz_get_frame(), filled);
	for (int p = 0; p < 16; ++p)
		if (perPlayer[p] != 0)
			printf(" %d=%d", p, perPlayer[p]);
	printf("\n");

	// A few objects that belong to players, to see that positions and health come through.
	int shown = 0;
	for (int i = 0; i < filled && shown < 3; ++i)
	{
		const GzObjectInfo &o = objects[i];
		if (o.playerIndex < 2 || o.maxHealth <= 0.0f)
			continue;
		printf("  #%u %s player %d at (%.1f, %.1f, %.1f) angle %.2f health %.0f/%.0f%s\n",
			o.id, o.templateName, o.playerIndex, o.x, o.y, o.z, o.orientation, o.health, o.maxHealth,
			(o.flags & GZ_OBJECT_FLAG_DEAD) ? " dead" : "");
		++shown;
	}
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("Usage: %s <gameDir> <replayName> [engine arguments...]\n", argv[0]);
		return 2;
	}

	// Unbuffered, so that the output is not lost if the engine ends the process.
	setvbuf(stdout, nullptr, _IONBF, 0);

	if (gz_init(argv[1], argc - 3, argv + 3) != 0)
	{
		printf("gz_init failed\n");
		return 1;
	}

	if (gz_load_replay(argv[2]) != 0)
	{
		printf("gz_load_replay failed\n");
		gz_shutdown();
		return 1;
	}

	printf("Replay %s has %u frames\n", argv[2], gz_get_replay_frame_count());

	int result = GZ_STEP_OK;
	while ((result = gz_step()) == GZ_STEP_OK)
	{
		// Every minute of game time.
		if (gz_get_frame() % (60 * GZ_LOGIC_FRAMES_PER_SECOND) == 0)
			printWorld();
	}
	printWorld();

	const bool ok = result == GZ_STEP_FINISHED;
	printf("Result: %s at frame %u\n", ok ? "replay finished without CRC mismatch" :
		result == GZ_STEP_CRC_MISMATCH ? "CRC MISMATCH" : "error", gz_get_frame());

	gz_shutdown();
	printf("Shutdown returned to the host\n");
	return ok ? 0 : 1;
}
