// ==============================================================
//	This file is part of Glest (www.glest.org)
//
//	Copyright (C) 2001-2008 Martiño Figueroa
//
//	You can redistribute this code and/or modify it under
//	the terms of the GNU General Public License as published
//	by the Free Software Foundation; either version 2 of the
//	License, or (at your option) any later version
// ==============================================================

#include "world.h"

#include <algorithm>
#include <cassert>

#include "config.h"
#include "faction.h"
#include "unit.h"
#include "game.h"
#include "core_data.h"
#include "logger.h"
#include "sound_renderer.h"
#include "game_settings.h"
#include "cache_manager.h"
#include <iostream>
#include "sound.h"
#include "sound_renderer.h"

#include "leak_dumper.h"

using namespace Shared::Graphics;
using namespace Shared::Util;

namespace Glest{ namespace Game{

// =====================================================
// 	class World
// =====================================================

// This limit is to keep RAM use under control while offering better performance.
int MaxExploredCellsLookupItemCache = 9500;
//int MaxExploredCellsLookupItemCache = 0;
time_t ExploredCellsLookupItem::lastDebug = 0;

// ===================== PUBLIC ========================

World::World() : mutexFactionNextUnitId(new Mutex(CODE_AT_LINE)) {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	Config &config= Config::getInstance();

	unitParticlesEnabled=config.getBool("UnitParticles","true");

	animatedTilesetObjectPosListLoaded = false;

	ExploredCellsLookupItemCache.clear();
	ExploredCellsLookupItemCacheTimer.clear();
	ExploredCellsLookupItemCacheTimerCount = 0;

	nextCommandGroupId = 0;
	techTree = NULL;
	fogOfWarOverride = false;
	fogOfWarSkillTypeValue = -1;

	fogOfWarSmoothing= config.getBool("FogOfWarSmoothing");
	fogOfWarSmoothingFrameSkip= config.getInt("FogOfWarSmoothingFrameSkip");

	frameCount= 0;

	scriptManager= NULL;
	this->game = NULL;

	thisFactionIndex=0;
	thisTeamIndex=0;
	fogOfWar=false;
	originalGameFogOfWar = fogOfWar;
	perfTimerEnabled=false;
	queuedScenarioName="";
	queuedScenarioKeepFactions=false;
	disableAttackEffects = false;

	loadWorldNode = NULL;
	cacheFowAlphaTexture = false;
	cacheFowAlphaTextureFogOfWarValue = false;

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

void World::cleanup() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	animatedTilesetObjectPosListLoaded = false;

	ExploredCellsLookupItemCache.clear();
	ExploredCellsLookupItemCacheTimer.clear();
	//FowAlphaCellsLookupItemCache.clear();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	for(int i= 0; i < (int)factions.size(); ++i){
		factions[i]->end();
	}

	masterController.clearSlaves(true);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	for(int i= 0; i < (int)factions.size(); ++i){
		delete factions[i];
	}
	factions.clear();

#ifdef LEAK_CHECK_UNITS
	printf("%s::%s\n",__FILE__,__FUNCTION__);
	Unit::dumpMemoryList();
	UnitPathBasic::dumpMemoryList();
#endif

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	delete techTree;
	techTree = NULL;

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	for(std::map<string,::Shared::Sound::StaticSound *>::iterator iterMap = staticSoundList.begin();
		iterMap != staticSoundList.end(); ++iterMap) {
		delete iterMap->second;
	}
	staticSoundList.clear();

	for(std::map<string,::Shared::Sound::StrSound *>::iterator iterMap = streamSoundList.begin();
		iterMap != streamSoundList.end(); ++iterMap) {
		delete iterMap->second;
	}
	streamSoundList.clear();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

World::~World() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	cleanup();

	delete mutexFactionNextUnitId;
	mutexFactionNextUnitId = NULL;

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

void World::endScenario() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
    Logger::getInstance().add(Lang::getInstance().getString("LogScreenGameUnLoadingWorld","",true), true);

    animatedTilesetObjectPosListLoaded = false;

    ExploredCellsLookupItemCache.clear();
    ExploredCellsLookupItemCacheTimer.clear();

	fogOfWarOverride = false;
	originalGameFogOfWar = fogOfWar;
	fogOfWarSkillTypeValue = -1;
	cacheFowAlphaTexture = false;
	cacheFowAlphaTextureFogOfWarValue = false;

	map.end();

