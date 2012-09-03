
#include <stdlib.h>
#include <malloc.h>

#include <stdio.h>

#include "VoDirectX.h"
#include "img_format.h"
#include "vfcap.h"

#ifdef __cplusplus
extern "C" {
#endif

#define fast_memcpy(a,b,c) memcpy(a,b,c)
#define VO_NOTIMPL	-1
#define VO_TRUE		1
#define VO_FALSE	0

static voconf_t* voconfs[MAX_CONF];
static int voconfsnum = 0; 
static int vo_adapter_num = 0;

static HINSTANCE hddraw_dll = NULL;             //handle to ddraw.dll
static int hddraw_uses = 0;
static int adapter_count=0;
static GUID selected_guid;
static GUID *selected_guid_ptr = NULL;
static RECT monitor_rect;	                        //monitor coordinates

typedef BOOL (WINAPI* tMyGetMonitorInfo)(HMONITOR, LPMONITORINFO);
tMyGetMonitorInfo myGetMonitorInfo = NULL;

static voconf_t* alloc_voconf(void) {
    int i,n;
    n=voconfsnum;
    for(i=0;i<voconfsnum;i++) {
        if(!voconfs[i]) {
            n=i;
            break;
        }
    }
    if(n==MAX_CONF)
        return NULL;
    if(n==voconfsnum)
        voconfsnum++;
    voconfs[n]=(voconf_t*)malloc(sizeof(voconf_t));
	memset(voconfs[n],0,sizeof(voconf_t));
	
	voconfs[n]->vo_doublebuffering = 1;
	voconfs[n]->WinID = -1;
	voconfs[n]->vo_border = 1;
	voconfs[n]->windowcolor = RGB(0,0,16);
    voconfs[n]->last_rect.left = 0xDEADC0DE;
    voconfs[n]->last_rect.top = 0xDEADC0DE;
    voconfs[n]->last_rect.right = 0xDEADC0DE;
    voconfs[n]->last_rect.bottom = 0xDEADC0DE; 	
	return voconfs[n];
}

static void free_voconf(voconf_t* voconf) {
    int i;
    for(i=0;i<voconfsnum;i++) {
        if(voconfs[i]==voconf) {
            voconfs[i]=NULL;
            free(voconf);
            break;
        }
    }
}

static voconf_t* seek_voconf(HWND hWnd) {
    int i;
    for(i=0;i<voconfsnum;i++) {
        if(voconfs[i] && voconfs[i]->hWnd==hWnd) {
            return voconfs[i];
        }
    }
    return NULL;
}
 
/*****************************************************************************
 * DirectDraw GUIDs.
 * Defining them here allows us to get rid of the dxguid library during
 * the linking stage.
 *****************************************************************************/
#define IID_IDirectDraw7 MP_IID_IDirectDraw7
static const GUID MP_IID_IDirectDraw7 =
{
	0x15e65ec0,0x3b9c,0x11d2,{0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b}
};

#define IID_IDirectDrawColorControl MP_IID_IDirectDrawColorControl
static const GUID MP_IID_IDirectDrawColorControl =
{
	0x4b9f0ee0,0x0d7e,0x11d0,{0x9b,0x06,0x00,0xa0,0xc9,0x03,0xa3,0xb8}
};


typedef struct directx_fourcc_caps
{
   char*  img_format_name;      //human readable name
   uint32_t img_format;         //as MPlayer image format
   uint32_t drv_caps;           //what hw supports with this format
   DDPIXELFORMAT g_ddpfOverlay; //as Directx Sourface description
} directx_fourcc_caps;


static directx_fourcc_caps g_ddpf[] =
{
	{"YV12 ",IMGFMT_YV12 ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('Y','V','1','2'),0,0,0,0,0}},
	{"I420 ",IMGFMT_I420 ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('I','4','2','0'),0,0,0,0,0}},   //yv12 with swapped uv
	{"IYUV ",IMGFMT_IYUV ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('I','Y','U','V'),0,0,0,0,0}},   //same as i420
	{"YVU9 ",IMGFMT_YVU9 ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('Y','V','U','9'),0,0,0,0,0}},
	{"YUY2 ",IMGFMT_YUY2 ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('Y','U','Y','2'),0,0,0,0,0}},
	{"UYVY ",IMGFMT_UYVY ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('U','Y','V','Y'),0,0,0,0,0}},
 	{"BGR8 ",IMGFMT_BGR8 ,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 8,  0x00000000, 0x00000000, 0x00000000, 0}},
	{"RGB15",IMGFMT_RGB15,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x0000001F, 0x000003E0, 0x00007C00, 0}},   //RGB 5:5:5
	{"BGR15",IMGFMT_BGR15,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x00007C00, 0x000003E0, 0x0000001F, 0}},
	{"RGB16",IMGFMT_RGB16,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x0000001F, 0x000007E0, 0x0000F800, 0}},   //RGB 5:6:5
    {"BGR16",IMGFMT_BGR16,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x0000F800, 0x000007E0, 0x0000001F, 0}},
	{"RGB24",IMGFMT_RGB24,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 24,  0x000000FF, 0x0000FF00, 0x00FF0000, 0}},
    {"BGR24",IMGFMT_BGR24,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 24,  0x00FF0000, 0x0000FF00, 0x000000FF, 0}},
    {"RGB32",IMGFMT_RGB32,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 32,  0x000000FF, 0x0000FF00, 0x00FF0000, 0}},
    {"BGR32",IMGFMT_BGR32,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 32,  0x00FF0000, 0x0000FF00, 0x000000FF, 0}}
};
#define NUM_FORMATS (sizeof(g_ddpf) / sizeof(g_ddpf[0]))

static void printmsg(voconf_t* voconf, char* format_str, ...) {
	va_list ap;
	char* tmp;
	char* tmp2;

	tmp=(char*)malloc(8192);
	va_start(ap, format_str);
	vsnprintf(tmp,8192,format_str,ap);
	va_end(ap);
	if(!voconf->msg) {
		voconf->msg=strdup(tmp);
	} else {
		tmp2=(char*)malloc(strlen(voconf->msg)+strlen(tmp)+4);
		sprintf(tmp2,"%s%s",voconf->msg,tmp);
		free(voconf->msg);
		voconf->msg=tmp2;
	}
	free(tmp);
}

char* getmsg(voconf_t* voconf) {
	char* ret;
	if(voconf->msg) {
		ret=strdup(voconf->msg);
		free(voconf->msg);
		voconf->msg=NULL;
		return ret;
	}
	return strdup("(none)");
}

static int
query_format(voconf_t* voconf, uint32_t format)
{
    uint32_t i=0;
    while ( i < NUM_FORMATS )
    {
		if (g_ddpf[i].img_format == format)
		    return g_ddpf[i].drv_caps;
	    i++;
    }
    return 0;
}

static uint32_t Directx_CreatePrimarySurface(voconf_t* voconf)
{
    DDSURFACEDESC2   ddsd;
    //cleanup
	if(voconf->g_lpddsPrimary) 
		voconf->g_lpddsPrimary->Release();
	voconf->g_lpddsPrimary=NULL;

    if(voconf->vidmode)
			voconf->g_lpdd->SetDisplayMode(voconf->vm_width,voconf->vm_height,voconf->vm_bpp,voconf->vo_refresh_rate,0);
    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    //set flags and create a primary surface.
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if(voconf->g_lpdd->CreateSurface(&ddsd, &voconf->g_lpddsPrimary, NULL ) != DD_OK)
	{
		return 1;
	}
	return 0;
}

