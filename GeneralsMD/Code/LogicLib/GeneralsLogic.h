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

// GeneralsLogic.h: plain C interface of the Zero Hour game logic library.
//
// The library runs the game engine headless, without the W3D renderer, so that another program,
// such as a different engine, can drive the simulation and present it in its own way. This header
// deliberately includes no game headers, so a host never sees the game's macros, types or its
// replacement of the global operator new.
//
// Usage:
//   gz_init() once, then either gz_load_replay() or gz_start_skirmish(), then call gz_step() until
//   it does not return GZ_STEP_OK, reading the world with gz_get_objects() and giving orders with
//   gz_order_*() in between. gz_shutdown() at the end. The engine uses global state, so it can be
//   initialized only once per process, and all functions must be called from the same thread.
//
//   Orders are given on behalf of the local player and take effect in the next gz_step(), like
//   the clicks of a player. A skirmish records a replay to 00000000.rep, as the game does.

#pragma once

#include <stdint.h>

#ifdef GENERALSLOGIC_EXPORTS
#define GZ_API __declspec(dllexport)
#else
#define GZ_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GZ_LOGIC_FRAMES_PER_SECOND 30

// Results of gz_step.
#define GZ_STEP_OK            0 // A logic frame was simulated.
#define GZ_STEP_FINISHED      1 // Nothing to simulate anymore, for example the replay ended.
#define GZ_STEP_CRC_MISMATCH  2 // The simulation diverged from the replay. The frame was simulated.
#define GZ_STEP_ERROR        -1

typedef struct GzObjectInfo
{
	uint32_t id;
	int32_t  playerIndex;     // -1 if the object has no controlling player
	float    x, y, z;         // World position
	float    orientation;     // Angle around the z axis in radians
	float    health;
	float    maxHealth;
	uint32_t flags;           // GZ_OBJECT_FLAG_*
	char     templateName[64];// For example "AmericaTankCrusader", truncated if longer
	float    constructionPercent; // 0 to 100 while under construction, 100 otherwise
	int32_t  productionCount; // Units and upgrades in the production queue
	float    productionPercent; // 0 to 100 for the first entry of the queue
	uint32_t targetId;        // The object it attacks, or 0
	uint32_t lastShotFrame;   // The logic frame of its last shot, or 0
} GzObjectInfo;

// What an object can build or produce, see gz_get_build_options.
typedef struct GzBuildOption
{
	char     templateName[64];
	char     displayName[64]; // UTF-8, as the game shows it
	int32_t  cost;
	float    buildSeconds;
	int32_t  isStructure;     // 1 for structures that a dozer constructs, 0 for units
	int32_t  status;          // GZ_ORDER_OK if it can be ordered now, else why not
} GzBuildOption;

#define GZ_OBJECT_FLAG_DEAD               0x01 // Effectively dead, but not yet removed
#define GZ_OBJECT_FLAG_STRUCTURE          0x02
#define GZ_OBJECT_FLAG_INFANTRY           0x04
#define GZ_OBJECT_FLAG_VEHICLE            0x08
#define GZ_OBJECT_FLAG_UNDER_CONSTRUCTION 0x10
#define GZ_OBJECT_FLAG_PROJECTILE         0x20 // Missiles, shells and the like

// Results of the gz_order_* functions and gz_find_build_location.
#define GZ_ORDER_OK                  0
#define GZ_ORDER_NOT_IN_GAME        -1
#define GZ_ORDER_NO_SUCH_OBJECT     -2 // The ordered object does not exist
#define GZ_ORDER_NOT_YOURS          -3 // The ordered object does not belong to the local player
#define GZ_ORDER_NO_SUCH_TEMPLATE   -4
#define GZ_ORDER_NO_PREREQUISITES   -5
#define GZ_ORDER_NOT_ENOUGH_MONEY   -6
#define GZ_ORDER_CANNOT_MAKE        -7 // Any other reason why it cannot be built or produced
#define GZ_ORDER_ILLEGAL_LOCATION   -8
#define GZ_ORDER_NO_SUCH_TARGET     -9

// Initializes the engine. gameDir is the folder with the game data (the .big files). args are
// extra command line arguments for the engine, such as "-ignoreAsserts", without the program
// name. "-headless" is always added. Returns 0 on success.
GZ_API int gz_init(const char *gameDir, int argc, const char *const *args);

// Starts the playback of a replay. The name is relative to the Replays folder of the user data,
// for example "00000000.rep". Returns 0 on success.
GZ_API int gz_load_replay(const char *replayName);

// Returns the number of maps that can be played in a skirmish, and fills name (the path the
// game uses, for example "maps\tournament desert\tournament desert.map") and numPlayers of one.
GZ_API int gz_get_map_count(void);
GZ_API int gz_get_map(int index, char *name, int nameSize, int *numPlayers);

