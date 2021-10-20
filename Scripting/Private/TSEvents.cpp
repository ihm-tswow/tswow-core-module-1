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
#include "DBCStores.h"
#include "MapManager.h"
#include "base64.h"
#include "Config.h"

class TSServerScript : public ServerScript
{
public:
    TSServerScript() : ServerScript("TSServerScript"){}
};

class TSWorldScript : public WorldScript
{
public:
    TSWorldScript() : WorldScript("TSWorldScript"){}
    void OnOpenStateChange(bool open) FIRE(WorldOnOpenStateChange,open)
    void OnConfigLoad(bool reload) FIRE(WorldOnConfigLoad,reload)
    void OnMotdChange(std::string& newMotd) FIRE(WorldOnMotdChange,TSString(newMotd))
    void OnShutdownInitiate(ShutdownExitCode code,ShutdownMask mask) FIRE(WorldOnShutdownInitiate,code,mask)
    void OnUpdate(uint32 diff) FIRE(WorldOnUpdate,diff)
};

class TSFormulaScript : public FormulaScript
{
public:
    TSFormulaScript() : FormulaScript("TSFormulaScript"){}
    void OnHonorCalculation(float& honor,uint8 level,float multiplier) FIRE(FormulaOnHonorCalculation,TSMutable<float>(&honor),level,multiplier)
    void OnGrayLevelCalculation(uint8& grayLevel,uint8 playerLevel) FIRE(FormulaOnGrayLevelCalculation,TSMutable<uint8>(&grayLevel),playerLevel)
    void OnColorCodeCalculation(XPColorChar& color,uint8 playerLevel,uint8 mobLevel) FIRE(FormulaOnColorCodeCalculation,TSMutable<uint8>(&((uint8&)color)),playerLevel,mobLevel)
    void OnZeroDifferenceCalculation(uint8& diff,uint8 playerLevel) FIRE(FormulaOnZeroDifferenceCalculation,TSMutable<uint8>(&diff),playerLevel)
    void OnBaseGainCalculation(uint32& gain,uint8 playerLevel,uint8 mobLevel,ContentLevels content) FIRE(FormulaOnBaseGainCalculation,TSMutable<uint32>(&gain),playerLevel,mobLevel,content)
    void OnGainCalculation(uint32& gain,Player* player,Unit* unit) FIRE(FormulaOnGainCalculation,TSMutable<uint32>(&gain),TSPlayer(player),TSUnit(unit))
    void OnGroupRateCalculation(float& rate,uint32 count,bool isRaid) FIRE(FormulaOnGroupRateCalculation,TSMutable<float>(&rate),count,isRaid)
};

class TSUnitScript : public UnitScript
{
public:
    TSUnitScript() : UnitScript("TSUnitScript"){}
    void OnHeal(Unit* healer, Unit* reciever, uint32& gain) {
        FIRE(
              FormulaOnHeal
            , TSUnit(healer)
            , TSUnit(reciever)
            , TSMutable<uint32>(&gain)
        );
    }
    //void OnDamage(Unit* attacker,Unit* victim,uint32& damage) FIRE(UnitOnDamage,TSUnit(attacker),TSUnit(victim),TSMutable<uint32>(&damage))
    //void ModifyPeriodicDamageAurasTick(Unit* target,Unit* attacker,uint32& damage) FIRE(UnitModifyPeriodicDamageAurasTick,TSUnit(target),TSUnit(attacker),TSMutable<uint32>(&damage))
    //void ModifyMeleeDamage(Unit* target,Unit* attacker,uint32& damage) FIRE(UnitModifyMeleeDamage,TSUnit(target),TSUnit(attacker),TSMutable<uint32>(&damage))
    //void ModifySpellDamageTaken(Unit* target,Unit* attacker,int32& damage) FIRE(UnitModifySpellDamageTaken,TSUnit(target),TSUnit(attacker),TSMutable<int32>(&damage))
    //void ModifyVehiclePassengerExitPos(Unit* passenger,Vehicle* vehicle,Position& pos) FIRE(UnitModifyVehiclePassengerExitPos,TSUnit(passenger),TSVehicle(vehicle),TSMutable<Position>(pos))
};

