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
#include "TextFormatting.h"
#include "CppLexer.h"

extern CComPtr<EnvDTE80::DTE2>		g_dte;
extern long							g_highlightMarkerType;
extern CComPtr<IVsTextManager>		g_textMgr;

static const GUID					g_cppLangGUID		= { 0xB2F072B0, 0xABC1, 0x11D0, { 0x9D, 0x62, 0x00, 0xC0, 0x4F, 0xD9, 0xDF, 0xD9 } };

//															 {694DD9B6   -B865   -4C5B     -AD    85   -86    35    6E    9C    88    DC}
static const GUID					g_csharpLangGUID	= { 0x694DD9B6, 0xB865, 0x4C5B, { 0xAD, 0x85, 0x86, 0x35, 0x6E, 0x9C, 0x88, 0xDC } };

//															 {21feefb5   -ace1   -4461     -ba    7c   -6f    66    45    74    45    fd}
static const GUID					g_uscriptGUID		= { 0x21feefb5, 0xace1, 0x4461, { 0xba, 0x7c, 0x6f, 0x66, 0x45, 0x74, 0x45, 0xfd } };


struct Highlight
{
	unsigned int				start;
	unsigned int				end;
	Highlight*					next;
};

struct LineInfo
{
	unsigned int				flags;
	Highlight*					highlights;
};

typedef std::vector<LineInfo>	LineList;
typedef std::vector<Highlight>	HighlightList;


void ProcessLineMarkers(IVsTextLines* buffer, int type, const MarkerOperator& op)
{
	long numLines;
	HRESULT hr = buffer->GetLineCount(&numLines);
	if(FAILED(hr))
		return;

	CComPtr<IVsEnumLineMarkers> enumMarkers;
	hr = buffer->EnumMarkers(0, 0, numLines, 0, type, 0, &enumMarkers);
	if(FAILED(hr) || !enumMarkers)
		return;

	long numMarkers;
	hr = enumMarkers->GetCount(&numMarkers);
	if(FAILED(hr))
		return;

	op.NotifyCount(numMarkers);

	for(int idx = 0; idx < numMarkers; ++idx)
	{
		CComPtr<IVsTextLineMarker> marker;
		hr = enumMarkers->Next(&marker);
		if(FAILED(hr))
			break;

		op.Process(marker, idx);
	}
}

static bool GetFileName(CComBSTR& name, IVsTextLines* buffer)
{
	// Behold the absolutely ridiculous way of getting the file name for a given text buffer.
	CComPtr<IDispatch> disp;
	HRESULT hr = buffer->CreateEditPoint(0, 0, &disp);
	if(FAILED(hr) || !disp)
		return false;

	CComQIPtr<EnvDTE::EditPoint> ep = disp;
	if(!ep)
		return false;

	CComPtr<EnvDTE::TextDocument> txtDoc;
	hr = ep->get_Parent(&txtDoc);
	if(FAILED(hr) || !txtDoc)
		return false;

	CComPtr<EnvDTE::Document> doc;
	hr = txtDoc->get_Parent(&doc);
	if(FAILED(hr) || !doc)
		return false;

	hr = doc->get_FullName(&name);
	return SUCCEEDED(hr) && name;
}

static void FindBreakpoints(LineList& lines, IVsTextLines* buffer)
{
	CComBSTR fileName;
	if(!GetFileName(fileName, buffer))
		return;

	CComPtr<EnvDTE::Debugger> debugger;
	HRESULT hr = g_dte->get_Debugger(&debugger);
	if(FAILED(hr) || !debugger)
		return;

	CComPtr<EnvDTE::Breakpoints> breakpoints;
	hr = debugger->get_Breakpoints(&breakpoints);
	if(FAILED(hr) || !breakpoints)
		return;

	long numBp;
	hr = breakpoints->get_Count(&numBp);
	if(FAILED(hr))
		return;

	for(int bpIdx = 1; bpIdx <= numBp; ++bpIdx)
	{
		CComPtr<EnvDTE::Breakpoint> breakpoint;
		hr = breakpoints->Item(CComVariant(bpIdx), &breakpoint);
		if(FAILED(hr) || !breakpoint)
			continue;

		EnvDTE::dbgBreakpointLocationType bpType;
		hr = breakpoint->get_LocationType(&bpType);
		if( FAILED(hr) || (bpType != EnvDTE::dbgBreakpointLocationTypeFile) )
			continue;

		CComBSTR bpFile;
		hr = breakpoint->get_File(&bpFile);
		if(FAILED(hr) || !bpFile)
			continue;

		if(_wcsicmp(bpFile, fileName) != 0)
			continue;

		long line;
		hr = breakpoint->get_FileLine(&line);
		if(FAILED(hr))
			continue;

		lines[line - 1].flags |= LineFlag_Breakpoint;
	}
}

