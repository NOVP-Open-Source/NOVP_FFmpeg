
#include <stdlib.h>
#include <malloc.h>
#include <stdint.h>
#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include <WTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WNDCLASSNAME_WINDOWED   L"SPlayer - The Movie Player"
#define WNDCLASSNAME_FULLSCREEN L"SPlayer - Fullscreen"
#define WNDSTYLE WS_OVERLAPPEDWINDOW|WS_SIZEBOX

typedef struct voconf_s {
    int64_t     WinID;

    HWND        hWnd;
    HWND        hWndFS;
    int         nooverlay;
    int         vidmode;
	int			vo_refresh_rate;
    uint32_t    primary_image_format;
    uint32_t    vm_height;
    uint32_t    vm_width;
    uint32_t    vm_bpp;
    COLORREF    windowcolor;
    DWORD       destcolorkey;
    char*       vo_geometry;
    int         vo_colorkey;
	int			vo_dbpp;
    int         vo_dx;
    int         vo_dy;
    int         vo_keepaspect;
    float       window_aspect;
    int         vo_screenwidth;
    int         vo_screenheight;
    int         vo_border;
    int         vo_ontop;
    int         vo_rootwin;
    int         vo_fs;
    int         vo_doublebuffering;
    int         vo_directrendering;
    int         vo_adapter_num;
    uint32_t    image_format;
    uint32_t    image_width;
    uint32_t    image_height;
    uint32_t    d_image_width;
    uint32_t    d_image_height;
    uint32_t    dstride;

    uint8_t*    image;
    uint8_t*    tmp_image;


    LPDIRECTDRAWCOLORCONTROL    g_cc;                   //color control interface
    LPDIRECTDRAW7               g_lpdd;                 //DirectDraw Object
    LPDIRECTDRAWSURFACE7        g_lpddsPrimary;         //Primary Surface: viewport through the Desktop
    LPDIRECTDRAWSURFACE7        g_lpddsOverlay;         //Overlay Surface
    LPDIRECTDRAWSURFACE7        g_lpddsBack;            //Back surface
    LPDIRECTDRAWCLIPPER         g_lpddclipper;          //clipper object, can only be used without overlay
    DDSURFACEDESC2              ddsdsf;                 //surface descripiton needed for locking
    HBRUSH                      colorbrush;             // Handle to colorkey brush
    HBRUSH                      blackbrush;             // Handle to black brush
    RECT                        monitor_rect;           //monitor coordinates
    RECT                        last_rect;
    RECT                        rd;                     //rect of our stretched image
    RECT                        rs;                     //rect of our source image

	char*						msg;
} voconf_t;

char*			getmsg(voconf_t* voconf);
voconf_t*		preinit(int noa, HWND hWnd);
void            vo_uninit(voconf_t* voconf);
int				vo_query_format(uint32_t format);
int				vo_config(voconf_t* voconf, uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t options, char *title, uint32_t format);
int             vo_draw_frame(voconf_t* voconf, uint8_t *src[]);
void            vo_flip_page(voconf_t* voconf);

#if 0
voconf_t*       vo_init(HWND hWnd);
void            vo_uninit(voconf_t* voconf);
int				vo_query_format(uint32_t format);
int             vo_config(voconf_t* voconf, uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t options, uint32_t format);
int             vo_draw_frame(voconf_t* voconf, uint8_t *src);
void            vo_flip_page(voconf_t* voconf);
int             vo_errorcode();
int             vo_errorcode2();
#endif

#ifdef __cplusplus
};
#endif
