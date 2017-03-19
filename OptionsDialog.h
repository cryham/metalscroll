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

#pragma once

#include "ColorChip.h"

class OptionsDialog
{
public:
	static void					Init();
	static void					Uninit();

	void						Execute();

private:
	enum DlgRetCode
	{
		DlgRet_Cancel,
		DlgRet_Ok,
		DlgRet_Reset
	};

	static INT_PTR CALLBACK		DlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	ColorChip		m_whiteSpace, m_comments, m_uppercase, m_otherChars,
					m_matchingWord, m_modifLineSaved, m_modifLineUnsaved,
					m_cursorColor, m_cursorFrame, m_breakpoints, m_bookmarks, m_bookmarks2, m_previewBg,
					m_keyword, m_operators, m_numbers, m_strings, m_preproc;
	unsigned int	m_cursorTrans, m_barWidth, m_topSplit, m_bookmSize, m_FindSize, m_FindSize2,
					m_requireALT, m_findCase, m_findWhole,
					m_PrvWidth, m_PrvHeight, m_PrvFontSize;

	void						InitDialog(HWND hwnd);
	int							GetInt(HWND hwnd, int dlgItem, int defVal);
	void						OnOK(HWND hwnd);
};