static uint32_t Directx_CreateOverlay(voconf_t* voconf, uint32_t imgfmt)
{
    HRESULT ddrval;
    DDSURFACEDESC2   ddsdOverlay;
    uint32_t        i=0;
	while ( i < NUM_FORMATS +1 && imgfmt != g_ddpf[i].img_format)
	{
		i++;
	}
	if (!voconf->g_lpdd || !voconf->g_lpddsPrimary)
        return 1;
    //cleanup
	if (voconf->g_lpddsOverlay)
		voconf->g_lpddsOverlay->Release();
	if (voconf->g_lpddsBack)
		voconf->g_lpddsBack->Release();
	voconf->g_lpddsOverlay= NULL;
	voconf->g_lpddsBack = NULL;
	//create our overlay
    ZeroMemory(&ddsdOverlay, sizeof(ddsdOverlay));
    ddsdOverlay.dwSize = sizeof(ddsdOverlay);
    ddsdOverlay.ddsCaps.dwCaps=DDSCAPS_OVERLAY | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY;
    ddsdOverlay.dwFlags= DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_BACKBUFFERCOUNT| DDSD_PIXELFORMAT;
    ddsdOverlay.dwWidth=voconf->image_width;
    ddsdOverlay.dwHeight=voconf->image_height;
    ddsdOverlay.dwBackBufferCount=2;
	ddsdOverlay.ddpfPixelFormat=g_ddpf[i].g_ddpfOverlay;
	if(voconf->vo_doublebuffering)   //tribblebuffering
	{
		if (voconf->g_lpdd->CreateSurface(&ddsdOverlay, &voconf->g_lpddsOverlay, NULL)== DD_OK)
		{
            //get the surface directly attached to the primary (the back buffer)
            ddsdOverlay.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
            if(voconf->g_lpddsOverlay->GetAttachedSurface(&ddsdOverlay.ddsCaps, &voconf->g_lpddsBack) != DD_OK)
			{
				return 1;
			}
			return 0;
		}
		voconf->vo_doublebuffering=0; //disable tribblebuffering
	}
	//single buffer
    ddsdOverlay.dwBackBufferCount=0;
    ddsdOverlay.ddsCaps.dwCaps=DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
    ddsdOverlay.dwFlags= DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT;
    ddsdOverlay.ddpfPixelFormat=g_ddpf[i].g_ddpfOverlay;
	// try to create the overlay surface
	ddrval = voconf->g_lpdd->CreateSurface(&ddsdOverlay, &voconf->g_lpddsOverlay, NULL);
	if(ddrval != DD_OK)
	{
	   if(ddrval == DDERR_INVALIDPIXELFORMAT) printmsg(voconf, "<vo_directx><ERROR> invalid pixelformat: %s\n",g_ddpf[i].img_format_name);
       else printmsg(voconf, "<vo_directx><ERROR>");
       switch(ddrval)
	   {
	      case DDERR_INCOMPATIBLEPRIMARY:
			 {printmsg(voconf, "incompatible primary surface\n");break;}
		  case DDERR_INVALIDCAPS:
			 {printmsg(voconf, "invalid caps\n");break;}
	      case DDERR_INVALIDOBJECT:
		     {printmsg(voconf, "invalid object\n");break;}
	      case DDERR_INVALIDPARAMS:
		     {printmsg(voconf, "invalid parameters\n");break;}
	      case DDERR_NODIRECTDRAWHW:
		     {printmsg(voconf, "no directdraw hardware\n");break;}
	      case DDERR_NOEMULATION:
		     {printmsg(voconf, "can't emulate\n");break;}
	      case DDERR_NOFLIPHW:
		     {printmsg(voconf, "hardware can't do flip\n");break;}
	      case DDERR_NOOVERLAYHW:
		     {printmsg(voconf, "hardware can't do overlay\n");break;}
	      case DDERR_OUTOFMEMORY:
		     {printmsg(voconf, "not enough system memory\n");break;}
	      case DDERR_UNSUPPORTEDMODE:
			 {printmsg(voconf, "unsupported mode\n");break;}
		  case DDERR_OUTOFVIDEOMEMORY:
			 {printmsg(voconf, "not enough video memory\n");break;}
          default:
             printmsg(voconf, "create surface failed with 0x%x\n",ddrval);
	   }
	   return 1;
	}
    voconf->g_lpddsBack = voconf->g_lpddsOverlay;
	return 0;
}

static uint32_t Directx_CreateBackpuffer(voconf_t* voconf)
{
    DDSURFACEDESC2   ddsd;
	//cleanup
	if (voconf->g_lpddsBack) 
		voconf->g_lpddsBack->Release();
	voconf->g_lpddsBack=NULL;
	ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.ddsCaps.dwCaps= DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    ddsd.dwFlags= DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth=voconf->image_width;
    ddsd.dwHeight=voconf->image_height;
    if(voconf->g_lpdd->CreateSurface( &ddsd, &voconf->g_lpddsBack, 0 ) != DD_OK )
	{
		printmsg(voconf, "<vo_directx><FATAL ERROR>can't create backpuffer\n");
		return 1;
	}
    printmsg(voconf, "<vo_directx><INFO>backbuffer created\n");
	return 0;
}

void vo_uninit(voconf_t* voconf)
{
	if (voconf->g_cc != NULL)
	{
		voconf->g_cc->Release();
	}
	voconf->g_cc=NULL;
	if (voconf->g_lpddclipper != NULL) 
		voconf->g_lpddclipper->Release();
	voconf->g_lpddclipper = NULL;
	printmsg(voconf, "<vo_directx><INFO>clipper released\n");
	if (voconf->g_lpddsBack != NULL) 
		voconf->g_lpddsBack->Release();
	voconf->g_lpddsBack = NULL;
	printmsg(voconf, "<vo_directx><INFO>back surface released\n");
	if(voconf->vo_doublebuffering && !voconf->nooverlay)
	{
		if (voconf->g_lpddsOverlay != NULL)
			voconf->g_lpddsOverlay->Release();
        voconf->g_lpddsOverlay = NULL;
		printmsg(voconf, "<vo_directx><INFO>overlay surface released\n");
	}
	if (voconf->g_lpddsPrimary != NULL) 
		voconf->g_lpddsPrimary->Release();
    voconf->g_lpddsPrimary = NULL;
	printmsg(voconf, "<vo_directx><INFO>primary released\n");
	if(voconf->hWndFS)DestroyWindow(voconf->hWndFS);
	voconf->hWndFS = NULL;
	if((voconf->WinID == -1) && voconf->hWnd) DestroyWindow(voconf->hWnd);
	voconf->hWnd = NULL;
	printmsg(voconf, "<vo_directx><INFO>window destroyed\n");
	UnregisterClass(WNDCLASSNAME_WINDOWED, GetModuleHandle(NULL));
	UnregisterClass(WNDCLASSNAME_FULLSCREEN, GetModuleHandle(NULL));
#if 0
	if (mplayericon) DestroyIcon(mplayericon);
	mplayericon = NULL;
	if (mplayercursor) DestroyCursor(mplayercursor);
	mplayercursor = NULL;
#endif
	if (voconf->blackbrush) 
		DeleteObject(voconf->blackbrush);
	voconf->blackbrush = NULL;
	if (voconf->colorbrush) 
		DeleteObject(voconf->colorbrush);
	voconf->colorbrush = NULL;
	printmsg(voconf, "<vo_directx><INFO>GDI resources deleted\n");
	if (voconf->g_lpdd != NULL){
	    if(voconf->vidmode) 
			voconf->g_lpdd->RestoreDisplayMode();
	    voconf->g_lpdd->Release();
	}
	printmsg(voconf, "<vo_directx><INFO>directdrawobject released\n");
	hddraw_uses--;
	if(!hddraw_uses) {
		FreeLibrary( hddraw_dll);
		hddraw_dll= NULL;
	}
	printmsg(voconf, "<vo_directx><INFO>ddraw.dll freed\n");
	printmsg(voconf, "<vo_directx><INFO>uninitialized\n");
}

static BOOL WINAPI EnumCallbackEx(GUID FAR *lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID lpContext, HMONITOR  hm)
{
//	voconf_t* voconf = NULL;

//	printmsg(voconf, "<vo_directx> adapter %d: ", adapter_count);

    if (!lpGUID)
    {
//		printmsg(voconf, "%s", "Primary Display Adapter");
    }
    else
    {
//        printmsg(voconf, "%s", lpDriverDescription);
    }

    if(adapter_count == vo_adapter_num){
        MONITORINFO mi;
        if (!lpGUID)
            selected_guid_ptr = NULL;
        else
        {
            selected_guid = *lpGUID;
            selected_guid_ptr = &selected_guid;
        }
        mi.cbSize = sizeof(mi);

        if (myGetMonitorInfo(hm, &mi)) {
			monitor_rect = mi.rcMonitor;
        }
//        printmsg(voconf, "\t\t<--");
    }
//    printmsg(voconf, "\n");

    adapter_count++;

    return 1; // list all adapters
}

