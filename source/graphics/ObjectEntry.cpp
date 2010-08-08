/* Copyright (C) 2010 Wildfire Games.
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

#include "ObjectEntry.h"
#include "ObjectManager.h"
#include "ObjectBase.h"
#include "Model.h"
#include "ModelDef.h"
#include "ps/CLogger.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include "SkeletonAnim.h"

#include "lib/rand.h"

#include <sstream>

CObjectEntry::CObjectEntry(CObjectBase* base) :
	m_Base(base), m_Color(1.0f, 1.0f, 1.0f, 1.0f), m_Model(NULL)
{
}

template<typename T, typename S> static void delete_pair_2nd(std::pair<T,S> v) { delete v.second; }

CObjectEntry::~CObjectEntry()
{
	std::for_each(m_Animations.begin(), m_Animations.end(), delete_pair_2nd<CStr, CSkeletonAnim*>);

	delete m_Model;
}


bool CObjectEntry::BuildVariation(const std::vector<std::set<CStr> >& selections,
								  const std::vector<u8>& variationKey,
								  CObjectManager& objectManager)
{
	CObjectBase::Variation variation = m_Base->BuildVariation(variationKey);

	// Copy the chosen data onto this model:

	m_TextureName = variation.texture;
	m_ModelName = variation.model;

	if (! variation.color.empty())
	{
		std::stringstream str;
		str << variation.color;
		int r, g, b;
		if (! (str >> r >> g >> b)) // Any trailing data is ignored
			LOGERROR(L"Actor '%ls' has invalid RGB colour '%hs'", m_Base->m_ShortName.c_str(), variation.color.c_str());
		else
			m_Color = CColor(r/255.0f, g/255.0f, b/255.0f, 1.0f);
	}

	std::vector<CObjectBase::Prop> props;

	for (std::multimap<CStr, CObjectBase::Prop>::iterator it = variation.props.begin(); it != variation.props.end(); ++it)
		props.push_back(it->second);

	// Build the model:

/*
	// remember the old model so we can replace any models using it later on
	CModelDefPtr oldmodeldef = m_Model ? m_Model->GetModelDef() : CModelDefPtr();
*/

	// try and create a model
	CModelDefPtr modeldef (objectManager.GetMeshManager().GetMesh(m_ModelName));
	if (!modeldef)
	{
		LOGERROR(L"CObjectEntry::BuildVariation(): Model %ls failed to load", m_ModelName.string().c_str());
		return false;
	}

	// delete old model, create new 
	delete m_Model;
	m_Model = new CModel(objectManager.GetSkeletonAnimManager());
	m_Model->SetTexture(CTexture(m_TextureName));
	m_Model->SetMaterial(g_MaterialManager.LoadMaterial(m_Base->m_Material));
	m_Model->InitModel(modeldef);
	m_Model->SetPlayerColor(m_Color);

	// calculate initial object space bounds, based on vertex positions
	m_Model->CalcObjectBounds();

	// load the animations
	for (std::multimap<CStr, CObjectBase::Anim>::iterator it = variation.anims.begin(); it != variation.anims.end(); ++it)
	{
		CStr name = it->first.LowerCase();

		// TODO: Use consistent names everywhere, then remove this translation section.
		// (It's just mapping the names used in actors onto the names used by code.)
		if (name == "attack") name = "melee";
		else if (name == "chop") name = "gather";
		else if (name == "decay") name = "corpse";

		if (! it->second.m_FileName.empty())
		{
			CSkeletonAnim* anim = m_Model->BuildAnimation(it->second.m_FileName, name, it->second.m_Speed, it->second.m_ActionPos, it->second.m_ActionPos2);
			if (anim)
				m_Animations.insert(std::make_pair(name, anim));
		}
	}

	// ensure there's always an idle animation
	if (m_Animations.find("idle") == m_Animations.end())
	{
		CSkeletonAnim* anim = new CSkeletonAnim();
		anim->m_Name = "idle";
		anim->m_AnimDef = NULL;
		anim->m_Speed = 0.f;
		anim->m_ActionPos = 0.f;
		anim->m_ActionPos2 = 0.f;
		m_Animations.insert(std::make_pair("idle", anim));

		// Ignore errors, since they're probably saying this is a non-animated model
		m_Model->SetAnimation(anim);
	}
	else
	{
		// start up idling
		if (!m_Model->SetAnimation(GetRandomAnimation("idle")))
			LOGERROR(L"Failed to set idle animation in model \"%ls\"", m_ModelName.string().c_str());
	}

	// build props - TODO, RC - need to fix up bounds here
	// TODO: Make sure random variations get handled correctly when a prop fails
	for (size_t p = 0; p < props.size(); p++)
	{
		const CObjectBase::Prop& prop = props[p];

		// Pluck out the special attachpoint 'projectile'
		if (prop.m_PropPointName == "projectile")
		{
			m_ProjectileModelName = prop.m_ModelName.string();
			continue;
		}

		CObjectEntry* oe = objectManager.FindObjectVariation(prop.m_ModelName.string().c_str(), selections);
		if (!oe)
		{
			LOGERROR(L"Failed to build prop model \"%ls\" on actor \"%ls\"", prop.m_ModelName.string().c_str(), m_Base->m_ShortName.c_str());
			continue;
		}

		// If we don't have a projectile but this prop does (e.g. it's our rider), then
		// use that as our projectile too
		if (m_ProjectileModelName.empty() && !oe->m_ProjectileModelName.empty())
			m_ProjectileModelName = oe->m_ProjectileModelName;

		CStr ppn = prop.m_PropPointName;
		bool isAmmo = false;

		// Handle the special attachpoint 'loaded-<proppoint>'
		if (ppn.Find("loaded-") == 0)
		{
			ppn = prop.m_PropPointName.substr(7);
			isAmmo = true;
		}

		SPropPoint* proppoint = modeldef->FindPropPoint(ppn.c_str());
		if (proppoint)
		{
			CModel* propmodel = oe->m_Model->Clone();
			if (isAmmo)
				m_Model->AddAmmoProp(proppoint, propmodel, oe);
			else
				m_Model->AddProp(proppoint, propmodel, oe);
			propmodel->SetAnimation(oe->GetRandomAnimation("idle"));
		}
		else
			LOGERROR(L"Failed to find matching prop point called \"%hs\" in model \"%ls\" for actor \"%ls\"", ppn.c_str(), m_ModelName.string().c_str(), m_Base->m_ShortName.c_str());
	}

	// setup flags
	if (m_Base->m_Properties.m_CastShadows)
	{
		m_Model->SetFlags(m_Model->GetFlags()|MODELFLAG_CASTSHADOWS);
	}

	// replace any units using old model to now use new model; also reprop models, if necessary
	// FIXME, RC - ugh, doesn't recurse correctly through props
