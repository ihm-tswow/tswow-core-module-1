/*
 * This file is part of tswow (https://github.com/tswow/).
 * Copyright (C) 2020 tswow <https://github.com/tswow/>
 * 
 * This program is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation, version 3.
 * 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "ScriptMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "TSEvents.h"
#include "TSEventLoader.h"
#include "TSMutable.h"
#include "Player.h"
#include "TSPlayer.h"
#include "TSVehicle.h"
#include "TSUnit.h"
#include "TSSpell.h"
#include "TSCreature.h"
#include "TSGameObject.h"
#include "TSQuest.h"
#include "TSItem.h"
#include "QuestDef.h"
#include "TSMutableString.h"
#include "ItemTemplate.h"
#include "TSItemTemplate.h"
#include "TSSpellInfo.h"
#include "Group.h"
#include "TSGroup.h"
#include "Guild.h"
#include "TSGuild.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "TSIDs.h"
#include "TSChannel.h"
#include "TSWorldEntity.h"
#include "DBCStores.h"
#include "MapManager.h"
#include "base64.h"
#include "Config.h"
#include "TSMessageBuffer.h"

#include <fstream>
#include <map>
#include <limits>

TSEvents tsEvents;
std::map<std::string,TSEventHandlers> eventHandlers;

std::map<std::string,uint32_t> modIds;
std::vector<uint32_t> reloads;

/** Network Message maps */
std::vector<MessageHandle<void>> messageMap;
std::map<uint32_t, std::vector<uint16_t>> messageModMap;

std::vector<MessageHandle<void>> & getMessageMap()
{
    return messageMap;
}

TSEvents* GetTSEvents()
{
    return &tsEvents;
}

uint32_t GetReloads(uint32_t modid)
{
    return reloads[modid];
}

bool TSShouldLoadEventHandler(boost::filesystem::path const& name)
{
    std::string name_str = name.filename().string();
    if(name_str.size() <= 4)
    {
        return false;
    }
    auto name_offset = name_str.find("scripts_tswow_")+strlen("scripts_tswow_");
    name_str = name_str.substr(name_offset,name_str.find(".")-name_offset);
    std::string data_dir =
        sConfigMgr->GetStringDefault("DataDir","../../datasets/default");
    auto modulesfile =
        boost::filesystem::path(data_dir) / boost::filesystem::path("modules.txt");
    std::ifstream f(modulesfile.string().c_str());
    if(!f)
    {
        return true;
    }
    std::string line;
    while (std::getline(f, line))
    {
        if(line == name_str)
        {
            return true;
        }
    }
    return false;
}

TSEventHandlers* TSLoadEventHandler(boost::filesystem::path const& modulePath, std::string const& moduleName)
{
    std::string spath = modulePath.string();
    uint32_t modid = 0;
    if(modIds.find(spath) != modIds.end())
    {
        modid = modIds[spath];
    }
    else
    {
        modid = reloads.size();
        reloads.push_back(0);
    }

    auto handler = &(eventHandlers[spath] = TSEventHandlers(modid,moduleName));
    handler->LoadEvents(&tsEvents);
    return handler;
}

static void RemoveData(WorldObject* obj)
{
    obj->m_tsEntity.m_compiledClasses.clear();
    obj->m_tsWorldEntity.clear();
    obj->m_tsCollisions.callbacks.clear();
}

struct RemoveWorker {
    void Visit(std::unordered_map<ObjectGuid, Creature*>& creatureMap)
    {
        for(auto const& p : creatureMap)
            RemoveData(p.second);
    }

    void Visit(std::unordered_map<ObjectGuid, GameObject*>& gameObjectMap)
    {
        for(auto const& p : gameObjectMap)
            RemoveData(p.second);
    }

    template<class T>
    void Visit(std::unordered_map<ObjectGuid, T*>&) { }
};