class TSAreaTriggerScript : public AreaTriggerScript
{
public:
    TSAreaTriggerScript() : AreaTriggerScript("TSAreaTriggerScript"){}
    //bool OnTrigger(Player* player,AreaTriggerEntry const* trigger) FIRE_RETURN(AreaTriggerOnTrigger,bool,false,TSPlayer(player),trigger)
};

class TSWeatherScript : public WeatherScript
{
public:
    TSWeatherScript() : WeatherScript("TSWeatherScript"){}
    //void OnChange(Weather* weather,WeatherState state,float grade) FIRE(WeatherOnChange,weather,state,grade)
};

class TSAuctionHouseScript : public AuctionHouseScript
{
public:
    TSAuctionHouseScript() : AuctionHouseScript("TSAuctionHouseScript"){}
    void OnAuctionAdd(AuctionHouseObject* ah,AuctionEntry* entry) FIRE(AuctionHouseOnAuctionAdd,TSAuctionHouseObject(ah),TSAuctionEntry(entry))
    void OnAuctionRemove(AuctionHouseObject* ah,AuctionEntry* entry) FIRE(AuctionHouseOnAuctionRemove,TSAuctionHouseObject(ah),TSAuctionEntry(entry))
    void OnAuctionSuccessful(AuctionHouseObject* ah,AuctionEntry* entry) FIRE(AuctionHouseOnAuctionSuccessful,TSAuctionHouseObject(ah),TSAuctionEntry(entry))
    void OnAuctionExpire(AuctionHouseObject* ah,AuctionEntry* entry) FIRE(AuctionHouseOnAuctionExpire,TSAuctionHouseObject(ah),TSAuctionEntry(entry))
};

class TSConditionScript : public ConditionScript
{
public:
    TSConditionScript() : ConditionScript("TSConditionScript"){}
    //TODO: Fix return type
    //bool OnConditionCheck(Condition const* condition,ConditionSourceInfo& sourceInfo) FIRE_RETURN(ConditionOnConditionCheck,bool,TODO_FIXME,,condition,TSMutable<ConditionSourceInfo>(sourceInfo))
};

class TSVehicleScript : public VehicleScript
{
public:
    TSVehicleScript() : VehicleScript("TSVehicleScript"){}
    void OnInstall(Vehicle* veh) FIRE(VehicleOnInstall,TSVehicle(veh))
    void OnUninstall(Vehicle* veh) FIRE(VehicleOnUninstall,TSVehicle(veh))
    void OnReset(Vehicle* veh) FIRE(VehicleOnReset,TSVehicle(veh))
    void OnInstallAccessory(Vehicle* veh,Creature* accessory) FIRE(VehicleOnInstallAccessory,TSVehicle(veh),TSCreature(accessory))
    void OnAddPassenger(Vehicle* veh,Unit* passenger,int8 seatId) FIRE(VehicleOnAddPassenger,TSVehicle(veh),TSUnit(passenger),seatId)
    void OnRemovePassenger(Vehicle* veh,Unit* passenger) FIRE(VehicleOnRemovePassenger,TSVehicle(veh),TSUnit(passenger))
};

class TSAchievementCriteriaScript : public AchievementCriteriaScript
{
public:
    TSAchievementCriteriaScript() : AchievementCriteriaScript("TSAchievementCriteriaScript"){}
    //bool OnCheck(Player* source,Unit* target) FIRE_RETURN(AchievementCriteriaOnCheck,bool,false,TSPlayer(source),TSUnit(target))
};

