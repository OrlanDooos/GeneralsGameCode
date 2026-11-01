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

// GeneralsLogic.cpp: implements the C interface of GeneralsLogic.h. It takes the place of WinMain
// and GameMain: it starts the engine headless and lets the host drive the logic frame by frame.

#include "GeneralsLogic.h"

#include <windows.h>

#include <string>
#include <vector>

#include "Common/BuildAssistant.h"
#include "Common/CommandLine.h"
#include "Common/CriticalSection.h"
#include "Common/FramePacer.h"
#include "Common/GameEngine.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "Common/MessageStream.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/RandomValue.h"
#include "Common/Recorder.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/version.h"
#include "Common/WorkingDirectory.h"
#include "GameClient/MapUtil.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameNetwork/GameInfo.h"
#include "Win32Device/Common/Win32GameEngine.h"
#include "Win32Device/GameClient/Win32Mouse.h"


// Globals that the engine expects from WinMain.cpp. Without a window they stay empty.
HINSTANCE ApplicationHInstance = nullptr;
HWND ApplicationHWnd = nullptr;
Win32Mouse *TheWin32Mouse = nullptr;
DWORD TheMessageTime = 0;
const Char *g_strFile = "data\\Generals.str";
const Char *g_csfFile = "data\\%s\\Generals.csf";
const char *gAppPrefix = "logic_"; // Gives the debug log file its own name.

GameEngine *CreateGameEngine()
{
	Win32GameEngine *engine = NEW Win32GameEngine;
	engine->setIsActive(TRUE);
	return engine;
}

