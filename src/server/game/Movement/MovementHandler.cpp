
#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "Corpse.h"
#include "Player.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "Transport.h"
#include "BattleGround.h"
#include "FlightPathMovementGenerator.h"
#include "InstanceSaveMgr.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Pet.h"
#include "Chat.h"
#include "PlayerAntiCheat.h"
#include "GameTime.h"
#include "Map.h"
#include "MovementPacketSender.h"
#include "RBAC.h"
#include "Language.h"

/*
Sun:
Changes from chaosdib Movement implementation:
- Added HasPendingMovementChange
- Fixed SetLastMoveServerTimestamp time
- Removed all IsInClientControlSet checks. If client has lost control, it should still be able to receive speed changes and ack them.
  For example when receiving a fear, in the current implementation the client respond the speed ack after the mover change, so it's not in control anymore.
  We could either make sure all calls in the code are in the right order... or change this check.
  It is now replaced by better enforcement that the client is responding to a speed change we asked
- We clear pending move changes when changing maps. No reason to keep them + time is different on each map. Can this cause problems though?
*/

void WorldSession::HandleMoveWorldportAckOpcode(WorldPacket & /*recvData*/)
{
   // TC_LOG_DEBUG("network", "WORLD: got MSG_MOVE_WORLDPORT_ACK.");
    HandleMoveWorldportAck();
}

void WorldSession::HandleMoveWorldportAck()
{
    Player* player = GetPlayer();
    // ignore unexpected far teleports
    if (!player->IsBeingTeleportedFar())
        return;

    player->SetSemaphoreTeleportFar(false);

    // get the teleport destination
    WorldLocation &loc = player->GetTeleportDest();

    // possible errors in the coordinate validity check
    if(!MapManager::IsValidMapCoord(loc.m_mapId,loc.m_positionX,loc.m_positionY,loc.m_positionZ,loc.m_orientation))
    {
        LogoutPlayer(false);
        return;
    }

    // get the destination map entry, not the current one, this will fix homebind and reset greeting
    MapEntry const* mEntry = sMapStore.LookupEntry(loc.m_mapId);
    InstanceTemplate const* mInstance = sObjectMgr->GetInstanceTemplate(loc.m_mapId);

    // reset instance validity, except if going to an instance inside an instance
    if(player->m_InstanceValid == false && !mInstance)
        player->m_InstanceValid = true;

    Map* oldMap = player->GetMap();
    Map* newMap = sMapMgr->CreateMap(loc.GetMapId(), GetPlayer());
    player->SetTeleportingToTest(0);

    if (player->IsInWorld())
    {
        TC_LOG_ERROR("network", "%s %s is still in world when teleported from map %s (%u) to new map %s (%u)", ObjectGuid(player->GetGUID()).ToString().c_str(), GetPlayer()->GetName().c_str(), oldMap->GetMapName(), oldMap->GetId(), newMap ? newMap->GetMapName() : "Unknown", loc.GetMapId());
        oldMap->RemovePlayerFromMap(player, false);
    }

    // relocate the player to the teleport destination
    // the CannotEnter checks are done in TeleporTo but conditions may change
    // while the player is in transit, for example the map may get full
    if (!newMap || newMap->CannotEnter(player))
    {
        TC_LOG_ERROR("network", "Map %d (%s) could not be created for player %d (%s), porting player to homebind", loc.GetMapId(), newMap ? newMap->GetMapName() : "Unknown", ObjectGuid(player->GetGUID()).GetCounter(), player->GetName().c_str());
        player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ, player->GetOrientation());
        return;
    }

    float z = loc.GetPositionZ() + player->GetHoverOffset();
    player->Relocate(loc.m_positionX, loc.m_positionY, z, loc.m_orientation);
    player->SetFallInformation(0, player->GetPositionZ());

    player->ResetMap();
    player->SetMap(newMap);

    // check this before Map::Add(player), because that will create the instance save!
    bool reset_notify = (player->GetBoundInstance(player->GetMapId(), player->GetDifficulty()) == NULL);

    player->SendInitialPacketsBeforeAddToMap();
    // the CanEnter checks are done in TeleportTo but conditions may change
    // while the player is in transit, for example the map may get full
    if(!player->GetMap()->AddPlayerToMap(player))
    {
        TC_LOG_ERROR("network", "WORLD: failed to teleport player %s (%d) to map %d (%s) because of unknown reason!",
            player->GetName().c_str(), ObjectGuid(player->GetGUID()).GetCounter(), loc.GetMapId(), newMap ? newMap->GetMapName() : "Unknown");
        player->ResetMap();
        player->SetMap(oldMap);

        // teleport the player home
        if(!player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ, player->GetOrientation()))
        {
            // the player must always be able to teleport home
            TC_LOG_ERROR("network","WORLD: failed to teleport player %s (%d) to homebind location %d,%f,%f,%f,%f!", player->GetName().c_str(), player->GetGUID().GetCounter(), player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ, player->GetOrientation());
            DEBUG_ASSERT(false);
        }
        return;
    }

    //this will set player's team ... so IT MUST BE CALLED BEFORE SendInitialPacketsAfterAddToMap()
    // battleground state prepare (in case join to BG), at relogin/tele player not invited
    // only add to bg group and object, if the player was invited (else he entered through command)
    if(player->InBattleground())
    {
        // cleanup seting if outdated
        if(!mEntry->IsBattlegroundOrArena())
        {
            // Do next only if found in battleground
            player->SetBattlegroundId(0, BATTLEGROUND_TYPE_NONE);  // We're not in BG.
            // reset destination bg team
            player->SetBGTeam(0);
        }
        // join to bg case
        else if(Battleground *bg = player->GetBattleground())
        {
            if(player->IsInvitedForBattlegroundInstance(player->GetBattlegroundId()))
                bg->AddPlayer(player);

            if (bg->isSpectator(player->GetGUID()))
                bg->onAddSpectator(player);
        }
    }

    player->SendInitialPacketsAfterAddToMap();

    // flight fast teleport case
    if (player->IsInFlight())
    {
        if(!player->InBattleground())
        {
            // short preparations to continue flight
            FlightPathMovementGenerator* flight = (FlightPathMovementGenerator*)(player->GetMotionMaster()->GetCurrentMovementGenerator());
            flight->Initialize(player);
            return;
        }

        // battleground state prepare, stop flight
        player->FinishTaxiFlight();
    }

    if (!player->IsAlive() && player->GetTeleportOptions() & TELE_REVIVE_AT_TELEPORT)
        player->ResurrectPlayer(0.5f);

    // resurrect character at enter into instance where his corpse exist after add to map
    if (mEntry->IsDungeon() && !player->IsAlive())
    {
        if (player->GetCorpseLocation().GetMapId() == mEntry->MapID)
        {
            player->ResurrectPlayer(0.5f);
            player->SpawnCorpseBones();
            player->SaveToDB();
        }
    }

    if(mInstance)
    {
        if(reset_notify && mEntry->IsRaid())
        {
#ifdef LICH_KING
            FIXME; //LK has this message for dungeon as well
#else
            uint32 timeleft = sInstanceSaveMgr->GetResetTimeFor(player->GetMapId(), RAID_DIFFICULTY_NORMAL) - time(NULL);
            player->SendInstanceResetWarning(player->GetMapId(), timeleft); // greeting at the entrance of the resort raid instance
#endif
        }

        // check if instance is valid
        if (!player->CheckInstanceValidity(false))
            player->m_InstanceValid = false;
    }

    // mount allow check
    if(!mEntry->IsMountAllowed())
        player->RemoveAurasByType(SPELL_AURA_MOUNTED);

    // update zone immediately, otherwise leave channel will cause crash in mtmap
    uint32 newzone, newarea;
    player->GetZoneAndAreaId(newzone, newarea);
    player->UpdateZone(newzone, newarea);

    // honorless target
    if(player->pvpInfo.IsHostile)
        player->CastSpell(player, 2479, true);
    // in friendly area
    else if (player->IsPvP() && !player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
        player->UpdatePvP(false, false);

    // resummon pet
    player->ResummonPetTemporaryUnSummonedIfAny();

    //lets process all delayed operations on successful teleport
    player->ProcessDelayedOperations();
}

