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

struct MarkerOperator
{
	virtual void NotifyCount(int /*numMarkers*/) const {}
	virtual void Process(IVsTextLineMarker* marker, int idx) const = 0;
};

void ProcessLineMarkers(IVsTextLines* buffer, int type, const MarkerOperator& op);

enum LineFlags
{
	LineFlag_Hidden				= 0x01,
	LineFlag_ChangedUnsaved		= 0x02,
	LineFlag_ChangedSaved		= 0x04,
	LineFlag_Breakpoint			= 0x08,
	LineFlag_Bookmark			= 0x10,
	LineFlag_Match				= 0x20
};

enum TextFlags
{
	TextFlag_Comment			= 0x01,
	TextFlag_Highlight			= 0x02,
	TextFlag_Keyword	= 0x04,
	TextFlag_String		= 0x08,
	TextFlag_Preproc	= 0x10 //
};

struct RenderOperator
{
	virtual void Init(int numLines) = 0;
	virtual void EndLine(int line, int lastColumn, unsigned int lineFlags, bool textEnd) = 0;
	virtual void RenderSpaces(int line, int column, int count) = 0;
	virtual void RenderCharacter(int line, int column, wchar_t chr, unsigned int flags) = 0;
};

int RenderText(RenderOperator& renderOp, IVsTextView* view, IVsTextLines* buffer, const wchar_t* text, int numLines);
