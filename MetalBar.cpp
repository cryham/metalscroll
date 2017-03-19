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
#include "MetalBar.h"
#include "OptionsDialog.h"
#include "EditCmdFilter.h"
#include "Utils.h"
#include "CodePreview.h"
#include "CppLexer.h"
#include "TextFormatting.h"

#define REFRESH_CODE_TIMER_ID		1
#define REFRESH_CODE_INTERVAL		1000  // 1s, was 2000 ms

extern HWND							g_mainVSHwnd;
extern long							g_highlightMarkerType;


//  static vars
unsigned int MetalBar::s_barWidth, MetalBar::s_whitespaceColor, MetalBar::s_upperCaseColor, MetalBar::s_characterColor,
	MetalBar::s_numberColor, MetalBar::s_operatorColor, MetalBar::s_stringColor, MetalBar::s_preprocColor,
	MetalBar::s_commentColor, MetalBar::s_cursorColor, MetalBar::s_frameColor,
	MetalBar::s_matchColor, MetalBar::s_modifiedLineColor, MetalBar::s_unsavedLineColor,
	MetalBar::s_breakpointColor, MetalBar::s_bookmarkColor, MetalBar::s_bookmarkDarkColor, MetalBar::s_bookmarkSize, MetalBar::s_bookmarkClrT[BOOKM_LEVELS],
	MetalBar::s_requireAlt, MetalBar::s_bFindCase, MetalBar::s_bFindWhole, MetalBar::s_FindSize, MetalBar::s_FindSize2,
	MetalBar::s_codePreviewBg, MetalBar::s_keywordColor, MetalBar::s_PrvWidth, MetalBar::s_PrvHeight,
	MetalBar::s_PrvFontSize, MetalBar::s_topSplit, MetalBar::s_enabled = 1;

std::set<MetalBar*> MetalBar::s_bars;

static CodePreview					g_codePreviewWnd;
static bool							g_previewShown = false;

MetalBar::MetalBar(ScrollbarHandles& handles, IVsTextView* view)
{
	m_handles = handles;

	m_view = view;	m_view->AddRef();
	m_numLines = 0;

	m_codeImg = 0;	m_codeImgHeight = 0;	m_codeImgDirty = true;	m_imgDC = 0;
	m_backBufferImg = 0;	m_backBufferDC = 0;		m_backBufferBits = 0;
	m_backBufferWidth = 0;	m_backBufferHeight = 0;

	m_pageSize = 1;	m_scrollPos = 0;	m_scrollMin = 0;	m_scrollMax = 1;
	m_dragging = false;

	m_editCmdFilter = CEditCmdFilter::AttachFilter(this);

	s_bars.insert(this);

	m_oldProc = (WNDPROC)SetWindowLongPtr(m_handles.vert, GWLP_WNDPROC, (::LONG_PTR)WndProcHelper);

	// Call AdjustSize before setting the user data pointer, so that the resulting WM_WINDOWPOSCHANGING messages
	// don't call it again for no reason.
	if(s_enabled)
		AdjustSize(s_barWidth, 0);

	SetWindowLongPtr(m_handles.vert, GWL_USERDATA, (::LONG_PTR)this);
}

MetalBar::~MetalBar()
{
	// Release the various COM things we used.
	if(m_editCmdFilter)
	{
		m_editCmdFilter->RemoveFilter();
		m_editCmdFilter->Release();
	}
	if(m_view)
		m_view->Release();

	// Free the paint stuff.
	if(m_codeImg)
		DeleteObject(m_codeImg);
	if(m_backBufferImg)
		DeleteObject(m_backBufferImg);
	if(m_backBufferDC)
		DeleteObject(m_backBufferDC);
}

void MetalBar::RemoveWndProcHook()
{
	SetWindowLongPtr(m_handles.vert, GWL_USERDATA, 0);
	SetWindowLongPtr(m_handles.vert, GWL_WNDPROC, (::LONG_PTR)m_oldProc);
}

void MetalBar::RemoveAllBars()
{
	int regularBarWidth = GetSystemMetrics(SM_CXVSCROLL);
	for(std::set<MetalBar*>::iterator it = s_bars.begin(); it != s_bars.end(); ++it)
	{
		MetalBar* bar = *it;
		// We must remove the window proc hook before calling AdjustSize(), otherwise WM_SIZE comes and changes the width again.
		bar->RemoveWndProcHook();
		bar->AdjustSize(regularBarWidth, 0);
		delete bar;
	}
	s_bars.clear();
}