void WorldSession::HandleMoveTeleportAck(WorldPacket& recvData)
{
    //TC_LOG_DEBUG("network", "MSG_MOVE_TELEPORT_ACK");
    /* extract packet */
    ObjectGuid guid;
#ifdef LICH_KING
    recvData >> guid.ReadAsPacked();
#else
    recvData >> guid;
#endif

    // make sure this client is allowed to control the unit which guid is provided
    if (!_player->IsInClientControlSet(guid))
    {
        recvData.rfinish();                   // prevent warnings spam
        TC_LOG_ERROR("entities.unit", "WorldSession::HandleMoveTeleportAck: The client doesn't have the permission to move this unit!");
        return;
    }

    Unit* mover = ObjectAccessor::GetUnit(*_player, guid);

    uint32 movementCounter, time;
    recvData >> movementCounter >> time;
    //TC_LOG_DEBUG("network", "Guid " UI64FMTD, guid);
    //TC_LOG_DEBUG("network", "Flags %u, time %u", flags, time/IN_MILLISECONDS);

    // verify that indeed the client is replying with the changes that were send to him
    if (!mover->HasPendingMovementChange(recvData.GetOpcode()))
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleMoveTeleportAck: Player %s from account id %u kicked because no movement change ack was expected from this player",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }
    PlayerMovementPendingChange pendingChange = mover->PopPendingMovementChange();
    if (pendingChange.movementCounter != movementCounter || pendingChange.movementChangeType != TELEPORT)
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleMoveTeleportAck: Player %s from account id %u kicked for incorrect data returned in an ack",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    Player* plMover = _player->m_unitMovedByMe->ToPlayer();

    if (!plMover || !plMover->IsBeingTeleportedNear())
        return;

    plMover->SetSemaphoreTeleportNear(false);

    WorldLocation const& dest = plMover->GetTeleportDest();
    // now that it has been acknowledge, we can inform the observers of that teleport
    MovementInfo movementInfo = plMover->GetMovementInfo();
    movementInfo.pos.Relocate(dest);
    if (TransportBase* transportBase = plMover->GetDirectTransport())
    {
        float x, y, z, o;
        dest.GetPosition(x, y, z, o);
        transportBase->CalculatePassengerOffset(x, y, z, &o);
        movementInfo.transport.pos.Relocate(x, y, z, o);
    }
    MovementPacketSender::SendTeleportPacket(plMover, movementInfo);
    uint32 old_zone = plMover->GetZoneId();

    plMover->UpdatePosition(dest, true);
    plMover->SetFallInformation(0, GetPlayer()->GetPositionZ());

    uint32 newzone, newarea;
    plMover->GetZoneAndAreaId(newzone, newarea);
    plMover->UpdateZone(newzone, newarea);

    // new zone
    if (old_zone != newzone)
    {
        // honorless target
        if (plMover->pvpInfo.IsHostile)
            plMover->CastSpell(plMover, 2479, true);

        // in friendly area
        else if (plMover->IsPvP() && !plMover->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
            plMover->UpdatePvP(false, false);
    }

    // teleport pets if they are not unsummoned
    if (Pet* pet = plMover->GetPet())
    {
        if (!pet->IsWithinDist3d(plMover, plMover->GetMap()->GetVisibilityRange() - 5.0f))
            pet->NearTeleportTo(plMover->GetPositionX(), plMover->GetPositionY(), plMover->GetPositionZ(), pet->GetOrientation());
    }

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    //lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();

    plMover->GetMotionMaster()->ReinitializeMovement();
}

/*
These packets are sent by the client in order to transmit the movement of the character currently controlled by the client.
This character is usually the player but it can be a creature (in case of a possess (eg priest MC)) or a vehicle. Later in this handler explaination,
'player' will be used when 'player controlled unit' should have been used.
The server then retransmits these packets to the other clients, which will extrapolate the unit's motion.
All the server has to do with all these packets is:
1) validate & update the data received: position, orientation, fall data and movement flags (this list should be exhaustive. please update if there is something missing).
2) transmit this packet to the other players nearby
Handles:
MSG_MOVE_START_FORWARD
MSG_MOVE_START_BACKWARD
MSG_MOVE_STOP
MSG_MOVE_START_STRAFE_LEFT
MSG_MOVE_START_STRAFE_RIGHT
MSG_MOVE_STOP_STRAFE
MSG_MOVE_JUMP
MSG_MOVE_START_TURN_LEFT
MSG_MOVE_START_TURN_RIGHT
MSG_MOVE_STOP_TURN
MSG_MOVE_START_PITCH_UP
MSG_MOVE_START_PITCH_DOWN
MSG_MOVE_STOP_PITCH
MSG_MOVE_SET_RUN_MODE
MSG_MOVE_SET_WALK_MODE
MSG_MOVE_FALL_LAND
MSG_MOVE_START_SWIM
MSG_MOVE_STOP_SWIM
MSG_MOVE_SET_FACING
MSG_MOVE_SET_PITCH
MSG_MOVE_HEARTBEAT -- packet sent every 0.5 s when the player is moving.
MSG_MOVE_START_ASCEND
MSG_MOVE_STOP_ASCEND
MSG_MOVE_START_DESCEND
CMSG_MOVE_FALL_RESET -- the player has encounter an object while failing, thus modifing the trajectory of his fall. this packet gives info regarding the new trajectory. !!!! @todo: needs to be processed in a different handler and this opcode shouldn'nt be sent to other clients !!!!
CMSG_MOVE_SET_FLY -- the player has started or stopped to fly (toggle effect). !!!! @todo: needs to be processed in a different handler and this opcode shouldn'nt be sent to other clients !!!!
CMSG_MOVE_CHNG_TRANSPORT !!!! @todo: needs to be processed in a different handler and this opcode shouldn'nt be sent to other clients !!!!
*/
void WorldSession::HandleMovementOpcodes(WorldPacket& recvData)
{
    uint16 opcode = recvData.GetOpcode();
    /* extract packet */
    MovementInfo movementInfo;
#ifdef LICH_KING
    movementInfo.FillContentFromPacket(&recvData, true);
    recvData.rfinish();                         // prevent warnings spam

    // make sure this client is allowed to control the unit which guid is provided
    if (!_player->IsInClientControlSet(movementInfo.guid))
    {
        recvData.rfinish();                   // prevent warnings spam
        TC_LOG_ERROR("entities.unit", "WorldSession::HandleMovementOpcodes: The client doesn't have the permission to move this unit!");
        return;
    }

    Unit* mover = ObjectAccessor::GetUnit(*_player, movementInfo.guid);
#else
    movementInfo.FillContentFromPacket(&recvData, false);
    recvData.rfinish();                         // prevent warnings spam
    Unit* mover = _player->m_unitMovedByMe;
#endif

    Player* plrMover = mover->ToPlayer(); // The unit we're currently moving

    // ignore movement packets if the player is getting far teleported (change of map). keep processing movement packets when the unit is only doing a near teleport.
    if (plrMover && plrMover->IsBeingTeleportedFar())
    {
        recvData.rfinish();                     // prevent warnings spam
        return;
    }


    if (!movementInfo.pos.IsPositionValid())
        return;

    /* validate new movement packet */
    mover->ValidateMovementInfo(&movementInfo);

    /* handle special cases */
    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_ONTRANSPORT)) // @todo LK: move this stuff. CMSG_MOVE_CHNG_TRANSPORT should be handled elsewhere than here.
    {
        // We were teleported, skip packets that were broadcast before teleport
        if (movementInfo.pos.GetExactDist2d(mover) > SIZE_OF_GRIDS)
        {
            recvData.rfinish();                 // prevent warnings spam
            return;
        }

#ifdef LICH_KING
        // T_POS ON VEHICLES!
        if (mover->GetVehicle())
            movementInfo.transport.pos = mover->m_movementInfo.transport.pos;
#endif

        // transports size limited
        // (also received at zeppelin leave by some reason with t_* as absolute in continent coordinates, can be safely skipped)
        if (fabs(movementInfo.transport.pos.GetPositionX()) > 75.0f || fabs(movementInfo.transport.pos.GetPositionY()) > 75.0f || fabs(movementInfo.transport.pos.GetPositionZ()) > 75.0f )
        {
            recvData.rfinish();                   // prevent warnings spam
            return;
        }

        if (!Trinity::IsValidMapCoord(movementInfo.pos.GetPositionX() + movementInfo.transport.pos.GetPositionX(), movementInfo.pos.GetPositionY() + movementInfo.transport.pos.GetPositionY(),
            movementInfo.pos.GetPositionZ() + movementInfo.transport.pos.GetPositionZ(), movementInfo.pos.GetOrientation() + movementInfo.transport.pos.GetOrientation()))
        {
            recvData.rfinish();                 // prevent warnings spam
            return;
        }

        // if we boarded a transport, add us to it
        if (plrMover)
        {
            if (!plrMover->GetTransport())
            {
                if (Transport* transport = plrMover->GetMap()->GetTransport(movementInfo.transport.guid))
                {
                    plrMover->m_transport = transport;
                    transport->AddPassenger(plrMover);
                }
            }
            else if (plrMover->GetTransport()->GetGUID() != movementInfo.transport.guid)
            {
                bool foundNewTransport = false;
                plrMover->m_transport->RemovePassenger(plrMover);
                if (Transport* transport = plrMover->GetMap()->GetTransport(movementInfo.transport.guid))
                {
                    foundNewTransport = true;
                    plrMover->m_transport = transport;
                    transport->AddPassenger(plrMover);
                }

                if (!foundNewTransport)
                {
                    plrMover->m_transport = NULL;
                    movementInfo.transport.Reset();
                }
            }
        }

        if (!mover->GetTransport()
#ifdef LICH_KING
            && !mover->GetVehicle()
#endif
            )
            movementInfo.flags &= ~MOVEMENTFLAG_ONTRANSPORT;
    }
    else if (plrMover && plrMover->GetTransport())                // if we were on a transport, leave
    {
        plrMover->m_transport->RemovePassenger(plrMover);
        plrMover->m_transport = nullptr;
        movementInfo.transport.Reset();
    }

    if (plrMover)
    {
        //sunstrider: Client also send SWIMMING while flying so we can't just update InWater when client stops sending it. A player swimming then flying upward will be still considered in water
        // To fix this: It seems the client does not set the PLAYER_FLYING flag while swimming. But I'm not 100% sure there is no case it could happen. If this is false and we should check for Map::IsUnderWater as well
        if (((movementInfo.flags & MOVEMENTFLAG_PLAYER_FLYING) != 0) && plrMover->IsInWater())
        {
            plrMover->SetInWater(false);
        }
        else if (((movementInfo.flags & MOVEMENTFLAG_SWIMMING) != 0) != plrMover->IsInWater())
        {
            // now client not include swimming flag in case jumping under water
            plrMover->SetInWater(!plrMover->IsInWater() || plrMover->GetBaseMap()->IsUnderWater(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY(), movementInfo.pos.GetPositionZ()));
        }
    }

    // sunwell: do not allow to move with UNIT_FLAG_REMOVE_CLIENT_CONTROL
    if (mover->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_REMOVE_CLIENT_CONTROL))
    {
        // skip moving packets
        if (movementInfo.HasMovementFlag(MOVEMENTFLAG_MASK_MOVING))
            return;
    }

    anticheat->OnPlayerMoved(mover, movementInfo, OpcodeClient(opcode));

    /* process position-change */
    mover->UpdateMovementInfo(movementInfo);

    // as strange as it may be, retail servers actually use MSG_MOVE_START_SWIM_CHEAT & MSG_MOVE_STOP_SWIM_CHEAT to respectively set and unset the 'Flying' movement flag. 
    // The only thing left to do is to move the handling of CMSG_MOVE_SET_FLY into a different handler
    if (opcode == CMSG_MOVE_SET_FLY)
        opcode = movementInfo.HasMovementFlag(MOVEMENTFLAG_JUMPING_OR_FALLING) ? MSG_MOVE_START_SWIM_CHEAT : MSG_MOVE_STOP_SWIM_CHEAT;

    WorldPacket data(opcode, recvData.size());
    mover->GetMovementInfo().WriteContentIntoPacket(&data, true);  //this contains the server time, not the time provided by client
    mover->SendMessageToSet(&data, _player);

