#include "D3Drender.h"


//define to include log messages for debugging (still going to log errors anyway)
//#define D3DSLOG 1 

//vertex format for direct3d
struct CUSTOMVERTEX {FLOAT X, Y, Z, RHW; DWORD COLOR; FLOAT TU, TV; };
#define CUSTOMFVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 )

//helper function to calculate texture size
static int getNearPowTwo(int num);



    void 
    D3Drender::init(HWND hWnd)
	    {
            
            //check if the window handle we got is 0
            // or maybe the same window we already been drawing to
            if (hWnd == NULL) {  return; } //no window
            if (hWnd == mHwnd) {  return; } //same window 
            if (mHwnd != NULL && (mHwnd != hWnd)) { release(); Sleep(2); } //new window: get rid of old context
              
              mHwnd = hWnd;

            #ifdef D3DSLOG
            slog("D3Drender init device on hwnd %p \n", hWnd);
            #endif

		     d3d = Direct3DCreate9(D3D_SDK_VERSION);
             
             //occurs when there is more than one window -- not sure why
             if (d3d == 0)
             {
                 slog("D3Drender error -- cannot create device \n");
                 mHwnd = NULL;
                 return;
             }//endif

			    D3DPRESENT_PARAMETERS d3dpp;

			    ZeroMemory(&d3dpp, sizeof(d3dpp));
			    d3dpp.Windowed = TRUE;
			    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
			    d3dpp.hDeviceWindow = hWnd;
			    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
			    d3dpp.BackBufferWidth = 640;
			    d3dpp.BackBufferHeight = 480;

			    // create a device class using this information and the info from the d3dpp stuct
			    d3d->CreateDevice(D3DADAPTER_DEFAULT,
							      D3DDEVTYPE_HAL,
							      hWnd,
							      D3DCREATE_SOFTWARE_VERTEXPROCESSING,
							      &d3dpp,
							      &d3ddev);

            //init vertex buffer for our quad
    		    initVertBuffer();

            #ifdef D3DSLOG
                slog("D3Drender init succesful \n");
            #endif
	    }//initd3d




void 
D3Drender::setBuffer(int width, int height, unsigned char * data)
{ 
    if (d3ddev == NULL) 
        { 

#ifdef D3DSLOG
            slog("D3Drender setBuffer-- no device \n");
#endif

            return;
        }//endif

    mtex.setTexture(d3ddev, width, height, data);
    if (dw != width || dh != height)
    {
        dw = width;
        dh = height;

        updateVertBuffer(v_buffer, 640.0f, 480.0f, (float) mtex.width, (float) mtex.height, (float) width, (float) height);
    }//endif

}//setbuffer





void 
D3Drender::render(void)
	{
        if (d3ddev == NULL) 
        { 
            #ifdef D3DSLOG
                slog("D3Drender render-- no device \n");
            #endif

             release(); 
             mHwnd = NULL;

            return; 
        }//endif

          d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

            d3ddev->BeginScene();

		        d3ddev->SetTexture(0, mtex.ptex);
		        d3ddev->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
                d3ddev->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
                d3ddev->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
                d3ddev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

                // select which vertex format we are using
                d3ddev->SetFVF(CUSTOMFVF);

                // select the vertex buffer to display
                d3ddev->SetStreamSource(0, v_buffer, 0, sizeof(CUSTOMVERTEX));

                //draw our quad
		        d3ddev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

            d3ddev->EndScene();

             d3ddev->Present(NULL, NULL, NULL, NULL);

            //check if the device was lost (but not every render)
             //i have yet to encounter any of these log messages
             //so far device loss seem to be just d3ddev  becoming NULL
            wait += 1;
            if (wait >= 5)
            { 
                wait = 0;

                HRESULT coopResult;
                coopResult = d3ddev->TestCooperativeLevel();

                if (coopResult == D3DERR_DEVICELOST)
                { slog("D3DRender  D3DERR_DEVICELOST \n"); }

                if (coopResult == D3DERR_DEVICENOTRESET)
                { slog("D3DRender  D3DERR_DEVICENOTRESET \n"); }
 
                if (coopResult == D3DERR_DRIVERINTERNALERROR)
                { slog("D3DRender  D3DERR_DRIVERINTERNALERROR \n"); }

                if ((coopResult == D3DERR_DEVICELOST ) || (coopResult == D3DERR_DEVICENOTRESET) || (coopResult == D3DERR_DRIVERINTERNALERROR) )
                {
                  release(); 
                  mHwnd = NULL;
                }//endif
            }//endif

	}//update