// Starts a skirmish of the local player against one AI. The factions are player template names
// such as "FactionAmerica", "FactionChina" or "FactionGLA". aiLevel is 1 easy, 2 medium,
// 3 brutal. The map is loaded right away. Returns 0 on success.
GZ_API int gz_start_skirmish(const char *mapName, const char *playerFaction, const char *enemyFaction,
	int aiLevel, int startingCash, int seed);

// Simulates one logic frame. Returns one of GZ_STEP_*.
GZ_API int gz_step(void);

// The playable area of the map, from (0, 0) to (*maxX, *maxY), and the size of one height map
// cell. Returns 0 on success.
GZ_API int gz_get_terrain_extent(float *maxX, float *maxY, float *cellSize);

// Samples the ground height on a grid of nx * ny points that starts at (x0, y0) and is step apart,
// row by row along x. If waterHeights is not null, it gets the water surface height where there
// is water, or GZ_NO_WATER. Returns 0 on success.
GZ_API int gz_get_terrain_heights(float x0, float y0, float step, int nx, int ny,
	float *heights, float *waterHeights);

#define GZ_NO_WATER (-1.0e9f)

// Samples the color of the ground on the same kind of grid as gz_get_terrain_heights: 3 bytes
// red, green, blue per point. It is the average color of the terrain texture at the point, as the
// radar of the game shows it. Returns 0 on success.
GZ_API int gz_get_terrain_colors(float x0, float y0, float step, int nx, int ny, uint8_t *rgb);

// Returns the index of the player that gz_order_* commands, or -1.
GZ_API int gz_get_local_player(void);

// Returns the money of a player.
GZ_API int gz_get_player_money(int playerIndex);

// Fills info for one object. Returns 0 on success, or GZ_ORDER_NO_SUCH_OBJECT.
GZ_API int gz_get_object(uint32_t id, GzObjectInfo *info);

// Writes the name of the W3D model that the game shows for the object in its current state, for
// example "AVCrusader" (the file is Art/W3D/AVCrusader.w3d). Returns 0 on success, or -1 if the
// object has no model, like most projectiles that are drawn as effects.
GZ_API int gz_get_object_model(uint32_t id, char *name, int nameSize);

// Reads a file of the game, for example "Art/W3D/AVCrusader.w3d", from the folder of the game or
// its .big archives. With buffer null, returns the size. Otherwise reads up to bufferSize bytes and
// returns how many. Returns -1 if the file does not exist.
GZ_API int gz_read_file(const char *path, void *buffer, int bufferSize);

// Fills what an object of the local player can build (a dozer or worker) or produce (a factory),
// in the order of its command buttons in the game. Returns the number of options filled.
GZ_API int gz_get_build_options(uint32_t objectId, GzBuildOption *options, int maxOptions);

// Checks whether the builder can construct the structure at (x, y). Returns GZ_ORDER_*.
GZ_API int gz_check_build_location(uint32_t builderId, const char *templateName, float x, float y, float angle);

// Finds a spot near (x, y) where the builder can construct the structure. Returns GZ_ORDER_*.
GZ_API int gz_find_build_location(uint32_t builderId, const char *templateName, float x, float y,
	float *outX, float *outY);

// Orders a dozer or worker to construct a structure at (x, y). Returns GZ_ORDER_*.
GZ_API int gz_order_build(uint32_t builderId, const char *templateName, float x, float y, float angle);

// Orders a factory to produce a unit. Returns GZ_ORDER_*.
GZ_API int gz_order_produce(uint32_t factoryId, const char *templateName);

// Orders a unit to attack an object. Returns GZ_ORDER_*.
GZ_API int gz_order_attack(uint32_t unitId, uint32_t targetId);

// Orders a unit to move to (x, y). Returns GZ_ORDER_*.
GZ_API int gz_order_move(uint32_t unitId, float x, float y);

// Orders several units together, like a selected group in the game: they move in formation or
// attack the same target. Objects that are not of the local player are skipped. Returns GZ_ORDER_*.
GZ_API int gz_order_group_move(const uint32_t *unitIds, int count, float x, float y);
GZ_API int gz_order_group_attack(const uint32_t *unitIds, int count, uint32_t targetId);

// Returns the number of the current logic frame.
GZ_API uint32_t gz_get_frame(void);

// Returns the number of logic frames in the playing replay, or 0.
GZ_API uint32_t gz_get_replay_frame_count(void);

// Returns the number of objects in the world.
GZ_API int gz_get_object_count(void);

// Fills up to maxObjects entries and returns how many it filled.
GZ_API int gz_get_objects(GzObjectInfo *objects, int maxObjects);

// Ends the replay or skirmish, so that another one can start. The engine stays initialized, which
// suits a host like an editor that starts and stops the game many times in one process.
GZ_API void gz_end_game(void);

// Ends the game and shuts the engine down.
GZ_API void gz_shutdown(void);

#ifdef __cplusplus
}
#endif
