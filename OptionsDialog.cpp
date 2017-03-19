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
#include "OptionsDialog.h"
#include "resource.h"
#include "MetalBar.h"

extern HWND		g_mainVSHwnd;

void OptionsDialog::InitDialog(HWND hwnd)
{
	SetWindowLongPtr(hwnd, GWL_USERDATA, (::LONG_PTR)this);

	m_whiteSpace.Init(GetDlgItem(hwnd, IDC_WHITESPACE), MetalBar::s_whitespaceColor);
	m_comments.Init(GetDlgItem(hwnd, IDC_COMMENTS), MetalBar::s_commentColor);
	m_uppercase.Init(GetDlgItem(hwnd, IDC_UPPERCASE), MetalBar::s_upperCaseColor);
	m_otherChars.Init(GetDlgItem(hwnd, IDC_CHARACTERS), MetalBar::s_characterColor);

	m_matchingWord.Init(GetDlgItem(hwnd, IDC_MATCHING_WORD), MetalBar::s_matchColor);
	m_modifLineSaved.Init(GetDlgItem(hwnd, IDC_MODIF_LINE_SAVED), MetalBar::s_modifiedLineColor);
	m_modifLineUnsaved.Init(GetDlgItem(hwnd, IDC_MODIF_LINE_UNSAVED), MetalBar::s_unsavedLineColor);

	m_breakpoints.Init(GetDlgItem(hwnd, IDC_BREAKPOINTS), MetalBar::s_breakpointColor);
	m_bookmarks.Init(GetDlgItem(hwnd, IDC_BOOKMARKS), MetalBar::s_bookmarkColor);
	m_bookmarks2.Init(GetDlgItem(hwnd, IDC_BOOKMARKS2), MetalBar::s_bookmarkDarkColor);

	m_cursorColor.Init(GetDlgItem(hwnd, IDC_CURSOR_COLOR), MetalBar::s_cursorColor);
	m_cursorFrame.Init(GetDlgItem(hwnd, IDC_CURSORFRAME), MetalBar::s_frameColor);
	m_previewBg.Init(GetDlgItem(hwnd, IDC_PREVIEW_BG), MetalBar::s_codePreviewBg);

	m_keyword.Init(GetDlgItem(hwnd, IDC_KEYWORD), MetalBar::s_keywordColor); //
	m_operators.Init(GetDlgItem(hwnd, IDC_OPERATORS), MetalBar::s_operatorColor);
	m_numbers.Init(GetDlgItem(hwnd, IDC_NUMBERS), MetalBar::s_numberColor);
	m_strings.Init(GetDlgItem(hwnd, IDC_STRINGS), MetalBar::s_stringColor);
	m_preproc.Init(GetDlgItem(hwnd, IDC_PREPROC), MetalBar::s_preprocColor);

	int cursorTrans = MetalBar::s_cursorColor >> 24;
	SetDlgItemInt(hwnd, IDC_CURSOR_TRANS, cursorTrans, FALSE);
	SetDlgItemInt(hwnd, IDC_BAR_WIDTH, MetalBar::s_barWidth, FALSE);
	SetDlgItemInt(hwnd, IDC_SPLITTERH, MetalBar::s_topSplit, FALSE);
	SetDlgItemInt(hwnd, IDC_PREVIEW_WIDTH, MetalBar::s_PrvWidth, FALSE);
	SetDlgItemInt(hwnd, IDC_PREVIEW_HEIGHT, MetalBar::s_PrvHeight, FALSE);

	SetDlgItemInt(hwnd, IDC_PREVIEWFONT, MetalBar::s_PrvFontSize, FALSE);
	SetDlgItemInt(hwnd, IDC_BOOKMSIZE, MetalBar::s_bookmarkSize, FALSE);
	SetDlgItemInt(hwnd, IDC_FINDHEIGHT, MetalBar::s_FindSize, FALSE);
	SetDlgItemInt(hwnd, IDC_FINDHEIGHT2, MetalBar::s_FindSize2, FALSE);

	int state = MetalBar::s_requireAlt ? BST_CHECKED : BST_UNCHECKED;
	CheckDlgButton(hwnd, IDC_REQUIRE_ALT, state);
	state = MetalBar::s_bFindCase ? BST_CHECKED : BST_UNCHECKED;
	CheckDlgButton(hwnd, IDC_FINDCASE, state);
	state = MetalBar::s_bFindWhole ? BST_CHECKED : BST_UNCHECKED;
	CheckDlgButton(hwnd, IDC_FINDWHOLE, state);
}

int OptionsDialog::GetInt(HWND hwnd, int dlgItem, int defVal)
{
	BOOL ok;
	int val = GetDlgItemInt(hwnd, dlgItem, &ok, FALSE);
	return ok ? val : defVal;
}