	//stats will be deleted by BattleEnd
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

void World::end(){
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
    Logger::getInstance().add(Lang::getInstance().getString("LogScreenGameUnLoadingWorld","",true), true);

    animatedTilesetObjectPosListLoaded = false;

    ExploredCellsLookupItemCache.clear();
    ExploredCellsLookupItemCacheTimer.clear();

	for(int i= 0; i < (int)factions.size(); ++i){
		factions[i]->end();
	}

	masterController.clearSlaves(true);
	for(int i= 0; i < (int)factions.size(); ++i){
		delete factions[i];
	}
	factions.clear();

#ifdef LEAK_CHECK_UNITS
	printf("%s::%s\n",__FILE__,__FUNCTION__);
	Unit::dumpMemoryList();
	UnitPathBasic::dumpMemoryList();
#endif

	fogOfWarOverride = false;
	originalGameFogOfWar = fogOfWar;
	fogOfWarSkillTypeValue = -1;

	map.end();
	cacheFowAlphaTexture = false;
	cacheFowAlphaTextureFogOfWarValue = false;

	//stats will be deleted by BattleEnd
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

// ========================== init ===============================================

void World::addFogOfWarSkillType(const Unit *unit,const FogOfWarSkillType *fowst) {
	std::pair<const Unit *,const FogOfWarSkillType *> fowData;
	fowData.first = unit;
	fowData.second = fowst;

	mapFogOfWarUnitList[unit->getId()] = fowData;

	if((fowst->getApplyToTeam() == true && unit->getTeam() == this->getThisTeamIndex()) ||
		(fowst->getApplyToTeam() == false && unit->getFactionIndex() == this->getThisFactionIndex())) {
		if((fowst->getFowEnable() == false && fogOfWarSkillTypeValue != 0) ||
				(fowst->getFowEnable() == true && fogOfWarSkillTypeValue != 1)) {

			//printf("In [%s::%s Line: %d] current = %d new = %d\n",__FILE__,__FUNCTION__,__LINE__,fogOfWar,fowst->getFowEnable());
			setFogOfWar(fowst->getFowEnable());
		}
	}
}

bool World::removeFogOfWarSkillTypeFromList(const Unit *unit) {
	bool result = false;
	if(mapFogOfWarUnitList.find(unit->getId()) != mapFogOfWarUnitList.end()) {
		mapFogOfWarUnitList.erase(unit->getId());
		result = true;
	}
	return result;
}

void World::removeFogOfWarSkillType(const Unit *unit) {
	bool removedFromList = removeFogOfWarSkillTypeFromList(unit);
	if(removedFromList == true) {
		if(mapFogOfWarUnitList.empty() == true) {

			//printf("In [%s::%s Line: %d] current = %d new = %d\n",__FILE__,__FUNCTION__,__LINE__,fogOfWar,originalGameFogOfWar);
			fogOfWarSkillTypeValue = -1;
			fogOfWarOverride = false;
			minimap.restoreFowTex();
		}
		else {
			bool fowEnabled = false;
			for(std::map<int,std::pair<const Unit *,const FogOfWarSkillType *> >::const_iterator iterMap = mapFogOfWarUnitList.begin();
					iterMap != mapFogOfWarUnitList.end(); ++iterMap) {

				const Unit *unit 				= iterMap->second.first;
				const FogOfWarSkillType *fowst 	= iterMap->second.second;

				if((fowst->getApplyToTeam() == true && unit->getTeam() == this->getThisTeamIndex()) ||
					(fowst->getApplyToTeam() == false && unit->getFactionIndex() == this->getThisFactionIndex())) {
					if(fowst->getFowEnable() == true) {
						fowEnabled = true;
						break;
					}
				}
			}

			if((fowEnabled == false && fogOfWarSkillTypeValue != 0) ||
					(fowEnabled == true && fogOfWarSkillTypeValue != 1)) {
				//printf("In [%s::%s Line: %d] current = %d new = %d\n",__FILE__,__FUNCTION__,__LINE__,fogOfWar,fowEnabled);
				setFogOfWar(fowEnabled);
			}
		}
	}
}

void World::setFogOfWar(bool value) {
	//printf("In [%s::%s Line: %d] current = %d new = %d\n",__FILE__,__FUNCTION__,__LINE__,fogOfWar,value);

	if(fogOfWarOverride == false) {
		minimap.copyFowTex();
	}

	if(value == true) {
		fogOfWarSkillTypeValue = 1;
	}
	else {
		fogOfWarSkillTypeValue = 0;
	}

	fogOfWarOverride = true;
}

void World::clearTileset() {
	tileset = Tileset();
}

void World::restoreExploredFogOfWarCells() {
	for (int i = 0; i < map.getSurfaceW(); ++i) {
		for (int j = 0; j < map.getSurfaceH(); ++j) {
			for (int k = 0;	k < GameConstants::maxPlayers + GameConstants::specialFactions; ++k) {
				if (k == thisTeamIndex) {
					if (map.getSurfaceCell(i, j)->isExplored(k) == true) {
						const Vec2i pos(i, j);
						Vec2i surfPos = pos;
						//compute max alpha
						float maxAlpha = 0.0f;
						if (surfPos.x > 1 && surfPos.y > 1
								&& surfPos.x < map.getSurfaceW() - 2
								&& surfPos.y < map.getSurfaceH() - 2) {
							maxAlpha = 1.f;
						} else if (surfPos.x > 0 && surfPos.y > 0
								&& surfPos.x < map.getSurfaceW() - 1
								&& surfPos.y < map.getSurfaceH() - 1) {
							maxAlpha = 0.3f;
						}

						//compute alpha
						float alpha = maxAlpha;
						minimap.incFowTextureAlphaSurface(surfPos, alpha);
					}
				}
			}
		}
	}
}

void World::init(Game *game, bool createUnits, bool initFactions){

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	ExploredCellsLookupItemCache.clear();
	ExploredCellsLookupItemCacheTimer.clear();

	this->game = game;
	scriptManager= game->getScriptManager();

	GameSettings *gs = game->getGameSettings();
	if(fogOfWarOverride == false) {
		fogOfWar = gs->getFogOfWar();
	}
	originalGameFogOfWar = fogOfWar;

	if(loadWorldNode != NULL) {
		timeFlow.loadGame(loadWorldNode);
	}

	if(initFactions == true) {
		initFactionTypes(gs);
	}
	initCells(fogOfWar); //must be done after knowing faction number and dimensions
	initMap();
	initSplattedTextures();

	unitUpdater.init(game);
	if(loadWorldNode != NULL) {
		unitUpdater.loadGame(loadWorldNode);
	}

	//minimap must be init after sum computation
	initMinimap();

	bool gotError = false;
	bool skipStackTrace = false;
	string sErrBuf = "";

	try {
		if(createUnits) {
			initUnits();
		}
	}
	catch(const megaglest_runtime_error &ex) {
		gotError = true;
		if(ex.wantStackTrace() == true) {
			char szErrBuf[8096]="";
			snprintf(szErrBuf,8096,"In [%s::%s %d]",__FILE__,__FUNCTION__,__LINE__);
			sErrBuf = string(szErrBuf) + string("\nerror [") + string(ex.what()) + string("]\n");
		}
		else {
			skipStackTrace = true;
			sErrBuf = ex.what();
		}
		SystemFlags::OutputDebug(SystemFlags::debugError,sErrBuf.c_str());
	}
	catch(const std::exception &ex) {
		gotError = true;
		char szErrBuf[8096]="";
		snprintf(szErrBuf,8096,"In [%s::%s %d]",__FILE__,__FUNCTION__,__LINE__);
		sErrBuf = string(szErrBuf) + string("\nerror [") + string(ex.what()) + string("]\n");
		SystemFlags::OutputDebug(SystemFlags::debugError,sErrBuf.c_str());
	}

	if(loadWorldNode != NULL) {
		map.loadGame(loadWorldNode,this);

		if(fogOfWar == false) {
		    for(int i=0; i< map.getSurfaceW(); ++i) {
		        for(int j=0; j< map.getSurfaceH(); ++j) {

					SurfaceCell *sc= map.getSurfaceCell(i, j);
					if(sc == NULL) {
						throw megaglest_runtime_error("sc == NULL");
					}

					for (int k = 0; k < GameConstants::maxPlayers; k++) {
						//sc->setExplored(k, (game->getGameSettings()->getFlagTypes1() & ft1_show_map_resources) == ft1_show_map_resources);
						sc->setVisible(k, !fogOfWar);
					}
					for (int k = GameConstants::maxPlayers; k < GameConstants::maxPlayers + GameConstants::specialFactions; k++) {
						sc->setExplored(k, true);
						sc->setVisible(k, true);
					}
		        }
		    }
		}
		else {
			restoreExploredFogOfWarCells();
		}

		//minimap.loadGame(loadWorldNode);
	}

	//initExplorationState(); ... was only for !fog-of-war, now handled in initCells()
	computeFow();
	if(getFrameCount()>1){
		// this is needed for games that are loaded to "switch the light on".
		// otherwise the FowTexture is completely black like in the beginning of a game.
		minimap.updateFowTex(1.f);
	}

	if(gotError == true) {
		throw megaglest_runtime_error(sErrBuf,!skipStackTrace);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

//load tileset
Checksum World::loadTileset(const vector<string> pathList, const string &tilesetName,
		Checksum* checksum, std::map<string,vector<pair<string, string> > > &loadedFileList) {
    Checksum tilsetChecksum;

    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	tilsetChecksum = tileset.loadTileset(pathList, tilesetName, checksum, loadedFileList);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	timeFlow.init(&tileset);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	return tilsetChecksum;
}

Checksum World::loadTileset(const string &dir, Checksum *checksum, std::map<string,vector<pair<string, string> > > &loadedFileList) {
    Checksum tilesetChecksum;

    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	tileset.load(dir, checksum, &tilesetChecksum, loadedFileList);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	timeFlow.init(&tileset);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	return tilesetChecksum;
}

//load tech
Checksum World::loadTech(const vector<string> pathList, const string &techName,
		set<string> &factions, Checksum *checksum,
		std::map<string,vector<pair<string, string> > > &loadedFileList,
		bool validationMode) {
	Checksum techtreeChecksum;

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	techTree = new TechTree(pathList);
	techtreeChecksum = techTree->loadTech( techName, factions,
			checksum,loadedFileList,validationMode);
	return techtreeChecksum;
}

std::vector<std::string> World::validateFactionTypes() {
	return techTree->validateFactionTypes();
}

std::vector<std::string> World::validateResourceTypes() {
	return techTree->validateResourceTypes();
}

//load map
Checksum World::loadMap(const string &path, Checksum *checksum) {
    Checksum mapChecksum;
    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	checksum->addFile(path);
	mapChecksum = map.load(path, techTree, &tileset);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	return mapChecksum;
}

//load map
Checksum World::loadScenario(const string &path, Checksum *checksum, bool resetCurrentScenario, const XmlNode *rootNode) {
	//printf("[%s:%s] Line: %d path [%s]\n",__FILE__,__FUNCTION__,__LINE__,path.c_str());

    Checksum scenarioChecksum;
    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	checksum->addFile(path);

	//printf("[%s:%s] Line: %d\n",__FILE__,__FUNCTION__,__LINE__);

	if(resetCurrentScenario == true) {
		scenario = Scenario();
		if(scriptManager) scriptManager->init(this, this->getGame()->getGameCameraPtr(),rootNode);
	}

	scenarioChecksum = scenario.load(path);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	return scenarioChecksum;
}

// ==================== misc ====================

void World::setQueuedScenario(string scenarioName,bool keepFactions) {
	queuedScenarioName = scenarioName;
	queuedScenarioKeepFactions = keepFactions;
}

void World::updateAllTilesetObjects() {
	Gui *gui = this->game->getGuiPtr();
	if(gui != NULL) {
		Object *selObj = gui->getHighlightedResourceObject();
		if(selObj != NULL) {
			selObj->updateHighlight();
		}
	}

	if(animatedTilesetObjectPosListLoaded == false) {
		animatedTilesetObjectPosListLoaded = true;
		animatedTilesetObjectPosList.clear();

		for(int x = 0; x < map.getSurfaceW(); ++x) {
			for(int y = 0; y < map.getSurfaceH(); ++y) {
				SurfaceCell *sc = map.getSurfaceCell(x,y);
				if(sc != NULL) {
					Object *obj = sc->getObject();
					if(obj != NULL && obj->isAnimated() == true) {
						//obj->update();
						animatedTilesetObjectPosList.push_back(Vec2i(x,y));
					}
				}
			}
		}
	}
	if(animatedTilesetObjectPosList.empty() == false) {
		for(unsigned int i = 0; i < animatedTilesetObjectPosList.size(); ++i) {
			const Vec2i &pos = animatedTilesetObjectPosList[i];
			SurfaceCell *sc = map.getSurfaceCell(pos);
			if(sc != NULL) {
				Object *obj = sc->getObject();
				if(obj != NULL && obj->isAnimated() == true) {
					obj->update();
				}
			}
		}
	}

//	for(int x = 0; x < map.getSurfaceW(); ++x) {
//		for(int y = 0; y < map.getSurfaceH(); ++y) {
//			SurfaceCell *sc = map.getSurfaceCell(x,y);
//			if(sc != NULL) {
//				Object *obj = sc->getObject();
//				if(obj != NULL) {
//					obj->update();
//				}
//			}
//		}
//	}
}

void World::updateAllFactionUnits() {
	bool showPerfStats = Config::getInstance().getBool("ShowPerfStats","false");
	Chrono chronoPerf;
	if(showPerfStats) chronoPerf.start();
	char perfBuf[8096]="";
	std::vector<string> perfList;

	if(scriptManager) scriptManager->onTimerTriggerEvent();

	// Prioritize grouped command units so closest units to target go first
	// units
	int factionCount = getFactionCount();
	//printf("===== STARTING Frame: %d\n",frameCount);
//	Config &config= Config::getInstance();
//	bool sortedUnitsAllowed = config.getBool("AllowGroupedUnitCommands","true");
//
//	int factionCount = getFactionCount();
//	if(sortedUnitsAllowed == true) {
//		for(int i = 0; i < factionCount; ++i) {
//			Faction *faction = getFaction(i);
//			if(faction == NULL) {
//				throw megaglest_runtime_error("faction == NULL");
//			}
//
//			// Sort units by command groups
//			faction->sortUnitsByCommandGroups();
//		}
//	}

	// Clear pathfinder list restrictions
	for(int i = 0; i < factionCount; ++i) {
		Faction *faction = getFaction(i);
		faction->clearUnitsPathfinding();
		faction->clearWorldSynchThreadedLogList();
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	Chrono chrono;
	chrono.start();

	const bool newThreadManager = Config::getInstance().getBool("EnableNewThreadManager","false");
	if(newThreadManager == true) {
		masterController.signalSlaves(&frameCount);
		bool slavesCompleted = masterController.waitTillSlavesTrigger(20000);

		if(SystemFlags::VERBOSE_MODE_ENABLED && chrono.getMillis() >= 10) printf("In [%s::%s Line: %d] *** Faction thread preprocessing took [%lld] msecs for %d factions for frameCount = %d slavesCompleted = %d.\n",__FILE__,__FUNCTION__,__LINE__,(long long int)chrono.getMillis(),factionCount,frameCount,slavesCompleted);

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

	}
	else {
		// Signal the faction threads to do any pre-processing
		for(int i = 0; i < factionCount; ++i) {
			Faction *faction = getFaction(i);
			faction->signalWorkerThread(frameCount);
		}

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		Chrono chrono;
		chrono.start();

		const int MAX_FACTION_THREAD_WAIT_MILLISECONDS = 20000;
		for(;chrono.getMillis() < MAX_FACTION_THREAD_WAIT_MILLISECONDS;) {
			bool workThreadsFinished = true;
			for(int i = 0; i < factionCount; ++i) {
				Faction *faction = getFaction(i);
				if(faction->isWorkerThreadSignalCompleted(frameCount) == false) {
					workThreadsFinished = false;
					break;
				}
			}
			if(workThreadsFinished == true) {
				break;
			}
			// WARNING... Sleep in here causes the server to lag a bit
			//if(chrono.getMillis() % 5 == 0) {
			//	sleep(0);
			//}
		}

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		if(SystemFlags::VERBOSE_MODE_ENABLED && chrono.getMillis() >= 10) printf("In [%s::%s Line: %d] *** Faction thread preprocessing took [%lld] msecs for %d factions for frameCount = %d.\n",__FILE__,__FUNCTION__,__LINE__,(long long int)chrono.getMillis(),factionCount,frameCount);
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	//units
	Chrono chronoPerfUnit;
	int totalUnitsChecked = 0;
	int totalUnitsProcessed = 0;
	for(int i = 0; i < factionCount; ++i) {
		Faction *faction = getFaction(i);

		faction->dumpWorldSynchThreadedLogList();
		faction->clearUnitsPathfinding();

		std::map<CommandClass,int> mapCommandCount;
		std::map<SkillClass,int> mapSkillCount;
		int unitCountStuck = 0;
		int unitCountUpdated = 0;

		int unitCount = faction->getUnitCount();
		for(int j = 0; j < unitCount; ++j) {
			Unit *unit = faction->getUnit(j);
			if(unit == NULL) {
				throw megaglest_runtime_error("unit == NULL");
			}

			CommandClass unitCommandClass = ccCount;
			if(unit->getCurrCommand() != NULL) {
				unitCommandClass = unit->getCurrCommand()->getCommandType()->getClass();
			}

			SkillClass unitSkillClass = scCount;
			if(unit->getCurrSkill() != NULL) {
				unitSkillClass = unit->getCurrSkill()->getClass();
			}

			if(showPerfStats) chronoPerfUnit.start();

			int unitBlockCount = unit->getPath()->getBlockCount();
			bool isStuck = unit->getPath()->isStuck();
			//bool isStuckWithinTolerance = unit->isLastStuckFrameWithinCurrentFrameTolerance(false);
			//uint32 lastStuckFrame = unit->getLastStuckFrame();

			if(unitUpdater.updateUnit(unit) == true) {
				unitCountUpdated++;

				if(unit->getLastStuckFrame() == (unsigned int)frameCount) {
					unitCountStuck++;
				}
				mapCommandCount[unitCommandClass] = mapCommandCount[unitCommandClass] + 1;
				mapSkillCount[unitSkillClass] = mapSkillCount[unitSkillClass] + 1;
			}
			totalUnitsChecked++;

			if(showPerfStats && chronoPerfUnit.getMillis() >= 10) {
				//sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER " stuck: %d [%d] for unit:\n%sBEFORE unitBlockCount = %d, AFTER = %d, BEFORE lastStuckFrame = %u, AFTER: %u\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerfUnit.getMillis(),isStuck,isStuckWithinTolerance,unit->toString().c_str(),unitBlockCount,unit->getPath()->getBlockCount(),lastStuckFrame,unit->getLastStuckFrame());
				sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER " stuck: %d for unit:\n%sBEFORE unitBlockCount = %d, AFTER = %d, BEFORE , AFTER: %u\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerfUnit.getMillis(),isStuck,unit->toString().c_str(),unitBlockCount,unit->getPath()->getBlockCount(),unit->getLastStuckFrame());
				perfList.push_back(perfBuf);
			}

		}
		totalUnitsProcessed += unitCountUpdated;

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER " faction: %d / %d unitCount = %d unitCountUpdated = %d unitCountStuck = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis(),i+1,factionCount,unitCount,unitCountUpdated,unitCountStuck);
			perfList.push_back(perfBuf);

			for(std::map<CommandClass,int>::iterator iterMap = mapCommandCount.begin();
					iterMap != mapCommandCount.end(); ++iterMap) {

				sprintf(perfBuf,"Command class = %d, count = %d\n",iterMap->first,iterMap->second);
				perfList.push_back(perfBuf);
			}

			for(std::map<SkillClass,int>::iterator iterMap = mapSkillCount.begin();
					iterMap != mapSkillCount.end(); ++iterMap) {

				sprintf(perfBuf,"Skill class = %d, count = %d\n",iterMap->first,iterMap->second);
				perfList.push_back(perfBuf);
			}
		}
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER " totalUnitsProcessed = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis(),totalUnitsProcessed);
		perfList.push_back(perfBuf);
	}

	if(showPerfStats && chronoPerf.getMillis() >= 50) {
		for(unsigned int x = 0; x < perfList.size(); ++x) {
			printf("%s",perfList[x].c_str());
		}
	}

	if(SystemFlags::VERBOSE_MODE_ENABLED && chrono.getMillis() >= 20) printf("In [%s::%s Line: %d] *** Faction MAIN thread processing took [%lld] msecs for %d factions for frameCount = %d.\n",__FILE__,__FUNCTION__,__LINE__,(long long int)chrono.getMillis(),factionCount,frameCount);
}

void World::underTakeDeadFactionUnits() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	int factionCount = getFactionCount();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d] factionCount = %d\n",__FILE__,__FUNCTION__,__LINE__,factionCount);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	//undertake the dead
	for(int i = 0; i< factionCount; ++i) {
		Faction *faction = getFaction(i);
		int unitCount = faction->getUnitCount();
		if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d] i = %d, unitCount = %d\n",__FILE__,__FUNCTION__,__LINE__,i,unitCount);

		for(int j= unitCount - 1; j >= 0; j--) {
			Unit *unit= faction->getUnit(j);

			if(unit == NULL) {
				throw megaglest_runtime_error("unit == NULL");
			}

			if(unit->getToBeUndertaken() == true) {
				if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

				unit->undertake();
				delete unit;

				if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
			}
			}

	}
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
}

void World::updateAllFactionConsumableCosts() {
	//food costs
	bool warningSoundNeeded=false;
	int resourceTypeCount = techTree->getResourceTypeCount();
	int factionCount = getFactionCount();
	for(int i = 0; i < resourceTypeCount; ++i) {
		const ResourceType *rt = techTree->getResourceType(i);
		if(rt != NULL && rt->getClass() == rcConsumable && frameCount % (rt->getInterval() * GameConstants::updateFps) == 0) {
			for(int j = 0; j < factionCount; ++j) {
				Faction *faction = getFaction(j);
				if(faction == NULL) {
					throw megaglest_runtime_error("faction == NULL");
				}
				warningSoundNeeded = warningSoundNeeded || faction->applyCostsOnInterval(rt);
			}
		}
	}
	if (warningSoundNeeded) {
		CoreData &coreData = CoreData::getInstance();
		//StaticSound *sound= static_cast<const DieSkillType *>(unit->getType()->getFirstStOfClass(scDie))->getSound();
		SoundRenderer::getInstance().playFx(coreData.getHighlightSound());
	}
}

void World::update() {

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	bool showPerfStats = Config::getInstance().getBool("ShowPerfStats","false");
	Chrono chronoPerf;
	char perfBuf[8096]="";
	std::vector<string> perfList;
	if(showPerfStats) chronoPerf.start();

	Chrono chronoGamePerformanceCounts;

	++frameCount;

	//time
	timeFlow.update();
	if(scriptManager) scriptManager->onDayNightTriggerEvent();

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	//water effects
	waterEffects.update(1.0f);
	// attack effects
	attackEffects.update(0.25f);

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
	// objects on the map from tilesets
	if(this->game) chronoGamePerformanceCounts.start();

	updateAllTilesetObjects();

	if(this->game) this->game->addPerformanceCount("updateAllTilesetObjects",chronoGamePerformanceCounts.getMillis());

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	//units
	if(getFactionCount() > 0) {
		if(this->game) chronoGamePerformanceCounts.start();

		updateAllFactionUnits();

		if(this->game) this->game->addPerformanceCount("updateAllFactionUnits",chronoGamePerformanceCounts.getMillis());

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		//undertake the dead
		if(this->game) chronoGamePerformanceCounts.start();

		underTakeDeadFactionUnits();

		if(this->game) this->game->addPerformanceCount("underTakeDeadFactionUnits",chronoGamePerformanceCounts.getMillis());

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

		//food costs
		if(this->game) chronoGamePerformanceCounts.start();

		updateAllFactionConsumableCosts();

		if(this->game) this->game->addPerformanceCount("updateAllFactionConsumableCosts",chronoGamePerformanceCounts.getMillis());

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
		//fow smoothing
		if(fogOfWarSmoothing && ((frameCount+1) % (fogOfWarSmoothingFrameSkip+1)) == 0) {
			if(this->game) chronoGamePerformanceCounts.start();

			float fogFactor= static_cast<float>(frameCount % GameConstants::updateFps) / GameConstants::updateFps;
			minimap.updateFowTex(clamp(fogFactor, 0.f, 1.f));

			if(this->game) this->game->addPerformanceCount("minimap.updateFowTex",chronoGamePerformanceCounts.getMillis());
		}

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
		//tick
		bool needToTick = canTickWorld();

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		if(needToTick == true) {
			//printf("=========== World is about to be updated, current frameCount = %d\n",frameCount);
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

			if(this->game) chronoGamePerformanceCounts.start();

			tick();

			if(this->game) this->game->addPerformanceCount("world->tick",chronoGamePerformanceCounts.getMillis());
		}

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}
	}

	if(showPerfStats && chronoPerf.getMillis() >= 50) {
		for(unsigned int x = 0; x < perfList.size(); ++x) {
			printf("%s",perfList[x].c_str());
		}
	}
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
}

bool World::canTickWorld() const {
	//tick
	bool needToTick = (frameCount % GameConstants::updateFps == 0);
	return needToTick;
}

void World::tick() {
	bool showPerfStats = Config::getInstance().getBool("ShowPerfStats","false");
	Chrono chronoPerf;
	char perfBuf[8096]="";
	std::vector<string> perfList;
	if(showPerfStats) chronoPerf.start();

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	Chrono chronoGamePerformanceCounts;
	if(this->game) chronoGamePerformanceCounts.start();

	computeFow();

	if(this->game) this->game->addPerformanceCount("world->computeFow",chronoGamePerformanceCounts.getMillis());

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER " fogOfWar: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis(),fogOfWar);
		perfList.push_back(perfBuf);
	}