namespace
{

CriticalSection s_critSec1, s_critSec2, s_critSec3, s_critSec4;

bool s_initialized = false;

enum class GameKind { None, Replay, Skirmish };
GameKind s_gameKind = GameKind::None;

// Keeps the strings alive for the engine, which keeps pointers into them.
std::vector<std::string> s_argStrings;
std::vector<char *> s_argPointers;

void setArguments(int argc, const char *const *args)
{
	s_argStrings.clear();
	s_argStrings.push_back("generalslogic"); // Program name, which the parser skips.
	s_argStrings.push_back("-headless");
	for (int i = 0; i < argc; ++i)
		s_argStrings.push_back(args[i]);

	s_argPointers.clear();
	for (size_t i = 0; i < s_argStrings.size(); ++i)
		s_argPointers.push_back(&s_argStrings[i][0]);
	s_argPointers.push_back(nullptr);

	CommandLine::setArguments(static_cast<int>(s_argStrings.size()), s_argPointers.data());
}

void fillObjectInfo(const Object *obj, GzObjectInfo &info)
{
	memset(&info, 0, sizeof(info));

	info.id = obj->getID();

	const Player *player = obj->getControllingPlayer();
	info.playerIndex = player != nullptr ? player->getPlayerIndex() : -1;

	const Coord3D *pos = obj->getPosition();
	info.x = pos->x;
	info.y = pos->y;
	info.z = pos->z;
	info.orientation = obj->getOrientation();

	const BodyModuleInterface *body = obj->getBodyModule();
	if (body != nullptr)
	{
		info.health = body->getHealth();
		info.maxHealth = body->getMaxHealth();
	}

	if (obj->isEffectivelyDead())
		info.flags |= GZ_OBJECT_FLAG_DEAD;
	if (obj->isKindOf(KINDOF_STRUCTURE))
		info.flags |= GZ_OBJECT_FLAG_STRUCTURE;
	if (obj->isKindOf(KINDOF_INFANTRY))
		info.flags |= GZ_OBJECT_FLAG_INFANTRY;
	if (obj->isKindOf(KINDOF_VEHICLE))
		info.flags |= GZ_OBJECT_FLAG_VEHICLE;
	if (obj->isKindOf(KINDOF_PROJECTILE))
		info.flags |= GZ_OBJECT_FLAG_PROJECTILE;
	if (obj->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		info.flags |= GZ_OBJECT_FLAG_UNDER_CONSTRUCTION;

	const ThingTemplate *tmpl = obj->getTemplate();
	if (tmpl != nullptr)
		strncpy_s(info.templateName, sizeof(info.templateName), tmpl->getName().str(), _TRUNCATE);
}

// Returns the living object with this id that the local player controls, or sets error.
Object *findOrderedObject(uint32_t id, int &error)
{
	error = GZ_ORDER_OK;
	if (!s_initialized || s_gameKind != GameKind::Skirmish || !TheGameLogic->isInGame())
	{
		error = GZ_ORDER_NOT_IN_GAME;
		return nullptr;
	}
	Object *obj = TheGameLogic->findObjectByID(static_cast<ObjectID>(id));
	if (obj == nullptr || obj->isEffectivelyDead())
	{
		error = GZ_ORDER_NO_SUCH_OBJECT;
		return nullptr;
	}
	if (obj->getControllingPlayer() != ThePlayerList->getLocalPlayer())
	{
		error = GZ_ORDER_NOT_YOURS;
		return nullptr;
	}
	return obj;
}

int canMakeToOrderResult(CanMakeType canMake)
{
	switch (canMake)
	{
	case CANMAKE_OK: return GZ_ORDER_OK;
	case CANMAKE_NO_PREREQ: return GZ_ORDER_NO_PREREQUISITES;
	case CANMAKE_NO_MONEY: return GZ_ORDER_NOT_ENOUGH_MONEY;
	default: return GZ_ORDER_CANNOT_MAKE;
	}
}

// The order messages go straight to the command list, like the ones of a replay, because the
// engine does not run the client side translators of the message stream here. The selection
// message makes the object the selected group of the local player, which the order then uses.
void appendSelection(ObjectID id)
{
	GameMessage *msg = newInstance(GameMessage)(GameMessage::MSG_CREATE_SELECTED_GROUP);
	msg->appendBooleanArgument(TRUE);
	msg->appendObjectIDArgument(id);
	TheCommandList->appendMessage(msg);
}

const UnsignedInt BUILD_LOCATION_OPTIONS =
	BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::CLEAR_PATH |
	BuildAssistant::NO_OBJECT_OVERLAP | BuildAssistant::USE_QUICK_PATHFIND;

} // namespace


int gz_init(const char *gameDir, int argc, const char *const *args)
{
	if (s_initialized || gameDir == nullptr)
		return -1;

	try
	{
		TheAsciiStringCriticalSection = &s_critSec1;
		TheUnicodeStringCriticalSection = &s_critSec2;
		TheDmaCriticalSection = &s_critSec3;
		TheMemoryPoolCriticalSection = &s_critSec4;

		initMemoryManager();

		// Set before the parsing, which would otherwise switch to the folder of the executable.
		if (!rts::WorkingDirectory::setCustomWorkingDirectory(gameDir))
			return -2;

		setArguments(argc, args);
		CommandLine::parseCommandLineForStartup();

		TheVersion = NEW Version;
		TheVersion->setVersion(1, 4, 601, 0, AsciiString(""), AsciiString(""),
			AsciiString(__TIME__), AsciiString(__DATE__));

		TheFramePacer = new FramePacer();
		TheGameEngine = CreateGameEngine();
		TheGameEngine->init();
	}
	catch (...)
	{
		return -3;
	}

	s_initialized = true;
	return 0;
}

int gz_load_replay(const char *replayName)
{
	if (!s_initialized || s_gameKind != GameKind::None || replayName == nullptr)
		return -1;

	if (!TheRecorder->simulateReplay(AsciiString(replayName)))
		return -2;

	s_gameKind = GameKind::Replay;
	return 0;
}

int gz_get_map_count(void)
{
	if (!s_initialized || TheMapCache == nullptr)
		return 0;
	if (TheMapCache->empty())
		TheMapCache->updateCache();

	int count = 0;
	for (MapCache::const_iterator it = TheMapCache->begin(); it != TheMapCache->end(); ++it)
		if (it->second.m_isMultiplayer)
			++count;
	return count;
}

int gz_get_map(int index, char *name, int nameSize, int *numPlayers)
{
	if (!s_initialized || TheMapCache == nullptr || index < 0)
		return -1;
	if (TheMapCache->empty())
		TheMapCache->updateCache();

	for (MapCache::const_iterator it = TheMapCache->begin(); it != TheMapCache->end(); ++it)
	{
		if (!it->second.m_isMultiplayer || index-- != 0)
			continue;
		if (name != nullptr && nameSize > 0)
			strncpy_s(name, nameSize, it->first.str(), _TRUNCATE);
		if (numPlayers != nullptr)
			*numPlayers = it->second.m_numPlayers;
		return 0;
	}
	return -1;
}

int gz_start_skirmish(const char *mapName, const char *playerFaction, const char *enemyFaction,
	int aiLevel, int startingCash, int seed)
{
	if (!s_initialized || s_gameKind != GameKind::None || mapName == nullptr ||
		playerFaction == nullptr || enemyFaction == nullptr)
		return -1;

	if (TheMapCache->empty())
		TheMapCache->updateCache();

	const AsciiString map(mapName);
	const MapMetaData *md = TheMapCache->findMap(map);
	if (md == nullptr || !md->m_isMultiplayer || md->m_numPlayers < 2)
		return -2;

	const Int playerTemplate = ThePlayerTemplateStore->getTemplateNumByName(AsciiString(playerFaction));
	const Int enemyTemplate = ThePlayerTemplateStore->getTemplateNumByName(AsciiString(enemyFaction));
	if (playerTemplate < 0 || enemyTemplate < 0)
		return -3;

	// Like SkirmishGameOptionsMenuInit and reallyDoStart. Colors and start positions are left
	// random, GameLogic picks them.
	if (TheSkirmishGameInfo == nullptr)
		TheSkirmishGameInfo = NEW SkirmishGameInfo;
	TheSkirmishGameInfo->init();
	TheSkirmishGameInfo->clearSlotList();
	TheSkirmishGameInfo->reset();
	TheSkirmishGameInfo->setLocalIP(TheSkirmishGameInfo->getSlot(0)->getIP());
	TheSkirmishGameInfo->enterGame();

	GameSlot human;
	human.setState(SLOT_PLAYER, UnicodeString(L"Player"));
	human.setPlayerTemplate(playerTemplate);
	TheSkirmishGameInfo->setSlot(0, human);

	GameSlot ai;
	ai.setState(aiLevel >= 3 ? SLOT_BRUTAL_AI : aiLevel == 2 ? SLOT_MED_AI : SLOT_EASY_AI);
	ai.setPlayerTemplate(enemyTemplate);
	TheSkirmishGameInfo->setSlot(1, ai);

	for (Int i = 2; i < MAX_SLOTS; ++i)
	{
		GameSlot closed;
		closed.setState(SLOT_CLOSED);
		TheSkirmishGameInfo->setSlot(i, closed);
	}

	Money cash;
	cash.deposit(static_cast<UnsignedInt>(startingCash), FALSE, FALSE);
	TheSkirmishGameInfo->setStartingCash(cash);
	TheSkirmishGameInfo->setSuperweaponRestriction(0);
	TheSkirmishGameInfo->setSeed(seed);
	TheSkirmishGameInfo->setMap(map);
	TheSkirmishGameInfo->setMapCRC(md->m_CRC);
	TheSkirmishGameInfo->setMapSize(md->m_filesize);

	TheWritableGlobalData->m_mapName = map;
	TheSkirmishGameInfo->startGame(0);
	InitRandom(seed);

	GameMessage *msg = newInstance(GameMessage)(GameMessage::MSG_NEW_GAME);
	msg->appendIntegerArgument(GAME_SKIRMISH);
	msg->appendIntegerArgument(DIFFICULTY_NORMAL);
	msg->appendIntegerArgument(0);
	msg->appendIntegerArgument(TheGlobalData->m_framesPerSecondLimit);
	TheCommandList->appendMessage(msg);

	s_gameKind = GameKind::Skirmish;

	// The first logic update handles the new game message, which only flags the start. The second
	// one loads the map and creates the players and their starting objects.
	TheGameLogic->UPDATE();
	TheMessageStream->propagateMessages();
	TheGameLogic->UPDATE();
	return TheGameLogic->isInGame() && TheGameLogic->getGameMode() == GAME_SKIRMISH ? 0 : -4;
}

int gz_step(void)
{
	if (!s_initialized)
		return GZ_STEP_ERROR;

	if (s_gameKind == GameKind::Replay)
	{
		// Like ReplaySimulation::simulateReplaysInThisProcess, which runs replays headless.
		if (!TheRecorder->isPlaybackInProgress())
			return GZ_STEP_FINISHED;

		TheGameLogic->UPDATE();

		return TheRecorder->sawCRCMismatch() ? GZ_STEP_CRC_MISMATCH : GZ_STEP_OK;
	}

	if (s_gameKind == GameKind::Skirmish)
	{
		if (!TheGameLogic->isInGame())
			return GZ_STEP_FINISHED;

		// Moves the messages that the logic posts for the next frame, such as its CRC, to the
		// command list, like GameEngine::update does.
		TheMessageStream->propagateMessages();
		TheGameLogic->UPDATE();
		return GZ_STEP_OK;
	}

	return GZ_STEP_FINISHED;
}

int gz_get_terrain_extent(float *maxX, float *maxY, float *cellSize)
{
	if (!s_initialized || !TheGameLogic->isInGame() || maxX == nullptr || maxY == nullptr)
		return -1;

	Region3D extent;
	TheTerrainLogic->getExtent(&extent);
	*maxX = extent.hi.x;
	*maxY = extent.hi.y;
	if (cellSize != nullptr)
		*cellSize = MAP_XY_FACTOR;
	return extent.hi.x > 0.0f && extent.hi.y > 0.0f ? 0 : -2;
}

int gz_get_terrain_heights(float x0, float y0, float step, int nx, int ny,
	float *heights, float *waterHeights)
{
	if (!s_initialized || !TheGameLogic->isInGame() || heights == nullptr || nx <= 0 || ny <= 0)
		return -1;

	for (int j = 0; j < ny; ++j)
	{
		for (int i = 0; i < nx; ++i)
		{
			const Real x = x0 + i * step;
			const Real y = y0 + j * step;
			const int index = j * nx + i;
			heights[index] = TheTerrainLogic->getGroundHeight(x, y);
			if (waterHeights != nullptr)
			{
				Real waterZ = 0.0f;
				waterHeights[index] = TheTerrainLogic->isUnderwater(x, y, &waterZ) ? waterZ : GZ_NO_WATER;
			}
		}
	}
	return 0;
}

int gz_get_local_player(void)
{
	if (!s_initialized || !TheGameLogic->isInGame() || ThePlayerList->getLocalPlayer() == nullptr)
		return -1;
	return ThePlayerList->getLocalPlayer()->getPlayerIndex();
}

int gz_get_player_money(int playerIndex)
{
	if (!s_initialized || !TheGameLogic->isInGame())
		return 0;
	const Player *player = ThePlayerList->getNthPlayer(playerIndex);
	return player != nullptr ? static_cast<int>(player->getMoney()->countMoney()) : 0;
}

int gz_get_object(uint32_t id, GzObjectInfo *info)
{
	if (!s_initialized || info == nullptr || !TheGameLogic->isInGame())
		return GZ_ORDER_NO_SUCH_OBJECT;
	const Object *obj = TheGameLogic->findObjectByID(static_cast<ObjectID>(id));
	if (obj == nullptr)
		return GZ_ORDER_NO_SUCH_OBJECT;
	fillObjectInfo(obj, *info);
	return GZ_ORDER_OK;
}

int gz_find_build_location(uint32_t builderId, const char *templateName, float x, float y,
	float *outX, float *outY)
{
	int error;
	Object *builder = findOrderedObject(builderId, error);
	if (builder == nullptr)
		return error;
	const ThingTemplate *tmpl = TheThingFactory->findTemplate(AsciiString(templateName));
	if (tmpl == nullptr)
		return GZ_ORDER_NO_SUCH_TEMPLATE;

	// Rings of growing radius around the wanted spot.
	const Real step = 10.0f;
	for (Int ring = 0; ring <= 40; ++ring)
	{
		const Real radius = ring * step;
		const Int points = ring == 0 ? 1 : ring * 8;
		for (Int i = 0; i < points; ++i)
		{
			const Real a = (2.0f * PI * i) / points;
			Coord3D pos;
			pos.x = x + radius * Cos(a);
			pos.y = y + radius * Sin(a);
			pos.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
			if (TheBuildAssistant->isLocationLegalToBuild(&pos, tmpl, 0.0f, BUILD_LOCATION_OPTIONS,
				builder, builder->getControllingPlayer()) == LBC_OK)
			{
				*outX = pos.x;
				*outY = pos.y;
				return GZ_ORDER_OK;
			}
		}
	}
	return GZ_ORDER_ILLEGAL_LOCATION;
}

int gz_order_build(uint32_t builderId, const char *templateName, float x, float y, float angle)
{
	int error;
	Object *builder = findOrderedObject(builderId, error);
	if (builder == nullptr)
		return error;
	const ThingTemplate *tmpl = TheThingFactory->findTemplate(AsciiString(templateName));
	if (tmpl == nullptr)
		return GZ_ORDER_NO_SUCH_TEMPLATE;

	const int canMake = canMakeToOrderResult(TheBuildAssistant->canMakeUnit(builder, tmpl));
	if (canMake != GZ_ORDER_OK)
		return canMake;

	Coord3D pos;
	pos.x = x;
	pos.y = y;
	pos.z = TheTerrainLogic->getGroundHeight(x, y);
	if (TheBuildAssistant->isLocationLegalToBuild(&pos, tmpl, angle, BUILD_LOCATION_OPTIONS,
		builder, builder->getControllingPlayer()) != LBC_OK)
		return GZ_ORDER_ILLEGAL_LOCATION;

	// Like PlaceEventTranslator.
	appendSelection(builder->getID());
	GameMessage *msg = newInstance(GameMessage)(GameMessage::MSG_DOZER_CONSTRUCT);
	msg->appendIntegerArgument(tmpl->getTemplateID());
	msg->appendLocationArgument(pos);
	msg->appendRealArgument(angle);
	TheCommandList->appendMessage(msg);
	return GZ_ORDER_OK;
}

int gz_order_produce(uint32_t factoryId, const char *templateName)
{
	int error;
	Object *factory = findOrderedObject(factoryId, error);
	if (factory == nullptr)
		return error;
	const ThingTemplate *tmpl = TheThingFactory->findTemplate(AsciiString(templateName));
	if (tmpl == nullptr)
		return GZ_ORDER_NO_SUCH_TEMPLATE;

	const int canMake = canMakeToOrderResult(TheBuildAssistant->canMakeUnit(factory, tmpl));
	if (canMake != GZ_ORDER_OK)
		return canMake;

	ProductionUpdateInterface *pu = factory->getProductionUpdateInterface();
	if (pu == nullptr)
		return GZ_ORDER_CANNOT_MAKE;

	// Like ControlBar::processCommandUI for GUI_COMMAND_UNIT_BUILD.
	appendSelection(factory->getID());
	GameMessage *msg = newInstance(GameMessage)(GameMessage::MSG_QUEUE_UNIT_CREATE);
	msg->appendIntegerArgument(tmpl->getTemplateID());
	msg->appendIntegerArgument(pu->requestUniqueUnitID());
	TheCommandList->appendMessage(msg);
	return GZ_ORDER_OK;
}

int gz_order_attack(uint32_t unitId, uint32_t targetId)
{
	int error;
	Object *unit = findOrderedObject(unitId, error);
	if (unit == nullptr)
		return error;
	const Object *target = TheGameLogic->findObjectByID(static_cast<ObjectID>(targetId));
	if (target == nullptr || target->isEffectivelyDead())
		return GZ_ORDER_NO_SUCH_TARGET;

	appendSelection(unit->getID());
	GameMessage *msg = newInstance(GameMessage)(GameMessage::MSG_DO_ATTACK_OBJECT);
	msg->appendObjectIDArgument(target->getID());
	TheCommandList->appendMessage(msg);
	return GZ_ORDER_OK;
}

int gz_order_move(uint32_t unitId, float x, float y)
{
	int error;
	Object *unit = findOrderedObject(unitId, error);
	if (unit == nullptr)
		return error;

	Coord3D pos;
	pos.x = x;
	pos.y = y;
	pos.z = TheTerrainLogic->getGroundHeight(x, y);

	appendSelection(unit->getID());
	GameMessage *msg = newInstance(GameMessage)(GameMessage::MSG_DO_MOVETO);
	msg->appendLocationArgument(pos);
	TheCommandList->appendMessage(msg);
	return GZ_ORDER_OK;
}

uint32_t gz_get_frame(void)
{
	return s_initialized ? TheGameLogic->getFrame() : 0;
}

uint32_t gz_get_replay_frame_count(void)
{
	return s_initialized && TheRecorder->isPlaybackInProgress() ? TheRecorder->getPlaybackFrameCount() : 0;
}

int gz_get_object_count(void)
{
	if (!s_initialized)
		return 0;

	int count = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj != nullptr; obj = obj->getNextObject())
		++count;
	return count;
}

