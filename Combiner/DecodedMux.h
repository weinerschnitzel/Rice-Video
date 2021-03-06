/*
Copyright (C) 2002-2009 Rice1964

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef _DECODEDMUX_H_
#define _DECODEDMUX_H_

class DecodedMux
{
public:
	union {
		struct {
			uint8 aRGB0;
			uint8 bRGB0;
			uint8 cRGB0;
			uint8 dRGB0;
			
			uint8 aA0;
			uint8 bA0;
			uint8 cA0;
			uint8 dA0;
			
			uint8 aRGB1;
			uint8 bRGB1;
			uint8 cRGB1;
			uint8 dRGB1;
			
			uint8 aA1;
			uint8 bA1;
			uint8 cA1;
			uint8 dA1;
		};
		uint8  m_bytes[16];
	};
	
	union {
		struct {
			uint32 m_dwMux0;
			uint32 m_dwMux1;
		};
		uint64 m_u64Mux;
	};

	bool m_bTexel0IsUsed;
	bool m_bTexel1IsUsed;

	void Decode(uint32 dwMux0, uint32 dwMux1);
	void Hack(void);
	bool isUsed(uint8 fac, uint8 mask=MUX_MASK);
	bool isUsedInCycle(uint8 fac, int cycle, CombineChannel channel, uint8 mask=MUX_MASK);
	bool isUsedInCycle(uint8 fac, int cycle, uint8 mask=MUX_MASK);
	void CheckCombineInCycle1(void);
	void Simplify(void);

	void ReplaceVal(uint8 val1, uint8 val2, int cycle= -1, uint8 mask = MUX_MASK);

#ifdef _DEBUG
	void DisplayMuxString(const char *prompt);
	void DisplaySimpliedMuxString(const char *prompt);
	void DisplayConstantsWithShade(uint32 flag,CombineChannel channel);
#else
	void DisplayMuxString(const char *prompt) {}
	void DisplaySimpliedMuxString(const char *prompt){}
	void DisplayConstantsWithShade(uint32 flag,CombineChannel channel){}
#endif

	DecodedMux()
	{
		memset(m_bytes, 0, sizeof(m_bytes));
	}
	
	virtual ~DecodedMux() {}
} ;

#endif