/*	
	// (PT: Removed this, since I'm not entirely sure what it's useful for, and it
	//  gets a bit confusing with randomised actors)

	const std::vector<CUnit*>& units = g_UnitMan.GetUnits();
	for (size_t i = 0; i < units.size(); ++i)
	{
		CModel* unitmodel=units[i]->GetModel();
		if (unitmodel->GetModelDef() == oldmodeldef)
		{
			unitmodel->InitModel(m_Model->GetModelDef());
			unitmodel->SetFlags(m_Model->GetFlags());

			const std::vector<CModel::Prop>& newprops = m_Model->GetProps();
			for (size_t j = 0; j < newprops.size(); j++)
				unitmodel->AddProp(newprops[j].m_Point, newprops[j].m_Model->Clone());
		}

		std::vector<CModel::Prop>& mdlprops = unitmodel->GetProps();
		for (size_t j = 0; j < mdlprops.size(); j++)
		{
			CModel::Prop& prop = mdlprops[j];
			if (prop.m_Model)
			{
				if (prop.m_Model->GetModelDef() == oldmodeldef)
				{
					delete prop.m_Model;
					prop.m_Model = m_Model->Clone();
				}
			}
		}
	}
*/
	return true;
}

CSkeletonAnim* CObjectEntry::GetRandomAnimation(const CStr& animationName) const
{
	SkeletonAnimMap::const_iterator lower = m_Animations.lower_bound(animationName);
	SkeletonAnimMap::const_iterator upper = m_Animations.upper_bound(animationName);
	size_t count = std::distance(lower, upper);
	if (count == 0)
		return NULL;

	size_t id = rand(0, count);
	std::advance(lower, id);
	return lower->second;
}

std::vector<CSkeletonAnim*> CObjectEntry::GetAnimations(const CStr& animationName) const
{
	std::vector<CSkeletonAnim*> anims;

	SkeletonAnimMap::const_iterator lower = m_Animations.lower_bound(animationName);
	SkeletonAnimMap::const_iterator upper = m_Animations.upper_bound(animationName);
	for (SkeletonAnimMap::const_iterator it = lower; it != upper; ++it)
		anims.push_back(it->second);
	return anims;
}
