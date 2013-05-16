#pragma once

#include <Windows.h>
#include <d3d9.h>
#include "libxplayer.h"

#pragma comment (lib, "d3d9.lib")


//dynamic direct3d texture
class D3Dtex
{
public:
	int width;
	int height;
	IDirect3DTexture9 * ptex;
    

public:
	D3Dtex()
	{
		width = 0;
		height = 0;
		ptex = NULL;
	}//ctor

	virtual ~D3Dtex() 
	{
		release();
	}//dtor

	void release()
	{
		if (ptex != NULL) { ptex->Release(); } ptex = NULL;
		width = 0;
		height = 0;
	}//release


    void setTexture(IDirect3DDevice9 * pDevice, int pw, int ph, unsigned char * data);
	
private:   
    void initTexture(IDirect3DDevice9 * pDevice, int size);

};//classend D3Dtex


//handles direct3d device creation and textured quad rendering
class D3Drender
{
public:
	LPDIRECT3D9 d3d;    //  pointer to Direct3D interface
	LPDIRECT3DDEVICE9 d3ddev;    //  pointer to  device class
	LPDIRECT3DVERTEXBUFFER9 v_buffer;    // pointer to  vertex buffer

    HWND mHwnd;
	D3Dtex mtex;

    int wait;
    int dw;
    int dh;

public:
	D3Drender()
	{
		d3d = NULL;
		d3ddev = NULL;
		v_buffer = NULL;
        mHwnd = NULL;
        wait = 0;
        dw = 0;
        dh = 0;

		//slog("D3Drender ctor \n");
	}//ctor

	virtual ~D3Drender() 
	{
		release(); 
	}//dtor

	void release(void)
	{
	   //slog("D3Drender release \n");

	   mtex.release();
       
       dw = 0;
       dh = 0;


	   if (v_buffer != NULL) {   v_buffer->Release(); } v_buffer = NULL;    
	   if (d3ddev != NULL) {  d3ddev->Release(); } d3ddev = NULL;    
	   if (d3d != NULL) { d3d->Release(); } d3d = NULL;   
	}//release

    //auto detects if it was already init
	    void init(HWND hWnd);

    //render the image
	    void render(void);

    //set raw data
        void setBuffer(int width, int height, unsigned char * data);

private:
	    void initVertBuffer(void);


	    void updateVertBuffer(LPDIRECT3DVERTEXBUFFER9 vbuf, 
		    float screenWidth, float screenHeight,
		    float texWidth, float texHeight, 
		    float imageWidth, float imageHeight);


};//classend D3Drender