void MetalBar::AdjustSize(unsigned int requiredWidth, WINDOWPOS* vertSbPos)
{
	WINDOWPLACEMENT parentPlacement;
	GetWindowPlacement(GetParent(m_handles.vert), &parentPlacement);
	int parentWidth = parentPlacement.rcNormalPosition.right - parentPlacement.rcNormalPosition.left;
	int parentHeight = parentPlacement.rcNormalPosition.bottom - parentPlacement.rcNormalPosition.top;

	int resharperLeft, resharperWidth;
	if(m_handles.resharper)
	{
		WINDOWPLACEMENT resharperPlacement;
		resharperPlacement.length = sizeof(resharperPlacement);
		GetWindowPlacement(m_handles.resharper, &resharperPlacement);
		resharperLeft = resharperPlacement.rcNormalPosition.left;
		resharperWidth = resharperPlacement.rcNormalPosition.right - resharperPlacement.rcNormalPosition.left;
	}
	else
	{
		resharperLeft = 0x7fffffff;
		resharperWidth = 0;
	}

	// Figure out our current position.
	WINDOWPLACEMENT vertBarPlacement;
	vertBarPlacement.length = sizeof(vertBarPlacement);
	GetWindowPlacement(m_handles.vert, &vertBarPlacement);
	int width = vertBarPlacement.rcNormalPosition.right - vertBarPlacement.rcNormalPosition.left;
	
	// If we have the expected width and resharper isn't covering us, there's nothing for us to do here.
	if( (width == (int)requiredWidth) && ((resharperWidth == 0) || (vertBarPlacement.rcNormalPosition.right <= resharperLeft)) )
		return;

	// Resize the horizontal scroll bar.
	int horizBarHeight;
	if(m_handles.horiz)
	{
		WINDOWPLACEMENT horizBarPlacement;
		horizBarPlacement.length = sizeof(horizBarPlacement);
		GetWindowPlacement(m_handles.horiz, &horizBarPlacement);
		horizBarPlacement.rcNormalPosition.right = parentWidth - requiredWidth - resharperWidth;
		SetWindowPlacement(m_handles.horiz, &horizBarPlacement);
		horizBarHeight = horizBarPlacement.rcNormalPosition.bottom - horizBarPlacement.rcNormalPosition.top;
	}
	else
		horizBarHeight = 0;

	// Resize the editor window.
	WINDOWPLACEMENT editorPlacement;
	editorPlacement.length = sizeof(editorPlacement);
	GetWindowPlacement(m_handles.editor, &editorPlacement);
	editorPlacement.rcNormalPosition.right = parentWidth - requiredWidth - resharperWidth;
	SetWindowPlacement(m_handles.editor, &editorPlacement);

	// Make the vertical bar wider so we can draw our stuff in it. Also expand its height to fill the gap left when
	// shrinking the horizontal bar.
	if(vertSbPos)
	{
		vertSbPos->x = parentWidth - requiredWidth - resharperWidth;
		vertSbPos->cx = requiredWidth;
		vertSbPos->cy = parentHeight;
	}
	else
	{
		vertBarPlacement.rcNormalPosition.left = parentWidth - requiredWidth - resharperWidth;
		vertBarPlacement.rcNormalPosition.right = parentWidth - resharperWidth;
		vertBarPlacement.rcNormalPosition.bottom = parentHeight;
		SetWindowPlacement(m_handles.vert, &vertBarPlacement);
	}
}

void MetalBar::OnDrag(bool initial)
{
	POINT mouse;
	GetCursorPos(&mouse);
	ScreenToClient(m_handles.vert, &mouse);

	RECT clRect;
	GetClientRect(m_handles.vert, &clRect);

	int cursor = mouse.y - clRect.top;
	int barHeight = clRect.bottom - clRect.top;
	float normFactor = (m_numLines > barHeight) ? 1.0f * m_numLines / barHeight : 1.0f;
	int line = int(cursor * normFactor);

	line -= m_pageSize / 2;
	line = (line >= 0) ? line : 0;
	line = (line < m_numLines) ? line : m_numLines - 1;

	SetScrollPos(m_handles.vert, SB_CTL, line, TRUE);

	HWND parent = GetParent(m_handles.vert);
	WPARAM wparam = (line << 16);
	wparam |= initial ? SB_THUMBPOSITION : SB_THUMBTRACK;
	PostMessage(parent, WM_VSCROLL, wparam, (LPARAM)m_handles.vert);
	if(initial)
		PostMessage(parent, WM_VSCROLL, SB_ENDSCROLL, (LPARAM)m_handles.vert);
}

void MetalBar::OnTrackPreview()
{
	POINT mouse;
	GetCursorPos(&mouse);

	RECT clRect;
	GetWindowRect(m_handles.vert, &clRect);

	// Determine the line, taking care of the case when the code image is scaled.
	int pixelY = clamp<int>(mouse.y - clRect.top, 0, m_codeImgHeight);
	int line = m_codeImgHeight >= m_numLines ? pixelY : int(1.0f * pixelY * m_numLines / m_codeImgHeight);

	g_codePreviewWnd.Update(clRect.top + pixelY, line);
}

void MetalBar::ShowCodePreview()
{
	CComPtr<IVsTextLines> buffer;
	CComBSTR text;
	long numLines;
	if(!GetBufferAndText(&buffer, &text, &numLines))
		return;

	g_codePreviewWnd.Show(m_handles.vert, m_view, buffer, text, numLines);

	g_previewShown = true;
	OnTrackPreview();
}

