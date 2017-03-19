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

class CEditCmdFilter;
#define  BOOKM_LEVELS  4
#define  MARG_L  5
#define  MARG_R  8


class MetalBar
{
public:
	struct ScrollbarHandles
	{
		HWND		editor;
		HWND		vert;
		HWND		horiz;
		HWND		resharper;
	};

	MetalBar(ScrollbarHandles& handles, IVsTextView* view);
	~MetalBar();

	IVsTextView*					GetView() const { return m_view; }
	HWND							GetHwnd() const { return m_handles.vert; }

	static void						Init();
	static void						Uninit();
	static unsigned int				IsEnabled() { return s_enabled; }
	static void						SetBarsEnabled(unsigned int enabled);
	static void						ResetSettings();
	static void						SaveSettings();

	// User-controllable parameters.
	static unsigned int
		s_barWidth, s_whitespaceColor, s_upperCaseColor, s_characterColor,
		s_numberColor, s_operatorColor, s_stringColor, s_preprocColor,
		s_commentColor, s_cursorColor, s_frameColor,
		s_matchColor, s_modifiedLineColor, s_unsavedLineColor,
		s_breakpointColor, s_bookmarkColor, s_bookmarkDarkColor, s_bookmarkSize, s_bookmarkClrT[BOOKM_LEVELS],
		s_requireAlt, s_bFindCase, s_bFindWhole, s_FindSize, s_FindSize2,
		s_codePreviewBg, s_keywordColor, s_PrvWidth, s_PrvHeight,
		s_PrvFontSize, s_topSplit;
	static void UpdBookmClrT();

private:
	static std::set<MetalBar*>		s_bars;
	static unsigned int				s_enabled;

	static bool						ReadRegInt(unsigned int* to, HKEY key, const char* name);
	static void						WriteRegInt(HKEY key, const char* name, unsigned int val);
	static void						ReadSettings();
	static void						RemoveAllBars();

	// Handles and other window-related stuff.
	ScrollbarHandles				m_handles;
	WNDPROC							m_oldProc;

	// Text.
	IVsTextView*					m_view;
	int								m_numLines;
	CEditCmdFilter*					m_editCmdFilter;
	CComBSTR						m_highlightWord;

	// Painting.
	HBITMAP							m_codeImg;
	int								m_codeImgHeight;
	bool							m_codeImgDirty;
	HDC								m_imgDC;
	HBITMAP							m_backBufferImg;
	HDC								m_backBufferDC;
	unsigned int*					m_backBufferBits;
	unsigned int					m_backBufferWidth;
	unsigned int					m_backBufferHeight;

	// Scrollbar stuff.
	int								m_pageSize;
	int								m_scrollPos;
	int								m_scrollMin;
	int								m_scrollMax;
	bool							m_dragging;

	void							OnDrag(bool initial);
	void							OnTrackPreview();
	void							ShowCodePreview();
	void							OnPaint(HDC ctrlDC);
	void							AdjustSize(unsigned int requiredWidth, WINDOWPOS* vertSbPos);
	void							RemoveWndProcHook();

	bool							GetBufferAndText(IVsTextLines** buffer, BSTR* text, long* numLines);
	void							HighlightMatchingWords();
	void							RemoveWordHighlight(IVsTextLines* buffer);

	void							PaintLineFlags(unsigned int* img, int line, int imgHeight, unsigned int flags, int bookmlev);
	void							RefreshCodeImg(int barHeight);

	LRESULT							WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
	static LRESULT FAR PASCAL		WndProcHelper(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
};