	if(fogOfWarSmoothing == false) {
		if(this->game) chronoGamePerformanceCounts.start();

		minimap.updateFowTex(1.f);

		if(this->game) this->game->addPerformanceCount("minimap.updateFowTex",chronoGamePerformanceCounts.getMillis());
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	//increase hp
	if(this->game) chronoGamePerformanceCounts.start();

	int factionCount = getFactionCount();
	for(int factionIndex = 0; factionIndex < factionCount; ++factionIndex) {
		Faction *faction = getFaction(factionIndex);
		int unitCount = faction->getUnitCount();

		for(int unitIndex = 0; unitIndex < unitCount; ++unitIndex) {
			Unit *unit = faction->getUnit(unitIndex);
			if(unit == NULL) {
				throw megaglest_runtime_error("unit == NULL");
			}

			unit->tick();
		}
	}
	if(this->game) this->game->addPerformanceCount("world unit->tick()",chronoGamePerformanceCounts.getMillis());

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	//compute resources balance
	if(this->game) chronoGamePerformanceCounts.start();

	std::map<const UnitType *, std::map<const ResourceType *, const Resource *> > resourceCostCache;
	factionCount = getFactionCount();
	for(int factionIndex = 0; factionIndex < factionCount; ++factionIndex) {
		Faction *faction = getFaction(factionIndex);

		//for each resource
		for(int resourceTypeIndex = 0;
				resourceTypeIndex < techTree->getResourceTypeCount(); ++resourceTypeIndex) {
			const ResourceType *rt= techTree->getResourceType(resourceTypeIndex);

			//if consumable
			if(rt != NULL && rt->getClass() == rcConsumable) {

				int balance= 0;
				for(int unitIndex = 0;
						unitIndex < faction->getUnitCount(); ++unitIndex) {

					//if unit operative and has this cost
					const Unit *unit = faction->getUnit(unitIndex);
					if(unit != NULL && unit->isOperative()) {

						const UnitType *ut = unit->getType();
						const Resource *resource = NULL;
						std::map<const UnitType *, std::map<const ResourceType *, const Resource *> >::iterator iterFind = resourceCostCache.find(ut);
						if(iterFind != resourceCostCache.end() &&
								iterFind->second.find(rt) != iterFind->second.end()) {

							resource = iterFind->second.find(rt)->second;
						}
						else {
							resource = ut->getCost(rt);
							resourceCostCache[ut][rt] = resource;
						}
						if(resource != NULL) {
							balance -= resource->getAmount();
						}
					}
				}
				faction->setResourceBalance(rt, balance);
			}
		}
	}
	if(this->game) this->game->addPerformanceCount("world faction->setResourceBalance()",chronoGamePerformanceCounts.getMillis());

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	if(showPerfStats && chronoPerf.getMillis() >= 50) {
		for(unsigned int x = 0; x < perfList.size(); ++x) {
			printf("%s",perfList[x].c_str());
		}
	}
}

Unit* World::findUnitById(int id) const {
	for(int i= 0; i<getFactionCount(); ++i) {
		const Faction* faction= getFaction(i);
		Unit* unit = faction->findUnit(id);
		if(unit != NULL) {
			return unit;
		}
	}
	return NULL;
}

const UnitType* World::findUnitTypeById(const FactionType* factionType, int id) {
	if(factionType == NULL) {
		throw megaglest_runtime_error("factionType == NULL");
	}
	for(int i= 0; i < factionType->getUnitTypeCount(); ++i) {
		const UnitType *unitType = factionType->getUnitType(i);
		if(unitType != NULL && unitType->getId() == id) {
			return unitType;
		}
	}
	return NULL;
}

const UnitType * World::findUnitTypeByName(const string factionName, const string unitTypeName) {
	const UnitType *unitTypeResult = NULL;

	for(int index = 0; unitTypeResult == NULL && index < getFactionCount(); ++index) {
		const Faction *faction = getFaction(index);
		if(factionName == "" || factionName == faction->getType()->getName(false)) {
			for(int unitIndex = 0;
				unitTypeResult == NULL && unitIndex < faction->getType()->getUnitTypeCount(); ++unitIndex) {

				const UnitType *unitType = faction->getType()->getUnitType(unitIndex);
				if(unitType != NULL && unitType->getName(false) == unitTypeName) {
					unitTypeResult = unitType;
					break;
				}
			}
		}
	}
	return unitTypeResult;
}

//looks for a place for a unit around a start location, returns true if succeded
bool World::placeUnit(const Vec2i &startLoc, int radius, Unit *unit, bool spaciated, bool threaded) {
    if(unit == NULL) {
    	throw megaglest_runtime_error("unit == NULL");
    }

	bool freeSpace=false;
	int size= unit->getType()->getSize();
	Field currField= unit->getCurrField();

    for(int r = 1; r < radius; r++) {
        for(int i = -r; i < r; ++i) {
            for(int j = -r; j < r; ++j) {
                Vec2i pos= Vec2i(i,j) + startLoc;
				if(spaciated) {
                    const int spacing = 2;
					freeSpace= map.isFreeCells(pos-Vec2i(spacing), size+spacing*2, currField);
				}
				else {
                    freeSpace= map.isFreeCells(pos, size, currField);
				}

                if(freeSpace) {
                    unit->setPos(pos, false, threaded);
                    Vec2i meetingPos = pos-Vec2i(1);
					unit->setMeetingPos(meetingPos);
                    return true;
                }
            }
        }
    }
    return false;
}

//clears a unit old position from map and places new position
void World::moveUnitCells(Unit *unit, bool threaded) {
	if(unit == NULL) {
    	throw megaglest_runtime_error("unit == NULL");
    }

	Vec2i newPos= unit->getTargetPos();

	//newPos must be free or the same pos as current

	// Only change cell placement in map if the new position is different
	// from the old one
	if(newPos != unit->getPos()) {
		map.clearUnitCells(unit, unit->getPos());
		map.putUnitCells(unit, newPos, false, threaded);
	}
	// Add resources close by to the faction's cache
	unit->getFaction()->addCloseResourceTargetToCache(newPos);

	//water splash
	if(tileset.getWaterEffects() && unit->getCurrField() == fLand) {
		if(map.getSubmerged(map.getCell(unit->getLastPos()))) {

			if(Thread::isCurrentThreadMainThread() == false) {
				throw megaglest_runtime_error("#1 Invalid access to World random from outside main thread current id = " +
						intToStr(Thread::getCurrentThreadId()) + " main = " + intToStr(Thread::getMainThreadId()));
			}

			int unitSize= unit->getType()->getSize();
			for(int i = 0; i < 3; ++i) {
				waterEffects.addWaterSplash(
					Vec2f(unit->getLastPos().x+(float)unitSize/2.0f+random.randRange(-0.9f, -0.1f), unit->getLastPos().y+random.randRange(-0.9f, -0.1f)+(float)unitSize/2.0f), unit->getType()->getSize());
			}
		}
	}

	if(scriptManager) scriptManager->onCellTriggerEvent(unit);
}

void World::addAttackEffects(const Unit *unit) {
	attackEffects.addWaterSplash(
					Vec2f(unit->getPosNotThreadSafe().x, unit->getPosNotThreadSafe().y),1);
}

//returns the nearest unit that can store a type of resource given a position and a faction
Unit *World::nearestStore(const Vec2i &pos, int factionIndex, const ResourceType *rt) {
	float currDist = infinity;
    Unit *currUnit= NULL;

    if(factionIndex >= getFactionCount()) {
    	throw megaglest_runtime_error("factionIndex >= getFactionCount()");
    }

    for(int i=0; i < getFaction(factionIndex)->getUnitCount(); ++i) {
		Unit *u= getFaction(factionIndex)->getUnit(i);
		if(u != NULL) {
			float tmpDist= u->getPos().dist(pos);
			if(tmpDist < currDist &&  u->getType() != NULL && u->getType()->getStore(rt) > 0 && u->isOperative()) {
				currDist= tmpDist;
				currUnit= u;
			}
		}
    }
    return currUnit;
}

bool World::toRenderUnit(const Unit *unit, const Quad2i &visibleQuad) const {
    if(unit == NULL) {
    	throw megaglest_runtime_error("unit == NULL");
    }

	//a unit is rendered if it is in a visible cell or is attacking a unit in a visible cell
    return visibleQuad.isInside(unit->getPosNotThreadSafe()) && toRenderUnit(unit);
}

bool World::toRenderUnit(const Unit *unit) const {
    if(unit == NULL) {
    	throw megaglest_runtime_error("unit == NULL");
    }

    if(showWorldForPlayer(thisFactionIndex) == true) {
        return true;
    }

	return
        (map.getSurfaceCell(Map::toSurfCoords(unit->getCenteredPos()))->isVisible(thisTeamIndex) &&
         map.getSurfaceCell(Map::toSurfCoords(unit->getCenteredPos()))->isExplored(thisTeamIndex) ) ||
        (unit->getCurrSkill()->getClass()==scAttack &&
         map.getSurfaceCell(Map::toSurfCoords(unit->getTargetPos()))->isVisible(thisTeamIndex) &&
         map.getSurfaceCell(Map::toSurfCoords(unit->getTargetPos()))->isExplored(thisTeamIndex));
}

bool World::toRenderUnit(const UnitBuildInfo &pendingUnit) const {
    if(pendingUnit.unit == NULL) {
    	throw megaglest_runtime_error("unit == NULL");
    }

    if(showWorldForPlayer(thisFactionIndex) == true) {
        return true;
    }
    if(pendingUnit.unit->getTeam() != thisTeamIndex) {
    	return false;
    }

	return
        (map.getSurfaceCell(Map::toSurfCoords(pendingUnit.pos))->isVisible(thisTeamIndex) &&
         map.getSurfaceCell(Map::toSurfCoords(pendingUnit.pos))->isExplored(thisTeamIndex) );
}

void World::morphToUnit(int unitId,const string &morphName,bool ignoreRequirements) {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d] unit [%d] morphName [%s] forceUpgradesIfRequired = %d\n",__FILE__,__FUNCTION__,__LINE__,unitId,morphName.c_str(),ignoreRequirements);
	Unit* unit= findUnitById(unitId);
	if(unit != NULL) {
		for(int i = 0; i < unit->getType()->getCommandTypeCount(); ++i) {
			const CommandType *ct = unit->getType()->getCommandType(i);
			const MorphCommandType *mct = dynamic_cast<const MorphCommandType *>(ct);
			if(mct != NULL && mct->getName(false) == morphName) {
				if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d] unit [%d] morphName [%s] comparing mct [%s]\n",__FILE__,__FUNCTION__,__LINE__,unitId,morphName.c_str(),mct->getName(false).c_str());

				std::pair<CommandResult,string> cr(crFailUndefined,"");
				try {
					if(unit->getFaction()->reqsOk(mct) == false && ignoreRequirements == true) {
						if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d] unit [%d] morphName [%s] comparing mct [%s] mct->getUpgradeReqCount() = %d\n",__FILE__,__FUNCTION__,__LINE__,unitId,morphName.c_str(),mct->getName(false).c_str(),mct->getUpgradeReqCount());
						unit->setIgnoreCheckCommand(true);
					}

					const UnitType* unitType = mct->getMorphUnit();
					cr = this->game->getCommander()->tryGiveCommand(unit, mct,unit->getPos(), unitType,CardinalDir(CardinalDir::NORTH));
				}
				catch(const exception &ex) {
					if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
					unit->setIgnoreCheckCommand(false);

					throw megaglest_runtime_error(ex.what());
				}

				if(cr.first == crSuccess) {
					if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
				}
				else {
					if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
				}

				if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d] unit [%d] morphName [%s] comparing mct [%s] returned = %d\n",__FILE__,__FUNCTION__,__LINE__,unitId,morphName.c_str(),mct->getName(false).c_str(),cr.first);

				break;
			}
		}


		if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	}
}