LRESULT MetalBar::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch(message)
	{
		case WM_NCDESTROY:
		{
			RemoveWndProcHook();
			// Remove it from the list of scrollbars.
			std::set<MetalBar*>::iterator it = s_bars.find(this);
			if(it != s_bars.end())
				s_bars.erase(it);
			// Finally delete the object.
			delete this;
			break;
		}

		case (WM_USER + 1):
		{
			// This message is sent by the command filter to inform us we should highlight words matching
			// the current selection.
			HighlightMatchingWords();
			m_codeImgDirty = true;
			InvalidateRect(hwnd, 0, 0);
			return 0;
		}

		case WM_RBUTTONUP:
			if(!s_enabled)
				break;
			// Fall through.

		case (WM_USER + 2):
		{
			// Right-clicking on the bar or pressing ESC removes the matching word markers. ESC key presses
			// are sent to us by the command filter as WM_USER+2 messages.
			CComPtr<IVsTextLines> buffer;
			HRESULT hr = m_view->GetBuffer(&buffer);
			if(SUCCEEDED(hr) && buffer)
			{
				RemoveWordHighlight(buffer);
				m_codeImgDirty = true;
				InvalidateRect(hwnd, 0, 0);
			}
			// Don't let the default window procedure see right button up events, or it will display the
			// Windows scrollbar menu (scroll here, page up, page down etc.).
			return 0;
		}
	}

	if(!s_enabled)
		return CallWindowProc(m_oldProc, hwnd, message, wparam, lparam);

	switch(message)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			if(hdc)
			{
				OnPaint(hdc);
				EndPaint(hwnd, &ps);
			}
			return 0;
		}

		case WM_WINDOWPOSCHANGING:
		{
			WINDOWPOS* p = (WINDOWPOS*)lparam;
			AdjustSize(s_barWidth, p);
			return 0;
		}

		case WM_WINDOWPOSCHANGED:
		{
			AdjustSize(s_barWidth, 0);
			return 0;
		}

		case WM_ERASEBKGND:
		{
			return 0;
		}

		// Don't let the system see the SBM_ and mouse messages because it will draw the thumb over us.
		case SBM_SETSCROLLINFO:
		{
			SCROLLINFO* si = (SCROLLINFO*)lparam;
			if(si->fMask & SIF_PAGE)
				m_pageSize = si->nPage;
			if(si->fMask & SIF_POS)
				m_scrollPos = si->nPos;
			if(si->fMask & SIF_RANGE)
			{
				if( (m_scrollMin != si->nMin) || (m_scrollMax != si->nMax) )
				{
					// This event fires when the code changes, when a region is hidden/shown etc. We need to
					// update the code image in all those cases.
					m_codeImgDirty = true;
				}

				m_scrollMin = si->nMin;
				m_scrollMax = si->nMax;
			}

			// Call the default window procedure so that the scrollbar stores the position and range internally. Pass
			// 0 as wparam so it doesn't draw the thumb by itself.
			LRESULT res = CallWindowProc(m_oldProc, hwnd, message, 0, lparam);
			if(wparam)
			{
				InvalidateRect(hwnd, 0, TRUE);
				UpdateWindow(hwnd);
			}
			
			return res;
		}

		case SBM_SETPOS:
		{
			m_scrollPos = (int)wparam;
			InvalidateRect(hwnd, 0, FALSE);
			return m_scrollPos;
		}

		case SBM_SETRANGE:
		{
			if( (m_scrollMin != (int)wparam) || (m_scrollMax != (int)lparam) )
			{
				m_scrollMin = (int)wparam;
				m_scrollMax = (int)lparam;
				m_codeImgDirty = true;
				InvalidateRect(hwnd, 0, FALSE);
			}
			return m_scrollPos;
		}

		case WM_LBUTTONDOWN:
		{
			m_dragging = true;
			SetCapture(hwnd);
			OnDrag(true);
			return 0;
		}

		case WM_MOUSEMOVE:
		{
			if(m_dragging)
				OnDrag(false);

			if(g_previewShown)
				OnTrackPreview();

			return 0;
		}

		case WM_LBUTTONUP:
		{
			if(m_dragging)
			{
				ReleaseCapture();
				m_dragging = false;
			}
			return 0;
		}

		case WM_LBUTTONDBLCLK:
			return 0;
		
		//case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:  //+
		{
			OptionsDialog dlg;
			dlg.Execute();
			return 0;
		}

		case WM_MBUTTONDOWN:
		{
			SetCapture(m_handles.vert);
			ShowCodePreview();
			return 0;
		}

		case WM_MBUTTONUP:
		{
			if(!g_previewShown)
				return 0;

			ReleaseCapture();
			g_previewShown = false;
			g_codePreviewWnd.Hide();
			return 0;
		}

		case WM_TIMER:
		{
			if(wparam != REFRESH_CODE_TIMER_ID)
				break;

			// Remove the timer, invalidate the code image and repaint the control.
			KillTimer(hwnd, REFRESH_CODE_TIMER_ID);
			m_codeImgDirty = true;
			InvalidateRect(hwnd, 0, 0);
			return 0;
		}
	}

	return CallWindowProc(m_oldProc, hwnd, message, wparam, lparam);
}

LRESULT FAR PASCAL MetalBar::WndProcHelper(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	MetalBar* bar = (MetalBar*)GetWindowLongPtr(hwnd, GWL_USERDATA);
	if(!bar)
		return 0;

	return bar->WndProc(hwnd, message, wparam, lparam);
}

bool MetalBar::GetBufferAndText(IVsTextLines** buffer, BSTR* text, long* numLines)
{
	HRESULT hr = m_view->GetBuffer(buffer);
	if(FAILED(hr) || !*buffer)
	{
		static bool warningShown = false;
		if(!warningShown)
		{
			Log("MetalScroll: Failed to get buffer for view 0x%p.", m_view);
			warningShown = true;
		}
		return false;
	}

	hr = (*buffer)->GetLineCount(numLines);
	if(FAILED(hr))
		return false;

	long numCharsLastLine;
	hr = (*buffer)->GetLengthOfLine(*numLines - 1, &numCharsLastLine);
	if(FAILED(hr))
		return false;

	hr = (*buffer)->GetLineText(0, 0, *numLines - 1, numCharsLastLine, text);
	return SUCCEEDED(hr) && (*text);
}


