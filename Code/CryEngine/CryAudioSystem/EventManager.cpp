// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EventManager.h"
#include "Common.h"
#include "CVars.h"
#include "Object.h"
#include "Event.h"
#include <IImpl.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	#include "Managers.h"
	#include "ListenerManager.h"
	#include "Debug.h"
	#include <CryRenderer/IRenderAuxGeom.h>
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CEventManager::~CEventManager()
{
	CRY_ASSERT_MESSAGE(m_constructedEvents.empty(), "There are still events during %s", __FUNCTION__);
}

//////////////////////////////////////////////////////////////////////////
void CEventManager::Initialize(uint32 const poolSize)
{
	m_constructedEvents.reserve(static_cast<std::size_t>(poolSize));
}

//////////////////////////////////////////////////////////////////////////
void CEventManager::OnAfterImplChanged()
{
	CRY_ASSERT(m_constructedEvents.empty());
}

//////////////////////////////////////////////////////////////////////////
void CEventManager::ReleaseImplData()
{
	for (auto const pEvent : m_constructedEvents)
	{
		g_pIImpl->DestructEvent(pEvent->m_pImplData);
		pEvent->Release();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEventManager::Release()
{
	// Events cannot survive a middleware switch because we cannot
	// know which event types the new middleware backend will support so
	// the existing ones have to be destroyed now and new ones created
	// after the switch.
	for (auto const pEvent : m_constructedEvents)
	{
		CRY_ASSERT_MESSAGE(pEvent->m_pImplData == nullptr, "An event cannot have valid impl data during %s", __FUNCTION__);
		delete pEvent;
	}

	m_constructedEvents.clear();
}

//////////////////////////////////////////////////////////////////////////
CEvent* CEventManager::ConstructEvent()
{
	auto const pEvent = new CEvent;
	pEvent->m_pImplData = g_pIImpl->ConstructEvent(*pEvent);
	m_constructedEvents.push_back(pEvent);

	return pEvent;
}

//////////////////////////////////////////////////////////////////////////
void CEventManager::DestructEvent(CEvent* const pEvent)
{
	CRY_ASSERT(pEvent != nullptr && pEvent->m_pImplData != nullptr);

	auto iter(m_constructedEvents.begin());
	auto const iterEnd(m_constructedEvents.cend());

	for (; iter != iterEnd; ++iter)
	{
		if ((*iter) == pEvent)
		{
			if (iter != (iterEnd - 1))
			{
				(*iter) = m_constructedEvents.back();
			}

			m_constructedEvents.pop_back();
			break;
		}
	}

	g_pIImpl->DestructEvent(pEvent->m_pImplData);
	pEvent->Release();
	delete pEvent;
}

//////////////////////////////////////////////////////////////////////////
size_t CEventManager::GetNumConstructed() const
{
	return m_constructedEvents.size();
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
void CEventManager::DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY) const
{
	CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(g_cvars.m_pDebugFilter->GetString());
	lowerCaseSearchString.MakeLower();

	auxGeom.Draw2dLabel(posX, posY, Debug::g_managerHeaderFontSize, Debug::g_globalColorHeader.data(), false, "Audio Events [%" PRISIZE_T "]", m_constructedEvents.size());
	posY += Debug::g_managerHeaderLineHeight;

	for (auto const pEvent : m_constructedEvents)
	{
		Vec3 const& position = pEvent->m_pObject->GetTransformation().GetPosition();
		float const distance = position.GetDistance(g_listenerManager.GetActiveListenerTransformation().GetPosition());

		if (g_cvars.m_debugDistance <= 0.0f || (g_cvars.m_debugDistance > 0.0f && distance < g_cvars.m_debugDistance))
		{
			char const* const szTriggerName = pEvent->GetTriggerName();
			CryFixedStringT<MaxControlNameLength> lowerCaseTriggerName(szTriggerName);
			lowerCaseTriggerName.MakeLower();
			bool const draw = ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseTriggerName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos));

			if (draw)
			{
				float const* pColor = Debug::g_globalColorInactive.data();

				switch (pEvent->m_state)
				{
				case EEventState::Playing:
					{
						pColor = Debug::g_managerColorItemActive.data();
						break;
					}
				case EEventState::Virtual:
					{
						pColor = Debug::g_globalColorVirtual.data();
						break;
					}
				case EEventState::Loading:
					{
						pColor = Debug::g_managerColorItemLoading.data();
						break;
					}
				default:
					break;
				}

				auxGeom.Draw2dLabel(posX, posY, Debug::g_managerFontSize, pColor, false, "%s on %s", szTriggerName, pEvent->m_pObject->m_name.c_str());

				posY += Debug::g_managerLineHeight;
			}
		}
	}
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}      // namespace CryAudio