void World::createUnit(const string &unitName, int factionIndex, const Vec2i &pos, bool spaciated) {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d] unitName [%s] factionIndex = %d\n",__FILE__,__FUNCTION__,__LINE__,unitName.c_str(),factionIndex);

	if(factionIndex < (int)factions.size()) {
		Faction* faction= factions[factionIndex];

		if(faction->getIndex() != factionIndex) {
			throw megaglest_runtime_error("faction->getIndex() != factionIndex",true);
		}

		const FactionType* ft= faction->getType();
		const UnitType* ut= ft->getUnitType(unitName);

		UnitPathInterface *newpath = NULL;
		switch(game->getGameSettings()->getPathFinderType()) {
			case pfBasic:
				newpath = new UnitPathBasic();
				break;
			default:
				throw megaglest_runtime_error("detected unsupported pathfinder type!",true);
	    }

		Unit* unit= new Unit(getNextUnitId(faction), newpath, pos, ut, faction, &map, CardinalDir(CardinalDir::NORTH));

		if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d] unit created for unit [%s]\n",__FILE__,__FUNCTION__,__LINE__,unit->toString().c_str());

		if(placeUnit(pos, generationArea, unit, spaciated)) {
			unit->create(true);
			unit->born(NULL);
			if(scriptManager) {
				scriptManager->onUnitCreated(unit);
			}
			if(game->getPaused()==true){
				// new units added in pause mode might change the Fow. ( Scenarios do this )
				computeFow();
				minimap.updateFowTex(1.f);
			}
		}
		else {
			delete unit;
			unit = NULL;
			throw megaglest_runtime_error("Unit cant be placed",true);
		}

		if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d] unit created for unit [%s]\n",__FILE__,__FUNCTION__,__LINE__,unit->toString().c_str());
	}
	else {
		throw megaglest_runtime_error("Invalid faction index in createUnitAtPosition: " + intToStr(factionIndex),true);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

void World::setFactionTeam(int factionIndex, int newTeam) {

	if(factionIndex < 0 || factionIndex > getFactionCount()) {
		throw megaglest_runtime_error("Invalid faction index in getAllUnitsForFaction: " + intToStr(factionIndex),true);
	}

	Faction *faction = getFaction(factionIndex);
	int oldTeam = faction->getTeam();
	if(oldTeam == newTeam) {
		return;
	}
	faction->setTeam(newTeam);
	GameSettings *settings = getGameSettingsPtr();
	settings->setTeam(factionIndex,newTeam);
	getStats()->setTeam(factionIndex, newTeam);

	if(factionIndex == getThisFactionIndex()) {
		setThisTeamIndex(newTeam);
	}
}

int World::getFactionTeam(int factionIndex) {

	if(factionIndex < 0 || factionIndex > getFactionCount()) {
		throw megaglest_runtime_error("Invalid faction index in getAllUnitsForFaction: " + intToStr(factionIndex),true);
	}

	Faction *faction = getFaction(factionIndex);
	return faction->getTeam();
}

void World::giveResource(const string &resourceName, int factionIndex, int amount) {
	if(factionIndex < (int)factions.size()) {
		Faction* faction= factions[factionIndex];
		const ResourceType* rt= techTree->getResourceType(resourceName);
		faction->incResourceAmount(rt, amount);
	}
	else {
		throw megaglest_runtime_error("Invalid faction index in giveResource: " + intToStr(factionIndex),true);
	}
}

int World::getUnitCurrentField(int unitId) {
	int field = -1;
	if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d] unit [%d]\n",__FILE__,__FUNCTION__,__LINE__,unitId);
	Unit* unit= findUnitById(unitId);
	if(unit != NULL) {
		field = unit->getCurrField();
	}

	return field;
}

bool World::getIsUnitAlive(int unitId) {
	bool result = false;
	if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d] unit [%d]\n",__FILE__,__FUNCTION__,__LINE__,unitId);
	Unit* unit= findUnitById(unitId);
	if(unit != NULL) {
		result = unit->isAlive();
	}
	return result;
}

vector<int> World::getUnitsForFaction(int factionIndex,const string& commandTypeName, int field) {
	vector<int> units;

	if(factionIndex < 0 || factionIndex > getFactionCount()) {
		throw megaglest_runtime_error("Invalid faction index in getUnitsForFaction: " + intToStr(factionIndex),true);
	}
	Faction *faction = getFaction(factionIndex);
	if(faction != NULL) {
		CommandType *commandType = NULL;
		if(commandTypeName != "") {
			commandType = CommandTypeFactory::getInstance().newInstance(commandTypeName);
		}
		int unitCount = faction->getUnitCount();
		for(int i = 0; i < unitCount; ++i) {
			Unit *unit = faction->getUnit(i);
			if(unit != NULL) {
				bool addToList = true;
				if(commandType != NULL) {
					if(commandType->getClass() == ccAttack && field >= 0) {
						const AttackCommandType *act = unit->getType()->getFirstAttackCommand(static_cast<Field>(field));
						addToList = (act != NULL);
					}
					else if(commandType->getClass() == ccAttackStopped && field >= 0) {
						const AttackStoppedCommandType *asct = unit->getType()->getFirstAttackStoppedCommand(static_cast<Field>(field));
						addToList = (asct != NULL);
					}
					else {
						addToList = unit->getType()->hasCommandClass(commandType->getClass());
					}
				}
				else if(field >= 0) {
					addToList = (unit->getCurrField() == static_cast<Field>(field));
				}

				if(addToList == true) {
					units.push_back(unit->getId());
				}
			}
		}

		delete commandType;
		commandType = NULL;
	}
	return units;
}

void World::givePositionCommand(int unitId, const string &commandName, const Vec2i &pos) {
	Unit* unit= findUnitById(unitId);
	if(unit != NULL) {
		CommandClass cc;

		if(commandName=="move") {
			cc= ccMove;
		}
		else if(commandName=="attack") {
			cc= ccAttack;
		}
		else {
			throw megaglest_runtime_error("Invalid position command: " + commandName,true);
		}

		if(unit->getType()->getFirstCtOfClass(cc) == NULL) {
			throw megaglest_runtime_error("Invalid command: [" + commandName + "] for unit: [" + unit->getType()->getName(false) + "] id [" + intToStr(unit->getId()) + "]",true);
		}
		if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d] cc = %d Unit [%s]\n",__FILE__,__FUNCTION__,__LINE__,cc,unit->getFullName(false).c_str());
		unit->giveCommand(new Command( unit->getType()->getFirstCtOfClass(cc), pos ));
		if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	}
	else {
		throw megaglest_runtime_error("Invalid unitId index in givePositionCommand: " + intToStr(unitId) + " commandName = " + commandName,true);
	}
}

void World::giveStopCommand(int unitId) {
	Unit* unit= findUnitById(unitId);
	if(unit != NULL) {
		const CommandType *ct = unit->getType()->getFirstCtOfClass(ccStop);
		if(ct != NULL) {
			if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d] Unit [%s] will stop\n",__FILE__,__FUNCTION__,__LINE__,unit->getFullName(false).c_str());
			unit->giveCommand(new Command(ct));

			if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
		}
		else {
			throw megaglest_runtime_error("Invalid ct in giveStopCommand: " + intToStr(unitId),true);
		}
	}
	else {
		throw megaglest_runtime_error("Invalid unitId index in giveStopCommand: " + intToStr(unitId),true);
	}
}

bool World::selectUnit(int unitId) {
	bool result = false;
	Unit* unit= findUnitById(unitId);
	if(unit != NULL) {
		result = game->addUnitToSelection(unit);
	}

	return result;
}

void World::unselectUnit(int unitId) {
	Unit* unit= findUnitById(unitId);
	if(unit != NULL) {
		game->removeUnitFromSelection(unit);
	}
}

void World::addUnitToGroupSelection(int unitId,int groupIndex) {
	Unit* unit= findUnitById(unitId);
	if(unit != NULL) {
		game->addUnitToGroupSelection(unit,groupIndex);
	}
}

void World::removeUnitFromGroupSelection(int unitId,int groupIndex) {
	game->removeUnitFromGroupSelection(unitId,groupIndex);
}

void World::recallGroupSelection(int groupIndex) {
	game->recallGroupSelection(groupIndex);
}

void World::setAttackWarningsEnabled(bool enabled) {
	this->disableAttackEffects = !enabled;
}
bool World::getAttackWarningsEnabled() {
	return (this->disableAttackEffects == false);
}

void World::giveAttackCommand(int unitId, int unitToAttackId) {
	Unit* unit= findUnitById(unitId);
	if(unit != NULL) {
		Unit* targetUnit = findUnitById(unitToAttackId);
		if(targetUnit != NULL) {
			const CommandType *ct = unit->getType()->getFirstAttackCommand(targetUnit->getCurrField());
			if(ct != NULL) {
				if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d] Unit [%s] is attacking [%s]\n",__FILE__,__FUNCTION__,__LINE__,unit->getFullName(false).c_str(),targetUnit->getFullName(false).c_str());
				unit->giveCommand(new Command(ct,targetUnit));
				if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
			}
			else {
				throw megaglest_runtime_error("Invalid ct in giveAttackCommand: " + intToStr(unitId) + " unitToAttackId = " + intToStr(unitToAttackId),true);
			}
		}
		else {
			throw megaglest_runtime_error("Invalid unitToAttackId index in giveAttackCommand: " + intToStr(unitId) + " unitToAttackId = " + intToStr(unitToAttackId),true);
		}
	}
	else {
		throw megaglest_runtime_error("Invalid unitId index in giveAttackCommand: " + intToStr(unitId) + " unitToAttackId = " + intToStr(unitToAttackId),true);
	}
}

void World::giveProductionCommand(int unitId, const string &producedName) {
	//printf("File: %s line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__LINE__);
	Unit *unit= findUnitById(unitId);
	//printf("File: %s line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__LINE__);

	if(unit != NULL) {
		//printf("File: %s line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__LINE__);

		const UnitType *ut= unit->getType();

		//printf("File: %s line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__LINE__);

		//Search for a command that can produce the unit
		for(int i= 0; i< ut->getCommandTypeCount(); ++i) {
			const CommandType* ct= ut->getCommandType(i);
			if(ct != NULL && ct->getClass() == ccProduce) {
				const ProduceCommandType *pct= dynamic_cast<const ProduceCommandType*>(ct);
				if(pct != NULL && pct->getProducedUnit() != NULL &&
					pct->getProducedUnit()->getName(false) == producedName) {
					if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

					//printf("File: %s line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__LINE__);
					unit->giveCommand(new Command(pct));
					//printf("File: %s line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__LINE__);

					if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
					break;
				}
			}
		}
	}
	else {
		throw megaglest_runtime_error("Invalid unitId index in giveProductionCommand: " + intToStr(unitId) + " producedName = " + producedName,true);
	}
	//printf("File: %s line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__LINE__);
}

void World::giveAttackStoppedCommand(int unitId, const string &itemName, bool ignoreRequirements) {
	Unit *unit= findUnitById(unitId);
	if(unit != NULL) {
		const UnitType *ut= unit->getType();

		//Search for a command that can produce the unit
		for(int i= 0; i < ut->getCommandTypeCount(); ++i) {
			const CommandType* ct= ut->getCommandType(i);
			if(ct != NULL && ct->getClass() == ccAttackStopped) {

				const AttackStoppedCommandType *act= static_cast<const AttackStoppedCommandType*>(ct);
				if(act != NULL && act->getName(false) == itemName) {

					if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

					try {
						if(unit->getFaction()->reqsOk(act) == false && ignoreRequirements == true) {

							if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
							unit->setIgnoreCheckCommand(true);
						}

						unit->giveCommand(new Command(act));
					}
					catch(const exception &ex) {
						if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
						unit->setIgnoreCheckCommand(false);

						throw megaglest_runtime_error(ex.what());
					}


					if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
					break;
				}
			}
		}
	}
	else {
		throw megaglest_runtime_error("Invalid unitId index in giveAttackStoppedCommand: " + intToStr(unitId) + " itemName = " + itemName,true);
	}
}

void World::playStaticSound(const string &playSound) {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d] playSound [%s]\n",__FILE__,__FUNCTION__,__LINE__,playSound.c_str());

	string calculatedFilePath = playSound;
	bool changed = Properties::applyTagsToValue(calculatedFilePath);
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("\n\nPlay static sound changed: %d [%s] [%s]\n\n",changed, playSound.c_str(),calculatedFilePath.c_str());

	if(staticSoundList.find(calculatedFilePath) == staticSoundList.end()) {
		StaticSound *sound = new StaticSound();
		sound->load(calculatedFilePath);
		staticSoundList[calculatedFilePath] = sound;
	}
	StaticSound *playSoundItem = staticSoundList[calculatedFilePath];

	SoundRenderer &soundRenderer= SoundRenderer::getInstance();
	soundRenderer.playFx(playSoundItem);
}

void World::playStreamingSound(const string &playSound) {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d] playSound [%s]\n",__FILE__,__FUNCTION__,__LINE__,playSound.c_str());

	string calculatedFilePath = playSound;
	bool changed = Properties::applyTagsToValue(calculatedFilePath);
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("\n\nPlay streaming sound changed: %d [%s] [%s]\n\n",changed, playSound.c_str(),calculatedFilePath.c_str());

	if(streamSoundList.find(calculatedFilePath) == streamSoundList.end()) {
		StrSound *sound = new StrSound();
		sound->open(calculatedFilePath);
		sound->setNext(sound);
		streamSoundList[calculatedFilePath] = sound;
	}
	StrSound *playSoundItem = streamSoundList[calculatedFilePath];

	SoundRenderer &soundRenderer= SoundRenderer::getInstance();
	soundRenderer.playMusic(playSoundItem);
}

void World::stopStreamingSound(const string &playSound) {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d] playSound [%s]\n",__FILE__,__FUNCTION__,__LINE__,playSound.c_str());

	string calculatedFilePath = playSound;
	bool changed = Properties::applyTagsToValue(calculatedFilePath);
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("\n\nStop streaming sound changed: %d [%s] [%s]\n\n",changed, playSound.c_str(),calculatedFilePath.c_str());

	if(streamSoundList.find(calculatedFilePath) != streamSoundList.end()) {
		StrSound *playSoundItem = streamSoundList[calculatedFilePath];
		SoundRenderer &soundRenderer= SoundRenderer::getInstance();
		soundRenderer.stopMusic(playSoundItem);
	}
}

void World::stopAllSound() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugLUA).enabled) SystemFlags::OutputDebug(SystemFlags::debugLUA,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	SoundRenderer &soundRenderer= SoundRenderer::getInstance();
	soundRenderer.stopAllSounds();
}

void World::moveToUnit(int unitId, int destUnitId) {
	Unit* unit= findUnitById(unitId);
	if(unit != NULL) {
		CommandClass cc= ccMove;
		if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d] cc = %d Unit [%s]\n",__FILE__,__FUNCTION__,__LINE__,cc,unit->getFullName(false).c_str());
		Unit* destUnit = findUnitById(destUnitId);
		if(destUnit != NULL) {
			if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d] cc = %d Unit [%s] destUnit [%s]\n",__FILE__,__FUNCTION__,__LINE__,cc,unit->getFullName(false).c_str(),destUnit->getFullName(false).c_str());
			unit->giveCommand(new Command( unit->getType()->getFirstCtOfClass(cc), destUnit));
			if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
		}
	}
	else {
		throw megaglest_runtime_error("Invalid unitId index in followUnit: " + intToStr(unitId),true);
	}
}