///  Markers

void MetalBar::PaintLineFlags(unsigned int* img, int line, int imgHeight, unsigned int flags, int bookmlev)
{
	unsigned int color;
	int xofs = 1, xw = (int)s_barWidth;

	// Left margin flags
	if( (flags & LineFlag_ChangedUnsaved) || (flags & LineFlag_ChangedSaved) )
	{
		color = (flags & LineFlag_ChangedUnsaved) ? s_unsavedLineColor : s_modifiedLineColor;
		int y1 = std::max(0, line - 0), y2 = std::min(imgHeight, line + 1);
		for(int y = y1; y < y2; ++y)
		{
			for(int x = 0; x < 3; ++x)
				img[y*xw + x] = color;
		}
	}
	if(flags & LineFlag_Breakpoint)
	{
		color = s_breakpointColor;
		xofs = MARG_L-3 +1;
		for(int y = std::max(0, line - 1); y < std::min(imgHeight, line + 2); ++y)
			for(int x = xofs; x < xofs + 3; ++x)
				img[y*xw + x] = color;
	}

	// Right margin flags
	if(flags & LineFlag_Match)
	{
		color = s_matchColor;
		xofs = MARG_R +1;  int yofs = ((int)s_FindSize2) /2;
		int y1 = std::max(0, line -yofs), y2 = std::min(imgHeight, line -yofs + (int)s_FindSize2);  //+1
		for(int y = y1; y < y2; ++y)
			for(int x = xw - xofs; x < xw - xofs + 3; ++x)
				img[y*xw + x] = color;
	}
	if(flags & LineFlag_Bookmark)
	{
		//color = s_bookmarkColor;
		color = s_bookmarkClrT[std::min(BOOKM_LEVELS-1, std::max(0, (int)bookmlev-1))];
		bookmlev += s_bookmarkSize;
		int lev2 = bookmlev/2/**/-1, xb1 = std::max(3, bookmlev + 2) -1;
		int y1 = std::max(0, line -lev2), y2 = std::min(imgHeight, line -lev2 + bookmlev);
		//int y1 = std::max(0, line -bookmlev), y2 = std::min(imgHeight, line -bookmlev+lev2 + 1);

		for(int y = y1; y < y2; ++y)
			for(int x = xw - xb1-1; x < xw-1; ++x)
				img[y*xw + x] = color;
	}
}

struct BarRenderOp : public RenderOperator
{
	typedef std::vector<std::pair<unsigned int, unsigned int> > MarkedLineList;

	BarRenderOp(std::vector<unsigned int>& _imgBuffer, MarkedLineList& _markedLines) : imgBuffer(_imgBuffer), markedLines(_markedLines) {}

	void Init(int numLines)
	{
		imgBuffer.reserve(numLines*MetalBar::s_barWidth);
		imgBuffer.resize(MetalBar::s_barWidth);
		markedLines.reserve(numLines);
	}

	void EndLine(int line, int lastColumn, unsigned int lineFlags, bool textEnd)
	{
		// Fill the remaining pixels with the whitespace color.
		for(int i = lastColumn; i < (int)MetalBar::s_barWidth-MARG_L-MARG_R; ++i)
			imgBuffer[line*MetalBar::s_barWidth + i + MARG_L] = MetalBar::s_whitespaceColor;

		// margins
		for(int i = 0; i < MARG_L; ++i)
			imgBuffer[line*MetalBar::s_barWidth + i] = MetalBar::s_whitespaceColor;
		for(int i = (int)MetalBar::s_barWidth-MARG_R; i < (int)MetalBar::s_barWidth; ++i)
			imgBuffer[line*MetalBar::s_barWidth + i] = MetalBar::s_whitespaceColor;

		if(lineFlags)
			markedLines.push_back(std::pair<unsigned int, unsigned int>(line, lineFlags));

		if(!textEnd)
		{
			// Advance the image pointer.
			imgBuffer.resize((line + 2) * MetalBar::s_barWidth);
		}
	}

	void RenderSpaces(int line, int column, int count)
	{
		for(int i = column; (i < column + count) && (i < (int)MetalBar::s_barWidth -MARG_L-MARG_R); ++i)
			imgBuffer[line*MetalBar::s_barWidth + i + MARG_L] = MetalBar::s_whitespaceColor;
	}

	void RenderCharacter(int line, int column, wchar_t chr, unsigned int flags)
	{
		if(column >= (int)MetalBar::s_barWidth -MARG_L-MARG_R)  //-0 (5+8)  **
			return;

		unsigned int color = MetalBar::s_characterColor;
		if (flags & TextFlag_Highlight)		color = MetalBar::s_matchColor;
		else if (flags & TextFlag_Comment)	color = MetalBar::s_commentColor;
		else if (flags & TextFlag_Keyword)	color = MetalBar::s_keywordColor;
		else if (flags & TextFlag_String)	color = MetalBar::s_stringColor;
		else if (flags & TextFlag_Preproc)	color = MetalBar::s_preprocColor;
		else if (chr >= 'A' && chr <= 'Z')	color = MetalBar::s_upperCaseColor;
		else if (chr >= 'a' && chr <= 'z')	color = MetalBar::s_characterColor;
		else if (chr >= '0' && chr <= '9')	color = MetalBar::s_numberColor;
		else  color = MetalBar::s_operatorColor;

		imgBuffer[line*MetalBar::s_barWidth + column + MARG_L] = color;  //+0  **
	}