static uint32_t Directx_InitDirectDraw(voconf_t* voconf)
{
    typedef HRESULT    (WINAPI *tOurDirectDrawCreateEx)(GUID *,LPVOID *, REFIID,IUnknown FAR *);
    tOurDirectDrawCreateEx OurDirectDrawCreateEx;
	DDSURFACEDESC2 ddsd;
	LPDIRECTDRAWENUMERATEEX OurDirectDrawEnumerateEx;
	HINSTANCE user32dll=LoadLibrary(L"user32.dll");

	adapter_count = 0;
	if(user32dll){
		myGetMonitorInfo=(tMyGetMonitorInfo)GetProcAddress(user32dll,"GetMonitorInfoA");
		if(!myGetMonitorInfo && vo_adapter_num){
			printmsg(voconf, "<vo_directx> -adapter is not supported on Win95\n");
			vo_adapter_num = 0;
		}
	}

	printmsg(voconf, "<vo_directx><INFO>Initing DirectDraw\n" );

	//load direct draw DLL: based on videolans code
	hddraw_uses++;
	if(!hddraw_dll)
		hddraw_dll = LoadLibrary(L"DDRAW.DLL");
	if( hddraw_dll == NULL )
    {
		hddraw_uses=0;
        printmsg(voconf, "<vo_directx><FATAL ERROR>failed loading ddraw.dll\n" );
		return 1;
    }

    voconf->last_rect.left = 0xDEADC0DE;   // reset window position cache

	if(vo_adapter_num){ //display other than default
        OurDirectDrawEnumerateEx = (LPDIRECTDRAWENUMERATEEX) GetProcAddress(hddraw_dll,"DirectDrawEnumerateExA");
        if (!OurDirectDrawEnumerateEx){
			hddraw_uses--;
			if(!hddraw_uses) {
				FreeLibrary( hddraw_dll);
				hddraw_dll= NULL;
			}
            printmsg(voconf, "<vo_directx><FATAL ERROR>failed geting proc address: DirectDrawEnumerateEx\n");
            printmsg(voconf, "<vo_directx><FATAL ERROR>no directx 7 or higher installed\n");
            return 1;
        }

        // enumerate all display devices attached to the desktop
        OurDirectDrawEnumerateEx((LPDDENUMCALLBACKEX)EnumCallbackEx, NULL, DDENUM_ATTACHEDSECONDARYDEVICES );

        if(vo_adapter_num >= adapter_count)
            printmsg(voconf, "Selected adapter (%d) doesn't exist: Default Display Adapter selected\n",vo_adapter_num);
    }
    FreeLibrary(user32dll);

    OurDirectDrawCreateEx = (tOurDirectDrawCreateEx)GetProcAddress(hddraw_dll, "DirectDrawCreateEx");
    if ( OurDirectDrawCreateEx == NULL )
    {
		hddraw_uses--;
		if(!hddraw_uses) {
			FreeLibrary( hddraw_dll);
			hddraw_dll= NULL;
		}
		printmsg(voconf, "<vo_directx><FATAL ERROR>failed geting proc address: DirectDrawCreateEx\n");
 		return 1;
    }

	// initialize DirectDraw and create directx v7 object
//    if (OurDirectDrawCreateEx(selected_guid_ptr, (VOID**)&g_lpdd, &IID_IDirectDraw7, NULL ) != DD_OK )
    if (OurDirectDrawCreateEx(selected_guid_ptr, (LPVOID*)&voconf->g_lpdd, (const IID)IID_IDirectDraw7, NULL ) != DD_OK )
    {
		hddraw_uses--;
		if(!hddraw_uses) {
			FreeLibrary( hddraw_dll);
			hddraw_dll= NULL;
		}
        printmsg(voconf, "<vo_directx><FATAL ERROR>can't initialize ddraw\n");
		return 1;
    }

	//get current screen siz for selected monitor ...
	ddsd.dwSize=sizeof(ddsd);
	ddsd.dwFlags=DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
	voconf->g_lpdd->GetDisplayMode(&ddsd);
	if(voconf->vo_screenwidth && voconf->vo_screenheight)
	{
	    voconf->vm_height=voconf->vo_screenheight;
	    voconf->vm_width=voconf->vo_screenwidth;
	}
    else
    {
	    voconf->vm_height=ddsd.dwHeight;
	    voconf->vm_width=ddsd.dwWidth;
	}


	if(voconf->vo_dbpp) 
		voconf->vm_bpp=voconf->vo_dbpp;
	else 
		voconf->vm_bpp=ddsd.ddpfPixelFormat.dwRGBBitCount;

	if(voconf->vidmode){
		if (voconf->g_lpdd->SetCooperativeLevel(voconf->hWnd, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN) != DD_OK)
		{
	        printmsg(voconf, "<vo_directx><FATAL ERROR>can't set cooperativelevel for exclusive mode\n");
            return 1;
		}
		/*SetDisplayMode(ddobject,width,height,bpp,refreshrate,aditionalflags)*/
		if(voconf->g_lpdd->SetDisplayMode(voconf->vm_width, voconf->vm_height, voconf->vm_bpp,0,0) != DD_OK)
		{
	        printmsg(voconf, "<vo_directx><FATAL ERROR>can't set displaymode\n");
	        return 1;
		}
	    printmsg(voconf, "<vo_directx><INFO>Initialized adapter %i for %i x %i @ %i \n",vo_adapter_num,voconf->vm_width,voconf->vm_height,voconf->vm_bpp);
	    return 0;
	}
	if (voconf->g_lpdd->SetCooperativeLevel(voconf->hWnd, DDSCL_NORMAL) != DD_OK) // or DDSCL_SETFOCUSWINDOW
     {
        printmsg(voconf, "<vo_directx><FATAL ERROR>could not set cooperativelevel for hardwarecheck\n");
		return 1;
    }
    printmsg(voconf, "<vo_directx><INFO>DirectDraw Initialized\n");
	return 0;
}