class TSPlayerScript : public PlayerScript
{
public:
    TSPlayerScript() : PlayerScript("TSPlayerScript"){}
    void OnPVPKill(Player* killer,Player* killed) FIRE(PlayerOnPVPKill,TSPlayer(killer),TSPlayer(killed))
    void OnCreatureKill(Player* killer,Creature* killed) FIRE(PlayerOnCreatureKill,TSPlayer(killer),TSCreature(killed))
    void OnPlayerKilledByCreature(Creature* killer,Player* killed) FIRE(PlayerOnPlayerKilledByCreature,TSCreature(killer),TSPlayer(killed))
    void OnLevelChanged(Player* player,uint8 oldLevel) FIRE(PlayerOnLevelChanged,TSPlayer(player),oldLevel)
    void OnFreeTalentPointsChanged(Player* player,uint32 points) FIRE(PlayerOnFreeTalentPointsChanged,TSPlayer(player),points)
    void OnTalentsReset(Player* player,bool noCost) FIRE(PlayerOnTalentsReset,TSPlayer(player),noCost)
    void OnMoneyChanged(Player* player,int32& amount) FIRE(PlayerOnMoneyChanged,TSPlayer(player),TSMutable<int32>(&amount))
    void OnMoneyLimit(Player* player,int32 amount) FIRE(PlayerOnMoneyLimit,TSPlayer(player),amount)
    void OnGiveXP(Player* player,uint32& amount,Unit* victim) FIRE(PlayerOnGiveXP,TSPlayer(player),TSMutable<uint32>(&amount),TSUnit(victim))
    void OnReputationChange(Player* player,uint32 factionId,int32& standing,bool incremental) FIRE(PlayerOnReputationChange,TSPlayer(player),factionId,TSMutable<int32>(&standing),incremental)
    void OnDuelRequest(Player* target,Player* challenger) FIRE(PlayerOnDuelRequest,TSPlayer(target),TSPlayer(challenger))
    void OnDuelStart(Player* player1,Player* player2) FIRE(PlayerOnDuelStart,TSPlayer(player1),TSPlayer(player2))
    void OnDuelEnd(Player* winner,Player* loser,DuelCompleteType type) FIRE(PlayerOnDuelEnd,TSPlayer(winner),TSPlayer(loser),type)
    void OnChat(Player* player,uint32 type,uint32 lang,std::string& msg) FIRE(PlayerOnSay,TSPlayer(player),type,lang,TSMutableString(&msg))
    void OnChat(Player* player,uint32 type,uint32 lang,std::string& msg,Player* receiver) {
        if(handleTSWoWGMMessage(player,receiver,msg))
        {
            TC_LOG_DEBUG("tswow","CHAT: Successfully handled TSWoW GM Message");
            return;
        }

        // needs to happen here, because we want to be sure
        // successful messages do not reach the normal OnWhisper events.
        if(handleAddonNetworkMessage(player,type,lang,msg,receiver))
        {
            TC_LOG_DEBUG("tswow","CHAT: Successfully handled TSWoW AddOn Message");
            return;
        }

        FIRE(PlayerOnWhisper,TSPlayer(player),type,lang,TSMutableString(&msg),TSPlayer(receiver))
    }
    void OnChat(Player* player,uint32 type,uint32 lang,std::string& msg,Group* group) FIRE(PlayerOnChatGroup,TSPlayer(player),type,lang,TSMutableString(&msg),TSGroup(group))
    void OnChat(Player* player,uint32 type,uint32 lang,std::string& msg,Guild* guild) FIRE(PlayerOnChatGuild,TSPlayer(player),type,lang,TSMutableString(&msg),TSGuild(guild))
    void OnChat(Player* player,uint32 type,uint32 lang,std::string& msg,Channel* channel) FIRE(PlayerOnChat,TSPlayer(player),type,lang,TSMutableString(&msg),TSChannel(channel))
    void OnEmote(Player* player,Emote emote) FIRE(PlayerOnEmote,TSPlayer(player),emote)
    void OnTextEmote(Player* player,uint32 textEmote,uint32 emoteNum,ObjectGuid guid) FIRE(PlayerOnTextEmote,TSPlayer(player),textEmote,emoteNum,guid.GetRawValue())
    void OnSpellCast(Player* player,Spell* spell,bool skipCheck) FIRE(PlayerOnSpellCast,TSPlayer(player),TSSpell(spell),skipCheck)
    void OnLogin(Player* player,bool firstLogin) FIRE(PlayerOnLogin,TSPlayer(player),firstLogin)
    void OnLogout(Player* player) FIRE(PlayerOnLogout,TSPlayer(player))
    void OnCreate(Player* player) FIRE(PlayerOnCreate,TSPlayer(player))
    void OnDelete(ObjectGuid guid,uint32 accountId) FIRE(PlayerOnDelete,guid.GetRawValue(),accountId)
    void OnFailedDelete(ObjectGuid guid,uint32 accountId) FIRE(PlayerOnFailedDelete,guid.GetRawValue(),accountId)
    void OnSave(Player* player) FIRE(PlayerOnSave,TSPlayer(player))
    void OnBindToInstance(Player* player,Difficulty difficulty,uint32 mapId,bool permanent,uint8 extendState) FIRE(PlayerOnBindToInstance,TSPlayer(player),difficulty,mapId,permanent,extendState)
    void OnUpdateZone(Player* player,uint32 newZone,uint32 newArea) FIRE(PlayerOnUpdateZone,TSPlayer(player),newZone,newArea)
    void OnMapChanged(Player* player) FIRE(PlayerOnMapChanged,TSPlayer(player))
    void OnQuestObjectiveProgress(Player* player,Quest const* quest,uint32 objectiveIndex,uint16 progress) FIRE(PlayerOnQuestObjectiveProgress,TSPlayer(player),quest,objectiveIndex,progress)
    void OnQuestStatusChange(Player* player,uint32 questId) FIRE(PlayerOnQuestStatusChange,TSPlayer(player),questId)
    void OnMovieComplete(Player* player,uint32 movieId) FIRE(PlayerOnMovieComplete,TSPlayer(player),movieId)
    void OnPlayerRepop(Player* player) FIRE(PlayerOnPlayerRepop,TSPlayer(player))
};