void OptionsDialog::OnOK(HWND hwnd)
{
	// Read the integers.
	m_cursorTrans = GetInt(hwnd, IDC_CURSOR_TRANS, MetalBar::s_cursorColor >> 24);
	m_barWidth = GetInt(hwnd, IDC_BAR_WIDTH, MetalBar::s_barWidth);
	m_topSplit = GetInt(hwnd, IDC_SPLITTERH, MetalBar::s_topSplit);
	m_PrvWidth = GetInt(hwnd, IDC_PREVIEW_WIDTH, MetalBar::s_PrvWidth);
	m_PrvHeight = GetInt(hwnd, IDC_PREVIEW_HEIGHT, MetalBar::s_PrvHeight);

	m_PrvFontSize = GetInt(hwnd, IDC_PREVIEWFONT, MetalBar::s_PrvFontSize);
	m_bookmSize = GetInt(hwnd, IDC_BOOKMSIZE, MetalBar::s_bookmarkSize);
	m_FindSize = GetInt(hwnd, IDC_FINDHEIGHT, MetalBar::s_FindSize);
	m_FindSize2 = GetInt(hwnd, IDC_FINDHEIGHT2, MetalBar::s_FindSize2);

	m_requireALT = (IsDlgButtonChecked(hwnd, IDC_REQUIRE_ALT) == BST_CHECKED);
	m_findCase = (IsDlgButtonChecked(hwnd, IDC_FINDCASE) == BST_CHECKED);
	m_findWhole = (IsDlgButtonChecked(hwnd, IDC_FINDWHOLE) == BST_CHECKED);
}

INT_PTR CALLBACK OptionsDialog::DlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if(msg == WM_INITDIALOG)
	{
		OptionsDialog* dlg = (OptionsDialog*)lparam;
		dlg->InitDialog(hwnd);
		return TRUE;
	}

	switch(msg)
	{
		case WM_CLOSE:
			EndDialog(hwnd, DlgRet_Cancel);
			break;

		case WM_COMMAND:
			switch(wparam)
			{
				case IDOK:
				{
					OptionsDialog* dlg = (OptionsDialog*)GetWindowLongPtr(hwnd, GWL_USERDATA);
					dlg->OnOK(hwnd);

					// Close the dialog.
					EndDialog(hwnd, DlgRet_Ok);
					break;
				}

				case IDCANCEL:
					EndDialog(hwnd, DlgRet_Cancel);
					break;

				case IDC_RESET:
				{
					if(MessageBoxA(hwnd, "Are you sure you want to reset all settings?", "MetalScroll", MB_YESNO|MB_ICONQUESTION|MB_APPLMODAL) == IDYES)
						EndDialog(hwnd, DlgRet_Reset);
				}
			}
			break;

		default:
			return 0;
	}

	return 1;
}

void OptionsDialog::Execute()
{
	int ret = DialogBoxParamA(_AtlModule.GetResourceInstance(), (LPSTR)IDD_OPTIONS, g_mainVSHwnd, DlgProc, (LPARAM)this);
	if(ret == DlgRet_Cancel)
		return;

	if(ret == DlgRet_Reset)
	{
		MetalBar::ResetSettings();
		MetalBar::SaveSettings();
		return;
	}

	MetalBar::s_whitespaceColor = m_whiteSpace.GetColor();
	MetalBar::s_commentColor = m_comments.GetColor();
	MetalBar::s_upperCaseColor = m_uppercase.GetColor();
	MetalBar::s_characterColor = m_otherChars.GetColor();

	MetalBar::s_matchColor = m_matchingWord.GetColor();
	MetalBar::s_modifiedLineColor = m_modifLineSaved.GetColor();
	MetalBar::s_unsavedLineColor = m_modifLineUnsaved.GetColor();

	MetalBar::s_breakpointColor = m_breakpoints.GetColor();
	MetalBar::s_bookmarkColor = m_bookmarks.GetColor();
	MetalBar::s_bookmarkDarkColor = m_bookmarks2.GetColor();	MetalBar::UpdBookmClrT();
	MetalBar::s_bookmarkSize = m_bookmSize;

	MetalBar::s_cursorColor = (m_cursorColor.GetColor() & 0xffffff) | ((m_cursorTrans & 0xff) << 24);
	MetalBar::s_frameColor = m_cursorFrame.GetColor();
	MetalBar::s_codePreviewBg = m_previewBg.GetColor();

	MetalBar::s_keywordColor = m_keyword.GetColor();
	MetalBar::s_operatorColor = m_operators.GetColor();
	MetalBar::s_numberColor = m_numbers.GetColor();
	MetalBar::s_stringColor = m_strings.GetColor();
	MetalBar::s_preprocColor = m_preproc.GetColor();

	MetalBar::s_barWidth = m_barWidth;
	MetalBar::s_topSplit = m_topSplit;
	MetalBar::s_PrvWidth = m_PrvWidth;
	MetalBar::s_PrvHeight = m_PrvHeight;
	MetalBar::s_PrvFontSize = m_PrvFontSize;

	MetalBar::s_requireAlt = m_requireALT;
	MetalBar::s_bFindCase = m_findCase;
	MetalBar::s_bFindWhole = m_findWhole;
	MetalBar::s_FindSize = m_FindSize;
	MetalBar::s_FindSize2 = m_FindSize2;

	MetalBar::SaveSettings();
}

void OptionsDialog::Init()
{
	ColorChip::Register();
}

void OptionsDialog::Uninit()
{
	ColorChip::Unregister();
}