static void FindHiddenLines(LineList& lines, IVsTextLines* buffer)
{
	CComQIPtr<IServiceProvider> sp = g_dte;
	if(!sp)
		return;

	CComPtr<IVsHiddenTextManager> hiddenMgr;
	HRESULT hr = sp->QueryService(SID_SVsTextManager, IID_IVsHiddenTextManager, (void**)&hiddenMgr);
	if(FAILED(hr) || !hiddenMgr)
		return;

	CComPtr<IVsHiddenTextSession> hiddenSess;
	hr = hiddenMgr->GetHiddenTextSession(buffer, &hiddenSess);
	if(FAILED(hr) || !hiddenSess)
		return;

	CComPtr<IVsEnumHiddenRegions> hiddenEnum;
	hr = hiddenSess->EnumHiddenRegions(FHR_ALL_REGIONS, 0, 0, &hiddenEnum);
	if(FAILED(hr) || !hiddenEnum)
		return;

	ULONG numRegions;
	IVsHiddenRegion* region;
	while( SUCCEEDED(hiddenEnum->Next(1, &region, &numRegions)) && numRegions )
	{
		DWORD state;
		region->GetState(&state);
		if(state == chrDefault)
		{
			TextSpan span;
			region->GetSpan(&span);
			for(int l = span.iStartLine + 1; l <= span.iEndLine; ++l)
				lines[l].flags |= LineFlag_Hidden;
		}

		region->Release();
	}
}

static void GetLineFlags(LineList& lines, IVsTextLines* buffer)
{
	FindHiddenLines(lines, buffer);

	struct MarkRangeOp : public MarkerOperator
	{
		MarkRangeOp(LineList& lines_, unsigned int flag_) : lines(lines_), flag(flag_) {}

		void Process(IVsTextLineMarker* marker, int /*idx*/) const
		{
			TextSpan span;
			marker->GetCurrentSpan(&span);
			for(int i = span.iStartLine; i <= span.iEndLine; ++i)
				lines[i].flags |= flag;
		}

		LineList& lines;
		unsigned int flag;
	};

	// The magic IDs for the changed lines are not in the MARKERTYPE enum, because they are useful and
	// we wouldn't want people to have access to useful stuff. I found them by disassembling RockScroll.
	ProcessLineMarkers(buffer, 0x13, MarkRangeOp(lines, LineFlag_ChangedUnsaved));
	ProcessLineMarkers(buffer, 0x14, MarkRangeOp(lines, LineFlag_ChangedSaved));
	ProcessLineMarkers(buffer, MARKER_BOOKMARK, MarkRangeOp(lines, LineFlag_Bookmark));

	// Breakpoints must be retrieved in a different way.
	FindBreakpoints(lines, buffer);
}

static void GetHighlights(LineList& lines, IVsTextLines* buffer, HighlightList& storage)
{
	struct AddHighlightOp : public MarkerOperator
	{
		AddHighlightOp(LineList& lines_, HighlightList& storage_) : lines(lines_), storage(storage_) {}

		void NotifyCount(int numMarkers) const { storage.resize(numMarkers); }

		void Process(IVsTextLineMarker* marker, int idx) const
		{
			TextSpan span;
			marker->GetCurrentSpan(&span);
			assert(span.iStartLine == span.iEndLine);

			LineInfo& line = lines[span.iStartLine];
			Highlight* newh = &storage[idx];
			newh->start = span.iStartIndex;
			newh->end = span.iEndIndex;

			// Add a marker on the right to make it easier to see.
			if(!line.highlights)
				lines[span.iStartLine].flags |= LineFlag_Match;

			// Preserve ordering. Since this is a linear search, it sounds like it would lead to quadratic behavior
			// (well, O(lines*matches_per_line), actually) but it doesn't. VS pushes newly created markers at the top
			// of a list; we create them in ascending order, by scanning the text, so they end up in reverse order in
			// the list. This means we get them in descending order here, so we add them to our list in constant time
			// in the following if block. Obviously, there's no guarantee to this, which is why we do the full scan if
			// needed, but it seems to hold true in all the scenarios I've tested.
			Highlight* h = line.highlights;
			if(!h || (h->start > newh->start))
			{
				newh->next = h;
				line.highlights = newh;
				return;
			}

			while(h->next && (newh->start > h->next->start))
				h = h->next;

			newh->next = h->next;
			h->next = newh;
		}

		LineList& lines;
		HighlightList& storage;
	};

	ProcessLineMarkers(buffer, g_highlightMarkerType, AddHighlightOp(lines, storage));
}