void TSUnloadEventHandler(boost::filesystem::path const& name)
{
    std::string sname = name.string();
    // Unload network message classes and handlers
    auto modid = modIds[sname];
    if(messageModMap.find(modid) != messageModMap.end())
    {
        auto vec = messageModMap[modid];
        for(auto g : vec)
        {
            if(g>messageMap.size()) {
                continue;
            }
            messageMap[g] = MessageHandle<void>();
        }
        messageModMap.erase(modid);
    }

    // Unload events
    std::map<std::string,TSEventHandlers>::iterator iter 
        = eventHandlers.find(sname);
    if(iter!=eventHandlers.end())
    {
        iter->second.Unload();
        reloads[iter->second.m_modid]++;
        eventHandlers.erase(sname);
    }

    // Clean up timers and storage for creatures and gameobjects
    sMapMgr->DoForAllMaps([](auto map){
        map->m_tsWorldEntity.clear();
        map->m_tsEntity.m_compiledClasses.clear();
        RemoveWorker worker;
        TypeContainerVisitor<RemoveWorker, MapStoredObjectTypesContainer> visitor(worker);
        visitor.Visit(map->GetObjectsStore());
    });

    // Clean up timers and storage for players
    for(auto &p : ObjectAccessor::GetPlayers())
    {
        RemoveData(p.second);
    }
}

struct ReloadGameObjectWorker {
    GameObjectOnReloadType _fn;
    uint32 _gobj_id;

    ReloadGameObjectWorker( GameObjectOnReloadType fn
                          , uint32 gobj_id
                          )
                          : _fn(fn)
                          , _gobj_id(gobj_id)
                          {}

    void Visit(std::unordered_map<ObjectGuid, GameObject*>& gameObjectMap)
    {
        for(auto const& p : gameObjectMap)
            if(_gobj_id == std::numeric_limits<uint32_t>::max() || p.second->GetGOInfo()->entry == _gobj_id)
                _fn(TSGameObject(p.second));
    }

    template<class T>
    void Visit(std::unordered_map<ObjectGuid, T*>&) { }
};
void ReloadGameObject(GameObjectOnReloadType fn, uint32 id)
{
    sMapMgr->DoForAllMaps([&](auto map){
        ReloadGameObjectWorker worker(fn,id);
        TypeContainerVisitor<ReloadGameObjectWorker, MapStoredObjectTypesContainer> visitor(worker);
        visitor.Visit(map->GetObjectsStore());
    });
}

void ReloadPlayer(PlayerOnReloadType fn, uint32 id)
{
    for(auto &p : ObjectAccessor::GetPlayers())
    {
        fn(TSPlayer(p.second), false);
    }
}

struct ReloadCreatureWorker {
    CreatureOnReloadType _fn;
    uint32 _creature_id;

    ReloadCreatureWorker( CreatureOnReloadType fn
                          , uint32 creature_id
                          )
                          : _fn(fn)
                          , _creature_id(creature_id)
                          {}

    void Visit(std::unordered_map<ObjectGuid, Creature*>& creatureMap)
    {
        for(auto const& p : creatureMap)
            if(_creature_id == std::numeric_limits<uint32_t>::max() || _creature_id == p.second->GetCreatureTemplate()->Entry)
                _fn(TSCreature(p.second));
    }

    template<class T>
    void Visit(std::unordered_map<ObjectGuid, T*>&) { }
};
void ReloadCreature(CreatureOnReloadType fn, uint32 id)
{
    sMapMgr->DoForAllMaps([&](auto map){
        ReloadCreatureWorker worker(fn,id);
        TypeContainerVisitor<ReloadCreatureWorker, MapStoredObjectTypesContainer> visitor(worker);
        visitor.Visit(map->GetObjectsStore());
    });
}

void ReloadMap(MapOnReloadType fn, uint32 id)
{
    sMapMgr->DoForAllMaps([&](auto map){
        if(id==std::numeric_limits<uint32_t>::max() || id == map->GetId())
        {
            fn(TSMap(map));
        }
    });
}


static std::map<uint32_t, TSMapDataExtra*> mapData;
TSMapDataExtra* GetMapDataExtra(uint32_t id)
{
    if(mapData.find(id) == mapData.end())
    {
        return (mapData[id] = new TSMapDataExtra());
    }
    else
    {
        return mapData[id];
    }
}

/** Network events */

void RegisterMessage(uint32_t modid, uint16_t opcode, uint8_t size, std::function<std::shared_ptr<void>(uint8_t*)> constructor)
{
    if(messageModMap.find(modid)==messageModMap.end())
    {
        messageModMap[modid] = std::vector<uint16_t>();
    }
    (&messageModMap[modid])->push_back(opcode);


    if(opcode>=messageMap.size())
    {
        messageMap.resize(opcode+1);
    }

    messageMap[opcode] = MessageHandle<void>(size,constructor);
}

MessageHandle<void>* GetMessage(uint16_t opcode)
{
    return &messageMap[opcode];
}