	std::vector<unsigned int>& imgBuffer;
	MarkedLineList& markedLines;
};

//  Draw Code  ++
//------------------------------------------------------------------------------------------------------------------
void MetalBar::RefreshCodeImg(int barHeight)
{
	// Get the text buffer.
	CComPtr<IVsTextLines> buffer;
	CComBSTR text;
	long numLines;
	if(!GetBufferAndText(&buffer, &text, &numLines))
		return;

	// Paint the code representation.
	std::vector<unsigned int> imgBuffer;
	BarRenderOp::MarkedLineList mli;//markedLines;
	BarRenderOp renderOp(imgBuffer, mli);
	m_numLines = RenderText(renderOp, m_view, buffer, text, numLines);
	m_codeImgHeight = m_numLines < barHeight ? m_numLines : barHeight;

	// Create a bitmap and put the code image inside, scaling it if necessary.
	if(m_codeImg)
		DeleteObject(m_codeImg);

	BITMAPINFO bi;
	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = s_barWidth;	bi.bmiHeader.biHeight = m_codeImgHeight;
	bi.bmiHeader.biPlanes = 1;	bi.bmiHeader.biBitCount = 32;	bi.bmiHeader.biCompression = BI_RGB;
	unsigned int* bmpBits = 0;
	m_codeImg = CreateDIBSection(0, &bi, DIB_RGB_COLORS, (void**)&bmpBits, 0, 0);

	float lineScaleFactor;
	if(m_numLines < barHeight)
	{
		lineScaleFactor = 1.0f;
		// Flip the image while copying it.
		std::vector<unsigned int>::iterator line1 = imgBuffer.begin();
		unsigned int* line2 = bmpBits + (m_numLines - 1)*s_barWidth;
		for(int i = 0; i < m_numLines; ++i)
		{
			std::copy(line1, line1 + s_barWidth, line2);
			line1 += s_barWidth;
			line2 -= s_barWidth;
		}
	}else{
		lineScaleFactor = 1.0f * barHeight / m_numLines;	// Scale
		FlipScaleImageVertically(bmpBits, barHeight, &imgBuffer[0], m_numLines, s_barWidth);
	}

	// Paint the line flags directly on the final image, which might have been scaled.
	
	/// Markers
	
	int si = (int)mli.size();
	for(int i = 0; i < si; ++i)
	{
		int line = mli[i].first;
		int imgLine = int(lineScaleFactor * line);
		
		//  bookmark level  1-4  +
		int bookmlev = 1;
		if (i+1 < si)  if ((mli[i+1].second & LineFlag_Bookmark) && mli[i+1].first == line+1)  bookmlev = 0;  else
		if (i-1 >= 0)  if ((mli[i-1].second & LineFlag_Bookmark) && mli[i-1].first == line-1)  {	++bookmlev;
		if (i-2 >= 0)  if ((mli[i-2].second & LineFlag_Bookmark) && mli[i-2].first == line-2)  {	++bookmlev;
		if (i-3 >= 0)  if ((mli[i-3].second & LineFlag_Bookmark) && mli[i-3].first == line-3)  ++bookmlev;	}	}

		// Flip it, since BMPs are upside down.
		imgLine = m_codeImgHeight - imgLine - 1;
		if (bookmlev > 0)
			PaintLineFlags(bmpBits, imgLine, m_codeImgHeight, mli[i].second, bookmlev);
	}
	
	///  Find highlights
	int len = m_highlightWord.Length();
	for (int i = 0; i < m_highlights.size(); i++)
	{
		int col = m_highlights[i].column + MARG_L +1;
		int imgLine = int(lineScaleFactor * m_highlights[i].line);
		// Flip BMP
		imgLine = m_codeImgHeight - imgLine - 1;

		int xmax = s_barWidth -MARG_R -1;
		int y1 = std::max(0, imgLine), y2 = std::min(m_codeImgHeight, imgLine + (int)s_FindSize);
		if (y1 < y2 && col+2 < xmax)  //+
		for(int y = y1; y < y2; ++y)  // +2
			for(int x = col; x < std::min(xmax, col + len); ++x)
				bmpBits[y*s_barWidth + x] = s_matchColor;
	}

	if(!m_imgDC)
		m_imgDC = CreateCompatibleDC(0);
	SelectObject(m_imgDC, m_codeImg);
}