class TSAccountScript : public AccountScript
{
public:
    TSAccountScript() : AccountScript("TSAccountScript"){}
    void OnAccountLogin(uint32 accountId) FIRE(AccountOnAccountLogin,accountId)
    void OnFailedAccountLogin(uint32 accountId) FIRE(AccountOnFailedAccountLogin,accountId)
    void OnEmailChange(uint32 accountId) FIRE(AccountOnEmailChange,accountId)
    void OnFailedEmailChange(uint32 accountId) FIRE(AccountOnFailedEmailChange,accountId)
    void OnPasswordChange(uint32 accountId) FIRE(AccountOnPasswordChange,accountId)
    void OnFailedPasswordChange(uint32 accountId) FIRE(AccountOnFailedPasswordChange,accountId)
};

class TSGuildScript : public GuildScript
{
public:
    TSGuildScript() : GuildScript("TSGuildScript"){}
    void OnAddMember(Guild* guild,Player* player,uint8& plRank) FIRE(GuildOnAddMember,TSGuild(guild),TSPlayer(player),TSMutable<uint8>(&plRank))
    void OnRemoveMember(Guild* guild,Player* player,bool isDisbanding,bool isKicked) FIRE(GuildOnRemoveMember,TSGuild(guild),TSPlayer(player),isDisbanding,isKicked)
    void OnMOTDChanged(Guild* guild,const std::string& newMotd) FIRE(GuildOnMOTDChanged,TSGuild(guild),TSString(newMotd))
    void OnInfoChanged(Guild* guild,const std::string& newInfo) FIRE(GuildOnInfoChanged,TSGuild(guild),TSString(newInfo))
    void OnCreate(Guild* guild,Player* leader,const std::string& name) FIRE(GuildOnCreate,TSGuild(guild),TSPlayer(leader),TSString(name))
    void OnDisband(Guild* guild) FIRE(GuildOnDisband,TSGuild(guild))
    void OnMemberWitdrawMoney(Guild* guild,Player* player,uint32& amount,bool isRepair) FIRE(GuildOnMemberWitdrawMoney,TSGuild(guild),TSPlayer(player),TSMutable<uint32>(&amount),isRepair)
    void OnMemberDepositMoney(Guild* guild,Player* player,uint32& amount) FIRE(GuildOnMemberDepositMoney,TSGuild(guild),TSPlayer(player),TSMutable<uint32>(&amount))
    void OnEvent(Guild* guild,uint8 eventType,ObjectGuid::LowType playerGuid1,ObjectGuid::LowType playerGuid2,uint8 newRank) FIRE(GuildOnEvent,TSGuild(guild),eventType,playerGuid1,playerGuid2,newRank)
    void OnBankEvent(Guild* guild,uint8 eventType,uint8 tabId,ObjectGuid::LowType playerGuid,uint32 itemOrMoney,uint16 itemStackCount,uint8 destTabId) FIRE(GuildOnBankEvent,TSGuild(guild),eventType,tabId,playerGuid,itemOrMoney,itemStackCount,destTabId)
};