#ifdef LICH_KING
    // this is almost never true (sunwell: only one packet when entering vehicle), normally use mover->IsVehicle()
    if (mover->GetVehicle())
    {
        mover->SetOrientation(movementInfo.pos.GetOrientation());
        mover->UpdatePosition(movementInfo.pos);
        return;
    }
#endif

    // sunwell: previously always mover->UpdatePosition(movementInfo.pos); mover->UpdatePosition(movementInfo.pos); // unsure if this can be safely deleted since it is also called in "mover->UpdateMovementInfo(movementInfo)" but the above if blocks may influence the unit's orintation
    if (movementInfo.flags & MOVEMENTFLAG_ONTRANSPORT && mover->GetTransport())
    {
        float x, y, z, o;
        movementInfo.transport.pos.GetPosition(x, y, z, o);
        mover->GetTransport()->CalculatePassengerPosition(x, y, z, &o);
        mover->UpdatePosition(x, y, z, o);
    }
    else
        mover->UpdatePosition(movementInfo.pos);

    if (!mover->IsStandState() && (movementInfo.flags & (MOVEMENTFLAG_MASK_MOVING | MOVEMENTFLAG_MASK_TURNING)))
        mover->SetStandState(UNIT_STAND_STATE_STAND);

    // fall damage generation (ignore in flight case that can be triggered also at lags in moment teleportation to another map).
    if (opcode == MSG_MOVE_FALL_LAND && plrMover && !plrMover->IsInFlight() && (!plrMover->GetTransport() || plrMover->GetTransport()->IsStaticTransport()))
        plrMover->HandleFall(movementInfo);