void World::clearCaches() {
	ExploredCellsLookupItemCache.clear();
	ExploredCellsLookupItemCacheTimer.clear();
	ExploredCellsLookupItemCacheTimerCount = 0;

	unitUpdater.clearCaches();
}

void World::togglePauseGame(bool pauseStatus,bool forceAllowPauseStateChange) {
	game->setPaused(pauseStatus, forceAllowPauseStateChange, false, false);
	// ensures that the Fow is really up to date when the game switches to pause mode. ( mainly for scenarios )
	computeFow();
	minimap.updateFowTex(1.f);
}

void World::addConsoleText(const string &text) {
	game->getConsole()->addStdScenarioMessage(text);
}
void World::addConsoleTextWoLang(const string &text) {
	game->getConsole()->addLine(text);
}

const string World::getSystemMacroValue(const string key) {
	std::string result = key;
	bool tagApplied = Properties::applyTagsToValue(result);
	if(tagApplied == false) {
		result = "";

		if(key == "$SCENARIO_PATH") {
			result = extractDirectoryPathFromFile(game->getGameSettings()->getScenarioDir());
		}
	}

	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("system macro key [%s] returning [%s]\n",key.c_str(),result.c_str());

	return result;
}

const string World::getPlayerName(int factionIndex) {
	return game->getGameSettings()->getNetworkPlayerName(factionIndex);
}

void World::giveUpgradeCommand(int unitId, const string &upgradeName) {
	Unit *unit= findUnitById(unitId);
	if(unit != NULL) {
		const UnitType *ut= unit->getType();

		//Search for a command that can produce the unit
		for(int i= 0; i < ut->getCommandTypeCount(); ++i) {
			const CommandType* ct= ut->getCommandType(i);
			if(ct != NULL && ct->getClass() == ccUpgrade) {
				const UpgradeCommandType *uct= static_cast<const UpgradeCommandType*>(ct);
				if(uct != NULL && uct->getProducedUpgrade()->getName() == upgradeName) {
					if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

					unit->giveCommand(new Command(uct));

					if(SystemFlags::getSystemSettingType(SystemFlags::debugUnitCommands).enabled) SystemFlags::OutputDebug(SystemFlags::debugUnitCommands,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
					break;
				}
			}
		}
	}
	else {
		throw megaglest_runtime_error("Invalid unitId index in giveUpgradeCommand: " + intToStr(unitId) + " upgradeName = " + upgradeName,true);
	}
}


int World::getResourceAmount(const string &resourceName, int factionIndex) {
	if(factionIndex < (int)factions.size()) {
		Faction* faction= factions[factionIndex];
		const ResourceType* rt= techTree->getResourceType(resourceName);
		return faction->getResource(rt)->getAmount();
	}
	else {
		throw megaglest_runtime_error("Invalid faction index in giveResource: " + intToStr(factionIndex) + " resourceName = " + resourceName,true);
	}
}

Vec2i World::getStartLocation(int factionIndex) {
	if(factionIndex < (int)factions.size()) {
		Faction* faction= factions[factionIndex];
		return map.getStartLocation(faction->getStartLocationIndex());
	}
	else {
		printf("\n=================================================\n%s\n",game->getGameSettings()->toString().c_str());

		throw megaglest_runtime_error("Invalid faction index in getStartLocation: " + intToStr(factionIndex) + " : " + intToStr(factions.size()),true);
	}
}

Vec2i World::getUnitPosition(int unitId) {
	Unit* unit= findUnitById(unitId);
	if(unit == NULL) {
		throw megaglest_runtime_error("Can not find unit to get position unitId = " + intToStr(unitId),true);
	}
	return unit->getPos();
}

void World::setUnitPosition(int unitId, Vec2i pos) {
	Unit* unit= findUnitById(unitId);
	if(unit == NULL) {
		throw megaglest_runtime_error("Can not find unit to set position unitId = " + intToStr(unitId),true);
	}
	unit->setTargetPos(pos);
	this->moveUnitCells(unit, false);
}

void World::addCellMarker(Vec2i pos, int factionIndex, const string &note, const string textureFile) {
	const Faction *faction = NULL;
	if(factionIndex >= 0) {
		faction = this->getFaction(factionIndex);
	}

	Vec2i surfaceCellPos = map.toSurfCoords(pos);
	SurfaceCell *sc = map.getSurfaceCell(surfaceCellPos);
	if(sc == NULL) {
		throw megaglest_runtime_error("sc == NULL",true);
	}
	Vec3f vertex = sc->getVertex();
	Vec2i targetPos(vertex.x,vertex.z);

	//printf("pos [%s] scPos [%s][%p] targetPos [%s]\n",pos.getString().c_str(),surfaceCellPos.getString().c_str(),sc,targetPos.getString().c_str());

	MarkedCell mc(targetPos,faction,note,(faction != NULL ? faction->getStartLocationIndex() : -1));
	game->addCellMarker(surfaceCellPos, mc);
}

void World::removeCellMarker(Vec2i pos, int factionIndex) {
	const Faction *faction = NULL;
	if(factionIndex >= 0) {
		faction = this->getFaction(factionIndex);
	}

	Vec2i surfaceCellPos = map.toSurfCoords(pos);

	game->removeCellMarker(surfaceCellPos, faction);
}

void World::showMarker(Vec2i pos, int factionIndex, const string &note, const string textureFile, int flashCount) {
	const Faction *faction = NULL;
	if(factionIndex >= 0) {
		faction = this->getFaction(factionIndex);
	}

	Vec2i surfaceCellPos = map.toSurfCoords(pos);
	SurfaceCell *sc = map.getSurfaceCell(surfaceCellPos);
	if(sc == NULL) {
		throw megaglest_runtime_error("sc == NULL",true);
	}
	Vec3f vertex = sc->getVertex();
	Vec2i targetPos(vertex.x,vertex.z);

	//printf("pos [%s] scPos [%s][%p] targetPos [%s]\n",pos.getString().c_str(),surfaceCellPos.getString().c_str(),sc,targetPos.getString().c_str());

	MarkedCell mc(targetPos,faction,note,(faction != NULL ? faction->getStartLocationIndex() : -1));
	if(flashCount > 0) {
		mc.setAliveCount(flashCount);
	}
	game->showMarker(surfaceCellPos, mc);
}

void World::highlightUnit(int unitId,float radius, float thickness, Vec4f color) {
	Unit* unit= findUnitById(unitId);
	if(unit == NULL) {
		throw megaglest_runtime_error("Can not find unit to set highlight unitId = " + intToStr(unitId),true);
	}
	game->highlightUnit(unitId,radius, thickness, color);
}

void World::unhighlightUnit(int unitId) {
	Unit* unit= findUnitById(unitId);
	if(unit == NULL) {
		throw megaglest_runtime_error("Can not find unit to set highlight unitId = " + intToStr(unitId),true);
	}
	game->unhighlightUnit(unitId);
}


int World::getUnitFactionIndex(int unitId) {
	Unit* unit= findUnitById(unitId);
	if(unit == NULL) {
		throw megaglest_runtime_error("Can not find Faction unit to get position unitId = " + intToStr(unitId),true);
	}
	return unit->getFactionIndex();
}
const string World::getUnitName(int unitId) {
	Unit* unit= findUnitById(unitId);
	if(unit == NULL) {
		throw megaglest_runtime_error("Can not find Faction unit to get position unitId = " + intToStr(unitId),true);
	}
	return unit->getFullName(game->showTranslatedTechTree());
}

int World::getUnitCount(int factionIndex) {
	if(factionIndex < (int)factions.size()) {
		Faction* faction= factions[factionIndex];
		int count= 0;

		for(int i= 0; i < faction->getUnitCount(); ++i) {
			const Unit* unit= faction->getUnit(i);
			if(unit != NULL && unit->isAlive()) {
				++count;
			}
		}
		return count;
	}
	else {
		throw megaglest_runtime_error("Invalid faction index in getUnitCount: " + intToStr(factionIndex),true);
	}
}

int World::getUnitCountOfType(int factionIndex, const string &typeName) {
	if(factionIndex < (int)factions.size()) {
		Faction* faction= factions[factionIndex];
		int count= 0;

		for(int i= 0; i< faction->getUnitCount(); ++i) {
			const Unit* unit= faction->getUnit(i);
			if(unit != NULL && unit->isAlive() && unit->getType()->getName(false) == typeName) {
				++count;
			}
		}
		return count;
	}
	else {
		throw megaglest_runtime_error("Invalid faction index in getUnitCountOfType: " + intToStr(factionIndex),true);
	}
}

// ==================== PRIVATE ====================

// ==================== private init ====================

//init basic cell state
void World::initCells(bool fogOfWar) {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	Logger::getInstance().add(Lang::getInstance().getString("LogScreenGameLoadingStateCells","",true), true);
    for(int i=0; i< map.getSurfaceW(); ++i) {
        for(int j=0; j< map.getSurfaceH(); ++j) {

			SurfaceCell *sc= map.getSurfaceCell(i, j);
			if(sc == NULL) {
				throw megaglest_runtime_error("sc == NULL");
			}
			if(sc->getObject()!=NULL){
        		sc->getObject()->initParticles();
			}
			sc->setFowTexCoord(Vec2f(
				i/(next2Power(map.getSurfaceW())-1.f),
				j/(next2Power(map.getSurfaceH())-1.f)));

			for (int k = 0; k < GameConstants::maxPlayers; k++) {
				sc->setExplored(k, (game->getGameSettings()->getFlagTypes1() & ft1_show_map_resources) == ft1_show_map_resources);
				sc->setVisible(k, !fogOfWar);
			}

			for (int k = GameConstants::maxPlayers; k < GameConstants::maxPlayers + GameConstants::specialFactions; k++) {
				sc->setExplored(k, true);
				sc->setVisible(k, true);
			}

			if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled == true) {
				char szBuf[8096]="";
				snprintf(szBuf,8096,"In initCells() x = %d y = %d %s %s",i,j,sc->isVisibleString().c_str(),sc->isExploredString().c_str());
				if(Thread::isCurrentThreadMainThread()) {
					//unit->logSynchDataThreaded(__FILE__,__LINE__,szBuf);
					SystemFlags::OutputDebug(SystemFlags::debugWorldSynch,szBuf);
				}
			}
		}
    }
    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

//init surface textures
void World::initSplattedTextures() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	for(int i = 0; i < map.getSurfaceW() - 1; ++i) {
        for(int j = 0; j < map.getSurfaceH() - 1; ++j) {
			Vec2f coord;
			const Texture2D *texture=NULL;
			SurfaceCell *sc00= map.getSurfaceCell(i, j);
			SurfaceCell *sc10= map.getSurfaceCell(i+1, j);
			SurfaceCell *sc01= map.getSurfaceCell(i, j+1);
			SurfaceCell *sc11= map.getSurfaceCell(i+1, j+1);

			if(sc00 == NULL) {
				throw megaglest_runtime_error("sc00 == NULL");
			}
			if(sc10 == NULL) {
				throw megaglest_runtime_error("sc10 == NULL");
			}
			if(sc01 == NULL) {
				throw megaglest_runtime_error("sc01 == NULL");
			}
			if(sc11 == NULL) {
				throw megaglest_runtime_error("sc11 == NULL");
			}

			tileset.addSurfTex( sc00->getSurfaceType(),
								sc10->getSurfaceType(),
								sc01->getSurfaceType(),
								sc11->getSurfaceType(),
								coord, texture,j,i);
			sc00->setSurfTexCoord(coord);
			sc00->setSurfaceTexture(texture);
		}
	}
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

//creates each faction looking at each faction name contained in GameSettings
void World::initFactionTypes(GameSettings *gs) {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	Logger::getInstance().add(Lang::getInstance().getString("LogScreenGameLoadingFactionTypes","",true), true);

	if(gs == NULL) {
		throw megaglest_runtime_error("gs == NULL");
	}

	if(gs->getFactionCount() > map.getMaxPlayers()) {
		throw megaglest_runtime_error("This map only supports "+intToStr(map.getMaxPlayers())+" players, factionCount is " + intToStr(gs->getFactionCount()));
	}

	//create stats
	//printf("World gs->getThisFactionIndex() = %d\n",gs->getThisFactionIndex());

	if(this->game->isMasterserverMode() == true) {
		this->thisFactionIndex = -1;
	}
	else {
		this->thisFactionIndex= gs->getThisFactionIndex();
	}

	stats.init(gs->getFactionCount(), this->thisFactionIndex, gs->getDescription(),gs->getTech());

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	//create factions
	//printf("this->thisFactionIndex = %d\n",this->thisFactionIndex);

	//factions.resize(gs->getFactionCount());
	for(int i= 0; i < gs->getFactionCount(); ++i){
		Faction *newFaction = new Faction();
		factions.push_back(newFaction);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] factions.size() = %d\n",__FILE__,__FUNCTION__,__LINE__,factions.size());

	for(int i=0; i < (int)factions.size(); ++i) {
		FactionType *ft= techTree->getTypeByName(gs->getFactionTypeName(i));
		if(ft == NULL) {
			throw megaglest_runtime_error("ft == NULL");
		}
		factions[i]->init(ft, gs->getFactionControl(i), techTree, game, i, gs->getTeam(i),
						 gs->getStartLocationIndex(i), i==thisFactionIndex,
						 gs->getDefaultResources(),loadWorldNode);

		stats.setTeam(i, gs->getTeam(i));
		stats.setFactionTypeName(i, formatString(gs->getFactionTypeName(i)));
		stats.setPersonalityType(i, getFaction(i)->getType()->getPersonalityType());
		stats.setControl(i, gs->getFactionControl(i));
		stats.setResourceMultiplier(i,(gs->getResourceMultiplierIndex(i)+5)*0.1f);
		stats.setPlayerName(i,gs->getNetworkPlayerName(i));
		if(getFaction(i)->getTexture()) {
			stats.setPlayerColor(i,getFaction(i)->getTexture()->getPixmapConst()->getPixel3f(0, 0));
		}
	}

	if(Config::getInstance().getBool("EnableNewThreadManager","false") == true) {
		std::vector<SlaveThreadControllerInterface *> slaveThreadList;
		for(unsigned int i = 0; i < factions.size(); ++i) {
			Faction *faction = factions[i];
			slaveThreadList.push_back(faction->getWorkerThread());
		}
		masterController.setSlaves(slaveThreadList);
	}

	if(loadWorldNode != NULL) {
		stats.loadGame(loadWorldNode);
		random.setLastNumber(loadWorldNode->getAttribute("random")->getIntValue());

		thisFactionIndex = loadWorldNode->getAttribute("thisFactionIndex")->getIntValue();
	//	int thisTeamIndex;
		thisTeamIndex = loadWorldNode->getAttribute("thisTeamIndex")->getIntValue();
	//	int frameCount;
		frameCount = loadWorldNode->getAttribute("frameCount")->getIntValue();

		//printf("**LOAD World thisFactionIndex = %d\n",thisFactionIndex);

		MutexSafeWrapper safeMutex(mutexFactionNextUnitId,string(__FILE__) + "_" + intToStr(__LINE__));
	//	std::map<int,int> mapFactionNextUnitId;
//		for(std::map<int,int>::iterator iterMap = mapFactionNextUnitId.begin();
//				iterMap != mapFactionNextUnitId.end(); ++iterMap) {
//			XmlNode *factionNextUnitIdNode = worldNode->addChild("FactionNextUnitId");
//
//			factionNextUnitIdNode->addAttribute("key",intToStr(iterMap->first), mapTagReplacements);
//			factionNextUnitIdNode->addAttribute("value",intToStr(iterMap->second), mapTagReplacements);
//		}
		//!!!
		vector<XmlNode *> factionNextUnitIdNodeList = loadWorldNode->getChildList("FactionNextUnitId");
		for(unsigned int i = 0; i < factionNextUnitIdNodeList.size(); ++i) {
			XmlNode *factionNextUnitIdNode = factionNextUnitIdNodeList[i];

			int key = factionNextUnitIdNode->getAttribute("key")->getIntValue();
			int value = factionNextUnitIdNode->getAttribute("value")->getIntValue();

			mapFactionNextUnitId[key] = value;
		}
		safeMutex.ReleaseLock();
	//	//config
	//	bool fogOfWarOverride;
		fogOfWarOverride = loadWorldNode->getAttribute("fogOfWarOverride")->getIntValue() != 0;
	//	bool fogOfWar;
		fogOfWar = loadWorldNode->getAttribute("fogOfWar")->getIntValue() != 0;
	//	int fogOfWarSmoothingFrameSkip;
		fogOfWarSmoothingFrameSkip = loadWorldNode->getAttribute("fogOfWarSmoothingFrameSkip")->getIntValue();
	//	bool fogOfWarSmoothing;
		fogOfWarSmoothing = loadWorldNode->getAttribute("fogOfWarSmoothing")->getIntValue() != 0;
	//	Game *game;
	//	Chrono chronoPerfTimer;
	//	bool perfTimerEnabled;
	//
	//	bool unitParticlesEnabled;
		unitParticlesEnabled = loadWorldNode->getAttribute("unitParticlesEnabled")->getIntValue() != 0;
	//	std::map<string,StaticSound *> staticSoundList;
	//	std::map<string,StrSound *> streamSoundList;
	//
	//	uint32 nextCommandGroupId;
		nextCommandGroupId = loadWorldNode->getAttribute("nextCommandGroupId")->getIntValue();
	//	string queuedScenarioName;
		queuedScenarioName = loadWorldNode->getAttribute("queuedScenarioName")->getValue();
	//	bool queuedScenarioKeepFactions;
		queuedScenarioKeepFactions = loadWorldNode->getAttribute("queuedScenarioKeepFactions")->getIntValue() != 0;

		if(loadWorldNode->hasAttribute("disableAttackEffects") == true) {
			disableAttackEffects = loadWorldNode->getAttribute("disableAttackEffects")->getIntValue() != 0;
		}
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	if(factions.empty() == false) {
		if(this->game->isMasterserverMode() == true) {
			thisTeamIndex = -1;
		}
		else {
			thisTeamIndex = getFaction(thisFactionIndex)->getTeam();
		}
	}
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

void World::initMinimap() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);

	minimap.init(map.getW(), map.getH(), this, game->getGameSettings()->getFogOfWar());
	Logger::getInstance().add(Lang::getInstance().getString("LogScreenGameLoadingMinimapSurface","",true), true);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

void World::initUnitsForScenario() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	Logger::getInstance().add(Lang::getInstance().getString("LogScreenGameLoadingGenerateGameElements","",true), true);

	//put starting units
	for(int i = 0; i < getFactionCount(); ++i) {
		Faction *f= factions[i];
		const FactionType *ft= f->getType();

		for(int j = 0; j < f->getUnitCount(); ++j) {
			Unit *unit = f->getUnit(j);

			int startLocationIndex= f->getStartLocationIndex();

			if(placeUnit(map.getStartLocation(startLocationIndex), generationArea, unit, true)) {
				map.putUnitCells(unit, unit->getPos());
				unit->setCurrSkill(scStop);
				//unit->create(true);
				//unit->born();
			}
			else {
				string unitName = unit->getType()->getName(false);
				delete unit;
				unit = NULL;
				throw megaglest_runtime_error("Unit: " + unitName + " can't be placed, this error is caused because there\nis not enough room to put all units near their start location.\nmake a better/larger map. Faction: #" + intToStr(i) + " name: " + ft->getName(false));
			}

			if (unit->getType()->hasSkillClass(scBeBuilt)) {
				map.flatternTerrain(unit);
			}
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] unit created for unit [%s]\n",__FILE__,__FUNCTION__,__LINE__,unit->toString().c_str());
		}

		// Ensure Starting Resource Amount are adjusted to max store levels
		//f->limitResourcesToStore();
	}
	map.computeNormals();
	map.computeInterpolatedHeights();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

void World::placeUnitAtLocation(const Vec2i &location, int radius, Unit *unit, bool spaciated) {
	if(placeUnit(location, generationArea, unit, spaciated)) {
		unit->create(true);
		unit->born(NULL);
	}
	else {
		string unitName = unit->getType()->getName(false);
		string unitFactionName = unit->getFaction()->getType()->getName(false);
		int unitFactionIndex = unit->getFactionIndex();

		delete unit;
		unit = NULL;

		char szBuf[8096]="";
		snprintf(szBuf,8096,"Unit: [%s] can't be placed, this error is caused because there\nis not enough room to put all units near their start location.\nmake a better/larger map. Faction: #%d name: [%s]",
				unitName.c_str(),unitFactionIndex,unitFactionName.c_str());
		throw megaglest_runtime_error(szBuf,false);
	}
	if (unit->getType()->hasSkillClass(scBeBuilt)) {
		map.flatternTerrain(unit);
	}
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] unit created for unit [%s]\n",__FILE__,__FUNCTION__,__LINE__,unit->toString().c_str());

}