class TSGroupScript : public GroupScript
{
public:
    TSGroupScript() : GroupScript("TSGroupScript"){}
    void OnAddMember(Group* group,ObjectGuid guid) FIRE(GroupOnAddMember,TSGroup(group),guid.GetRawValue())
    void OnInviteMember(Group* group,ObjectGuid guid) FIRE(GroupOnInviteMember,TSGroup(group),guid.GetRawValue())
    void OnRemoveMember(Group* group,ObjectGuid guid,RemoveMethod method,ObjectGuid kicker,char const* reason) FIRE(GroupOnRemoveMember,TSGroup(group),guid.GetRawValue(),method,kicker.GetRawValue(),TSString(reason))
    void OnChangeLeader(Group* group,ObjectGuid newLeaderGuid,ObjectGuid oldLeaderGuid) FIRE(GroupOnChangeLeader,TSGroup(group),newLeaderGuid.GetRawValue(),oldLeaderGuid.GetRawValue())
    void OnDisband(Group* group) FIRE(GroupOnDisband,TSGroup(group))
};

void TSSpellMap::OnAdd(uint32_t key, TSSpellEvents* events)
{
    auto spellInfo = sSpellMgr->GetSpellInfo(key);
    if (spellInfo == nullptr) {
        throw std::runtime_error(
              "Tried registering event for invalid Spell entry: "
            + std::to_string(key)
            + "(did you remember to build your datascripts?)"
        );
    }
    const_cast<SpellInfo*>(spellInfo)->events = events;
}

void TSSpellMap::OnRemove(uint32_t key)
{
    
}

void TSCreatureMap::OnAdd(uint32_t key, TSCreatureEvents* events)
{
    auto creatureTemplate = sObjectMgr->GetCreatureTemplate(key);
    if (creatureTemplate == nullptr)
    {
        throw std::runtime_error(
              "Tried registering event for invalid Creature entry: "
            + std::to_string(key)
            + "(did you remember to build your datascripts?)"
        );
    }
    const_cast<CreatureTemplate*>(creatureTemplate)->events = events;
}

void TSCreatureMap::OnRemove(uint32_t key)
{
    
}

void TSGameObjectMap::OnAdd(uint32_t key, TSGameObjectEvents* events)
{
    auto gobjTemplate = sObjectMgr->GetGameObjectTemplate(key);
    if (gobjTemplate == nullptr)
    {
        throw std::runtime_error(
            "Tried registering event for invalid GameObject entry: "
            + std::to_string(key)
            + "(did you remember to build your datascripts?)"
        );
    }
    const_cast<GameObjectTemplate*>(sObjectMgr->GetGameObjectTemplate(key))->events = events;
}

void TSGameObjectMap::OnRemove(uint32_t key)
{

}

void TSItemMap::OnAdd(uint32_t key, TSItemEvents* events)
{
    auto itemTemplate = sObjectMgr->GetItemTemplate(key);
    if (itemTemplate == nullptr)
    {
        throw std::runtime_error(
            "Tried registering event for invalid Item entry: "
            + std::to_string(key)
            + "(did you remember to build your datascripts?)"
        );
    }
    const_cast<ItemTemplate*>(itemTemplate)->events = events;
}

void TSItemMap::OnRemove(uint32_t key)
{
}

void TSMapMap::OnAdd(uint32_t key, TSMapEvents* events)
{
    // TODO: should check if this is a valid map, will now allow to store this on any id.
    GetMapDataExtra(key)->events = events;
}

void TSMapMap::OnRemove(uint32_t key)
{
    
}

void TSAchievementMap::OnAdd(uint32_t key, TSAchievementEvents* events)
{
    InitializeAchievementEvent(key, events);
}

void TSAchievementMap::OnRemove(uint32_t key)
{
}

void TSAreaTriggerMap::OnAdd(uint32_t key, TSAreaTriggerEvents* events)
{
    InitializeAreaTriggerEvents(key, events);
}

void TSAreaTriggerMap::OnRemove(uint32_t key)
{

}

void TSLoadEvents()
{
    new TSServerScript();
    new TSWorldScript();
    new TSFormulaScript();
    new TSUnitScript();
    //new TSAreaTriggerScript();
    //new TSWeatherScript();
    new TSAuctionHouseScript();
    //new TSConditionScript();
    //new TSVehicleScript();
    //new TSAchievementCriteriaScript();
    new TSPlayerScript();
    new TSAccountScript();
    new TSGuildScript();
    new TSGroupScript();
}
