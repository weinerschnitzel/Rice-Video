/*

  Copyright (C) 2003-2009 Rice1964

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

#include "..\stdafx.h"

CGraphicsContext* CGraphicsContext::g_pGraphicsContext = NULL;
bool CGraphicsContext::needCleanScene = false;
int CGraphicsContext::m_FullScreenResolutions[40][2] = {
	{ 320, 200 }, { 400, 300 }, { 480, 360 }, { 512, 384 }, { 640, 480 },
	{ 800, 600 }, { 1024, 768 }, { 1152, 864 }, { 1280, 960 },
	{ 1400, 1050 }, { 1600, 1200 }, { 1920, 1440 }, { 2048, 1536 } };;
int CGraphicsContext::m_numOfResolutions = 0;

CGraphicsContext * CGraphicsContext::Get(void)
{	
	return CGraphicsContext::g_pGraphicsContext;
}
	
CGraphicsContext::CGraphicsContext() :
	m_bReady(false), 
	m_bActive(false),
	m_bWindowed(true)
{
}
CGraphicsContext::~CGraphicsContext()
{
	g_pFrameBufferManager->CloseUp();
}

HWND		CGraphicsContext::m_hWnd=NULL;
HWND		CGraphicsContext::m_hWndStatus=NULL;
HMENU		CGraphicsContext::m_hMenu=NULL;
uint32		CGraphicsContext::m_dwWindowStyle=0;     // Saved window style for mode switches
uint32		CGraphicsContext::m_dwWindowExStyle=0;   // Saved window style for mode switches
uint32		CGraphicsContext::m_dwStatusWindowStyle=0;     // Saved window style for mode switches

void CGraphicsContext::InitWindowInfo()
{
	m_hWnd = g_GraphicsInfo.hWnd;

	m_hWndStatus = g_GraphicsInfo.hStatusBar;

	m_hMenu = GetMenu(m_hWnd);

	// Save window properties
	m_dwWindowStyle = GetWindowLong( m_hWnd, GWL_STYLE );
	m_dwWindowExStyle = GetWindowLong( m_hWnd, GWL_EXSTYLE );
	m_dwStatusWindowStyle = GetWindowLong( m_hWndStatus, GWL_STYLE );

	RECT rcStatus;

	// Add extra margin for the status bar
	windowSetting.statusBarHeight = 0;
	if ( IsWindow( m_hWndStatus ) )
	{
		// Add on enough space for the status bar
		GetClientRect(m_hWndStatus, &rcStatus);
		windowSetting.statusBarHeight = (rcStatus.bottom - rcStatus.top);
	}
}


bool CGraphicsContext::Initialize(HWND hWnd, HWND hWndStatus, uint32 dwWidth, uint32 dwHeight, BOOL bWindowed )
{
	if( windowSetting.bDisplayFullscreen )
	{
		windowSetting.uDisplayWidth  = windowSetting.uFullScreenDisplayWidth;
		windowSetting.uDisplayHeight = windowSetting.uFullScreenDisplayHeight;
	}
	else
	{
		windowSetting.uDisplayWidth  = windowSetting.uWindowDisplayWidth;
		windowSetting.uDisplayHeight = windowSetting.uWindowDisplayHeight;
	}
	
	RECT rcScreen;
	SetRect(&rcScreen, 0,0, windowSetting.uDisplayWidth, windowSetting.uDisplayHeight);
	rcScreen.bottom += windowSetting.statusBarHeight;

	// Calculate window size to give desired window size...
	AdjustWindowRectEx(&rcScreen, m_dwWindowStyle & (~(WS_THICKFRAME|WS_MAXIMIZEBOX)), TRUE, m_dwWindowExStyle);
	SetWindowPos(m_hWnd, 0, rcScreen.left, rcScreen.top, rcScreen.right-rcScreen.left, rcScreen.bottom-rcScreen.top,
		SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);

	GetWindowRect( m_hWnd, &m_rcWindowBounds );

    EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &m_DMsaved);

	if( !m_bWindowed )
		ShowCursor( FALSE );
	else
		ShowCursor( TRUE );

	g_pFrameBufferManager->Initialize();

	return true;
}

void CGraphicsContext::CleanUp()
{
    m_bActive = false;
    m_bReady  = false;

	if ( IsWindow( m_hWnd ) )
	{
		SetWindowLong( m_hWnd, GWL_STYLE, m_dwWindowStyle );
	}

	if ( IsWindow(m_hWndStatus) )
	{
		SetWindowLong( m_hWndStatus, GWL_STYLE, m_dwStatusWindowStyle);
	}
}

int _cdecl SortResolutionsCallback( const VOID* arg1, const VOID* arg2 )
{
	UINT* p1 = (UINT*)arg1;
	UINT* p2 = (UINT*)arg2;

	if( *p1 < *p2 )   
		return -1;
	else if( *p1 > *p2 )   
		return 1;
	else 
	{
		if( p1[1] < p2[1] )   
			return -1;
		else if( p1[1] > p2[1] )   
			return 1;
		else
			return 0;
	}
}

// This is a static function, will be called when the plugin DLL is initialized
void CGraphicsContext::InitDeviceParameters(void)
{
	// Initialize common device parameters

	int i=0,j;
	DEVMODE deviceMode;
	CGraphicsContext::m_numOfResolutions=0;
	memset(&CGraphicsContext::m_FullScreenResolutions, 0, 40*2*sizeof(int));

	while (EnumDisplaySettings( NULL, i, &deviceMode ) != 0)
	{
		//Lets ensure that the current display resolution is not already in our list
		for (j = 0; j < CGraphicsContext::m_numOfResolutions; j++)
		{
			if ((deviceMode.dmPelsWidth  == CGraphicsContext::m_FullScreenResolutions[j][0]) &&
				(deviceMode.dmPelsHeight == CGraphicsContext::m_FullScreenResolutions[j][1]))
			{
				break;
			}
		}

		//If where currently in the top position, add to our list
		if (j == CGraphicsContext::m_numOfResolutions)
		{
			CGraphicsContext::m_FullScreenResolutions[CGraphicsContext::m_numOfResolutions][0] = deviceMode.dmPelsWidth;
			CGraphicsContext::m_FullScreenResolutions[CGraphicsContext::m_numOfResolutions][1] = deviceMode.dmPelsHeight;
			CGraphicsContext::m_numOfResolutions++;
		}
		i++;
	}

	qsort( &CGraphicsContext::m_FullScreenResolutions, CGraphicsContext::m_numOfResolutions, sizeof(int)*2, SortResolutionsCallback );

	// To initialze device parameters for DirectX
	CDXGraphicsContext::InitDeviceParameters();
}

void OutputText(char *msg, RECT *prect, uint32 flag)
{

	HDC hdc = GetDC(g_GraphicsInfo.hWnd);
	if( hdc )
	{
		SelectObject(hdc,GetStockObject(SYSTEM_FONT));
		SelectObject(hdc,GetStockObject(BLACK_BRUSH));
		Rectangle(hdc,prect->left,prect->top,prect->right,prect->bottom);
		SetBkColor(hdc, 0x00000000);

		SetTextColor(hdc,0x00FFFFFF);
		::DrawText(hdc, msg, strlen(msg), prect, flag);
		//TextOut(hdc,prect->left,prect->bottom,msg, strlen(msg));
		ReleaseDC(g_GraphicsInfo.hWnd,hdc);
	}
}