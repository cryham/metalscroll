/*
*	Copyright 2009 Griffin Software
*
*	Licensed under the Apache License, Version 2.0 (the "License");
*	you may not use this file except in compliance with the License.
*	You may obtain a copy of the License at
*
*		http://www.apache.org/licenses/LICENSE-2.0
*
*	Unless required by applicable law or agreed to in writing, software
*	distributed under the License is distributed on an "AS IS" BASIS,
*	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*	See the License for the specific language governing permissions and
*	limitations under the License. 
*/

#include "MetalScrollPCH.h"
#include "EditCmdFilter.h"
#include "MetalBar.h"

CEditCmdFilter::CEditCmdFilter()
{
	m_bar = 0;
	m_nextHandler = 0;
}

// I have no idea which lib contains this (if any), so I'll just paste it here. I got it from stdicmd.h.
static const GUID g_stdCmdGUID = { 0x1496A755, 0x94DE, 0x11D0, { 0x8C, 0x3F, 0x00, 0xC0, 0x4F, 0xC2, 0xAA, 0xE2 } };

STDMETHODIMP CEditCmdFilter::QueryStatus(const GUID* cmdGroup, ULONG numCmds, OLECMD cmds[], OLECMDTEXT* cmdText)
{
	// Some handler which gets called after us decides ECMD_DOUBLECLICK is disabled or something, so we never get
	// an Exec() call for that ID. Since catching double clicks in the editor is the whole point of this filter,
	// that's a bit of a bummer. I'll just make this function say the command is always enabled and never call the
	// rest of the chain for ECMD_DOUBLECLICK. I don't know if there are nasty side effects to that, but I can't
	// find any other solution either. RockScroll probably does the same, but I've done enough disassembling
	// for the day, so I didn't bother checking.
	if(InlineIsEqualGUID(*cmdGroup, g_stdCmdGUID) && (numCmds == 1) && (cmds[0].cmdID == ECMD_DOUBLECLICK) )
	{
		cmds[0].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
		return S_OK;
	}

	if(m_nextHandler)
		return m_nextHandler->QueryStatus(cmdGroup, numCmds, cmds, cmdText);
	else
		return S_OK;
}

STDMETHODIMP CEditCmdFilter::Exec(const GUID* cmdGroup, DWORD cmdID, DWORD cmdexecopt, VARIANT* in, VARIANT* out)
{
	assert(m_bar);

	if(InlineIsEqualGUID(*cmdGroup, g_stdCmdGUID))
	{
		switch(cmdID)
		{
			case ECMD_DOUBLECLICK:
			{
				if(MetalBar::s_requireAlt)
				{
					DWORD state = GetAsyncKeyState(VK_MENU);
					if(!(state & 80000000))
						break;
				}

				// Tell the scrollbar to apply a marker on all occurrences of the selected word and highlights it in the code image.
				PostMessage(m_bar->GetHwnd(), (WM_USER + 1), 0, 0);
				break;
			}

			case ECMD_CANCEL:
			{
				// Remove the markers set by ALT-doubleclick.
				PostMessage(m_bar->GetHwnd(), (WM_USER + 2), 0, 0);
				break;
			}
		}
	}

	if(m_nextHandler)
		return m_nextHandler->Exec(cmdGroup, cmdID, cmdexecopt, in, out);
	else
		return S_OK;
}

CEditCmdFilter* CEditCmdFilter::AttachFilter(MetalBar* bar)
{
	CEditCmdFilter* filter;
	HRESULT hr = CoCreateInstance(__uuidof(EditCmdFilter), 0, CLSCTX_INPROC_SERVER, IID_IOleCommandTarget, (void**)&filter);
	if(FAILED(hr) || !filter)
		return 0;

	IVsTextView* view = bar->GetView();

	hr = view->AddCommandFilter((IOleCommandTarget*)filter, &filter->m_nextHandler);
	if(FAILED(hr))
	{
		filter->Release();
		return 0;
	}

	filter->m_bar = bar;
	return filter;
}

void CEditCmdFilter::RemoveFilter()
{
	if(m_bar)
	{
		m_bar->GetView()->RemoveCommandFilter(this);
		m_bar = 0;
	}

	m_nextHandler->Release();
	m_nextHandler = 0;
}
