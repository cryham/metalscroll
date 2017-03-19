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

class CodePreview
{
public:
	CodePreview();

	static void					Register();
	static void					Unregister();

	void						Create(HWND parent, int width, int height);
	void						Destroy();

	void						Show(HWND bar, IVsTextView* view, IVsTextLines* buffer, const wchar_t* text, int numLines);
	void						Hide();
	void						Update(int y, int line);
	void						Resize(int width, int height);

private:
	static const char			s_className[];
	static HFONT				s_normalFont;
	static HFONT				s_boldFont;
	static int					s_charWidth;
	static int					s_lineHeight;

	enum FormatType
	{
		FT_match,  //Highlight
		FT_comment,
		FT_keyword,
		FT_string,
		FT_preproc,
		FT_upperCase,
		FT_character,
		FT_number,
		FT_operator,
		FT_EOL
	};

	struct CharInfo
	{
		wchar_t			chr;
		unsigned char	format;
	};

	HWND						m_hwnd;
	HDC							m_paintDC;
	HBITMAP						m_codeBmp;
	int							m_wndWidth;
	int							m_wndHeight;
	int							m_rightEdge;
	int							m_parentYMin;
	int							m_parentYMax;
	std::vector<CharInfo>		m_text;
	int							m_imgNumLines;

	static LRESULT FAR PASCAL	WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
	void						OnPaint(HDC dc);
	void						FlushTextBuf(std::vector<wchar_t>& buf, unsigned char format, int x, int y);

	friend struct PreviewRenderOp;
};