static void check_events(void)
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0,PM_REMOVE))
    {
		TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static uint32_t Directx_ManageDisplay(voconf_t* voconf)
{
    HRESULT         ddrval;
    DDCAPS          capsDrv;
    DDOVERLAYFX     ovfx;
    DWORD           dwUpdateFlags=0;
    int width,height;

	width=320;
	height=200;
    if(!voconf->vidmode && !voconf->vo_fs && voconf->WinID!=-1) {
      RECT current_rect = {0, 0, 0, 0};
      GetWindowRect(voconf->hWnd, &current_rect);
      if ((current_rect.left   == voconf->last_rect.left)
      &&  (current_rect.top    == voconf->last_rect.top)
      &&  (current_rect.right  == voconf->last_rect.right)
      &&  (current_rect.bottom == voconf->last_rect.bottom))
        return 0;
      voconf->last_rect = current_rect;
    }

    if(voconf->vo_fs || voconf->vidmode){
//      aspect(&width,&height,A_ZOOM);
      voconf->rd.left=(voconf->vo_screenwidth-width)/2;
      voconf->rd.top=(voconf->vo_screenheight-height)/2;
      if (voconf->WinID == -1)
        if(ShowCursor(FALSE)>=0)while(ShowCursor(FALSE)>=0){}
    }
    else if (voconf->WinID != -1 && voconf->vo_geometry) {
      POINT pt;
      pt.x = voconf->vo_dx;
      pt.y = voconf->vo_dy;
      ClientToScreen(voconf->hWnd,&pt);
      width=voconf->d_image_width;
      height=voconf->d_image_height;
      voconf->rd.left = pt.x;
      voconf->rd.top = pt.y;
      while(ShowCursor(TRUE)<=0){}
    }
    else {
      POINT pt;
      pt.x = 0;  //overlayposition relative to the window
      pt.y = 0;
      ClientToScreen(voconf->hWnd,&pt);
      GetClientRect(voconf->hWnd, &voconf->rd);
	  width=voconf->rd.right - voconf->rd.left;
	  height=voconf->rd.bottom - voconf->rd.top;
      pt.x -= monitor_rect.left;    /* move coordinates from global to local monitor space */
      pt.y -= monitor_rect.top;
      voconf->rd.right -= monitor_rect.left;
      voconf->rd.bottom -= monitor_rect.top;
	  voconf->rd.left = pt.x;
      voconf->rd.top = pt.y;
      if(!voconf->nooverlay && (!width || !height)){
	    /*window is minimized*/
	    ddrval = voconf->g_lpddsOverlay->UpdateOverlay(NULL, voconf->g_lpddsPrimary, NULL, DDOVER_HIDE, NULL);
	    return 0;
	  }
      if(voconf->vo_keepaspect){
          int tmpheight=((float)width/voconf->window_aspect);
          tmpheight+=tmpheight%2;
          if(tmpheight > height){
            width=((float)height*voconf->window_aspect);
            width+=width%2;
          }
          else height=tmpheight;
      }
      if (voconf->WinID == -1)
          while(ShowCursor(TRUE)<=0){}
    }
    voconf->rd.right=voconf->rd.left+width;
    voconf->rd.bottom=voconf->rd.top+height;

	/*ok, let's workaround some overlay limitations*/
	if(!voconf->nooverlay)
	{
		uint32_t        uStretchFactor1000;  //minimum stretch
        uint32_t        xstretch1000,ystretch1000;
		/*get driver capabilities*/
        ZeroMemory(&capsDrv, sizeof(capsDrv));
        capsDrv.dwSize = sizeof(capsDrv);
        if(voconf->g_lpdd->GetCaps(&capsDrv, NULL) != DD_OK)
			return 1;
		/*get minimum stretch, depends on display adaptor and mode (refresh rate!) */
        uStretchFactor1000 = capsDrv.dwMinOverlayStretch>1000 ? capsDrv.dwMinOverlayStretch : 1000;
        voconf->rd.right = ((width+voconf->rd.left)*uStretchFactor1000+999)/1000;
        voconf->rd.bottom = (height+voconf->rd.top)*uStretchFactor1000/1000;
        /*calculate xstretch1000 and ystretch1000*/
        xstretch1000 = ((voconf->rd.right - voconf->rd.left)* 1000)/voconf->image_width ;
        ystretch1000 = ((voconf->rd.bottom - voconf->rd.top)* 1000)/voconf->image_height;
		voconf->rs.left=0;
		voconf->rs.right=voconf->image_width;
		voconf->rs.top=0;
		voconf->rs.bottom=voconf->image_height;
        if(voconf->rd.left < 0)
			voconf->rs.left=(-voconf->rd.left*1000)/xstretch1000;
        if(voconf->rd.top < 0)
			voconf->rs.top=(-voconf->rd.top*1000)/ystretch1000;
        if(voconf->rd.right > voconf->vo_screenwidth)
			voconf->rs.right=((voconf->vo_screenwidth-voconf->rd.left)*1000)/xstretch1000;
        if(voconf->rd.bottom > voconf->vo_screenheight)
			voconf->rs.bottom=((voconf->vo_screenheight-voconf->rd.top)*1000)/ystretch1000;
		/*do not allow to zoom or shrink if hardware isn't able to do so*/
		if((width < voconf->image_width)&& !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKX))
		{
			if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKXN) 
				printmsg(voconf, "<vo_directx><ERROR>can only shrinkN\n");
	        else 
				printmsg(voconf, "<vo_directx><ERROR>can't shrink x\n");
	        voconf->rd.right=voconf->rd.left+voconf->image_width;
		}
		else if((width > voconf->image_width)&& !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHX))
		{
			if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHXN) 
				printmsg(voconf, "<vo_directx><ERROR>can only stretchN\n");
	        else 
				printmsg(voconf, "<vo_directx><ERROR>can't stretch x\n");
	        voconf->rd.right = voconf->rd.left+voconf->image_width;
		}
		if((height < voconf->image_height) && !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKY))
		{
			if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKYN) 
				printmsg(voconf, "<vo_directx><ERROR>can only shrinkN\n");
	        else 
				printmsg(voconf, "<vo_directx><ERROR>can't shrink y\n");
	        voconf->rd.bottom = voconf->rd.top + voconf->image_height;
		}
		else if((height > voconf->image_height ) && !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHY))
		{
			if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHYN) 
				printmsg(voconf, "<vo_directx><ERROR>can only stretchN\n");
	        else 
				printmsg(voconf, "<vo_directx><ERROR>can't stretch y\n");
	        voconf->rd.bottom = voconf->rd.top + voconf->image_height;
		}
		/*the last thing to check are alignment restrictions
          these expressions (x & -y) just do alignment by dropping low order bits...
          so to round up, we add first, then truncate*/
		if((capsDrv.dwCaps & DDCAPS_ALIGNBOUNDARYSRC) && capsDrv.dwAlignBoundarySrc)
		  voconf->rs.left = (voconf->rs.left + capsDrv.dwAlignBoundarySrc / 2) & -(signed)(capsDrv.dwAlignBoundarySrc);
        if((capsDrv.dwCaps & DDCAPS_ALIGNSIZESRC) && capsDrv.dwAlignSizeSrc)
		  voconf->rs.right = voconf->rs.left + ((voconf->rs.right - voconf->rs.left + capsDrv.dwAlignSizeSrc / 2) & -(signed) (capsDrv.dwAlignSizeSrc));
        if((capsDrv.dwCaps & DDCAPS_ALIGNBOUNDARYDEST) && capsDrv.dwAlignBoundaryDest)
		  voconf->rd.left = (voconf->rd.left + capsDrv.dwAlignBoundaryDest / 2) & -(signed)(capsDrv.dwAlignBoundaryDest);
        if((capsDrv.dwCaps & DDCAPS_ALIGNSIZEDEST) && capsDrv.dwAlignSizeDest)
		  voconf->rd.right = voconf->rd.left + ((voconf->rd.right - voconf->rd.left) & -(signed) (capsDrv.dwAlignSizeDest));
		/*create an overlay FX structure to specify a destination color key*/
		ZeroMemory(&ovfx, sizeof(ovfx));
        ovfx.dwSize = sizeof(ovfx);
        if(voconf->vo_fs || voconf->vidmode)
		{
			ovfx.dckDestColorkey.dwColorSpaceLowValue = 0;
            ovfx.dckDestColorkey.dwColorSpaceHighValue = 0;
		}
		else
		{
			ovfx.dckDestColorkey.dwColorSpaceLowValue = voconf->destcolorkey;
            ovfx.dckDestColorkey.dwColorSpaceHighValue = voconf->destcolorkey;
		}
        // set the flags we'll send to UpdateOverlay      //DDOVER_AUTOFLIP|DDOVERFX_MIRRORLEFTRIGHT|DDOVERFX_MIRRORUPDOWN could be useful?;
        dwUpdateFlags = DDOVER_SHOW | DDOVER_DDFX;
        /*if hardware can't do colorkeying set the window on top*/
		if(capsDrv.dwCKeyCaps & DDCKEYCAPS_DESTOVERLAY) 
			dwUpdateFlags |= DDOVER_KEYDESTOVERRIDE;
        else if (!voconf->tmp_image) 
			voconf->vo_ontop = 1;
	}
    else
    {
        voconf->g_lpddclipper->SetHWnd(0,(voconf->vo_fs && !voconf->vidmode)?voconf->hWndFS: voconf->hWnd);
    }

    if(!voconf->vidmode && !voconf->vo_fs){
      if(voconf->WinID == -1) {
          RECT rdw=voconf->rd;
          if (voconf->vo_border)
          AdjustWindowRect(&rdw,WNDSTYLE,FALSE);
//          printmsg("window: %i %i %ix%i\n",rdw.left,rdw.top,rdw.right - rdw.left,rdw.bottom - rdw.top);
		  rdw.left += monitor_rect.left; /* move to global coordinate space */
          rdw.top += monitor_rect.top;
		  rdw.right += monitor_rect.left;
		  rdw.bottom += monitor_rect.top;
          SetWindowPos(voconf->hWnd,(voconf->vo_ontop)?HWND_TOPMOST:(voconf->vo_rootwin?HWND_BOTTOM:HWND_NOTOPMOST),rdw.left,rdw.top,rdw.right-rdw.left,rdw.bottom-rdw.top,SWP_NOOWNERZORDER);
      }
    }
    else SetWindowPos(voconf->vidmode?voconf->hWnd:voconf->hWndFS,voconf->vo_rootwin?HWND_BOTTOM:HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOOWNERZORDER|SWP_NOCOPYBITS);

    /*make sure the overlay is inside the screen*/
    if(voconf->rd.left<0)
		voconf->rd.left=0;
    if(voconf->rd.right>voconf->vo_screenwidth)
		voconf->rd.right=voconf->vo_screenwidth;
    if(voconf->rd.top<0)
		voconf->rd.top=0;
    if(voconf->rd.bottom>voconf->vo_screenheight)
		voconf->rd.bottom=voconf->vo_screenheight;

  	/*for nonoverlay mode we are finished, for overlay mode we have to display the overlay first*/
	if(voconf->nooverlay)
		return 0;

//    printmsg("overlay: %i %i %ix%i\n",rd.left,rd.top,rd.right - rd.left,rd.bottom - rd.top);
	ddrval = voconf->g_lpddsOverlay->UpdateOverlay(&voconf->rs, voconf->g_lpddsPrimary, &voconf->rd, dwUpdateFlags, &ovfx);
    if(FAILED(ddrval))
    {
        // one cause might be the driver lied about minimum stretch
        // we should try upping the destination size a bit, or
        // perhaps shrinking the source size
	   	printmsg(voconf, "<vo_directx><ERROR>UpdateOverlay failed\n" );
	  	printmsg(voconf, "<vo_directx><ERROR>Overlay:x1:%i,y1:%i,x2:%i,y2:%i,w:%i,h:%i\n",voconf->rd.left,voconf->rd.top,voconf->rd.right,voconf->rd.bottom,voconf->rd.right - voconf->rd.left,voconf->rd.bottom - voconf->rd.top );
	  	printmsg(voconf, "<vo_directx><ERROR>");
		switch (ddrval)
		{
			case DDERR_NOSTRETCHHW:
				{printmsg(voconf, "hardware can't stretch: try to size the window back\n");break;}
            case DDERR_INVALIDRECT:
				{printmsg(voconf, "invalid rectangle\n");break;}
			case DDERR_INVALIDPARAMS:
				{printmsg(voconf, "invalid parameters\n");break;}
			case DDERR_HEIGHTALIGN:
				{printmsg(voconf, "height align\n");break;}
			case DDERR_XALIGN:
				{printmsg(voconf, "x align\n");break;}
			case DDERR_UNSUPPORTED:
				{printmsg(voconf, "unsupported\n");break;}
			case DDERR_INVALIDSURFACETYPE:
				{printmsg(voconf, "invalid surfacetype\n");break;}
			case DDERR_INVALIDOBJECT:
				{printmsg(voconf, "invalid object\n");break;}
			case DDERR_SURFACELOST:
				{
					printmsg(voconf, "surfaces lost\n");
					voconf->g_lpddsOverlay->Restore(); //restore and try again
			        voconf->g_lpddsPrimary->Restore();
			        ddrval = voconf->g_lpddsOverlay->UpdateOverlay(&voconf->rs, voconf->g_lpddsPrimary, &voconf->rd, dwUpdateFlags, &ovfx);
					if(ddrval !=DD_OK) 
						printmsg(voconf, "<vo_directx><FATAL ERROR>UpdateOverlay failed again\n" );
					break;
				}
            default:
                printmsg(voconf, " 0x%x\n",ddrval);
		}
	    /*ok we can't do anything about it -> hide overlay*/
		if(ddrval != DD_OK)
		{
			ddrval = voconf->g_lpddsOverlay->UpdateOverlay(NULL, voconf->g_lpddsPrimary, NULL, DDOVER_HIDE, NULL);
            return 1;
		}
	}
    return 0;
}

