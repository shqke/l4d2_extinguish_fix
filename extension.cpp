/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

ExtinguishFixExt g_ExtinguishFixExt;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_ExtinguishFixExt);

using namespace SourceHook;

SH_DECL_MANUALHOOK0_void(ExtinguishHook, 0, 0, 0);

int m_hEffectEntity = -1;
int m_nWaterLevel = -1;
int m_fFlags = -1;
int m_hEntAttached = -1;
int max_players = 0;

bool ExtinguishFixExt::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	sm_sendprop_info_t info;
	int offset;
	bool success = false;
	IGameConfig *gc;
	
	if (strcmp(g_pSM->GetGameFolderName(), "left4dead2")) return false;
	if (gamehelpers->FindSendPropInfo("CTerrorPlayer", "m_hEffectEntity", &info)) m_hEffectEntity = info.actual_offset;
	if (gamehelpers->FindSendPropInfo("CTerrorPlayer", "m_nWaterLevel", &info)) m_nWaterLevel = info.actual_offset;
	if (gamehelpers->FindSendPropInfo("CTerrorPlayer", "m_fFlags", &info)) m_fFlags = info.actual_offset;
	if (gamehelpers->FindSendPropInfo("CEntityFlame", "m_hEntAttached", &info)) m_hEntAttached = info.actual_offset;
	if (m_hEffectEntity < 0 || m_nWaterLevel < 0 || m_fFlags < 0 || m_hEntAttached < 0) {
		g_pSM->Format(error, maxlength, "Unable to retrieve SendProp offsets!");
		return false;
	}

	if (!gameconfs->LoadGameConfigFile("sdktools.games", &gc, error, maxlength)) return false;
	success = gc->GetOffset("Extinguish", &offset);
	gameconfs->CloseGameConfigFile(gc);
	if (!success) return false;

	SH_MANUALHOOK_RECONFIGURE(ExtinguishHook, offset, 0, 0);

	playerhelpers->AddClientListener(this);

	if (late && playerhelpers->IsServerActivated()) {
		OnServerActivated(playerhelpers->GetMaxClients());

		for (int i = max_players; i > 0; i--) {
			if (playerhelpers->GetGamePlayer(i)->IsInGame()) {
				OnClientPutInServer(i);
			}
		}
	}

	return true;
}

void ExtinguishFixExt::SDK_OnUnload()
{
	for (int i = max_players; i > 0; i--) {
		if (playerhelpers->GetGamePlayer(i)->IsInGame()) {
			OnClientDisconnected(i);
		}
	}

	playerhelpers->RemoveClientListener(this);
}

void ExtinguishFixExt::OnClientPutInServer(int client)
{
	SH_ADD_MANUALHOOK(ExtinguishHook, gamehelpers->ReferenceToEntity(client), SH_MEMBER(this, &ExtinguishFixExt::Extinguish), false);
}

void ExtinguishFixExt::OnClientDisconnected(int client)
{
	SH_REMOVE_MANUALHOOK(ExtinguishHook, gamehelpers->ReferenceToEntity(client), SH_MEMBER(this, &ExtinguishFixExt::Extinguish), false);
}

void ExtinguishFixExt::OnServerActivated(int max_clients)
{
	max_players = max_clients;
}

void ExtinguishFixExt::Extinguish()
{
	CBaseEntity *pClient = META_IFACEPTR(CBaseEntity);
	int& entFlags = *(int *)((char *)pClient + m_fFlags);
	CBaseHandle *efxEntity = ((CBaseHandle *)((char *)pClient + m_hEffectEntity));
	int level = *(char *)((char *)pClient + m_nWaterLevel);

	if (entFlags & FL_ONFIRE && !level && efxEntity) {
		edict_t *edictClient = gamehelpers->EdictOfIndex(gamehelpers->EntityToBCompatRef(pClient));
		edict_t *edictEfxEntity = gamehelpers->GetHandleEntity(*efxEntity);
		const char *name;

		entFlags &= ~FL_ONFIRE;
		gamehelpers->SetEdictStateChanged(edictClient, m_fFlags);

		if (edictEfxEntity && (name = gamehelpers->GetEntityClassname(edictEfxEntity)) && !strcmp(name, "entityflame")) {
			*(int *)((char *)efxEntity + m_hEntAttached) = 0;
		}

		*efxEntity = NULL;
		gamehelpers->SetEdictStateChanged(edictClient, m_hEffectEntity);

		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_HANDLED);
}