//place units randomly aroud start location
void World::initUnits() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	Logger::getInstance().add(Lang::getInstance().getString("LogScreenGameLoadingGenerateGameElements","",true), true);

	bool gotError = false;
	bool skipStackTrace = false;
	string sErrBuf="";
	try {
		//put starting units
		if(loadWorldNode == NULL) {
			for(int i = 0; i < getFactionCount(); ++i) {
				Faction *f= factions[i];
				const FactionType *ft= f->getType();
				for(int j = 0; j < ft->getStartingUnitCount(); ++j) {
					const UnitType *ut= ft->getStartingUnit(j);
					int initNumber= ft->getStartingUnitAmount(j);

					for(int l = 0; l < initNumber; l++) {

						UnitPathInterface *newpath = NULL;
						switch(game->getGameSettings()->getPathFinderType()) {
							case pfBasic:
								newpath = new UnitPathBasic();
								break;
							default:
								throw megaglest_runtime_error("detected unsupported pathfinder type!");
						}

						Unit *unit= new Unit(getNextUnitId(f), newpath, Vec2i(0), ut, f, &map, CardinalDir(CardinalDir::NORTH));
						int startLocationIndex= f->getStartLocationIndex();
						placeUnitAtLocation(map.getStartLocation(startLocationIndex), generationArea, unit, true);
					}
				}
			}
			// the following is done here in an extra loop and not in the loop above, because shared resources games
			// need to load all factions first, before calculating limitResourcesToStore()
			for(int i = 0; i < getFactionCount(); ++i) {
				Faction *f= factions[i];
				// Ensure Starting Resource Amount are adjusted to max store levels
				f->limitResourcesToStore();
			}
		}
		else {
			//printf("Load game setting unit pos\n");
			refreshAllUnitExplorations();
		}
	}
	catch(const megaglest_runtime_error &ex) {
		gotError = true;
		if(ex.wantStackTrace() == true) {
			char szErrBuf[8096]="";
			snprintf(szErrBuf,8096,"In [%s::%s %d]",__FILE__,__FUNCTION__,__LINE__);
			sErrBuf = string(szErrBuf) + string("\nerror [") + string(ex.what()) + string("]\n");
		}
		else {
			skipStackTrace = true;
			sErrBuf = ex.what();
		}
		SystemFlags::OutputDebug(SystemFlags::debugError,sErrBuf.c_str());
	}
	catch(const std::exception &ex) {
		gotError = true;
		char szErrBuf[8096]="";
		snprintf(szErrBuf,8096,"In [%s::%s %d]",__FILE__,__FUNCTION__,__LINE__);
		sErrBuf = string(szErrBuf) + string("\nerror [") + string(ex.what()) + string("]\n");
		SystemFlags::OutputDebug(SystemFlags::debugError,sErrBuf.c_str());
	}

	map.computeNormals();
	map.computeInterpolatedHeights();

	if(gotError == true) {
		throw megaglest_runtime_error(sErrBuf,!skipStackTrace);
	}
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

void World::refreshAllUnitExplorations() {
	for(unsigned int i = 0; i < (unsigned int)getFactionCount(); ++i) {
		Faction *faction = factions[i];

		for(unsigned int j = 0; j < (unsigned int)faction->getUnitCount(); ++j) {
			Unit *unit = faction->getUnit(j);
			unit->refreshPos();
		}
	}
}

void World::initMap() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
	map.init(&tileset);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",__FILE__,__FUNCTION__,__LINE__);
}

void World::exploreCells(int teamIndex, ExploredCellsLookupItem &exploredCellsCache) {
	std::vector<SurfaceCell*> &cellList = exploredCellsCache.exploredCellList;
	for (int idx2 = 0; idx2 < (int)cellList.size(); ++idx2) {
		SurfaceCell* sc = cellList[idx2];
		sc->setExplored(teamIndex, true);
	}
	cellList = exploredCellsCache.visibleCellList;
	for (int idx2 = 0; idx2 < (int)cellList.size(); ++idx2) {
		SurfaceCell* sc = cellList[idx2];
		sc->setVisible(teamIndex, true);
	}
}

// ==================== exploration ====================

ExploredCellsLookupItem World::exploreCells(const Vec2i &newPos, int sightRange, int teamIndex, Unit *unit) {
	// cache lookup of previously calculated cells + sight range
	if(MaxExploredCellsLookupItemCache > 0) {
		if(difftime(time(NULL),ExploredCellsLookupItem::lastDebug) >= 10) {
			ExploredCellsLookupItem::lastDebug = time(NULL);
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] ExploredCellsLookupItemCache.size() = %d\n",__FILE__,__FUNCTION__,__LINE__,ExploredCellsLookupItemCache.size());
			if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] ExploredCellsLookupItemCache.size() = %d\n",__FILE__,__FUNCTION__,__LINE__,ExploredCellsLookupItemCache.size());
		}

		// Ok we limit the cache size due to possible RAM constraints when
		// the threshold is met
		bool MaxExploredCellsLookupItemCacheTriggered = false;
		if((int)ExploredCellsLookupItemCache.size() >= MaxExploredCellsLookupItemCache) {
			MaxExploredCellsLookupItemCacheTriggered = true;
			// Snag the oldest entry in the list
			std::map<int,ExploredCellsLookupKey>::reverse_iterator purgeItem = ExploredCellsLookupItemCacheTimer.rbegin();

			ExploredCellsLookupItemCache[purgeItem->second.pos].erase(purgeItem->second.sightRange);

			if(ExploredCellsLookupItemCache[purgeItem->second.pos].empty() == true) {
				ExploredCellsLookupItemCache.erase(purgeItem->second.pos);
			}

			ExploredCellsLookupItemCacheTimer.erase(purgeItem->first);
		}

		// Check the cache for the pos, sightrange and use from
		// cache if already found
		std::map<Vec2i, std::map<int, ExploredCellsLookupItem> >::iterator iterFind = ExploredCellsLookupItemCache.find(newPos);
		if(iterFind != ExploredCellsLookupItemCache.end()) {

			std::map<int, ExploredCellsLookupItem>::iterator iterFind2 = iterFind->second.find(sightRange);
			if(iterFind2 != iterFind->second.end()) {

				ExploredCellsLookupItem &exploredCellsCache = iterFind2->second;
				exploreCells(teamIndex, exploredCellsCache);

				// Only start worrying about updating the cache timer if we
				// have hit the threshold
				if(MaxExploredCellsLookupItemCacheTriggered == true) {
					// Remove the old timer entry since the search key id is stale
					ExploredCellsLookupItemCacheTimer.erase(exploredCellsCache.ExploredCellsLookupItemCacheTimerCountIndex);
					exploredCellsCache.ExploredCellsLookupItemCacheTimerCountIndex = ExploredCellsLookupItemCacheTimerCount++;

					ExploredCellsLookupKey lookupKey;
					lookupKey.pos = newPos;
					lookupKey.sightRange = sightRange;
					lookupKey.teamIndex = teamIndex;

					// Add a new search key since we just used this exploredCellsCache
					ExploredCellsLookupItemCacheTimer[exploredCellsCache.ExploredCellsLookupItemCacheTimerCountIndex] = lookupKey;
				}

				return exploredCellsCache;
			}
		}
	}

	Vec2i newSurfPos= Map::toSurfCoords(newPos);
	int surfSightRange= sightRange / Map::cellScale+1;

	// Explore, this code is quite expensive when we have lots of units
	ExploredCellsLookupItem exploredCellsCache;
	exploredCellsCache.exploredCellList.reserve(surfSightRange + indirectSightRange * 4);
	exploredCellsCache.visibleCellList.reserve(surfSightRange + indirectSightRange * 4);

	//int loopCount = 0;
    for(int i = -surfSightRange - indirectSightRange -1; i <= surfSightRange + indirectSightRange +1; ++i) {
        for(int j = -surfSightRange - indirectSightRange -1; j <= surfSightRange + indirectSightRange +1; ++j) {
        	//loopCount++;
        	Vec2i currRelPos= Vec2i(i, j);
            Vec2i currPos= newSurfPos + currRelPos;
            if(map.isInsideSurface(currPos) == true){
				SurfaceCell *sc= map.getSurfaceCell(currPos);
				if(sc == NULL) {
					throw megaglest_runtime_error("sc == NULL");
				}

				//explore
				float posLength = currRelPos.length();
				bool updateExplored = (posLength < surfSightRange + indirectSightRange + 1);
				bool updateVisible = (posLength < surfSightRange);

				if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled == true &&
						SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynchMax).enabled == true) {
					char szBuf[8096]="";
					snprintf(szBuf,8096,"In exploreCells() currRelPos = %s currPos = %s posLength = %f updateExplored = %d updateVisible = %d sightRange = %d teamIndex = %d",
							currRelPos.getString().c_str(), currPos.getString().c_str(),posLength,updateExplored, updateVisible, sightRange, teamIndex);
					if(Thread::isCurrentThreadMainThread() == false) {
						unit->logSynchDataThreaded(__FILE__,__LINE__,szBuf);
					}
					else {
						unit->logSynchData(__FILE__,__LINE__,szBuf);
					}
				}

				if(updateExplored) {
                    sc->setExplored(teamIndex, true);
                    exploredCellsCache.exploredCellList.push_back(sc);
				}
				//visible
				if(updateVisible) {
					sc->setVisible(teamIndex, true);
					exploredCellsCache.visibleCellList.push_back(sc);
				}
            }
        }
    }

    // Ok update our caches with the latest info for this position, sight and team
    if(MaxExploredCellsLookupItemCache > 0) {
		if(exploredCellsCache.exploredCellList.empty() == false || exploredCellsCache.visibleCellList.empty() == false) {
			exploredCellsCache.ExploredCellsLookupItemCacheTimerCountIndex = ExploredCellsLookupItemCacheTimerCount++;
			ExploredCellsLookupItemCache[newPos][sightRange] = exploredCellsCache;

			ExploredCellsLookupKey lookupKey;
			lookupKey.pos 			= newPos;
			lookupKey.sightRange 	= sightRange;
			lookupKey.teamIndex 	= teamIndex;
			ExploredCellsLookupItemCacheTimer[exploredCellsCache.ExploredCellsLookupItemCacheTimerCountIndex] = lookupKey;
		}
    }
    return exploredCellsCache;
}