//find out supported overlay pixelformats
static uint32_t Directx_CheckOverlayPixelformats(voconf_t* voconf)
{
    DDCAPS          capsDrv;
    HRESULT         ddrval;
    DDSURFACEDESC2   ddsdOverlay;
	uint32_t        i;
    uint32_t        formatcount = 0;
	//get driver caps to determine overlay support
    ZeroMemory(&capsDrv, sizeof(capsDrv));
    capsDrv.dwSize = sizeof(capsDrv);
	ddrval = voconf->g_lpdd->GetCaps(&capsDrv, NULL);
    if (FAILED(ddrval))
	{
        printmsg(voconf, "<vo_directx><ERROR>failed getting ddrawcaps\n");
		return 1;
	}
    if (!(capsDrv.dwCaps & DDCAPS_OVERLAY))
    {
		printmsg(voconf, "<vo_directx><ERROR>Your card doesn't support overlay\n");
		return 1;
	}
    printmsg(voconf, "<vo_directx><INFO>testing supported overlay pixelformats\n");
    //it is not possible to query for pixel formats supported by the
    //overlay hardware: try out various formats till one works
    ZeroMemory(&ddsdOverlay, sizeof(ddsdOverlay));
    ddsdOverlay.dwSize = sizeof(ddsdOverlay);
    ddsdOverlay.ddsCaps.dwCaps=DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
    ddsdOverlay.dwFlags= DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH| DDSD_PIXELFORMAT;
    ddsdOverlay.dwWidth=300;
    ddsdOverlay.dwHeight=280;
    ddsdOverlay.dwBackBufferCount=0;
    //try to create an overlay surface using one of the pixel formats in our global list
	i=0;
    do
    {
   		ddsdOverlay.ddpfPixelFormat=g_ddpf[i].g_ddpfOverlay;
        ddrval = voconf->g_lpdd->CreateSurface(&ddsdOverlay, &voconf->g_lpddsOverlay, NULL);
        if (ddrval == DD_OK)
		{
			 printmsg(voconf, "<vo_directx><FORMAT OVERLAY>%i %s supported\n",i,g_ddpf[i].img_format_name);
			 g_ddpf[i].drv_caps = VFCAP_CSP_SUPPORTED |VFCAP_OSD |VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_HWSCALE_UP;
			 formatcount++;}
	    else printmsg(voconf, "<vo_directx><FORMAT OVERLAY>%i %s not supported\n",i,g_ddpf[i].img_format_name);
		if (voconf->g_lpddsOverlay != NULL) {
			voconf->g_lpddsOverlay->Release(); 
			voconf->g_lpddsOverlay = NULL;
		}
	} while( ++i < NUM_FORMATS );
    printmsg(voconf, "voconf, <vo_directx><INFO>Your card supports %i of %i overlayformats\n",formatcount, NUM_FORMATS);
	if (formatcount == 0)
	{
		printmsg(voconf, "<vo_directx><WARN>Your card supports overlay, but we couldn't create one\n");
		printmsg(voconf, "<vo_directx><INFO>This can have the following reasons:\n");
		printmsg(voconf, "<vo_directx><INFO>- you are already using an overlay with another app\n");
		printmsg(voconf, "<vo_directx><INFO>- you don't have enough videomemory\n");
		printmsg(voconf, "<vo_directx><INFO>- vo_directx doesn't support the cards overlay pixelformat\n");
		return 1;
	}
    if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYMIRRORLEFTRIGHT) 
		printmsg(voconf, "<vo_directx><INFO>can mirror left right\n"); //I don't have hardware which
    if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYMIRRORUPDOWN ) 
		printmsg(voconf, "<vo_directx><INFO>can mirror up down\n");      //supports those send me one and I'll implement ;)
	return 0;
}

//find out the Pixelformat of the Primary Surface
static uint32_t Directx_CheckPrimaryPixelformat(voconf_t* voconf)
{
	uint32_t i=0;
    uint32_t formatcount = 0;
	DDPIXELFORMAT	ddpf;
	DDSURFACEDESC2   ddsd;
    HDC             hdc;
    HRESULT         hres;
	COLORREF        rgbT=RGB(0,0,0);
	printmsg(voconf, "<vo_directx><INFO>checking primary surface\n");
	memset( &ddpf, 0, sizeof( DDPIXELFORMAT ));
    ddpf.dwSize = sizeof( DDPIXELFORMAT );
    //we have to create a primary surface first
	if(Directx_CreatePrimarySurface(voconf)!=0)return 1;
	if(voconf->g_lpddsPrimary->GetPixelFormat( &ddpf ) != DD_OK )
	{
		printmsg(voconf, "<vo_directx><FATAL ERROR>can't get pixelformat\n");
		return 1;
	}
    while ( i < NUM_FORMATS )
    {
	   if (g_ddpf[i].g_ddpfOverlay.dwRGBBitCount == ddpf.dwRGBBitCount)
	   {
           if (g_ddpf[i].g_ddpfOverlay.dwRBitMask == ddpf.dwRBitMask)
		   {
			   printmsg(voconf, "<vo_directx><FORMAT PRIMARY>%i %s supported\n",i,g_ddpf[i].img_format_name);
			   g_ddpf[i].drv_caps = VFCAP_CSP_SUPPORTED |VFCAP_OSD;
			   formatcount++;
               voconf->primary_image_format=g_ddpf[i].img_format;
		   }
	   }
	   i++;
    }
    //get the colorkey for overlay mode
	voconf->destcolorkey = CLR_INVALID;
    if (voconf->windowcolor != CLR_INVALID && voconf->g_lpddsPrimary->GetDC(&hdc) == DD_OK)
    {
        rgbT = GetPixel(hdc, 0, 0);
        SetPixel(hdc, 0, 0, voconf->windowcolor);
        voconf->g_lpddsPrimary->ReleaseDC(hdc);
    }
    // read back the converted color
    ddsd.dwSize = sizeof(ddsd);
    while ((hres = voconf->g_lpddsPrimary->Lock(NULL, &ddsd, 0, NULL)) == DDERR_WASSTILLDRAWING)
        ;
    if (hres == DD_OK)
    {
        voconf->destcolorkey = *(DWORD *) ddsd.lpSurface;
        if (ddsd.ddpfPixelFormat.dwRGBBitCount < 32)
            voconf->destcolorkey &= (1 << ddsd.ddpfPixelFormat.dwRGBBitCount) - 1;
        voconf->g_lpddsPrimary->Unlock(NULL);
    }
    if (voconf->windowcolor != CLR_INVALID && voconf->g_lpddsPrimary->GetDC(&hdc) == DD_OK)
    {
        SetPixel(hdc, 0, 0, rgbT);
        voconf->g_lpddsPrimary->ReleaseDC(hdc);
    }
	//release primary
	voconf->g_lpddsPrimary->Release();
	voconf->g_lpddsPrimary = NULL;
	if(formatcount==0)
	{
		printmsg(voconf, "<vo_directx><FATAL ERROR>Unknown Pixelformat\n");
		return 1;
	}
	return 0;
}