void 
D3Drender::initVertBuffer(void)
	{
        //default render is a bluish screen
        CUSTOMVERTEX vertices[] =
		{
			{ 0.0f, 0.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 255), 0.0f, 0.0f },
			{ 640.0f, 480.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 1.0f },
			{ 0.0f, 480.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 128), 0.0f, 1.0f },

			{ 640.0f, 480.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 1.0f },
			{ 0.0f, 0.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 255), 0.0f, 0.0f },
			{ 640.0f, 0.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 128), 1.0f, 0.0f },

		};


		// create a vertex buffer interface called v_buffer
		d3ddev->CreateVertexBuffer(6*sizeof(CUSTOMVERTEX),
								   0,
								   CUSTOMFVF,
								   D3DPOOL_MANAGED,
								   &v_buffer,
								   NULL);

		VOID* pVoid;    // a void pointer

		// lock v_buffer and load the vertices into it
			v_buffer->Lock(0, 0, (void**)&pVoid, 0);
			memcpy(pVoid, vertices, sizeof(vertices));
			v_buffer->Unlock();

	}//initbuff






/*
the way this works is
that we got a texture that is a square
bigger than the video
the video is copied onto the texture
we render a quad (2 triangles actually) that fits the whole drawing area
but we don't want the entire texture on it
just the part with the video
so we need to calculate the right UV texture coordinates
e.g.
512 texture
320 video width
320/512  --  u1 is 0.625
*/


void 
D3Drender::updateVertBuffer(LPDIRECT3DVERTEXBUFFER9 vbuf, 
	float screenWidth, float screenHeight,
	float texWidth, float texHeight, 
	float imageWidth, float imageHeight)
{
	 if (vbuf == NULL) { return; } //no buffer specified

	 float u0;
	 float u1;
	 float v0;
	 float v1;

	 u0 = 0.0f;
	 v0 = 0.0f;

	 u1 = imageWidth / texWidth;
	 v1 = imageHeight / texHeight;

	CUSTOMVERTEX vertices[] =
    {
        { 0.0f, 0.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 255, 255), u0, v0 },
        { screenWidth, screenHeight, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 255, 255), u1, v1 },
        { 0.0f, screenHeight, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 255, 255), u0, v1 },

		{ screenWidth, screenHeight, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 255, 255), u1, v1 },
		{ 0.0f, 0.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 255, 255), u0, v0 },
		{ screenWidth, 0.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 255, 255), u1, v0 },
    };


	VOID* pVoid;    // a void pointer

    // lock buffer and load the vertices into it
		vbuf->Lock(0, 0, (void**)&pVoid, 0);
			memcpy(pVoid, vertices, sizeof(vertices));
		vbuf->Unlock();
}//upbuffer





void 
D3Dtex::initTexture(IDirect3DDevice9 * pDevice, int size)
    {
      

        if (ptex != NULL) { release(); }

        //make sure texture size is power of 2
        //todo: check max texture size supported by device

        size = getNearPowTwo(size);

            if (size > 4096) { size = 4096; }

        width = size;
        height = size;

        #ifdef D3DSLOG
         slog("D3Dtex creating texture [%d x %d] \n", width, height);
        #endif   

        int num; 
         num = width * height;
  
         pDevice->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &ptex, NULL);

             if (ptex == NULL) { slog("D3Dtex error: couldn't create texture \n"); return; }
 
             #ifdef D3DSLOG
                slog("D3Dtex  texture creation succesful  \n");
            #endif
	}//inittex


void 
D3Dtex::setTexture(IDirect3DDevice9 * pDevice, int pw, int ph, unsigned char * pdata)
{
    //texture doesn't exist yet
    if (ptex == NULL || ((pw > width || ph > height)&&(width != 4096 && height != 4096)))
    {
        int size;
        size = pw > ph ? pw : ph;

        if (pw > 4096 || ph > 4096)
        {
            slog("D3Dtex warning: video size [%d x %d] not supported, too large (max: 4096x4096) \n ", pw, ph);
        }//endif

        initTexture(pDevice, size);
    }//endif

        //an extra test but just to prevent crashing
          if (pw > width) { pw = width; }
          if (ph > height) { ph = height; }

    unsigned int * data;
    int i;

    data = (unsigned int *) pdata;

    	D3DLOCKED_RECT lockedRect;
				HRESULT hResult = ptex->LockRect(0, &lockedRect, NULL, D3DLOCK_DISCARD);
				
				if(FAILED(hResult))	{  return;  }// Or handle error
				
				BYTE* pBits = (BYTE*)lockedRect.pBits;
	
				for (i = 0; i < ph; ++i)
				{  memcpy(pBits + i*lockedRect.Pitch, data + i*pw,  pw*4); }
			
				ptex->UnlockRect(0);


}//settexture





//http://jeffreystedfast.blogspot.hu/2008/06/calculating-nearest-power-of-2.html
		static int getNearPowTwo(int num)
		{
			int n = num > 0 ? num - 1 : 0;

			n |= n >> 1;
			n |= n >> 2;
			n |= n >> 4;
			n |= n >> 8;
			n |= n >> 16;
			n++;

			return n;
		}//getnearpowtwo
				