bool World::showWorldForPlayer(int factionIndex, bool excludeFogOfWarCheck) const {
    bool ret = false;
    if(factionIndex >= 0) {
		if(excludeFogOfWarCheck == false &&
			fogOfWarSkillTypeValue == 0 && fogOfWarOverride == true) {
			ret = true;
		}
		else if(factionIndex == thisFactionIndex && game != NULL) {

			//printf("Show FOW thisTeamIndex = %d (%d) game->getGameOver() = %d game->getGameSettings()->isNetworkGame() = %d game->getGameSettings()->getEnableObserverModeAtEndGame() = %d thisFactionIndex = %d Is Winning Faction = %d\n",
			//		thisTeamIndex,(GameConstants::maxPlayers -1 + fpt_Observer),game->getGameOver(),
			//		game->getGameSettings()->isNetworkGame(),game->getGameSettings()->getEnableObserverModeAtEndGame(),
			//		thisFactionIndex,getStats()->getVictory(thisFactionIndex));

			// Player is an Observer
			if(thisTeamIndex == GameConstants::maxPlayers -1 + fpt_Observer) {
				ret = true;
			}
			// Game over and not a network game
			else if(game->getGameOver() == true &&
					game->getGameSettings()->isNetworkGame() == false) {
				ret = true;
			}
			// Game is over but playing a Network game so check if we can
			// turn off fog of war?
			else if(game->getGameOver() == true &&
					game->getGameSettings()->isNetworkGame() == true &&
					game->getGameSettings()->getEnableObserverModeAtEndGame() == true) {
				ret = true;

				// If the faction is NOT on the winning team, don't let them see the map
				// until all mobile units are dead
				if(getStats()->getVictory(factionIndex) == false) {
					// If the player has at least 1 Unit alive that is mobile (can move)
					// then we cannot turn off fog of war

					const Faction *faction = getFaction(factionIndex);
	//                for(int i = 0; i < faction->getUnitCount(); ++i) {
	//                    Unit *unit = getFaction(factionIndex)->getUnit(i);
	//                    if(unit != NULL && unit->isAlive() && unit->getType()->isMobile() == true) {
	//                        ret = false;
	//                        break;
	//                    }
	//                }
					if(faction->hasAliveUnits(true,false) == true) {
						ret = false;
					}
				}
			}
		}
    }
    //printf("showWorldForPlayer for %d is: %d\n",factionIndex,ret);
    return ret;
}

//computes the fog of war texture, contained in the minimap
void World::computeFow() {
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s] Line: %d in frame: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,getFrameCount());

	Chrono chronoGamePerformanceCounts;
	if(this->game) chronoGamePerformanceCounts.start();

	minimap.resetFowTex();

	if(this->game) this->game->addPerformanceCount("world minimap.resetFowTex",chronoGamePerformanceCounts.getMillis());

	// reset cells
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s] Line: %d in frame: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,getFrameCount());

	if(this->game) chronoGamePerformanceCounts.start();

	// Once we have calculated fog of war texture alpha, they are cached so we
	// restore the default texture in one shot for speed
	if(fogOfWar && cacheFowAlphaTexture == true) {
		if(cacheFowAlphaTextureFogOfWarValue != fogOfWar) {
			cacheFowAlphaTexture = false;
		}
		else {
			minimap.restoreFowTexAlphaSurface();
		}
	}
	int resetFowAlphaFactionCount = 0;

	for(int factionIndex = 0; factionIndex < GameConstants::maxPlayers + GameConstants::specialFactions; ++factionIndex) {
		if(factionIndex >= getFactionCount()) {
			continue;
		}
		Faction *faction = getFaction(factionIndex);
//	for(int indexTeamFaction = 0;
//			indexTeamFaction < GameConstants::maxPlayers + GameConstants::specialFactions;
//			++indexTeamFaction) {

		// If fog of war enabled set cell visible to false and later set those close to units to true
		if(fogOfWar) {
			for(int indexSurfaceW = 0; indexSurfaceW < map.getSurfaceW(); ++indexSurfaceW) {
				for(int indexSurfaceH = 0; indexSurfaceH < map.getSurfaceH(); ++indexSurfaceH) {
					// set all cells to not visible
					map.getSurfaceCell(indexSurfaceW, indexSurfaceH)->setVisible(faction->getTeam(), false);
				}
			}
		}

		// Remove fog of war for factions NOT on my team which i can see
		if(!fogOfWar || (faction->getTeam() != thisTeamIndex)) {
			bool showWorldForFaction = showWorldForPlayer(factionIndex);
			//printf("showWorldForFaction indexFaction = %d thisTeamIndex = %d showWorldForFaction = %d\n",indexFaction,thisTeamIndex,showWorldForFaction);
			if(showWorldForFaction == true) {
				resetFowAlphaFactionCount++;
			}
			for(int indexSurfaceW = 0; indexSurfaceW < map.getSurfaceW(); ++indexSurfaceW) {
				for(int indexSurfaceH = 0; indexSurfaceH < map.getSurfaceH(); ++indexSurfaceH) {
					// reset fog of ware texture alpha values
					if(!fogOfWar || (cacheFowAlphaTexture == false &&
						showWorldForFaction == true &&
							resetFowAlphaFactionCount <= 1)) {
						const Vec2i surfPos(indexSurfaceW,indexSurfaceH);

						//compute max alpha
						float maxAlpha= 0.0f;
						if(surfPos.x > 1 && surfPos.y > 1 &&
						   surfPos.x < map.getSurfaceW() - 2 &&
						   surfPos.y < map.getSurfaceH() - 2) {
							maxAlpha= 1.f;
						}
						else if(surfPos.x > 0 && surfPos.y > 0 &&
								surfPos.x < map.getSurfaceW() - 1 &&
								surfPos.y < map.getSurfaceH() - 1){
							maxAlpha= 0.3f;
						}

						// compute alpha
						float alpha = maxAlpha;
						minimap.incFowTextureAlphaSurface(surfPos, alpha);
					}
				}
			}
		}
		// Remove fog of war for factions on my team
		else if(fogOfWar && (faction->getTeam() == thisTeamIndex)) {
			bool showWorldForFaction = showWorldForPlayer(factionIndex);
			//printf("#2 showWorldForFaction thisFactionIndex = %d thisTeamIndex = %d showWorldForFaction = %d\n",thisFactionIndex,thisTeamIndex,showWorldForFaction);
			if(showWorldForFaction == true) {
				for(int indexSurfaceW = 0; indexSurfaceW < map.getSurfaceW(); ++indexSurfaceW) {
					for(int indexSurfaceH = 0; indexSurfaceH < map.getSurfaceH(); ++indexSurfaceH) {
						// reset fog of ware texture alpha values
						if(!fogOfWar || (cacheFowAlphaTexture == false &&
							showWorldForFaction == true)) {
							const Vec2i surfPos(indexSurfaceW,indexSurfaceH);

							//compute max alpha
							float maxAlpha= 0.0f;
							if(surfPos.x > 1 && surfPos.y > 1 &&
							   surfPos.x < map.getSurfaceW() - 2 &&
							   surfPos.y < map.getSurfaceH() - 2) {
								maxAlpha= 1.f;
							}
							else if(surfPos.x > 0 && surfPos.y > 0 &&
									surfPos.x < map.getSurfaceW() - 1 &&
									surfPos.y < map.getSurfaceH() - 1){
								maxAlpha= 0.3f;
							}

							// compute alpha
							float alpha = maxAlpha;
							minimap.incFowTextureAlphaSurface(surfPos, alpha);
						}
					}
				}
			}
		}

		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s] Line: %d in frame: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,getFrameCount());
	}

	// Once we have calculated fog of war texture alpha, will we cache it so that we
	// can restore it later
	if(fogOfWar && cacheFowAlphaTexture == false && resetFowAlphaFactionCount > 0) {
		cacheFowAlphaTexture = true;
		cacheFowAlphaTextureFogOfWarValue = fogOfWar;
		minimap.copyFowTexAlphaSurface();
	}

	if(this->game) this->game->addPerformanceCount("world reset cells",chronoGamePerformanceCounts.getMillis());

	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s] Line: %d in frame: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,getFrameCount());

	//compute cells
	if(this->game) chronoGamePerformanceCounts.start();

	for(int factionIndex = 0; factionIndex < getFactionCount(); ++factionIndex) {
		Faction *faction = getFaction(factionIndex);
		bool cellVisibleForFaction = showWorldForPlayer(thisFactionIndex);
		//printf("computeFow thisFactionIndex = %d factionIndex = %d thisTeamIndex = %d faction->getTeam() = %d cellVisibleForFaction = %d\n",thisFactionIndex,factionIndex,thisTeamIndex,faction->getTeam(),cellVisibleForFaction);

		int unitCount = faction->getUnitCount();
		for(int unitIndex = 0; unitIndex < unitCount; ++unitIndex) {
			Unit *unit= faction->getUnit(unitIndex);
			// exploration
			unit->exploreCells();

			// fire particle visible
			ParticleSystem *fire = unit->getFire();
			if(fire != NULL) {
				bool cellVisible = cellVisibleForFaction;
				if(cellVisible == false) {
					Vec2i sCoords = Map::toSurfCoords(unit->getPos());
					SurfaceCell *sc = map.getSurfaceCell(sCoords);
					if(sc != NULL) {
						cellVisible = sc->isVisible(thisTeamIndex);
					}
				}

				fire->setActive(cellVisible);
			}

			// compute fog of war render texture
			if(fogOfWar == true &&
				faction->getTeam() == thisTeamIndex &&
					unit->isOperative() == true) {

				//printf("computeFow unit->isOperative() == true\n");

				const FowAlphaCellsLookupItem &cellList = unit->getCachedFow();
				for(std::map<Vec2i,float>::const_iterator iterMap = cellList.surfPosAlphaList.begin();
					iterMap != cellList.surfPosAlphaList.end(); ++iterMap) {
					const Vec2i &surfPos = iterMap->first;
					const float &alpha = iterMap->second;

					minimap.incFowTextureAlphaSurface(surfPos, alpha, true);
				}
			}
		}
	}

	if(this->game) this->game->addPerformanceCount("world compute cells",chronoGamePerformanceCounts.getMillis());
}

GameSettings * World::getGameSettingsPtr() {
    return (game != NULL ? game->getGameSettings() : NULL);
}

const GameSettings * World::getGameSettings() const {
    return (game != NULL ? game->getReadOnlyGameSettings() : NULL);
}

// WARNING! This id is critical! Make sure it fits inside the network packet
// (currently cannot be larger than 2,147,483,647)
// Make sure each faction has their own unique section of id #'s for proper
// multi-platform play
// Calculates the unit unit ID for each faction
//
int World::getNextUnitId(Faction *faction)	{
	MutexSafeWrapper safeMutex(mutexFactionNextUnitId,string(__FILE__) + "_" + intToStr(__LINE__));
	if(mapFactionNextUnitId.find(faction->getIndex()) == mapFactionNextUnitId.end()) {
		mapFactionNextUnitId[faction->getIndex()] = faction->getIndex() * 100000;
	}
	return mapFactionNextUnitId[faction->getIndex()]++;
}

// Get a unique commandid when sending commands to a group of units
int World::getNextCommandGroupId() {
	return ++nextCommandGroupId;
}

void World::playStaticVideo(const string &playVideo) {
	string calculatedFilePath = playVideo;
	bool changed = Properties::applyTagsToValue(calculatedFilePath);
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("\n\nPlay static video changed: %d [%s] [%s]\n\n",changed, playVideo.c_str(),calculatedFilePath.c_str());

	this->game->playStaticVideo(calculatedFilePath);
}

void World::playStreamingVideo(const string &playVideo) {
	string calculatedFilePath = playVideo;
	bool changed = Properties::applyTagsToValue(calculatedFilePath);
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("\n\nPlay streaming video changed: %d [%s] [%s]\n\n",changed, playVideo.c_str(),calculatedFilePath.c_str());

	this->game->playStreamingVideo(calculatedFilePath);
}