//function handles input
static LRESULT CALLBACK WndProc(HWND h_Wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	voconf_t* voconf = seek_voconf(h_Wnd);

    if(!voconf) {
        return DefWindowProc(h_Wnd, message, wParam, lParam);
    }
	
	switch (message)
    {
        case WM_MOUSEACTIVATE:
            return MA_ACTIVATEANDEAT;
	    case WM_NCACTIVATE:
        {
            if(voconf->vidmode && adapter_count > 2) //only disable if more than one adapter.
			    return 0;
			break;
		}
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
        case WM_CLOSE:
		{
//			mplayer_put_key(KEY_CLOSE_WIN);
			return 0;
		}
        case WM_WINDOWPOSCHANGED:
		{
			//printmsg("Windowposchange\n");
			if(voconf->g_lpddsBack != NULL)  //or it will crash with -vm
			{
				Directx_ManageDisplay(voconf);
			}
		    break;
		}
        case WM_SYSCOMMAND:
		{
			switch (wParam)
			{   //kill screensaver etc.
				//note: works only when the window is active
				//you can workaround this by disabling the allow screensaver option in
				//the link to the app
				case SC_SCREENSAVE:
				case SC_MONITORPOWER:
                printmsg(voconf, "<vo_directx><INFO>killing screensaver\n" );
                return 0;
				case SC_MAXIMIZE:
//					if (!vo_fs) control(VOCTRL_FULLSCREEN, NULL);
                return 0;
			}
			break;
		}
        case WM_KEYDOWN:
		{
/*
			switch (wParam)
			{
				case VK_LEFT:
					{mplayer_put_key(KEY_LEFT);break;}
                case VK_UP:
					{mplayer_put_key(KEY_UP);break;}
                case VK_RIGHT:
					{mplayer_put_key(KEY_RIGHT);break;}
	            case VK_DOWN:
					{mplayer_put_key(KEY_DOWN);break;}
	            case VK_TAB:
					{mplayer_put_key(KEY_TAB);break;}
		        case VK_BACK:
					{mplayer_put_key(KEY_BS);break;}
		        case VK_DELETE:
					{mplayer_put_key(KEY_DELETE);break;}
		        case VK_INSERT:
					{mplayer_put_key(KEY_INSERT);break;}
		        case VK_HOME:
					{mplayer_put_key(KEY_HOME);break;}
		        case VK_END:
					{mplayer_put_key(KEY_END);break;}
		        case VK_PRIOR:
			        {mplayer_put_key(KEY_PAGE_UP);break;}
		        case VK_NEXT:
			        {mplayer_put_key(KEY_PAGE_DOWN);break;}
		        case VK_ESCAPE:
					{mplayer_put_key(KEY_ESC);break;}
			}
*/
			break;
		}
        case WM_CHAR:
		{
//			mplayer_put_key(wParam);
			break;
		}
        case WM_LBUTTONDOWN:
		{
//			if (!vo_nomouse_input)
//				mplayer_put_key(MOUSE_BTN0);
			break;
		}
        case WM_MBUTTONDOWN:
		{
//			if (!vo_nomouse_input)
//				mplayer_put_key(MOUSE_BTN1);
			break;
		}
        case WM_RBUTTONDOWN:
		{
//			if (!vo_nomouse_input)
//				mplayer_put_key(MOUSE_BTN2);
			break;
		}
		case WM_LBUTTONDBLCLK:
		{
//			if(!vo_nomouse_input)
//				mplayer_put_key(MOUSE_BTN0_DBL);
			break;
		}
		case WM_MBUTTONDBLCLK:
		{
//			if(!vo_nomouse_input)
//				mplayer_put_key(MOUSE_BTN1_DBL);
			break;
		}
		case WM_RBUTTONDBLCLK:
		{
//			if(!vo_nomouse_input)
//				mplayer_put_key(MOUSE_BTN2_DBL);
			break;
		}
        case WM_MOUSEWHEEL:
		{
			int x;
//			if (vo_nomouse_input)
//				break;
			x = GET_WHEEL_DELTA_WPARAM(wParam);
//			if (x > 0)
//				mplayer_put_key(MOUSE_BTN3);
//			else
//				mplayer_put_key(MOUSE_BTN4);
			break;
		}
        case WM_XBUTTONDOWN:
		{
//			if (vo_nomouse_input)
//				break;
//			if (HIWORD(wParam) == 1)
//				mplayer_put_key(MOUSE_BTN5);
//			else
//				mplayer_put_key(MOUSE_BTN6);
			break;
		}
        case WM_XBUTTONDBLCLK:
		{
//			if (vo_nomouse_input)
//				break;
//			if (HIWORD(wParam) == 1)
//				mplayer_put_key(MOUSE_BTN5_DBL);
//			else
//				mplayer_put_key(MOUSE_BTN6_DBL);
			break;
		}

    }
	return DefWindowProc(h_Wnd, message, wParam, lParam);
}


voconf_t* preinit(int noa, HWND hWnd)
{
    voconf_t* voconf = alloc_voconf(); 

	HINSTANCE hInstance = GetModuleHandle(NULL);
    char exedir[MAX_PATH];
    WNDCLASS   wc;
	
	if(noa)
	{
		printmsg(voconf, "<vo_directx><INFO>disabled overlay\n");
	    voconf->nooverlay = 1;
	}
	/*load icon from the main app*/
/*
	if(GetModuleFileName(NULL,exedir,MAX_PATH))
    {
        mplayericon = ExtractIcon( hInstance, exedir, 0 );
  	}
    if(!mplayericon)mplayericon=LoadIcon(NULL,IDI_APPLICATION);
*/
//	mplayercursor = LoadCursor(NULL, IDC_ARROW);
    monitor_rect.right=GetSystemMetrics(SM_CXSCREEN);
    monitor_rect.bottom=GetSystemMetrics(SM_CYSCREEN);

    voconf->windowcolor = voconf->vo_colorkey;
    voconf->colorbrush = CreateSolidBrush(voconf->windowcolor);
    voconf->blackbrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style         =  CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   =  WndProc;
    wc.cbClsExtra    =  0;
    wc.cbWndExtra    =  0;
    wc.hInstance     =  hInstance;
    wc.hCursor       =  LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         =  LoadIcon(NULL,IDI_APPLICATION);
    wc.hbrBackground =  voconf->vidmode ? voconf->blackbrush : voconf->colorbrush;
    wc.lpszClassName =  WNDCLASSNAME_WINDOWED;
    wc.lpszMenuName  =  NULL;
    RegisterClass(&wc);
	if(hWnd)
			voconf->WinID = 1;
	else
			voconf->WinID = -1;
    if (voconf->WinID != -1) voconf->hWnd = (HWND)hWnd;
    else
    voconf->hWnd = CreateWindowEx(voconf->vidmode?WS_EX_TOPMOST:0,
        WNDCLASSNAME_WINDOWED,L"SPlayer",(voconf->vidmode || !voconf->vo_border)?WS_POPUP:WNDSTYLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,NULL,NULL,hInstance,NULL);
//    wc.hbrBackground = blackbrush;
//    wc.lpszClassName = WNDCLASSNAME_FULLSCREEN;
//    RegisterClass(&wc);

	if (Directx_InitDirectDraw(voconf)!= 0) {
		free(voconf);
		return NULL;          //init DirectDraw
	}

//    if(!vidmode)h_WndFS = CreateWindow(WNDCLASSNAME_FULLSCREEN,L"MPlayer Fullscreen",WS_POPUP,monitor_rect.left,monitor_rect.top,monitor_rect.right-monitor_rect.left,monitor_rect.bottom-monitor_rect.top,h_Wnd,NULL,hInstance,NULL);
    printmsg(voconf, "<vo_directx><INFO>initial mplayer windows created\n");

    if (Directx_CheckPrimaryPixelformat(voconf)!=0) {
		free(voconf);
		return NULL;
	}
	if (!voconf->nooverlay && Directx_CheckOverlayPixelformats(voconf) == 0)        //check for supported hardware
	{
		printmsg(voconf, "<vo_directx><INFO>hardware supports overlay\n");
		voconf->nooverlay = 0;
 	}
	else   //if we can't have overlay we create a backpuffer with the same imageformat as the primary surface
	{
       	printmsg(voconf, "<vo_directx><INFO>using backpuffer\n");
		voconf->nooverlay = 1;
	}
 	printmsg(voconf, "<vo_directx><INFO>preinit succesfully finished\n");
    if (voconf->vidmode) {
        voconf->vo_screenwidth = voconf->vm_width;
        voconf->vo_screenheight = voconf->vm_height;
    } else {
        voconf->vo_screenwidth = monitor_rect.right - monitor_rect.left;
        voconf->vo_screenheight = monitor_rect.bottom - monitor_rect.top;
    }
	return voconf;
}

#if 0
int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y )
{
	uint8_t *s;
    uint8_t *d;
    uint32_t uvstride=dstride/2;
	// copy Y
    d=image+dstride*y+x;
    s=src[0];
    mem2agpcpy_pic(d,s,w,h,dstride,stride[0]);

	w/=2;h/=2;x/=2;y/=2;

	// copy U
    d=image+dstride*image_height + uvstride*y+x;
    if(image_format == IMGFMT_YV12)s=src[2];
	else s=src[1];
    mem2agpcpy_pic(d,s,w,h,uvstride,stride[1]);

	// copy V
    d=image+dstride*image_height +uvstride*(image_height/2) + uvstride*y+x;
    if(image_format == IMGFMT_YV12)s=src[1];
	else s=src[2];
    mem2agpcpy_pic(d,s,w,h,uvstride,stride[2]);
    return 0;
}
#endif

void vo_flip_page(voconf_t* voconf)
{
   	HRESULT dxresult;
	voconf->g_lpddsBack->Unlock(NULL);
	if (voconf->vo_doublebuffering)
    {
		// flip to the next image in the sequence
		dxresult = voconf->g_lpddsOverlay->Flip(NULL, DDFLIP_WAIT);
		if(dxresult == DDERR_SURFACELOST)
		{
			printmsg(voconf, "<vo_directx><ERROR><vo_directx><INFO>Restoring Surface\n");
			voconf->g_lpddsBack->Restore();
			// restore overlay and primary before calling
			// Directx_ManageDisplay() to avoid error messages
			voconf->g_lpddsOverlay->Restore();
			voconf->g_lpddsPrimary->Restore();
			// update overlay in case we return from screensaver
			Directx_ManageDisplay(voconf);
		    dxresult = voconf->g_lpddsOverlay->Flip(NULL, DDFLIP_WAIT);
		}
		if(dxresult != DD_OK) 
			printmsg(voconf, "<vo_directx><ERROR>can't flip page\n");
    }
	if(voconf->nooverlay)
	{
		DDBLTFX  ddbltfx;
        // ask for the "NOTEARING" option
	    memset( &ddbltfx, 0, sizeof(DDBLTFX) );
        ddbltfx.dwSize = sizeof(DDBLTFX);
        ddbltfx.dwDDFX = DDBLTFX_NOTEARING;
        voconf->g_lpddsPrimary->Blt(&voconf->rd, voconf->g_lpddsBack, NULL, DDBLT_WAIT, &ddbltfx);
	}
	if (voconf->g_lpddsBack->Lock(NULL,&voconf->ddsdsf, DDLOCK_NOSYSLOCK | DDLOCK_WAIT , NULL) == DD_OK) {
	    if(voconf->vo_directrendering && (voconf->dstride != voconf->ddsdsf.lPitch)){
	        printmsg(voconf, "<vo_directx><WARN>stride changed !!!! disabling direct rendering\n");
	        voconf->vo_directrendering=0;
	    }
	    if (voconf->tmp_image)
		    free(voconf->tmp_image);
	    voconf->tmp_image = NULL;
	    voconf->dstride = voconf->ddsdsf.lPitch;
	    voconf->image = (uint8_t*)voconf->ddsdsf.lpSurface;
		memset(voconf->image,63,voconf->dstride*voconf->image_height);
	} else if (!voconf->tmp_image) {
		printmsg(voconf, "<vo_directx><WARN>Locking the surface failed, rendering to a hidden surface!\n");
		voconf->tmp_image = voconf->image = (uint8_t*)calloc(1, voconf->image_height * voconf->dstride * 2);
	}
}