int gz_get_objects(GzObjectInfo *objects, int maxObjects)
{
	if (!s_initialized || objects == nullptr)
		return 0;

	int count = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj != nullptr && count < maxObjects; obj = obj->getNextObject())
		fillObjectInfo(obj, objects[count++]);
	return count;
}

void gz_end_game(void)
{
	if (!s_initialized)
		return;

	if (s_gameKind == GameKind::Replay)
		TheRecorder->stopPlayback();

	// Like leaving the game from the menu. For a skirmish this also makes the recorder complete
	// the replay.
	if (TheGameLogic->isInGame())
	{
		TheCommandList->appendMessage(newInstance(GameMessage)(GameMessage::MSG_CLEAR_GAME_DATA));
		TheMessageStream->propagateMessages();
		TheGameLogic->UPDATE();
	}
	s_gameKind = GameKind::None;
}

void gz_shutdown(void)
{
	if (!s_initialized)
		return;

	gz_end_game();

	// Like the end of GameMain and WinMain.
	delete TheFramePacer;
	TheFramePacer = nullptr;
	delete TheGameEngine;
	TheGameEngine = nullptr;
	delete TheVersion;
	TheVersion = nullptr;

	shutdownMemoryManager();

	TheUnicodeStringCriticalSection = nullptr;
	TheDmaCriticalSection = nullptr;
	TheMemoryPoolCriticalSection = nullptr;

	s_initialized = false;
}