void MetalBar::OnPaint(HDC ctrlDC)
{
	BLENDFUNCTION blendFunc;
	blendFunc.BlendOp = AC_SRC_OVER;
	blendFunc.BlendFlags = 0;
	blendFunc.SourceConstantAlpha = 255;
	blendFunc.AlphaFormat = AC_SRC_ALPHA;

	RECT clRect;
	GetClientRect(m_handles.vert, &clRect);
	int barHeight = clRect.bottom - clRect.top;

	if(!m_backBufferDC)
		m_backBufferDC = CreateCompatibleDC(ctrlDC);

	if(!m_backBufferImg || (m_backBufferWidth != s_barWidth) || (m_backBufferHeight != (unsigned int)barHeight))
	{
		if(m_backBufferImg)
			DeleteObject(m_backBufferImg);

		BITMAPINFO bi;
		memset(&bi, 0, sizeof(bi));
		bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
		bi.bmiHeader.biWidth = s_barWidth;
		bi.bmiHeader.biHeight = barHeight;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		m_backBufferImg = CreateDIBSection(0, &bi, DIB_RGB_COLORS, (void**)&m_backBufferBits, 0, 0);
		SelectObject(m_backBufferDC, m_backBufferImg);
		m_backBufferWidth = s_barWidth;
		m_backBufferHeight = barHeight;
	}

	// If the bar height has changed and we have more lines than vertical pixels, redraw the code image.
	if( (m_numLines > barHeight) && (m_codeImgHeight != barHeight) )
		m_codeImgDirty = true;

	// Don't refresh the image while the preview is shown.
	if(!m_codeImg || (m_codeImgDirty && !g_previewShown))
	{
		m_codeImgDirty = false;
		RefreshCodeImg(barHeight);
		// Re-arm the refresh timer.
		SetTimer(m_handles.vert, REFRESH_CODE_TIMER_ID, REFRESH_CODE_INTERVAL, 0);
	}

	// Blit the code image and fill the remaining space with the whitespace color.
	BitBlt(m_backBufferDC, clRect.left, clRect.top, s_barWidth, m_codeImgHeight, m_imgDC, 0, 0, SRCCOPY);
	RECT remainingRect = clRect;
	remainingRect.top += m_codeImgHeight;
	FillSolidRect(m_backBufferDC, s_whitespaceColor, remainingRect);

	// Compute the size and position of the current page marker.
	int range = m_scrollMax - m_scrollMin - m_pageSize + 2;
	int cursor = m_scrollPos - m_scrollMin;
	float normFact = (m_numLines < barHeight) ? 1.0f : 1.0f * barHeight / range;
	int normalizedCursor = int(cursor * normFact);
	int normalizedPage = int(m_pageSize * normFact);
	normalizedPage = std::max(/*15*/5, normalizedPage);	// par min cur
	if(normalizedPage > barHeight)
		normalizedPage = barHeight;
	if(clRect.top + normalizedCursor + normalizedPage > clRect.bottom)
		normalizedCursor = barHeight - normalizedPage;

	// Overlay the current page marker. Since the bitmap is upside down, we must flip the cursor.
	unsigned char opacity = (unsigned char)(s_cursorColor >> 24);
	unsigned char cursorColor[3] =
	{
		s_cursorColor & 0xff,
		(s_cursorColor >> 8) & 0xff,
		(s_cursorColor >> 16) & 0xff
	};

	unsigned char* end = (unsigned char*)(m_backBufferBits + (barHeight - normalizedCursor) * s_barWidth);
	unsigned char* pixel = end - normalizedPage * s_barWidth * 4;
	for(; pixel < end; pixel += 4)
	{
		for(int i = 0; i < 3; ++i)
		{
			int p = (opacity*pixel[i])/255 + cursorColor[i];
			pixel[i] = (p <= 255) ? (unsigned char)p : 255;
		}
	}
	// cursor frame |
	if (s_frameColor != s_cursorColor)
	{
		unsigned int* pEnd = (unsigned int*)(m_backBufferBits + (barHeight - normalizedCursor) * s_barWidth);
		unsigned int* pPixel = pEnd - normalizedPage * s_barWidth;
		for(; pPixel < pEnd; pPixel += s_barWidth)
		{
			*pPixel = s_frameColor;
			*(pPixel + s_barWidth-1) = s_frameColor;
		}
	}

	// Blit the backbuffer onto the control.
	BitBlt(ctrlDC, clRect.left, clRect.top, s_barWidth, barHeight, m_backBufferDC, 0, 0, SRCCOPY);
}

//  Default settings
//------------------------------------------------------------------------------------------------------------------
void MetalBar::ResetSettings()
{
	s_barWidth = 64;
	s_whitespaceColor = 0xfff5f5f5;	s_upperCaseColor = 0xff101010;	s_characterColor = 0xff808080;
	s_numberColor = 0x0ff808080;	s_operatorColor = 0xff404040;	s_stringColor = 0xff606060;		s_preprocColor = 0xffC0C0C0;
	s_commentColor = 0xff008000;	s_cursorColor = 0xe0000028;		s_frameColor = 0xff000000;
	s_matchColor = 0xffff8000;	s_modifiedLineColor = 0xff0000ff;	s_unsavedLineColor = 0xffe1621e;
	s_breakpointColor = 0xffff0000;	s_bookmarkColor = 0xff0000ff;	s_bookmarkDarkColor = 0xff000080;	s_bookmarkSize = 1;
	s_requireAlt = TRUE;	s_bFindCase = false;	s_bFindWhole = false;	s_FindSize = 2;  s_FindSize2 = 2;
	s_codePreviewBg = 0xffffffe1;	s_keywordColor = 0xff000000;	s_PrvWidth = 80;	s_PrvHeight = 15;
	s_PrvFontSize = 12;		s_topSplit = 0;
	UpdBookmClrT();
}