int vo_draw_frame(voconf_t* voconf, uint8_t *src[])
{
    if (voconf->WinID != -1) Directx_ManageDisplay(voconf);
  	fast_memcpy( voconf->image, *src, voconf->dstride * voconf->image_height );
	return 0;
}

#if 0
static uint32_t get_image(mp_image_t *mpi)
{
    if(mpi->flags&MP_IMGFLAG_READABLE) {printmsg("<vo_directx><ERROR>slow video ram\n");return VO_FALSE;}
    if(mpi->type==MP_IMGTYPE_STATIC) {printmsg("<vo_directx><ERROR>not static\n");return VO_FALSE;}
    if((mpi->width==dstride) || (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH)))
	{
		if(mpi->flags&MP_IMGFLAG_PLANAR)
		{
		    if(image_format == IMGFMT_YV12)
			{
				mpi->planes[2]= image + dstride*image_height;
	            mpi->planes[1]= image + dstride*image_height+ dstride*image_height/4;
                mpi->stride[1]=mpi->stride[2]=dstride/2;
			}
			else if(image_format == IMGFMT_IYUV || image_format == IMGFMT_I420)
			{
				mpi->planes[1]= image + dstride*image_height;
	            mpi->planes[2]= image + dstride*image_height+ dstride*image_height/4;
			    mpi->stride[1]=mpi->stride[2]=dstride/2;
			}
			else if(image_format == IMGFMT_YVU9)
			{
				mpi->planes[2] = image + dstride*image_height;
				mpi->planes[1] = image + dstride*image_height+ dstride*image_height/16;
			    mpi->stride[1]=mpi->stride[2]=dstride/4;
			}
		}
		mpi->planes[0]=image;
        mpi->stride[0]=dstride;
   		mpi->width=image_width;
		mpi->height=image_height;
        mpi->flags|=MP_IMGFLAG_DIRECT;
        printmsg("<vo_directx><INFO>Direct Rendering ENABLED\n");
        return VO_TRUE;
    }
    return VO_FALSE;
}

static uint32_t put_image(mp_image_t *mpi){

    uint8_t   *d;
	uint8_t   *s;
    uint32_t x = mpi->x;
	uint32_t y = mpi->y;
	uint32_t w = mpi->w;
	uint32_t h = mpi->h;

    if (WinID != -1) Directx_ManageDisplay();

    if((mpi->flags&MP_IMGFLAG_DIRECT)||(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK))
	{
		mp_msg(MSGT_VO, MSGL_DBG3 ,"<vo_directx><INFO>put_image: nothing to do: drawslices\n");
		return VO_TRUE;
    }

    if (mpi->flags&MP_IMGFLAG_PLANAR)
	{

		if(image_format!=IMGFMT_YVU9)draw_slice(mpi->planes,mpi->stride,mpi->w,mpi->h,0,0);
		else
		{
			// copy Y
            d=image+dstride*y+x;
            s=mpi->planes[0];
            mem2agpcpy_pic(d,s,w,h,dstride,mpi->stride[0]);
            w/=4;h/=4;x/=4;y/=4;
    	    // copy V
            d=image+dstride*image_height + dstride*y/4+x;
	        s=mpi->planes[2];
            mem2agpcpy_pic(d,s,w,h,dstride/4,mpi->stride[1]);
  	        // copy U
            d=image+dstride*image_height + dstride*image_height/16 + dstride/4*y+x;
		    s=mpi->planes[1];
            mem2agpcpy_pic(d,s,w,h,dstride/4,mpi->stride[2]);
		}
	}
	else //packed
	{
		mem2agpcpy_pic(image, mpi->planes[0], w * (mpi->bpp / 8), h, dstride, mpi->stride[0]);
	}
	return VO_TRUE;
}
#endif

int vo_config(voconf_t* voconf, uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t options, char *title, uint32_t format)
{
    RECT rd;
    voconf->vo_fs = options & 0x01;
	voconf->image_format =  format;
	voconf->image_width = width;
	voconf->image_height = height;
	voconf->d_image_width = d_width;
	voconf->d_image_height = d_height;
    if(format != voconf->primary_image_format) 
		voconf->nooverlay = 0;
    voconf->window_aspect= (float)voconf->d_image_width / (float)voconf->d_image_height;

#ifdef CONFIG_GUI
    if(use_gui){
        guiGetEvent(guiSetShVideo, 0);
    }
#endif
    /*release all directx objects*/
    if (voconf->g_cc != NULL)
		voconf->g_cc->Release();
    voconf->g_cc=NULL;
    if(voconf->g_lpddclipper)
		voconf->g_lpddclipper->Release();
    voconf->g_lpddclipper=NULL;
    if (voconf->g_lpddsBack != NULL) 
		voconf->g_lpddsBack->Release();
    voconf->g_lpddsBack = NULL;
    if(voconf->vo_doublebuffering)
        if (voconf->g_lpddsOverlay != NULL)
			voconf->g_lpddsOverlay->Release();
    voconf->g_lpddsOverlay = NULL;
    if (voconf->g_lpddsPrimary != NULL) 
		voconf->g_lpddsPrimary->Release();
    voconf->g_lpddsPrimary = NULL;
    printmsg(voconf, "<vo_directx><INFO>overlay surfaces released\n");

    if(!voconf->vidmode){
        if(!voconf->vo_geometry){
            GetWindowRect(voconf->hWnd,&rd);
            voconf->vo_dx=rd.left;
            voconf->vo_dy=rd.top;
        }
        voconf->vo_dx += monitor_rect.left; /* move position to global window space */
        voconf->vo_dy += monitor_rect.top;
        rd.left = voconf->vo_dx;
        rd.top = voconf->vo_dy;
        rd.right = rd.left + voconf->d_image_width;
        rd.bottom = rd.top + voconf->d_image_height;
        if (voconf->WinID == -1) {
        if (voconf->vo_border)
        AdjustWindowRect(&rd,WNDSTYLE,FALSE);
        SetWindowPos(voconf->hWnd,NULL, voconf->vo_dx, voconf->vo_dy,rd.right-rd.left,rd.bottom-rd.top,SWP_SHOWWINDOW|SWP_NOOWNERZORDER);
        }
    }
    else ShowWindow(voconf->hWnd,SW_SHOW);

    if(voconf->vo_fs && !voconf->vidmode)ShowWindow(voconf->hWndFS,SW_SHOW);
//	if (WinID == -1)
//	SetWindowText(h_Wnd,title);

    if(voconf->vidmode)
		voconf->vo_fs=0;

	/*create the surfaces*/
    if(Directx_CreatePrimarySurface(voconf))return 1;

	//create palette for 256 color mode
	if(voconf->image_format==IMGFMT_BGR8){
		LPDIRECTDRAWPALETTE ddpalette=NULL;
		char* palette=(char*)malloc(4*256);
		int i;
		for(i=0; i<256; i++){
			palette[4*i+0] = ((i >> 5) & 0x07) * 255 / 7;
			palette[4*i+1] = ((i >> 2) & 0x07) * 255 / 7;
			palette[4*i+2] = ((i >> 0) & 0x03) * 255 / 3;
			palette[4*i+3] = PC_NOCOLLAPSE;
		}
		voconf->g_lpdd->CreatePalette(DDPCAPS_8BIT|DDPCAPS_INITIALIZE,(LPPALETTEENTRY)palette,&ddpalette,NULL);
		voconf->g_lpddsPrimary->SetPalette(ddpalette);
		free(palette);
		ddpalette->Release();
	}

	if (!voconf->nooverlay && Directx_CreateOverlay(voconf, voconf->image_format))
	{
			if(format == voconf->primary_image_format)
				voconf->nooverlay=1; /*overlay creation failed*/
			else {
              printmsg(voconf, "<vo_directx><FATAL ERROR>can't use overlay mode: please use -vo directx:noaccel\n");
			  return 1;
            }
	}
	if(voconf->nooverlay)
	{
		if(Directx_CreateBackpuffer(voconf))
		{
			printmsg(voconf, "<vo_directx><FATAL ERROR>can't get the driver to work on your system :(\n");
			return 1;
		}
		printmsg(voconf, "<vo_directx><INFO>back surface created\n");
		voconf->vo_doublebuffering = 0;
		/*create clipper for nonoverlay mode*/
	    if(voconf->g_lpdd->CreateClipper(0, &voconf->g_lpddclipper,NULL)!= DD_OK) {
			printmsg(voconf, "<vo_directx><FATAL ERROR>can't create clipper\n");
			return 1;
		}
        if(voconf->g_lpddclipper->SetHWnd(0, voconf->hWnd)!= DD_OK) {
			printmsg(voconf, "<vo_directx><FATAL ERROR>can't associate clipper with window\n");
			return 1;
		}
        if(voconf->g_lpddsPrimary->SetClipper(voconf->g_lpddclipper)!=DD_OK) {
			printmsg(voconf, "<vo_directx><FATAL ERROR>can't associate primary surface with clipper\n");
			return 1;
		}
	    printmsg(voconf, "<vo_directx><INFO>clipper succesfully created\n");
	}else{
//		if(DD_OK != g_lpddsOverlay->QueryInterface(&IID_IDirectDrawColorControl,(void*)&g_cc))
		if(DD_OK != voconf->g_lpddsOverlay->QueryInterface((const IID)IID_IDirectDrawColorControl,(LPVOID*)&voconf->g_cc))
			printmsg(voconf, "<vo_directx><WARN>unable to get DirectDraw ColorControl interface\n");
	}
	Directx_ManageDisplay(voconf);
	memset(&voconf->ddsdsf, 0,sizeof(DDSURFACEDESC2));
	voconf->ddsdsf.dwSize = sizeof (DDSURFACEDESC2);
	if (voconf->g_lpddsBack->Lock(NULL,&voconf->ddsdsf, DDLOCK_NOSYSLOCK | DDLOCK_WAIT, NULL) == DD_OK) {
        voconf->dstride = voconf->ddsdsf.lPitch;
        voconf->image = (uint8_t*)voconf->ddsdsf.lpSurface;
        return 0;
	}
	printmsg(voconf, "<vo_directx><ERROR>Initial Lock on the Surface failed.\n");
	return 1;
}