#ifdef LICH_KING
    //  interrupt parachutes upon falling or landing in water
    if (opcode == MSG_MOVE_FALL_LAND || opcode == MSG_MOVE_START_SWIM)
        mover->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_LANDING); // Parachutes
#endif

    if (plrMover && plrMover->GetMap()) // Nothing is charmed, or player charmed
    {
        plrMover->UpdateFallInformationIfNeed(movementInfo, opcode);

        // Used to handle spell interrupts on move (client does not always does it by itself)
        if (plrMover->isMoving())
            plrMover->SetHasMovedInUpdate(true);

        // Anti Undermap
        float minHeight = plrMover->GetMap()->GetMinHeight(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY());
        if (movementInfo.pos.GetPositionZ() < minHeight)
        {
            // Is there any ground existing above min height? If there is a ground at this position, we very probably fell undermap, try to recover player.
            // Else, just kill him
            float gridMapHeight = plrMover->GetMap()->GetGridMapHeight(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY());
            if(gridMapHeight > minHeight)
                plrMover->UndermapRecall(); // Port player back to last safe position
            else
            {
                plrMover->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_IS_OUT_OF_BOUNDS);
                if (plrMover->IsAlive()) // Still alive while falling
                {
                    plrMover->EnvironmentalDamage(DAMAGE_FALL_TO_VOID, plrMover->GetHealth());
                    // Change the death state to CORPSE to prevent the death timer from
                    // Starting in the next player update
                    plrMover->KillPlayer();
                }
            }
        }
        else if (plrMover->CanFreeMove() && !movementInfo.HasMovementFlag(MOVEMENTFLAG_JUMPING_OR_FALLING)) // If player is able to move and not falling or jumping..
        {
            plrMover->SaveSafePosition(movementInfo.pos); // Save current position for UndermapRecall()
        }
    }
}

