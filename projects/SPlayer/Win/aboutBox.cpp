
#include <windows.h>
#include <Winuser.h>

#include "winres.h"

//dialog based on
//http://msdn.microsoft.com/en-us/library/windows/desktop/ms644996(v=vs.85).aspx#template_in_memory


static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{


	HDC hdc;
	HDC hdcMem;
	static HBITMAP hBitmap;



    switch (message)
    {
    case WM_INITDIALOG:
        return TRUE;

    case WM_PAINT:
        hdc = GetDC(hwndDlg);

        if (hBitmap == NULL)
        {
            //we need to get the logo from the resource in this crazy way for some reason
            //(there doesn't seem any documentation on if there is a proper way to do this with firebreath) 
          HMODULE hModule; 
          GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, L"npSPlayer.dll", &hModule); 
          hBitmap = LoadBitmap(hModule, MAKEINTRESOURCE(IDB_BITMAPCAELOGO));
        }//endif

            if (hBitmap == NULL)    {   ReleaseDC(hwndDlg, hdc);   return TRUE;    }
          
					hdcMem = CreateCompatibleDC(NULL);
					SelectObject(hdcMem, hBitmap);

					BitBlt(hdc, 10, 10, 140, 140, hdcMem, 0, 0, SRCCOPY);

					DeleteDC(hdcMem);

        ReleaseDC(hwndDlg, hdc);
        return TRUE;//wmpaint

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
            //release the bitmap (it's only ok here because there can be at most 1 about box dialog active at a time)
            if (hBitmap != NULL)
            {
                DeleteObject(hBitmap);
                hBitmap = NULL;
            }

            EndDialog(hwndDlg, LOWORD(wParam));
            return TRUE;
        }
    }
    return FALSE;
}


static LPWORD lpwAlign(LPWORD lpIn, ULONG dw2Power = 4)
{
    ULONG ul;

    ul = (ULONG)lpIn;
    ul += dw2Power-1;
    ul &= ~(dw2Power-1);
    return (LPWORD)ul;
}


static LRESULT DisplayMyMessage(HINSTANCE hinst, HWND hwndOwner, LPSTR lpszMessage)
{
    HGLOBAL hgbl;
    LPDLGTEMPLATE lpdt;
    LPDLGITEMTEMPLATE lpdit;
    LPWORD lpw;
    LPWSTR lpwsz;
    LRESULT ret;
    int nchar;

    hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024);
    if (!hgbl)
        return -1;

    lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);

    // Define a dialog box.

    lpdt->style = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION;
    lpdt->cdit = 2;         // Number of controls
    lpdt->x  = 10;  lpdt->y  = 10;
    //lpdt->cx = 100; lpdt->cy = 100; //(width and height /2)
    lpdt->cx = 200; lpdt->cy = 120;


    lpw = (LPWORD)(lpdt + 1);
    *lpw++ = 0;             // No menu
    *lpw++ = 0;             // Predefined dialog box class (by default)

    lpwsz = (LPWSTR)lpw;
    nchar = MultiByteToWideChar(CP_ACP, 0, "About SPlayer", -1, lpwsz, 50);
    lpw += nchar;

    //-----------------------
    // Define an OK button.
    //-----------------------
    lpw = lpwAlign(lpw);    // Align DLGITEMTEMPLATE on DWORD boundary
    lpdit = (LPDLGITEMTEMPLATE)lpw;
    lpdit->x  = 10; lpdit->y  = 90;
    lpdit->cx = 80; lpdit->cy = 20;
    lpdit->id = IDOK;       // OK button identifier
    lpdit->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;

    lpw = (LPWORD)(lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0080;        // Button class

    lpwsz = (LPWSTR)lpw;
    nchar = MultiByteToWideChar(CP_ACP, 0, "OK", -1, lpwsz, 50);
    lpw += nchar;
    *lpw++ = 0;             // No creation data


    //-----------------------
    // Define a static text control.
    //-----------------------
    lpw = lpwAlign(lpw);    // Align DLGITEMTEMPLATE on DWORD boundary
    lpdit = (LPDLGITEMTEMPLATE)lpw;
    lpdit->x  = 110; lpdit->y  = 10;
    lpdit->cx = 40; lpdit->cy = 20;
    //lpdit->id = ID_TEXT;    // Text identifier
    lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;

    lpw = (LPWORD)(lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0082;        // Static class

    lpwsz = (LPWSTR)lpw;
    nchar = MultiByteToWideChar(CP_ACP, 0, lpszMessage, -1, lpwsz, 150);
    lpw += nchar;
    *lpw++ = 0;             // No creation data

    GlobalUnlock(hgbl);
    ret = DialogBoxIndirect(hinst,
        (LPDLGTEMPLATE)hgbl,
        hwndOwner,
        (DLGPROC)DialogProc);
    GlobalFree(hgbl);
    return ret;
}//dialogtemp


void showCaeAboutBox(HWND hWnd)
{
  if (hWnd == NULL) { return; }
  DisplayMyMessage(GetModuleHandle(NULL), hWnd, "SPlayer by CAE 2013");
}//showcaeabout