void World::stopStreamingVideo(const string &playVideo) {
	string calculatedFilePath = playVideo;
	bool changed = Properties::applyTagsToValue(calculatedFilePath);
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("\n\nStop streaming video changed: %d [%s] [%s]\n\n",changed, playVideo.c_str(),calculatedFilePath.c_str());

	this->game->stopStreamingVideo(calculatedFilePath);
}

void World::stopAllVideo() {
	this->game->stopAllVideo();
}

void World::initTeamResource(const ResourceType *rt,int teamIndex, int value) {
	std::string resourceTypeName = rt->getName(false);
	TeamResources[teamIndex][resourceTypeName].init(rt,value);
}

const Resource *World::getResourceForTeam(const ResourceType *rt, int teamIndex) {
	if(rt == NULL) {
		return NULL;
	}

	std::map<std::string, Resource> &resourceList 	= TeamResources[teamIndex];
	std::string resourceTypeName 					= rt->getName(false);
	resourceList[resourceTypeName].init(rt,0);
	Resource &teamResource 							= resourceList[resourceTypeName];

	for(int index = 0; index < (int)factions.size(); ++index) {

		Faction *faction = factions[index];
		if(faction != NULL && faction->getTeam() == teamIndex) {
			const Resource *factionResource = faction->getResource(rt,true);
			if(factionResource != NULL && factionResource->getType() != NULL) {

				int teamResourceAmount 	= teamResource.getAmount();
				int teamResourceBalance = teamResource.getBalance();

				teamResource.setAmount(teamResourceAmount 	+ factionResource->getAmount());
				teamResource.setBalance(teamResourceBalance + factionResource->getBalance());
			}
		}
	}

	return &teamResource;
}

int World::getStoreAmountForTeam(const ResourceType *rt, int teamIndex) const {

	int teamStoreAmount = 0;
	for(int index = 0; index < (int)factions.size(); ++index) {

		Faction *faction = factions[index];
		if(faction != NULL && faction->getTeam() == teamIndex) {

			teamStoreAmount += faction->getStoreAmount(rt,true);
		}
	}

	return teamStoreAmount;
}

bool World::showResourceTypeForTeam(const ResourceType *rt, int teamIndex) const {
	//if any unit produces the resource
	bool showResource = false;
	for(int index = 0; showResource == false && index < (int)factions.size(); ++index) {
		const Faction *teamFaction = factions[index];
		if(teamFaction != NULL && teamFaction->getTeam() == teamIndex) {

			if(teamFaction->hasUnitTypeWithResourceCostInCache(rt) == true) {
				showResource = true;
			}
		}
	}
	return showResource;
}


bool World::showResourceTypeForFaction(const ResourceType *rt, const Faction *faction) const {
	//if any unit produces the resource
	bool showResource = false;
	for(int factionUnitTypeIndex = 0;
			factionUnitTypeIndex < faction->getType()->getUnitTypeCount();
				++factionUnitTypeIndex) {

		const UnitType *ut = faction->getType()->getUnitType(factionUnitTypeIndex);
		if(ut->getCost(rt) != NULL) {
			showResource = true;
			break;
		}
	}
	return showResource;
}

void World::removeResourceTargetFromCache(const Vec2i &pos) {
	for(int i= 0; i < (int)factions.size(); ++i) {
		factions[i]->removeResourceTargetFromCache(pos);
	}
}

string World::getExploredCellsLookupItemCacheStats() {
	string result = "";

	int posCount = 0;
	int sightCount = 0;
	int exploredCellCount = 0;
	int visibleCellCount = 0;

	for(std::map<Vec2i, std::map<int, ExploredCellsLookupItem > >::iterator iterMap1 = ExploredCellsLookupItemCache.begin();
		iterMap1 != ExploredCellsLookupItemCache.end(); ++iterMap1) {
		posCount++;

		for(std::map<int, ExploredCellsLookupItem >::iterator iterMap2 = iterMap1->second.begin();
			iterMap2 != iterMap1->second.end(); ++iterMap2) {
			sightCount++;

			exploredCellCount += (int)iterMap2->second.exploredCellList.size();
			visibleCellCount += (int)iterMap2->second.visibleCellList.size();
		}
	}

	uint64 totalBytes = exploredCellCount * sizeof(SurfaceCell *);
	totalBytes += visibleCellCount * sizeof(SurfaceCell *);

	totalBytes /= 1000;

	char szBuf[8096]="";
	snprintf(szBuf,8096,"pos [%d] sight [%d] [%d][%d] total KB: %s",posCount,sightCount,exploredCellCount,visibleCellCount,formatNumber(totalBytes).c_str());
	result = szBuf;
	return result;
}

string World::getFowAlphaCellsLookupItemCacheStats() {
	string result = "";

	int surfPosAlphaCount = 0;

	for(int factionIndex = 0; factionIndex < getFactionCount(); ++factionIndex) {
		Faction *faction= getFaction(factionIndex);
		if(faction->getTeam() == thisTeamIndex) {
			for(int unitIndex = 0; unitIndex < faction->getUnitCount(); ++unitIndex) {
				const Unit *unit= faction->getUnit(unitIndex);
				const FowAlphaCellsLookupItem &cache = unit->getCachedFow();

				surfPosAlphaCount += (int)cache.surfPosAlphaList.size();
			}
		}
	}
	uint64 totalBytes = surfPosAlphaCount * sizeof(Vec2i);
	totalBytes += surfPosAlphaCount * sizeof(float);

	totalBytes /= 1000;

	char szBuf[8096]="";
	snprintf(szBuf,8096,"surface count [%d] total KB: %s",surfPosAlphaCount,formatNumber(totalBytes).c_str());
	result = szBuf;
	return result;
}

string World::getAllFactionsCacheStats() {
	string result = "";

	uint64 totalBytes = 0;
	uint64 totalCache1Size = 0;
	uint64 totalCache2Size = 0;
	uint64 totalCache3Size = 0;
	uint64 totalCache4Size = 0;
	uint64 totalCache5Size = 0;
	for(int i = 0; i < getFactionCount(); ++i) {
		uint64 cache1Size = 0;
		uint64 cache2Size = 0;
		uint64 cache3Size = 0;
		uint64 cache4Size = 0;
		uint64 cache5Size = 0;

		totalBytes += getFaction(i)->getCacheKBytes(&cache1Size,&cache2Size,&cache3Size,&cache4Size,&cache5Size);
		totalCache1Size += cache1Size;
		totalCache2Size += cache2Size;
		totalCache3Size += cache3Size;
		totalCache4Size += cache4Size;
		totalCache5Size += cache5Size;
	}

	char szBuf[8096]="";
	snprintf(szBuf,8096,"Cache1 [%llu] Cache2 [%llu] Cache3 [%llu] Cache4 [%llu] Cache5 [%llu] total KB: %s",
			(long long unsigned)totalCache1Size,(long long unsigned)totalCache2Size,(long long unsigned)totalCache3Size,
			(long long unsigned)totalCache4Size,(long long unsigned)totalCache5Size,formatNumber(totalBytes).c_str());
	result = szBuf;
	return result;
}

bool World::factionLostGame(int factionIndex) {
	if(this->game != NULL) {
		return this->game->factionLostGame(factionIndex);
	}
	return false;
}

std::string World::DumpWorldToLog(bool consoleBasicInfoOnly) const {

	string debugWorldLogFile = Config::getInstance().getString("DebugWorldLogFile","debugWorld.log");
	if(getGameReadWritePath(GameConstants::path_logs_CacheLookupKey) != "") {
		debugWorldLogFile = getGameReadWritePath(GameConstants::path_logs_CacheLookupKey) + debugWorldLogFile;
	}
	else {
        string userData = Config::getInstance().getString("UserData_Root","");
        if(userData != "") {
        	endPathWithSlash(userData);
        }
        debugWorldLogFile = userData + debugWorldLogFile;
	}

	if(consoleBasicInfoOnly == true) {
		std::cout << "World debug information:"  << std::endl;
		std::cout << "========================"  << std::endl;

		//factions (and their related info)
		for(int i = 0; i < getFactionCount(); ++i) {
			std::cout << "Faction detail for index: " << i << std::endl;
			std::cout << "--------------------------" << std::endl;

			std::cout << "FactionName = " << getFaction(i)->getType()->getName(false) << std::endl;
			std::cout << "FactionIndex = " << intToStr(getFaction(i)->getIndex()) << std::endl;
			std::cout << "teamIndex = " << intToStr(getFaction(i)->getTeam()) << std::endl;
			std::cout << "startLocationIndex = " << intToStr(getFaction(i)->getStartLocationIndex()) << std::endl;
			std::cout << "thisFaction = " << intToStr(getFaction(i)->getThisFaction()) << std::endl;
			std::cout << "control = " << intToStr(getFaction(i)->getControlType()) << std::endl;
		}
	}
	else {

#if defined(WIN32) && !defined(__MINGW32__)
		FILE *fp = _wfopen(utf8_decode(debugWorldLogFile).c_str(), L"w");
		std::ofstream logFile(fp);
#else
		std::ofstream logFile;
		logFile.open(debugWorldLogFile.c_str(), ios_base::out | ios_base::trunc);
#endif
		logFile << "World debug information:"  << std::endl;
		logFile << "========================"  << std::endl;

		//factions (and their related info)
		for(int i = 0; i < getFactionCount(); ++i) {
			logFile << "Faction detail for index: " << i << std::endl;
			logFile << "--------------------------" << std::endl;
			logFile << getFaction(i)->toString() << std::endl;
		}

		//undertake the dead
		logFile << "Undertake stats:" << std::endl;
		for(int i = 0; i < getFactionCount(); ++i){
			logFile << "Faction: " << getFaction(i)->getType()->getName(false) << std::endl;
			int unitCount = getFaction(i)->getUnitCount();
			for(int j= unitCount - 1; j >= 0; j--){
				Unit *unit= getFaction(i)->getUnit(j);
				if(unit->getToBeUndertaken()) {
					logFile << "Undertake unit index = " << j << unit->getFullName(false) << std::endl;
				}
			}
		}

		logFile.close();
#if defined(WIN32) && !defined(__MINGW32__)
		if(fp) {
			fclose(fp);
		}
#endif
	}

	//printf("Check savegame\n");
	if(this->game != NULL) {
		//printf("Saving...\n");
		this->game->saveGame(GameConstants::saveGameFileDefault);
	}

    return debugWorldLogFile;
}

void World::saveGame(XmlNode *rootNode) {
	std::map<string,string> mapTagReplacements;
	XmlNode *worldNode = rootNode->addChild("World");

//	Map map;
	map.saveGame(worldNode);
//	Tileset tileset;
	worldNode->addAttribute("tileset",tileset.getName(), mapTagReplacements);
//	//TechTree techTree;
//	TechTree *techTree;
	if(techTree != NULL) {
		techTree->saveGame(worldNode);
	}
	worldNode->addAttribute("techTree",(techTree != NULL ? techTree->getName() : ""), mapTagReplacements);
//	TimeFlow timeFlow;
	timeFlow.saveGame(worldNode);
//	Scenario scenario;
//
//	UnitUpdater unitUpdater;
	unitUpdater.saveGame(worldNode);
//    WaterEffects waterEffects;
//    WaterEffects attackEffects; // onMiniMap
//	Minimap minimap;
	minimap.saveGame(worldNode);
//    Stats stats;	//BattleEnd will delete this object
	stats.saveGame(worldNode);
//
//	Factions factions;
	for(unsigned int i = 0; i < factions.size(); ++i) {
		factions[i]->saveGame(worldNode);
	}
//	RandomGen random;
	worldNode->addAttribute("random",intToStr(random.getLastNumber()), mapTagReplacements);
//	ScriptManager* scriptManager;
//
//	int thisFactionIndex;
	worldNode->addAttribute("thisFactionIndex",intToStr(thisFactionIndex), mapTagReplacements);
//	int thisTeamIndex;
	worldNode->addAttribute("thisTeamIndex",intToStr(thisTeamIndex), mapTagReplacements);
//	int frameCount;
	worldNode->addAttribute("frameCount",intToStr(frameCount), mapTagReplacements);
//	//int nextUnitId;
//	Mutex mutexFactionNextUnitId;
	MutexSafeWrapper safeMutex(mutexFactionNextUnitId,string(__FILE__) + "_" + intToStr(__LINE__));
//	std::map<int,int> mapFactionNextUnitId;
	for(std::map<int,int>::iterator iterMap = mapFactionNextUnitId.begin();
			iterMap != mapFactionNextUnitId.end(); ++iterMap) {
		XmlNode *factionNextUnitIdNode = worldNode->addChild("FactionNextUnitId");

		factionNextUnitIdNode->addAttribute("key",intToStr(iterMap->first), mapTagReplacements);
		factionNextUnitIdNode->addAttribute("value",intToStr(iterMap->second), mapTagReplacements);
	}
	safeMutex.ReleaseLock();
//	//config
//	bool fogOfWarOverride;
	worldNode->addAttribute("fogOfWarOverride",intToStr(fogOfWarOverride), mapTagReplacements);
//	bool fogOfWar;
	worldNode->addAttribute("fogOfWar",intToStr(fogOfWar), mapTagReplacements);
//	int fogOfWarSmoothingFrameSkip;
	worldNode->addAttribute("fogOfWarSmoothingFrameSkip",intToStr(fogOfWarSmoothingFrameSkip), mapTagReplacements);
//	bool fogOfWarSmoothing;
	worldNode->addAttribute("fogOfWarSmoothing",intToStr(fogOfWarSmoothing), mapTagReplacements);
//	Game *game;
//	Chrono chronoPerfTimer;
//	bool perfTimerEnabled;
//
//	bool unitParticlesEnabled;
	worldNode->addAttribute("unitParticlesEnabled",intToStr(unitParticlesEnabled), mapTagReplacements);
//	std::map<string,StaticSound *> staticSoundList;
//	std::map<string,StrSound *> streamSoundList;
//
//	uint32 nextCommandGroupId;
	worldNode->addAttribute("nextCommandGroupId",intToStr(nextCommandGroupId), mapTagReplacements);
//	string queuedScenarioName;
	worldNode->addAttribute("queuedScenarioName",queuedScenarioName, mapTagReplacements);
//	bool queuedScenarioKeepFactions;
	worldNode->addAttribute("queuedScenarioKeepFactions",intToStr(queuedScenarioKeepFactions), mapTagReplacements);

	worldNode->addAttribute("disableAttackEffects",intToStr(disableAttackEffects), mapTagReplacements);
}

void World::loadGame(const XmlNode *rootNode) {
	loadWorldNode = rootNode;
}

}}//end namespace
