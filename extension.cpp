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

ExtinguishFix g_ExtinguishFix;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_ExtinguishFix);

using namespace SourceHook;

IHookManagerAutoGen *g_HookMgr = NULL;

int m_hEffectEntity = -1;
int m_nWaterLevel = -1;
int m_fFlags = -1;
int m_hEntAttached = -1;
short max_players = 0;

bool ExtinguishFix::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	sm_sendprop_info_t info;
	CProtoInfoBuilder protoInfo(ProtoInfo::CallConv_ThisCall);
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

	hookMgrIFace = g_HookMgr->MakeHookMan(protoInfo, 0, offset);

	sharesys->AddDependency(myself, "bintools.ext", true, true);
	playerhelpers->AddClientListener(this);

	if (late && playerhelpers->IsServerActivated()) OnServerActivated(playerhelpers->GetMaxClients());

	return true;
}

void ExtinguishFix::SDK_OnUnload()
{
	playerhelpers->RemoveClientListener(this);

	if (hookMgrIFace) g_HookMgr->ReleaseHookMan(hookMgrIFace);
}

void ExtinguishFix::SDK_OnAllLoaded()
{
	for (int i = max_players; i > 0; i--) {
		if (playerhelpers->GetGamePlayer(i)->IsInGame()) {
			OnClientPutInServer(i);
		}
	}
}

bool ExtinguishFix::QueryRunning(char *error, size_t maxlength)
{
	return true;
}

bool ExtinguishFix::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	g_HookMgr = static_cast<IHookManagerAutoGen *>(ismm->MetaFactory(MMIFACE_SH_HOOKMANAUTOGEN, NULL, NULL));

	return g_HookMgr != NULL;
}

void ExtinguishFix::OnClientPutInServer(int client)
{
	g_SHPtr->AddHook(g_PLID, ISourceHook::Hook_Normal, gamehelpers->ReferenceToEntity(client), 0, hookMgrIFace, &handler, false);
}

void ExtinguishFix::OnClientDisconnected(int client)
{
	g_SHPtr->RemoveHook(g_PLID, gamehelpers->ReferenceToEntity(client), 0, hookMgrIFace, &handler, false);
}

void ExtinguishFix::OnServerActivated(int max_clients)
{
	max_players = max_clients;
}

void ExtinguishDelegate::Extinguish()
{
	CBaseEntity *pClient = META_IFACEPTR(CBaseEntity);

	int& entFlags = *(int *)((char *)pClient + m_fFlags);
	int& efxEntity = *(int *)((char *)pClient + m_hEffectEntity);
	int level = *(char *)((char *)pClient + m_nWaterLevel);

	if (entFlags & FL_ONFIRE && !level && efxEntity) {
		edict_t *edictClient = gamehelpers->EdictOfIndex(gamehelpers->EntityToBCompatRef(pClient));
		edict_t *edictEfxEntity = gamehelpers->GetHandleEntity(*(CBaseHandle *)efxEntity);
		const char *name;

		entFlags &= ~FL_ONFIRE;
		gamehelpers->SetEdictStateChanged(edictClient, m_fFlags);

		if (edictEfxEntity && (name = gamehelpers->GetEntityClassname(edictEfxEntity)) && !strcmp(name, "entityflame")) {
			*(int *)((char *)efxEntity + m_hEntAttached) = 0;
		}

		efxEntity = 0;
		gamehelpers->SetEdictStateChanged(edictClient, m_hEffectEntity);

		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_HANDLED);
}