void MetalBar::UpdBookmClrT()
{
	int clrB1[3] =
	{
		s_bookmarkColor & 0xff,
		(s_bookmarkColor >> 8) & 0xff,
		(s_bookmarkColor >> 16) & 0xff
	};
	int clrB2[3] =
	{
		s_bookmarkDarkColor & 0xff,
		(s_bookmarkDarkColor >> 8) & 0xff,
		(s_bookmarkDarkColor >> 16) & 0xff
	};
	for (int i=0; i < BOOKM_LEVELS; i++)
	{
		float f = float(i)/float(BOOKM_LEVELS-1);
		s_bookmarkClrT[i] = 0xff000000
			+ ( (int)(clrB1[0] + (float)(clrB2[0] - clrB1[0]) * f) )
			+ ( (int)(clrB1[1] + (float)(clrB2[1] - clrB1[1]) * f) ) *256
			+ ( (int)(clrB1[2] + (float)(clrB2[2] - clrB1[2]) * f) ) *256*256;
}
}


bool MetalBar::ReadRegInt(unsigned int* to, HKEY key, const char* name)
{
	unsigned int val;
	DWORD type = REG_DWORD;
	DWORD size = sizeof(val);
	if(RegQueryValueExA(key, name, 0, &type, (LPBYTE)&val, &size) != ERROR_SUCCESS)
		return false;

	if(type != REG_DWORD)
		return false;

	*to = val;
	return true;
}