typedef bool(*IsKeywordFnPtr)(const wchar_t* c, unsigned int l);
bool IsUscriptKeyword(const wchar_t* c, unsigned int l);

int RenderText(RenderOperator& renderOp, IVsTextView* view, IVsTextLines* buffer, const wchar_t* text, int numLines)
{
	LANGPREFERENCES langPrefs;
	int wrapAfter = INT_MAX;
	int tabSize = 4;
	
	bool isCppLikeLanguage = false;
	bool isUScript = false;

	IsKeywordFnPtr keywordFn = 0;

	if( SUCCEEDED(buffer->GetLanguageServiceID(&langPrefs.guidLang)) && SUCCEEDED(g_textMgr->GetUserPreferences(0, 0, &langPrefs, 0)) )
	{
		tabSize = langPrefs.uTabSize;
		isUScript = InlineIsEqualGUID(langPrefs.guidLang, g_uscriptGUID) ? true : false;
		isCppLikeLanguage = InlineIsEqualGUID(langPrefs.guidLang, g_cppLangGUID) || 
							InlineIsEqualGUID(langPrefs.guidLang, g_csharpLangGUID) ||
							isUScript;
		if(isUScript)
			keywordFn = IsUscriptKeyword;
		else if(isCppLikeLanguage)
			keywordFn = IsCppKeyword;

		if(langPrefs.fWordWrap)
		{
			long min, max, pageWidth, pos;
			if(SUCCEEDED(view->GetScrollInfo(SB_HORZ, &min, &max, &pageWidth, &pos)) && (pageWidth > 1))
				wrapAfter = pageWidth - 1;
		}
	}

	if(numLines < 1)
		numLines = 1;

	LineInfo defaultLineInfo = { 0 };
	LineList lines(numLines, defaultLineInfo);
	GetLineFlags(lines, buffer);
	HighlightList highlightStorage;
	GetHighlights(lines, buffer, highlightStorage);

	renderOp.Init(numLines);

	enum CommentType
	{
		CommentType_None,
		CommentType_SingleLine,
		CommentType_MultiLine
	} commentType = CommentType_None;
	bool inKeyword = false;
	bool inString = false;

	// Here "virtual" refers to rendered coordinates, which can differ from real text coordinates due to word wrapping,
	// hidden text regions and tabs.
	int virtualLine = 0;
	int virtualColumn = 0;
	int realLine = 0;
	int realColumn = 0;
	Highlight* crHighlight = lines[0].highlights;

	for(const wchar_t* chr = text; ; ++chr)
	{
		// Check for a real newline, a virtual newline (due to word wrapping) or the end of the text.
		bool isRealNewline = (chr[0] == L'\r') || (chr[0] == L'\n');
		bool isVirtualNewline = (virtualColumn >= wrapAfter);
		bool isTextEnd = (chr[0] == 0);
		bool isLineVisible = !(lines[realLine].flags & LineFlag_Hidden);
		if(isRealNewline || isVirtualNewline || isTextEnd)
		{
			if(isLineVisible)
			{
				if(isVirtualNewline && !isRealNewline)
				{
					CharClass crChrClass = GetCharClass(*chr);
					if(crChrClass != CharClass_Other)
					{
						// Go back looking for the beginning of the current "word", i.e. for the first character which doesn't
						// match the class of the current character, or the (virtual) start of the line.
						int currentWordLen = 1;
						for(; currentWordLen <= virtualColumn; ++currentWordLen)
						{
							if(GetCharClass(chr[-currentWordLen]) != crChrClass)
							{
								--currentWordLen;
								break;
							}
						}

						// VS moves the entire word on the next line unless it starts on the 1st or 2nd column.
						if(virtualColumn - currentWordLen > 1)
						{
							// Rewind the text so we can draw the entire wrapped word on the next line.
							chr -= currentWordLen;
							realColumn -= currentWordLen;
							// Move back the virtual column so that the end of line handler can erase the
							// part of the word we've already painted.
							virtualColumn -= currentWordLen;
						}
					}
				}

				renderOp.EndLine(virtualLine, virtualColumn, lines[realLine].flags, isTextEnd);

				// Advance the virtual line.
				virtualColumn = 0;
				++virtualLine;
			}

			if(isTextEnd)
				break;

			if(isRealNewline)
			{
				++realLine;
				realColumn = 0;
				inKeyword = false;
				inString = false;

				// In case of CRLF, eat the next character too.
				if( (chr[0] == L'\r') && (chr[1] == L'\n') )
					++chr;

				crHighlight = lines[realLine].highlights;

				if(commentType == CommentType_SingleLine)
					commentType = CommentType_None;
				continue;
			}

			// If it's a virtual newline, we keep processing the current character.
		}

		int numChars = 1;
		if(*chr > L' ')
		{
			unsigned int textFlags = 0;

			switch(commentType)
			{
				case CommentType_None:
					if(!isCppLikeLanguage)
						break;

					if( !inString && (chr[0] == L'/') && (chr[1] == L'/') )
					{
						textFlags |= TextFlag_Comment;
						commentType = CommentType_SingleLine;
						inKeyword = false;
					}
					else if( !inString && (chr[0] == L'/') && (chr[1] == L'*') )
					{
						textFlags |= TextFlag_Comment;
						commentType = CommentType_MultiLine;
						inKeyword = false;
					}
					else if(chr[0] == L'"')
					{
						inKeyword = false;
						if(inString)
						{
							const wchar_t* backslashStart = chr - 1;
							while( (backslashStart >= text) && (*backslashStart == L'\\') )
								--backslashStart;
							int numBackslashes = int(chr - backslashStart - 1);
							if(numBackslashes % 2 == 0)
								inString = false;
						}
						else
							inString = true;
					}
					else if(!inKeyword && !inString && IsCppIdStart(chr[0]) && ((chr == text) || IsCppIdSeparator(chr[-1])) )
					{
						const wchar_t* keywordEnd = chr + 1;
						while(!IsCppIdSeparator(*keywordEnd))
							++keywordEnd;
						int len = int(keywordEnd - chr);
						inKeyword = keywordFn(chr, len);
					}
					else if(inKeyword && IsCppIdSeparator(chr[0]))
						inKeyword = false;
					break;

				case CommentType_SingleLine:
					textFlags |= TextFlag_Comment;
					break;

				case CommentType_MultiLine:
					textFlags |= TextFlag_Comment;
					if( (chr[-1] == L'*') && (chr[0] == L'/') )
						commentType = CommentType_None;
					break;
			}

			if(inKeyword)
				textFlags |= TextFlag_Keyword;
			if(inString)  //new
				textFlags |= TextFlag_String;
			//TextFlag_Preproc  ...

			// Advance the highlight interval, if needed.
			while(crHighlight && (realColumn >= (int)crHighlight->end))
				crHighlight = crHighlight->next;

			// Override the color with the match color if inside a marker.
			if(crHighlight && (realColumn >= (int)crHighlight->start))
				textFlags |= TextFlag_Highlight;

			if(isLineVisible)
				renderOp.RenderCharacter(virtualLine, virtualColumn, *chr, textFlags);
		}
		else
		{
			inKeyword = false;

			if(*chr == L'\t')
				numChars = tabSize - (virtualColumn % tabSize);

			if(isLineVisible)
				renderOp.RenderSpaces(virtualLine, virtualColumn, numChars);
		}

		++realColumn;
		virtualColumn += numChars;
	}

	assert(realLine == numLines - 1);
	return virtualLine;
}