//function to set color controls
//  brightness	[0, 10000]
//  contrast	[0, 20000]
//  hue		[-180, 180]
//  saturation	[0, 20000]
static uint32_t color_ctrl_set(voconf_t* voconf, char *what, int value)
{
	uint32_t	r = VO_NOTIMPL;
	DDCOLORCONTROL	dcc;
	//printmsg("\n*** %s = %d\n", what, value);
	if (!voconf->g_cc) {
		//printmsg("\n *** could not get color control interface!!!\n");
		return VO_NOTIMPL;
	}
	ZeroMemory(&dcc, sizeof(dcc));
	dcc.dwSize = sizeof(dcc);

	if (!strcmp(what, "brightness")) {
		dcc.dwFlags = DDCOLOR_BRIGHTNESS;
		dcc.lBrightness = (value + 100) * 10000 / 200;
		r = VO_TRUE;
	} else if (!strcmp(what, "contrast")) {
		dcc.dwFlags = DDCOLOR_CONTRAST;
		dcc.lContrast = (value + 100) * 20000 / 200;
		r = VO_TRUE;
	} else if (!strcmp(what, "hue")) {
		dcc.dwFlags = DDCOLOR_HUE;
		dcc.lHue = value * 180 / 100;
		r = VO_TRUE;
	} else if (!strcmp(what, "saturation")) {
		dcc.dwFlags = DDCOLOR_SATURATION;
		dcc.lSaturation = (value + 100) * 20000 / 200;
		r = VO_TRUE;
	}

	if (r == VO_TRUE) {
		voconf->g_cc->SetColorControls(&dcc);
	}
	return r;
}

//analoguous to color_ctrl_set
static uint32_t color_ctrl_get(voconf_t* voconf, char *what, int *value)
{
	uint32_t	r = VO_NOTIMPL;
	DDCOLORCONTROL	dcc;
	if (!voconf->g_cc) {
		//printmsg("\n *** could not get color control interface!!!\n");
		return VO_NOTIMPL;
	}
	ZeroMemory(&dcc, sizeof(dcc));
	dcc.dwSize = sizeof(dcc);

	if (voconf->g_cc->GetColorControls(&dcc) != DD_OK) {
		return r;
	}

	if (!strcmp(what, "brightness") && (dcc.dwFlags & DDCOLOR_BRIGHTNESS)) {
		*value = dcc.lBrightness * 200 / 10000 - 100;
		r = VO_TRUE;
	} else if (!strcmp(what, "contrast") && (dcc.dwFlags & DDCOLOR_CONTRAST)) {
		*value = dcc.lContrast * 200 / 20000 - 100;
		r = VO_TRUE;
	} else if (!strcmp(what, "hue") && (dcc.dwFlags & DDCOLOR_HUE)) {
		*value = dcc.lHue * 100 / 180;
		r = VO_TRUE;
	} else if (!strcmp(what, "saturation") && (dcc.dwFlags & DDCOLOR_SATURATION)) {
		*value = dcc.lSaturation * 200 / 20000 - 100;
		r = VO_TRUE;
	}
//	printmsg("\n*** %s = %d\n", what, *value);

	return r;
}

#if 0
static int control(uint32_t request, void *data, ...)
{
    switch (request) {

	case VOCTRL_GET_IMAGE:
      	return get_image(data);
    case VOCTRL_QUERY_FORMAT:
        last_rect.left = 0xDEADC0DE;   // reset window position cache
        return query_format(*((uint32_t*)data));
	case VOCTRL_DRAW_IMAGE:
        return put_image(data);
    case VOCTRL_BORDER:
			if(WinID != -1) return VO_TRUE;
	        if(vidmode)
			{
				mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>border has no meaning in exclusive mode\n");
			}
	        else
			{
				if(vo_border) {
					vo_border = 0;
					SetWindowLong(h_Wnd, GWL_STYLE, WS_POPUP);
				} else {
					vo_border = 1;
					SetWindowLong(h_Wnd, GWL_STYLE, WNDSTYLE);
				}
				// needed AFAICT to force the window to
				// redisplay with the new style.  --Joey
				if (!vo_fs) {
					ShowWindow(h_Wnd,SW_HIDE);
					ShowWindow(h_Wnd,SW_SHOW);
				}
				last_rect.left = 0xDEADC0DE;   // reset window position cache
				Directx_ManageDisplay();
			}
		return VO_TRUE;
    case VOCTRL_ONTOP:
			if(WinID != -1) return VO_TRUE;
	        if(vidmode)
			{
				mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>ontop has no meaning in exclusive mode\n");
			}
	        else
			{
				if(vo_ontop) vo_ontop = 0;
				else vo_ontop = 1;
				last_rect.left = 0xDEADC0DE;   // reset window position cache
				Directx_ManageDisplay();
			}
		return VO_TRUE;
    case VOCTRL_ROOTWIN:
			if(WinID != -1) return VO_TRUE;
	        if(vidmode)
			{
				mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>rootwin has no meaning in exclusive mode\n");
			}
	        else
			{
				if(vo_rootwin) vo_rootwin = 0;
				else vo_rootwin = 1;
				last_rect.left = 0xDEADC0DE;   // reset window position cache
				Directx_ManageDisplay();
			}
		return VO_TRUE;
    case VOCTRL_FULLSCREEN:
		{
	        if(vidmode)
			{
				mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>currently we do not allow to switch from exclusive to windowed mode\n");
			}
	        else
			{
			    if(!vo_fs)
				{
					vo_fs=1;
					ShowWindow(h_WndFS,SW_SHOW);
					ShowWindow(h_Wnd,SW_HIDE);
					SetForegroundWindow(h_WndFS);
				}
                else
				{
					vo_fs=0;
					ShowWindow(h_WndFS,SW_HIDE);
					ShowWindow(h_Wnd,SW_SHOW);
				}
	      			last_rect.left = 0xDEADC0DE;   // reset window position cache
				Directx_ManageDisplay();
                break;
			}
		    return VO_TRUE;
		}
	case VOCTRL_SET_EQUALIZER: {
		va_list	ap;
		int	value;

		va_start(ap, data);
		value = va_arg(ap, int);
		va_end(ap);
		return color_ctrl_set(data, value);
	}
	case VOCTRL_GET_EQUALIZER: {
		va_list	ap;
		int	*value;

		va_start(ap, data);
		value = va_arg(ap, int*);
		va_end(ap);
		return color_ctrl_get(data, value);
	}
    case VOCTRL_UPDATE_SCREENINFO:
        if (vidmode) {
            vo_screenwidth = vm_width;
            vo_screenheight = vm_height;
        } else {
            vo_screenwidth = monitor_rect.right - monitor_rect.left;
            vo_screenheight = monitor_rect.bottom - monitor_rect.top;
        }
        aspect_save_screenres(vo_screenwidth, vo_screenheight);
        return VO_TRUE;
    case VOCTRL_RESET:
        last_rect.left = 0xDEADC0DE;   // reset window position cache
        // fall-through intended
    };
    return VO_NOTIMPL;
}
#endif

#ifdef __cplusplus
};
#endif

