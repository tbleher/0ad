/* Copyright (C) 2020 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "JSInterface_ModIo.h"

#include "ps/CLogger.h"
#include "ps/ModIo.h"

void JSI_ModIo::StartGetGameId(ScriptInterface::CmptPrivate* UNUSED(pCmptPrivate))
{
	if (!g_ModIo)
		g_ModIo = new ModIo();

	ENSURE(g_ModIo);

	g_ModIo->StartGetGameId();
}

void JSI_ModIo::StartListMods(ScriptInterface::CmptPrivate* UNUSED(pCmptPrivate))
{
	if (!g_ModIo)
	{
		LOGERROR("ModIoStartListMods called before ModIoStartGetGameId");
		return;
	}

	g_ModIo->StartListMods();
}

void JSI_ModIo::StartDownloadMod(ScriptInterface::CmptPrivate* UNUSED(pCmptPrivate), uint32_t idx)
{
	if (!g_ModIo)
	{
		LOGERROR("ModIoStartDownloadMod called before ModIoStartGetGameId");
		return;
	}

	g_ModIo->StartDownloadMod(idx);
}

bool JSI_ModIo::AdvanceRequest(ScriptInterface::CmptPrivate* pCmptPrivate)
{
	if (!g_ModIo)
	{
		LOGERROR("ModIoAdvanceRequest called before ModIoGetMods");
		return false;
	}

	ScriptInterface* scriptInterface = pCmptPrivate->pScriptInterface;
	return g_ModIo->AdvanceRequest(*scriptInterface);
}

void JSI_ModIo::CancelRequest(ScriptInterface::CmptPrivate* UNUSED(pCmptPrivate))
{
	if (!g_ModIo)
	{
		LOGERROR("ModIoCancelRequest called before ModIoGetMods");
		return;
	}

	g_ModIo->CancelRequest();
}

JS::Value JSI_ModIo::GetMods(ScriptInterface::CmptPrivate* pCmptPrivate)
{
	if (!g_ModIo)
	{
		LOGERROR("ModIoGetMods called before ModIoStartGetGameId");
		return JS::NullValue();
	}

	ScriptInterface* scriptInterface = pCmptPrivate->pScriptInterface;
	ScriptRequest rq(scriptInterface);

	const std::vector<ModIoModData>& availableMods = g_ModIo->GetMods();

	JS::RootedValue mods(rq.cx);
	ScriptInterface::CreateArray(rq, &mods, availableMods.size());

	u32 i = 0;
	for (const ModIoModData& mod : availableMods)
	{
		JS::RootedValue m(rq.cx);
		ScriptInterface::CreateObject(rq, &m);

		for (const std::pair<const std::string, std::string>& prop : mod.properties)
			scriptInterface->SetProperty(m, prop.first.c_str(), prop.second, true);

		scriptInterface->SetProperty(m, "dependencies", mod.dependencies, true);
		scriptInterface->SetPropertyInt(mods, i++, m);
	}

	return mods;
}

const std::map<DownloadProgressStatus, std::string> statusStrings = {
	{ DownloadProgressStatus::NONE, "none" },
	{ DownloadProgressStatus::GAMEID, "gameid" },
	{ DownloadProgressStatus::READY, "ready" },
	{ DownloadProgressStatus::LISTING, "listing" },
	{ DownloadProgressStatus::LISTED, "listed" },
	{ DownloadProgressStatus::DOWNLOADING, "downloading" },
	{ DownloadProgressStatus::SUCCESS, "success" },
	{ DownloadProgressStatus::FAILED_GAMEID, "failed_gameid" },
	{ DownloadProgressStatus::FAILED_LISTING, "failed_listing" },
	{ DownloadProgressStatus::FAILED_DOWNLOADING, "failed_downloading" },
	{ DownloadProgressStatus::FAILED_FILECHECK, "failed_filecheck" }
};

JS::Value JSI_ModIo::GetDownloadProgress(ScriptInterface::CmptPrivate* pCmptPrivate)
{
	if (!g_ModIo)
	{
		LOGERROR("ModIoGetDownloadProgress called before ModIoGetMods");
		return JS::NullValue();
	}

	ScriptInterface* scriptInterface = pCmptPrivate->pScriptInterface;
	ScriptRequest rq(scriptInterface);

	const DownloadProgressData& progress = g_ModIo->GetDownloadProgress();

	JS::RootedValue progressData(rq.cx);
	ScriptInterface::CreateObject(rq, &progressData);
	scriptInterface->SetProperty(progressData, "status", statusStrings.at(progress.status), true);
	scriptInterface->SetProperty(progressData, "progress", progress.progress, true);
	scriptInterface->SetProperty(progressData, "error", progress.error, true);

	return progressData;
}

void JSI_ModIo::RegisterScriptFunctions(const ScriptInterface& scriptInterface)
{
	scriptInterface.RegisterFunction<void, &JSI_ModIo::StartGetGameId>("ModIoStartGetGameId");
	scriptInterface.RegisterFunction<void, &JSI_ModIo::StartListMods>("ModIoStartListMods");
	scriptInterface.RegisterFunction<void, uint32_t, &JSI_ModIo::StartDownloadMod>("ModIoStartDownloadMod");
	scriptInterface.RegisterFunction<bool, &JSI_ModIo::AdvanceRequest>("ModIoAdvanceRequest");
	scriptInterface.RegisterFunction<void, &JSI_ModIo::CancelRequest>("ModIoCancelRequest");
	scriptInterface.RegisterFunction<JS::Value, &JSI_ModIo::GetMods>("ModIoGetMods");
	scriptInterface.RegisterFunction<JS::Value, &JSI_ModIo::GetDownloadProgress>("ModIoGetDownloadProgress");
}