/*
CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK
CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK
CMSG_FORCE_RUN_SPEED_CHANGE_ACK
CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK
CMSG_FORCE_SWIM_SPEED_CHANGE_ACK
CMSG_FORCE_WALK_SPEED_CHANGE_ACK
CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK
CMSG_FORCE_TURN_RATE_CHANGE_ACK
*/
void WorldSession::HandleForceSpeedChangeAck(WorldPacket &recvData)
{
    uint32 opcode = recvData.GetOpcode();
   // TC_LOG_DEBUG("network", "WORLD: Recvd %s (%u, 0x%X) opcode", GetOpcodeNameForLogging(static_cast<OpcodeClient>(opcode)).c_str(), opcode, opcode);

    /* extract packet */
    ObjectGuid guid;

#ifdef LICH_KING
    recvData >> guid.ReadAsPacked();
#else
    recvData >> guid;
#endif

    /*See top file comment
    if (!_player->IsInClientControlSet(guid))
    {
        recvData.rfinish();                   // prevent warnings spam
        return;
    }
    */

    Unit* mover = ObjectAccessor::GetUnit(*_player, guid);

    UnitMoveType move_type;
    switch (recvData.GetOpcode())
    {
        case CMSG_FORCE_WALK_SPEED_CHANGE_ACK:          move_type = MOVE_WALK;          break;
        case CMSG_FORCE_RUN_SPEED_CHANGE_ACK:           move_type = MOVE_RUN;           break;
        case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:      move_type = MOVE_RUN_BACK;      break;
        case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:          move_type = MOVE_SWIM;          break;
        case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:     move_type = MOVE_SWIM_BACK;     break;
        case CMSG_FORCE_TURN_RATE_CHANGE_ACK:           move_type = MOVE_TURN_RATE;     break;
        case CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK:        move_type = MOVE_FLIGHT;        break;
        case CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK:   move_type = MOVE_FLIGHT_BACK;   break;
#ifdef LICH_KING
        case CMSG_FORCE_PITCH_RATE_CHANGE_ACK:          move_type = MOVE_PITCH_RATE;    break;
#endif
        default:
            TC_LOG_ERROR("network", "WorldSession::HandleForceSpeedChangeAck: Unknown move type opcode: %s", GetOpcodeNameForLogging(static_cast<OpcodeClient>(recvData.GetOpcode())).c_str());
            return;
    }

    uint32 movementCounter;
    float  speedReceived;

    MovementInfo movementInfo;
    movementInfo.guid = guid;

    recvData >> movementCounter;
    movementInfo.FillContentFromPacket(&recvData, false);
    recvData >> speedReceived;

    // verify that indeed the client is replying with the changes that were send to him
    if (!mover->HasPendingMovementChange(recvData.GetOpcode()))
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleForceSpeedChangeAck: Player %s from account id %u kicked because no movement change ack was expected from this player",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();

        return;
    }

    PlayerMovementPendingChange pendingChange = mover->PopPendingMovementChange();
    float speedSent = pendingChange.newValue;
    MovementChangeType changeType = pendingChange.movementChangeType;
    UnitMoveType moveTypeSent;

    switch (changeType)
    {
    case SPEED_CHANGE_WALK:                 moveTypeSent = MOVE_WALK; break;
    case SPEED_CHANGE_RUN:                  moveTypeSent = MOVE_RUN; break;
    case SPEED_CHANGE_RUN_BACK:             moveTypeSent = MOVE_RUN_BACK; break;
    case SPEED_CHANGE_SWIM:                 moveTypeSent = MOVE_SWIM; break;
    case SPEED_CHANGE_SWIM_BACK:            moveTypeSent = MOVE_SWIM_BACK; break;
    case RATE_CHANGE_TURN:                  moveTypeSent = MOVE_TURN_RATE; break;
    case SPEED_CHANGE_FLIGHT_SPEED:         moveTypeSent = MOVE_FLIGHT; break;
    case SPEED_CHANGE_FLIGHT_BACK_SPEED:    moveTypeSent = MOVE_FLIGHT_BACK; break;
#ifdef LICH_KING
    case RATE_CHANGE_PITCH:                 moveTypeSent = MOVE_PITCH_RATE; break;
#endif
    default:
        TC_LOG_INFO("cheat", "WorldSession::HandleForceSpeedChangeAck: Player %s from account id %u kicked for incorrect data returned in an ack",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    if (pendingChange.movementCounter != movementCounter || std::fabs(speedSent - speedReceived) > 0.01f || moveTypeSent != move_type)
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleForceSpeedChangeAck: Player %s from account id %u kicked for incorrect data returned in an ack",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    /* the client data has been verified. let's do the actual change now */
    float newSpeedRate = speedSent / baseMoveSpeed[move_type];
    mover->UpdateMovementInfo(movementInfo);
    mover->SetSpeedRateReal(move_type, newSpeedRate);
    MovementPacketSender::SendSpeedChangeToObservers(mover, move_type, speedSent);
}

#ifdef LICH_KING
void WorldSession::HandleCollisionHeightChangeAck(WorldPacket &recvData)
{
    /* extract packet */
    ObjectGuid guid;
    uint32 movementCounter;
    MovementInfo movementInfo;
    float  heightReceived;

    recvData >> guid;
    movementInfo.guid = guid;
    recvData >> movementCounter;
    movementInfo.FillContentFromPacket(&recvData, false);
    recvData >> heightReceived;

    /*See top file comment
    // make sure this client is allowed to control the unit which guid is provided
    if (!_player->IsInClientControlSet(guid))
    {
        recvData.rfinish();                   // prevent warnings spam
        TC_LOG_ERROR("entities.unit", "WorldSession::HandleCollisionHeightChangeAck: The client doesn't have the permission to move this unit!");
        return;
    }*/

    Unit* mover = ObjectAccessor::GetUnit(*_player, guid);

    // verify that indeed the client is replying with the changes that were send to him
    if (!mover->HasPendingMovementChange(recvData.GetOpcode()))
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleCollisionHeightChangeAck: Player %s from account id %u kicked because no movement change ack was expected from this player",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    PlayerMovementPendingChange pendingChange = mover->PopPendingMovementChange();
    float heightSent = pendingChange.newValue;
    MovementChangeType changeType = pendingChange.movementChangeType;

    if (pendingChange.movementCounter != movementCounter || changeType != SET_COLLISION_HGT || std::fabs(heightSent - heightReceived) > 0.01f)
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleCollisionHeightChangeAck: Player %s from account id %u kicked for incorrect data returned in an ack",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    mover->ValidateMovementInfo(&movementInfo);
    /* the client data has been verified. let's do the actual change now */
    mover->UpdateMovementInfo(movementInfo);
    mover->SetCollisionHeightReal(heightSent);
    MovementPacketSender::SendHeightChangeToObservers(mover, heightSent);
}
#endif

// sent by client when gaining control of a unit
void WorldSession::HandleSetActiveMoverOpcode(WorldPacket &recvData)
{
    //TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_SET_ACTIVE_MOVER");

    //LK OK
    ObjectGuid guid;
    recvData >> guid; //Client started controlling this unit

    _player->InsertIntoClientControlSet(guid);
    _player->m_pendingNewAllowedMover = false;
 }

//CMSG_MOVE_NOT_ACTIVE_MOVER
//sent by client when loosing control of a unit
void WorldSession::HandleMoveNotActiveMover(WorldPacket &recvData)
{
    TC_LOG_DEBUG("network", "WORLD: Recvd CMSG_MOVE_NOT_ACTIVE_MOVER");

    MovementInfo movementInfo;
    movementInfo.FillContentFromPacket(&recvData, true);

    // make sure this client is allowed to control the unit which guid is provided
    if (!_player->IsInClientControlSet(movementInfo.guid))
    {
        recvData.rfinish();                   // prevent warnings spam
        TC_LOG_ERROR("entities.unit", "WorldSession::HandleMoveNotActiveMover: The client doesn't have the permission to move this unit!");
        return;
    }

    Unit* mover = ObjectAccessor::GetUnit(*_player, movementInfo.guid);

    mover->ValidateMovementInfo(&movementInfo);
    mover->UpdateMovementInfo(movementInfo);

    if (!_player->m_pendingNewAllowedMover)
        _player->RemoveFromClientControlSet(movementInfo.guid);

    _player->m_pendingNewAllowedMover = false;
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPacket& /*recvData*/)
{
    WorldPacket data(SMSG_MOUNTSPECIAL_ANIM, 8);
    data << uint64(GetPlayer()->GetGUID());

    GetPlayer()->SendMessageToSet(&data, false);
}

// CMSG_MOVE_KNOCK_BACK_ACK
void WorldSession::HandleMoveKnockBackAck(WorldPacket& recvData)
{
    /* extract packet */
    ObjectGuid guid;
    uint32 movementCounter;
    MovementInfo movementInfo;

#ifdef LICH_KING
    recvData >> guid.ReadAsPacked();
#else
    recvData >> guid;
#endif
    movementInfo.guid = guid;
    recvData >> movementCounter;
    movementInfo.FillContentFromPacket(&recvData, false);

    /*See top file comment
    // make sure this client is allowed to control the unit which guid is provided
    if (!_player->IsInClientControlSet(guid))
    {
        recvData.rfinish();                   // prevent warnings spam
        TC_LOG_ERROR("entities.unit", "WorldSession::HandleMoveKnockBackAck: The client doesn't have the permission to move this unit!");
        return;
    }*/

    Unit* mover = ObjectAccessor::GetUnit(*_player, guid);

    // verify that indeed the client is replying with the changes that were send to him
    if (!mover->HasPendingMovementChange(recvData.GetOpcode()))
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleMoveKnockBackAck: Player %s from account id %u kicked because no movement change ack was expected from this player",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    PlayerMovementPendingChange pendingChange = mover->PopPendingMovementChange();
    if (pendingChange.movementCounter != movementCounter || pendingChange.movementChangeType != KNOCK_BACK
        || std::fabs(pendingChange.knockbackInfo.speedXY - movementInfo.jump.xyspeed) > 0.01f
        || std::fabs(pendingChange.knockbackInfo.speedZ - movementInfo.jump.zspeed) > 0.01f
        || std::fabs(pendingChange.knockbackInfo.vcos - movementInfo.jump.cosAngle) > 0.01f
        || std::fabs(pendingChange.knockbackInfo.vsin - movementInfo.jump.sinAngle) > 0.01f)
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleMoveKnockBackAck: Player %s from account id %u kicked for incorrect data returned in an ack",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    // knocking a player removes the CanFly flag (the client reacts the same way).
    mover->SetFlyingReal(false);

    mover->ValidateMovementInfo(&movementInfo);
    /* the client data has been verified. let's do the actual change now */
    mover->UpdateMovementInfo(movementInfo);
    MovementPacketSender::SendKnockBackToObservers(mover, movementInfo.jump.cosAngle, movementInfo.jump.sinAngle, movementInfo.jump.xyspeed, movementInfo.jump.zspeed);

    anticheat->OnPlayerKnockBack(_player);
}
/*
handles those packets:

APPLY:

CMSG_FORCE_MOVE_ROOT_ACK
CMSG_MOVE_GRAVITY_DISABLE_ACK

UNAPPLY:

CMSG_FORCE_MOVE_UNROOT_ACK
CMSG_MOVE_GRAVITY_ENABLE_ACK
*/
void WorldSession::HandleMovementFlagChangeAck(WorldPacket& recvData)
{
    /* extract packet */
    ObjectGuid guid;
    uint32 movementCounter;
    MovementInfo movementInfo;

#ifdef LICH_KING
    recvData >> guid.ReadAsPacked();
#else
    recvData >> guid;
#endif
    movementInfo.guid = guid;
    recvData >> movementCounter;
    movementInfo.FillContentFromPacket(&recvData);

    /*See top file comment
    // make sure this client is allowed to control the unit which guid is provided
    if (!_player->IsInClientControlSet(guid))
    {
        recvData.rfinish();                   // prevent warnings spam
        TC_LOG_ERROR("entities.unit", "WorldSession::HandleMovementFlagChangeAck: The client doesn't have the permission to move this unit!");
        return;
    }*/

    Unit* mover = ObjectAccessor::GetUnit(*_player, guid);

    // verify that indeed the client is replying with the changes that were send to him
    if (!mover->HasPendingMovementChange(recvData.GetOpcode()))
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleMovementFlagChangeAck: Player %s from account id %u kicked because no movement change ack was expected from this player",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    PlayerMovementPendingChange pendingChange = mover->PopPendingMovementChange();
    bool applySent = pendingChange.apply;
    MovementChangeType changeTypeSent = pendingChange.movementChangeType;

    MovementFlags mFlag;
    MovementChangeType changeTypeReceived;
    bool applyReceived;
    switch (recvData.GetOpcode())
    {
    case CMSG_FORCE_MOVE_ROOT_ACK:      changeTypeReceived = ROOT; applyReceived = true; mFlag = MOVEMENTFLAG_ROOT; break;
    case CMSG_FORCE_MOVE_UNROOT_ACK:    changeTypeReceived = ROOT; applyReceived = false; mFlag = MOVEMENTFLAG_ROOT; break;
#ifdef LICH_KING
    case CMSG_MOVE_GRAVITY_DISABLE_ACK: changeTypeReceived = GRAVITY_DISABLE; applyReceived = true; mFlag = MOVEMENTFLAG_DISABLE_GRAVITY; break;
    case CMSG_MOVE_GRAVITY_ENABLE_ACK:  changeTypeReceived = GRAVITY_DISABLE; applyReceived = false; mFlag = MOVEMENTFLAG_DISABLE_GRAVITY; break;
#endif
    default:
        TC_LOG_ERROR("network", "WorldSession::HandleMovementFlagChangeAck: Unknown move type opcode: %s", GetOpcodeNameForLogging(static_cast<OpcodeClient>(recvData.GetOpcode())).c_str());
        return;
    }

    if (pendingChange.movementCounter != movementCounter
        || applySent != applyReceived
        || changeTypeSent != changeTypeReceived)
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleMovementFlagChangeAck: Player %s from account id %u kicked for incorrect data returned in an ack",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    switch (changeTypeReceived)
    {
    case ROOT: mover->SetRootedReal(applyReceived); break;
    case GRAVITY_DISABLE: mover->SetDisableGravityReal(applyReceived); break;
    default:
        TC_LOG_ERROR("network", "WorldSession::HandleMovementFlagChangeAck: Unknown move type opcode: %s", GetOpcodeNameForLogging(static_cast<OpcodeClient>(recvData.GetOpcode())).c_str());
        return;
    }

    mover->ValidateMovementInfo(&movementInfo);
    mover->UpdateMovementInfo(movementInfo);
    MovementPacketSender::SendMovementFlagChangeToObservers(mover, mFlag, applySent);
}

/*
handles those packets:

CMSG_MOVE_WATER_WALK_ACK
CMSG_MOVE_HOVER_ACK
CMSG_MOVE_SET_CAN_FLY_ACK
CMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY_ACK
CMSG_MOVE_FEATHER_FALL_ACK
*/
void WorldSession::HandleMovementFlagChangeToggleAck(WorldPacket& recvData)
{
    /* extract packet */
    ObjectGuid guid;
    uint32 movementCounter;
    MovementInfo movementInfo;
    uint32 applyInt;
    bool applyReceived;

#ifdef LICH_KING
    recvData >> guid.ReadAsPacked();
#else
    recvData >> guid;
#endif
    movementInfo.guid = guid;
    recvData >> movementCounter;
    movementInfo.FillContentFromPacket(&recvData);
    recvData >> applyInt;
    applyReceived = applyInt == 0u ? false : true;

    // make sure this client is allowed to control the unit which guid is provided
    if (!_player->IsInClientControlSet(guid))
    {
        recvData.rfinish();                   // prevent warnings spam
        TC_LOG_ERROR("entities.unit", "WorldSession::HandleMovementFlagChangeToggleAck: The client doesn't have the permission to move this unit!");
        return;
    }

    Unit* mover = ObjectAccessor::GetUnit(*_player, guid);

    // verify that indeed the client is replying with the changes that were send to him
    if (!mover->HasPendingMovementChange(recvData.GetOpcode()))
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleMovementFlagChangeToggleAck: Player %s from account id %u kicked because no movement change ack was expected from this player",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    PlayerMovementPendingChange pendingChange = mover->PopPendingMovementChange();
    bool applySent = pendingChange.apply;
    MovementChangeType changeTypeSent = pendingChange.movementChangeType;

    MovementFlags mFlag = MOVEMENTFLAG_NONE;
    MovementFlags2 mFlag2 = MOVEMENTFLAG2_NONE;
    MovementChangeType changeTypeReceived;
    switch (recvData.GetOpcode())
    {
    case CMSG_MOVE_WATER_WALK_ACK:      changeTypeReceived = WATER_WALK; mFlag = MOVEMENTFLAG_WATERWALKING; break;
    case CMSG_MOVE_HOVER_ACK:           changeTypeReceived = SET_HOVER; mFlag = MOVEMENTFLAG_HOVER; break;
    case CMSG_MOVE_SET_CAN_FLY_ACK:     changeTypeReceived = SET_CAN_FLY; mFlag = MOVEMENTFLAG_CAN_FLY; break;
    case CMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY_ACK:
        /*TODO tbc: what to do with this one? 
        This will currently never be received since we never send the SMSG one.
        packets: SMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY 
                 SMSG_MOVE_UNSET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY
                 CMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY_ACK
        They do exists on TBC, but they're related toMOVEMENTFLAG2_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY not existing on TBC. 
        Were those packets ever used? Or did the flag2 moved to another place?
        */
        return;
#ifdef LICH_KING
        changeTypeReceived = SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY;
        mFlag2 = MOVEMENTFLAG2_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY; 
#endif
        break;
    case CMSG_MOVE_FEATHER_FALL_ACK:    changeTypeReceived = FEATHER_FALL; mFlag = MOVEMENTFLAG_FALLING_SLOW; break;
    default:
        TC_LOG_ERROR("network", "WorldSession::HandleMovementFlagChangeToggleAck: Unknown move type opcode: %s", GetOpcodeNameForLogging(static_cast<OpcodeClient>(recvData.GetOpcode())).c_str());
        return;
    }

    if (pendingChange.movementCounter != movementCounter
        || applySent != applyReceived
        || changeTypeSent != changeTypeReceived)
    {
        TC_LOG_INFO("cheat", "WorldSession::HandleMovementFlagChangeToggleAck: Player %s from account id %u kicked for incorrect data returned in an ack",
            _player->GetName().c_str(), _player->GetSession()->GetAccountId());
        _player->GetSession()->KickPlayer();
        return;
    }

    switch (changeTypeReceived)
    {
    case WATER_WALK:            mover->SetWaterWalkingReal(applyReceived); break;
    case SET_HOVER:             mover->SetHoverReal(applyReceived); break;
    case SET_CAN_FLY:           mover->SetFlyingReal(applyReceived); break;
#ifdef LICH_KING
    case SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY:
        mover->SetCanTransitionBetweenSwimAndFlyReal(applyReceived); break;
#endif
    case FEATHER_FALL:          mover->SetFeatherFallReal(applyReceived); break;
    default:
        TC_LOG_ERROR("network", "WorldSession::HandleMovementFlagChangeToggleAck: Unknown move type opcode: %s", GetOpcodeNameForLogging(static_cast<OpcodeClient>(recvData.GetOpcode())).c_str());
        return;
    }

    mover->ValidateMovementInfo(&movementInfo);
    mover->UpdateMovementInfo(movementInfo);
    if (mFlag != MOVEMENTFLAG_NONE)
        MovementPacketSender::SendMovementFlagChangeToObservers(mover, mFlag, applySent);
#ifdef LICH_KING
    else
        MovementPacketSender::SendMovementFlagChangeToObservers(mover, mFlag2);
#endif
}

void WorldSession::HandleMoveTimeSkippedOpcode(WorldPacket& recvData)
{
    /*  WorldSession::Update(getMSTime());*/
    TC_LOG_DEBUG("network", "WORLD: Received CMSG_MOVE_TIME_SKIPPED");

    ObjectGuid guid;

#ifdef LICH_KING
    recvData >> guid.ReadAsPacked();
#else
    recvData >> guid;
#endif
    recvData.read_skip<uint32>();
    /*
    uint64 guid;
    uint32 time_skipped;
    recvData >> guid;
    recvData >> time_skipped;
    TC_LOG_DEBUG("network", "WORLD: CMSG_MOVE_TIME_SKIPPED");

    //// @todo
    must be need use in Trinity
    We substract server Lags to move time (AntiLags)
    for exmaple
    GetPlayer()->ModifyLastMoveTime(-int32(time_skipped));
    */
}


/*
Handles CMSG_WORLD_TELEPORT. That packet is sent by the client when the user types a special build-in command restricted to GMs.
cf http://wow.gamepedia.com/Console_variables#GM_Commands
The usage of this packet is therefore restricted to GMs and will never be used by normal players.
*/
void WorldSession::HandleWorldTeleportOpcode(WorldPacket& recvData)
{
    uint32 time;
    uint32 mapid;
    float PositionX;
    float PositionY;
    float PositionZ;
    float Orientation;

    recvData >> time;                                      // time in m.sec.
    recvData >> mapid;
    recvData >> PositionX;
    recvData >> PositionY;
    recvData >> PositionZ;
    recvData >> Orientation;                               // o (3.141593 = 180 degrees)

    TC_LOG_DEBUG("network", "WORLD: Received CMSG_WORLD_TELEPORT");

    if (GetPlayer()->IsInFlight())
    {
        TC_LOG_DEBUG("network", "Player '%s' (GUID: %u) in flight, ignore worldport command.",
            GetPlayer()->GetName().c_str(), GetPlayer()->GetGUID().GetCounter());
        return;
    }

    TC_LOG_DEBUG("network", "CMSG_WORLD_TELEPORT: Player = %s, Time = %u, map = %u, x = %f, y = %f, z = %f, o = %f",
        GetPlayer()->GetName().c_str(), time, mapid, PositionX, PositionY, PositionZ, Orientation);

    if (GetSecurity() >= SEC_GAMEMASTER3)
    //if (HasPermission(rbac::RBAC_PERM_OPCODE_WORLD_TELEPORT))
        GetPlayer()->TeleportTo(mapid, PositionX, PositionY, PositionZ, Orientation);
    else
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
}

void WorldSession::HandleSummonResponseOpcode(WorldPacket& recvData)
{
    if (!_player->IsAlive() || _player->IsInCombat())
        return;

    ObjectGuid summoner_guid;
    bool agree;
    recvData >> summoner_guid;
    recvData >> agree;

    _player->SummonIfPossible(agree);
}