//  Load settings
//------------------------------------------------------------------------------------------------------------------
void MetalBar::ReadSettings()
{
	// Make sure we have sane defaults, in case stuff is missing.
	ResetSettings();

	HKEY key;
	if(RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Griffin Software\\MetalScroll", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
		return;

	ReadRegInt(&s_barWidth, key, "BarWidth");
	ReadRegInt(&s_whitespaceColor, key, "WhitespaceColor");
	ReadRegInt(&s_upperCaseColor, key, "UpperCaseColor");
	ReadRegInt(&s_characterColor, key, "OtherCharColor");

	ReadRegInt(&s_numberColor,	key, "NumberColor");
	ReadRegInt(&s_operatorColor,key, "OperatorColor");
	ReadRegInt(&s_stringColor,	key, "StringColor");
	ReadRegInt(&s_preprocColor,	key, "PreprocColor");

	ReadRegInt(&s_commentColor, key, "CommentColor");
	ReadRegInt(&s_cursorColor, key, "CursorColor");
	ReadRegInt(&s_frameColor,	key, "CursorFrameColor");
	
	ReadRegInt(&s_matchColor, key, "MatchedWordColor");
	ReadRegInt(&s_modifiedLineColor, key, "ModifiedLineColor");
	ReadRegInt(&s_unsavedLineColor, key, "UnsavedLineColor");
	
	ReadRegInt(&s_breakpointColor, key, "BreakpointColor");
	ReadRegInt(&s_bookmarkColor, key, "BookmarkColor");
	ReadRegInt(&s_bookmarkDarkColor,key, "BookmarkDarkColor");
	ReadRegInt(&s_bookmarkSize,		key, "BookmarkSize");	UpdBookmClrT();
	
	ReadRegInt(&s_requireAlt,	key, "RequireALT");
	ReadRegInt(&s_bFindCase,	key, "FindCaseSensitive");
	ReadRegInt(&s_bFindWhole,	key, "FindWholeWord");
	ReadRegInt(&s_FindSize,		key, "FindSize");
	ReadRegInt(&s_FindSize2,	key, "FindSize2");
	
	ReadRegInt(&s_codePreviewBg, key, "CodePreviewBg");
	ReadRegInt(&s_keywordColor,	key, "KeywordColor");
	ReadRegInt(&s_PrvWidth,		key, "CodePreviewWidth");
	ReadRegInt(&s_PrvHeight,	key, "CodePreviewHeight");

	ReadRegInt(&s_PrvFontSize,	key, "PreviewFontHeight");  //,REG_SZ
	ReadRegInt(&s_topSplit,		key, "TopSplitterHeight");

	RegCloseKey(key);
}

void MetalBar::WriteRegInt(HKEY key, const char* name, unsigned int val)
{
	RegSetValueExA(key, name, 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
}

//  Save settings
//------------------------------------------------------------------------------------------------------------------
void MetalBar::SaveSettings()
{
	HKEY key;
	if(RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Griffin Software\\MetalScroll", 0, 0, 0, KEY_SET_VALUE, 0, &key, 0) != ERROR_SUCCESS)
		return;

	WriteRegInt(key, "BarWidth", s_barWidth);
	WriteRegInt(key, "WhitespaceColor", s_whitespaceColor);
	WriteRegInt(key, "UpperCaseColor", s_upperCaseColor);
	WriteRegInt(key, "OtherCharColor", s_characterColor);

	WriteRegInt(key, "NumberColor",		s_numberColor);
	WriteRegInt(key, "OperatorColor",	s_operatorColor);
	WriteRegInt(key, "StringColor",		s_stringColor);
	WriteRegInt(key, "PreprocColor",	s_preprocColor);

	WriteRegInt(key, "CommentColor", s_commentColor);
	WriteRegInt(key, "CursorColor", s_cursorColor);
	 WriteRegInt(key, "CursorFrameColor",s_frameColor);

	WriteRegInt(key, "MatchedWordColor", s_matchColor);
	WriteRegInt(key, "ModifiedLineColor", s_modifiedLineColor);
	WriteRegInt(key, "UnsavedLineColor", s_unsavedLineColor);
	
	WriteRegInt(key, "BreakpointColor", s_breakpointColor);
	WriteRegInt(key, "BookmarkColor", s_bookmarkColor);
	WriteRegInt(key, "BookmarkDarkColor",	s_bookmarkDarkColor);
	 WriteRegInt(key, "BookmarkSize",		s_bookmarkSize);
	 
	WriteRegInt(key, "RequireALT",			s_requireAlt);
	WriteRegInt(key, "FindCaseSensitive",	s_bFindCase);
	 WriteRegInt(key, "FindWholeWord",		s_bFindWhole);
	 WriteRegInt(key, "FindSize",			s_FindSize);
	 WriteRegInt(key, "FindSize2",			s_FindSize2);
	 
	WriteRegInt(key, "CodePreviewBg", s_codePreviewBg);
	WriteRegInt(key, "KeywordColor",		s_keywordColor);
	WriteRegInt(key, "CodePreviewWidth",	s_PrvWidth);
	 WriteRegInt(key, "CodePreviewHeight",	s_PrvHeight);

	 WriteRegInt(key, "PreviewFontHeight",	s_PrvFontSize);
	 WriteRegInt(key, "TopSplitterHeight",	s_topSplit);
	  
	RegCloseKey(key);

	// Refresh all the scrollbars.
	for(std::set<MetalBar*>::iterator it = s_bars.begin(); it != s_bars.end(); ++it)
	{
		MetalBar* bar = *it;
		bar->m_codeImgDirty = true;
		bar->AdjustSize(s_barWidth, 0);
		InvalidateRect(bar->m_handles.vert, 0, 0);
	}

	// Resize the code preview window.
	g_codePreviewWnd.Resize(s_PrvWidth, s_PrvHeight);
}

//  HighLights
//------------------------------------------------------------------------------------------------------------------
void MetalBar::RemoveWordHighlight(IVsTextLines* buffer)
{
	m_highlights.clear();
	struct DeleteMarkerOp : public MarkerOperator
	{
		void Process(IVsTextLineMarker* marker, int /*idx*/) const
		{
			marker->Invalidate();
		}
	};

	ProcessLineMarkers(buffer, g_highlightMarkerType, DeleteMarkerOp());
	m_highlightWord = (wchar_t*)0;
}

void MetalBar::HighlightMatchingWords()
{
	CComPtr<IVsTextLines> buffer;
	CComBSTR allText;
	long numLines;
	if(!GetBufferAndText(&buffer, &allText, &numLines))
		return;

	RemoveWordHighlight(buffer);

	HRESULT hr = m_view->GetSelectedText(&m_highlightWord);
	if(FAILED(hr) || !m_highlightWord)
	{
		m_highlightWord = (wchar_t*)0;
		return;
	}

	unsigned int selTextLen = m_highlightWord.Length();
	if(selTextLen < 1)
	{
		m_highlightWord = (wchar_t*)0;
		return;
	}

	bool allSpaces = true;
	for(unsigned int i = 0; i < selTextLen; ++i)
	{
		if( (m_highlightWord[i] != L'\t') && (m_highlightWord[i] != L' ') )
		{
			allSpaces = false;
			break;
		}
	}

	if(allSpaces)
	{
		m_highlightWord = (wchar_t*)0;
		return;
	}

	int line = 0;
	int column = 0;
	for(wchar_t* chr = allText; *chr; ++chr)
	{
		// Check for newline.
		if( (chr[0] == L'\r') || (chr[0] == L'\n') )
		{
			// In case of CRLF, eat the next character.
			if( (chr[0] == L'\r') && (chr[1] == L'\n') )
				++chr;
			++line;
			column = 0;
			continue;
		}

		///  find math options +
		bool found = false, whole = true;

		if (s_bFindWhole)
			whole = (chr == allText || IsCppIdSeparator(chr[-1])) && IsCppIdSeparator(chr[selTextLen]);

		if (s_bFindCase)
			found = wcsncmp(chr, m_highlightWord, selTextLen) == 0;
		else
			found = wcsnicmp(chr, m_highlightWord, selTextLen) == 0;

		if (found && whole)
		{
			buffer->CreateLineMarker(g_highlightMarkerType, line, column, line, column + selTextLen, 0, 0);

			/// + add highlight for internal use
			m_highlights.push_back(stFindHighlight(column, line));
			
			// Make sure we don't create overlapping markers.
			chr += selTextLen - 1;
			column += selTextLen - 1;
		}

		++column;
	}
}

void MetalBar::Init()
{
	ReadSettings();

	InitScaler();
	CodePreview::Register();
	OptionsDialog::Init();

	g_codePreviewWnd.Create(g_mainVSHwnd, s_PrvWidth, s_PrvHeight);
}

void MetalBar::Uninit()
{
	g_codePreviewWnd.Destroy();

	RemoveAllBars();
	CodePreview::Unregister();
	OptionsDialog::Uninit();
}

void MetalBar::SetBarsEnabled(unsigned int enabled)
{
	if(s_enabled == enabled)
		return;

	s_enabled = enabled;
	HKEY key;
	if(RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Griffin Software\\MetalScroll", 0, 0, 0, KEY_SET_VALUE, 0, &key, 0) == ERROR_SUCCESS)
	{
		WriteRegInt(key, "BarEnabled", s_enabled);
		RegCloseKey(key);
	}

	int width = enabled ? s_barWidth : GetSystemMetrics(SM_CXVSCROLL);
	for(std::set<MetalBar*>::iterator it = s_bars.begin(); it != s_bars.end(); ++it)
	{
		MetalBar* bar = *it;
		bar->AdjustSize(width, 0);
		InvalidateRect(bar->GetHwnd(), 0, TRUE);
		UpdateWindow(bar->GetHwnd());
	}
}