const std::string TSWOW_ITEM_PREFIX = "tswow_item:";
const std::string TSWOW_CREATURE_PREFIX = "tswow_creature:";

bool handleTSWoWGMMessage(Player* player, Player* receiver, std::string & msgIn)
{
    if(msgIn.size()<2) return false;
    std::string msg = msgIn.substr(1);

    if(player != receiver || !player->CanBeGameMaster()) {
        return false;
    }

    if(msg == "tswow_am_i_gm") {
        TSPlayer(player)->SendAddonMessage(JSTR(""),TSString("tswow_you_are_gm"),7,TSPlayer(player));
        return true;
    }

    if(msg.rfind(TSWOW_ITEM_PREFIX,0) == 0) {
        int itemId = atoi(msg.substr(TSWOW_ITEM_PREFIX.size()).c_str());
        auto data = sObjectMgr->GetItemTemplate(itemId);
        if(!data) return true;
        int displayId = data->DisplayInfoID;
        TSPlayer(player)->SendAddonMessage(
            JSTR("") ,
            TSString(
              std::string("tswow_item_response:") +
              std::to_string(itemId) +
              ":" +
              std::to_string(displayId)),
              7,
              TSPlayer(player));
        return true;
    }

    if(msg.rfind(TSWOW_CREATURE_PREFIX,0) == 0)
    {
        int creatureId = atoi(msg.substr(TSWOW_CREATURE_PREFIX.size()).c_str());
        auto data = sObjectMgr->GetCreatureTemplate(creatureId);
        if(!data) return true;
        TSPlayer(player)->SendAddonMessage(
              JSTR(""), TSString(
              std::string("tswow_creature_response:")+ std::to_string(creatureId) +
               ":" + std::to_string(data->faction)  +
               ":" + std::to_string(data->Modelid1) +
               ":" + std::to_string(data->Modelid2) +
               ":" + std::to_string(data->Modelid3) +
               ":" + std::to_string(data->Modelid4)),
               7,
               TSPlayer(player));
        return true;
    }

    return false;
}

bool handleAddonNetworkMessage(Player* player,uint32 type,uint32 lang,std::string& msg,Player* receiver)
{
    if(player!=receiver) {
        return false;
    }

    if(player->m_message_buffer.receiveFragment(msg.size(),msg.c_str()))
    {
        return true;
    }

    char * carr = const_cast<char*>(msg.c_str());
    int offset = 0;
    for(int i=0;i<msg.size();++i)
    {
        if(carr[i] == '\t' || carr[i] == ' ') offset++;
        else break;
    }

    if((msg.size()-offset)<=4) {
        return false;
    }

    auto preDecodeHeader = ((uint32_t*)(carr+offset))[0];
    if(preDecodeHeader != 0x50414753)
    {
        return false;
    }

    uint8_t outarr[250];

    int outlen = decodeBase64((uint8_t*)(carr+offset),msg.size()-offset,outarr);

    BinReader<uint8_t> reader(outarr,outlen);
    FIRE(AddonOnMessage,reader);

    if(outlen<=6) {
        TC_LOG_DEBUG("tswow.addonmessage","AddOnMessage: Message too short");
        return false;
    }

    if(reader.Read<uint32_t>(0)!=1007688) {
        TC_LOG_ERROR("tswow.addonmessage","AddOnMessage: Incorrect header (after decode) %x (expected 1007688)",reader.Read<uint32>(0));
        return false;
    }

    uint16_t opcode = reader.Read<uint16_t>(4);
    if(opcode>=getMessageMap().size()) {
        TC_LOG_DEBUG("tswow.addonmessage","AddOnMessage: Received invalid opcode %u",opcode);
        return true;
    }

    auto handler = &getMessageMap()[opcode];
    if(handler->size!=(outlen-6) || !handler->enabled) {
        TC_LOG_DEBUG("tswow.addonmessage","AddOnMessage: Received invalid message size %u for opcode %u (expected %u)",outlen,opcode,handler->size+6);
        return true;
    }

    handler->fire(TSPlayer(player),outarr+6);
    return true;
}

void AddMessageListener(uint16_t opcode, void(*func)(TSPlayer,std::shared_ptr<void>))
{
    if(opcode>=messageMap.size()) { return; }
    (&messageMap[opcode])->listeners.push_back(func);
}

void AddSC_tswow_commandscript();
void TSInitializeEvents()
{
    TSLoadEvents();
    LoadIDs();
    InitializeMessages();
    AddSC_tswow_commandscript();
};
