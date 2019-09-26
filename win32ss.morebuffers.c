#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <tchar.h>
#include <regstr.h>

#include "win32ss_hook.h"

#define dimof(x) (sizeof(x) / sizeof(*x))


/*#define SCR_DEBUGFILE _T("scroutput.txt")*/

#define SCRM_GETMONITORAREA (UINT)WM_APP    /* gets the client rectangle. 0=all, 1=topleft, 2=topright, 3=botright, 4=botleft corner */

/* These global variables are loaded at the start of WinMain */
static DWORD MouseThreshold;           // In pixels
static DWORD PasswordDelay;            // In seconds. Doesn't apply to NT/XP/Win2k.

/* and these are created when the dialog/saver starts */
static POINT InitCursorPos;
static DWORD InitTime;                 /* in ms */
static BOOL IsDialogActive;
static BOOL ReallyClose;               // for NT, so we know if a WM_CLOSE came from us or it.

// Some other minor global variables and prototypes
static HINSTANCE hInstance = 0;
static HICON hiconsm = 0, hiconbg = 0;
static HBITMAP hbmmonitor = 0;         // bitmap for the monitor class
static TCHAR const *reg_path;          // where in the registry our bits are
static struct
{
   int count;
   RECT arr[16];
} monitors;                      /* the rectangles of each monitor (smSaver) or of the preview window (smPreview) */

static struct
{
   HANDLE hbmBuffer, hbmBufferNative;
   int bw, bh, bp;              // dimensions of the back-buffer
   void *pixels;
} saverimage;

typedef struct
{
   HWND hwnd;
   int id;                       /* id=-1 for a preview, or 0..n for full-screen on the specified monitor */
   saver_state_t *saver_state;
} TSaverWindow;

//
// IMPORTANT GLOBAL VARIABLES:
static enum TScrMode{ smNone, smConfig, smPassword, smPreview, smSaver, smInstall, smUninstall } ScrMode = smNone;
static struct
{
   TSaverWindow *arr[16];
   int count;
} SaverWindow;                  // the saver windows, one per monitor. In preview mode there's just one.


/* utility functions */

void RegSave_int(TCHAR const *name, int val);
void RegSave_bool(TCHAR const *name, BOOL val);
void RegSave_str(TCHAR const *name, TCHAR const *val);

int RegLoad_int(TCHAR const *name, int def);
BOOL RegLoad_bool(TCHAR const *name, BOOL def);
TCHAR const *RegLoad_str(TCHAR const *name, TCHAR const *def);

#if defined(NDEBUG)
# define Debug(x) (void)(x)
#else
void Debug(TCHAR const *s);
#endif

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------


void CommonInit(void)
{
   static int refcount = 0;

   if (refcount != 0)
      return;
   refcount++;

   if (ScrMode == smPreview)
   {
      saverimage.bw = 320;
      saverimage.bh = 240;
   }
   else
   {
      saverimage.bw = 800;
      saverimage.bh = 600;
   }

   saverimage.bp = saverimage.bw + 8 + 7 & ~7;

   {
      BITMAPINFO bmi = { 0 };
      HDC sdc;

      bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
      bmi.bmiHeader.biWidth = saverimage.bp;
      bmi.bmiHeader.biHeight = saverimage.bh;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 8;
      bmi.bmiHeader.biCompression = BI_RGB;
      bmi.bmiHeader.biSizeImage = (bmi.bmiHeader.biWidth + 3 & ~3) * bmi.bmiHeader.biHeight;
      bmi.bmiHeader.biXPelsPerMeter = 1000000;
      bmi.bmiHeader.biYPelsPerMeter = 1000000;
      bmi.bmiHeader.biClrUsed = 256;
      bmi.bmiHeader.biClrImportant = 0;

      sdc = GetDC(0);
      saverimage.hbmBuffer = CreateDIBSection(sdc, &bmi, DIB_PAL_COLORS, &saverimage.pixels, NULL, 0);
      saverimage.hbmBufferNative = CreateCompatibleBitmap(sdc, saverimage.bw, saverimage.bh);
      ReleaseDC(0, sdc);
   }

   /* TODO: saver initialization - load settings from registry &c.
    * Some things are naturally global, and shared between configuration
    * dialog and saver-preview, or between multiple monitors when running full-screen.
    */
}



TSaverWindow *TSaverWindow_TSaverWindow(HWND hwnd, int id)
{
   TSaverWindow *state;
   
   state = malloc(sizeof(*state));

   if (state == NULL)
      return state;

   CommonInit();

   state->hwnd = hwnd;
   state->id = id;
   state->saver_state = saver_init(saverimage.bw, saverimage.bh, saverimage.bp);

   SetTimer(state->hwnd, 1, 33, NULL);

   return state;
}

void TSaverWindow_xTSaverWindow(TSaverWindow *state)
{
   KillTimer(state->hwnd, 1);

   saver_shutdown(state->saver_state);
   if (saverimage.hbmBuffer != 0)
   {
      DeleteObject(saverimage.hbmBuffer);
      DeleteObject(saverimage.hbmBufferNative);
   }
   saverimage.hbmBuffer = 0;
   saverimage.hbmBufferNative = 0;

   free(state);
}

void TSaverWindow_OnTimer(TSaverWindow *state)
{
   if (saverimage.hbmBuffer == 0)
      return;

   InvalidateRect(state->hwnd, NULL, TRUE);
}

void TSaverWindow_OtherWndProc(TSaverWindow *that, UINT x, WPARAM y, LPARAM z)
{
}

void TSaverWindow_OnPaint(TSaverWindow *state, HDC hdc, const RECT *rc)
{
   int result;
   HDC bufdc;
   saver_rgbquad const *palette;
   int start, count;

   if (saver_advance(state->saver_state, saverimage.pixels) != 0)
   {
      HDC bufdc2;

      bufdc = CreateCompatibleDC(hdc);
      bufdc2 = CreateCompatibleDC(bufdc);

      result = (SelectObject(bufdc, saverimage.hbmBuffer) != NULL);
      result = (SelectObject(bufdc2, saverimage.hbmBufferNative) != NULL);

      if (saver_checkpalette(state->saver_state, &palette, &start, &count) != 0)
         result = SetDIBColorTable(bufdc, start, count, (RGBQUAD *)palette);

      result = BitBlt(bufdc2, 0, 0, saverimage.bw, saverimage.bh, bufdc, (saverimage.bp - saverimage.bp) >> 1, 0, SRCCOPY);
      DeleteDC(bufdc2);
      DeleteDC(bufdc);
   }

   bufdc = CreateCompatibleDC(hdc);
   SelectObject(bufdc, saverimage.hbmBufferNative);
   StretchBlt(hdc, 0, 0, rc->right, rc->bottom, bufdc, 0, 0, saverimage.bw, saverimage.bh, SRCCOPY);
   DeleteDC(bufdc);
}



// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------

BOOL VerifyPassword(HWND hwnd)
{
   typedef BOOL (WINAPI *VERIFYSCREENSAVEPWD)(HWND hwnd);
   HINSTANCE hpwdcpl;
   OSVERSIONINFO osv;
   VERIFYSCREENSAVEPWD VerifyScreenSavePwd;
   BOOL bres;


   /* Under NT, we return TRUE immediately. This lets the saver quit, and the system manages passwords. */
   /* Under '95, we call VerifyScreenSavePwd. This checks the appropriate registry key and, if necessary, pops up a verify dialog */
   osv.dwOSVersionInfoSize = sizeof(osv);
   GetVersionEx(&osv);

   if (osv.dwPlatformId == VER_PLATFORM_WIN32_NT)
      return TRUE;

   if ((hpwdcpl = LoadLibrary(_T("PASSWORD.CPL"))) == NULL)
   {
      Debug(_T("Unable to load PASSWORD.CPL. Aborting"));
      return TRUE;
   }

   if ((VerifyScreenSavePwd = (VERIFYSCREENSAVEPWD)GetProcAddress(hpwdcpl, "VerifyScreenSavePwd")) == NULL)
   {
      Debug(_T("Unable to get VerifyPwProc address. Aborting"));
      FreeLibrary(hpwdcpl);
      return TRUE;
   }

   Debug(_T("About to call VerifyPwProc"));
   bres = VerifyScreenSavePwd(hwnd);
   FreeLibrary(hpwdcpl);

   return bres;
}

void ChangePassword(HWND hwnd)
{
   typedef VOID (WINAPI *PWDCHANGEPASSWORD)(LPCSTR lpcRegkeyname, HWND hwnd, UINT uiReserved1, UINT uiReserved2);
   HINSTANCE hmpr;
   PWDCHANGEPASSWORD PwdChangePassword;

   /* This only ever gets called under '95, when started with the /a option. */
   if ((hmpr = LoadLibrary(_T("MPR.DLL"))) == NULL)
   {
      Debug(_T("MPR.DLL not found: cannot change password."));
      return;
   }

   if ((PwdChangePassword = (PWDCHANGEPASSWORD)GetProcAddress(hmpr, "PwdChangePasswordA")) == NULL)
   {
      FreeLibrary(hmpr);
      Debug(_T("PwdChangeProc not found: cannot change password"));
      return;
   }
   PwdChangePassword("SCRSAVE", hwnd, 0, 0);
   FreeLibrary(hmpr);
}



void ReadGeneralRegistry(void)
{
   LONG res;
   HKEY skey;
   DWORD valtype, valsize, val;

   PasswordDelay = 15;
   MouseThreshold = 50;

   if (RegOpenKeyEx(HKEY_CURRENT_USER, REGSTR_PATH_SETUP _T("\\Screen Savers"), 0, KEY_READ, &skey) != ERROR_SUCCESS)
      return;

   valsize = sizeof (val);
   res = RegQueryValueEx(skey, _T("Password Delay"), 0, &valtype, (LPBYTE)&val, &valsize);
   if (res == ERROR_SUCCESS)
      PasswordDelay = val;

   valsize = sizeof (val);
   res = RegQueryValueEx(skey, _T("Mouse Threshold"), 0, &valtype, (LPBYTE)&val, &valsize);
   if (res == ERROR_SUCCESS)
      MouseThreshold = val;

   RegCloseKey(skey);
}

void WriteGeneralRegistry(void)
{
   HKEY skey;
   DWORD val;


   if (RegCreateKeyEx(HKEY_CURRENT_USER, REGSTR_PATH_SETUP _T("\\Screen Savers"), 0, 0, 0, KEY_ALL_ACCESS, 0, &skey, 0) != ERROR_SUCCESS)
      return;

   val = PasswordDelay;
   RegSetValueEx(skey, _T("Password Delay"), 0, REG_DWORD, (const BYTE *)&val, sizeof (val));

   val = MouseThreshold;
   RegSetValueEx(skey, _T("Mouse Threshold"), 0, REG_DWORD, (const BYTE *)&val, sizeof (val));

   RegCloseKey(skey);

   {
      HINSTANCE sagedll;

      if ((sagedll = LoadLibrary(_T("sage.dll"))) != 0 || (sagedll = LoadLibrary(_T("scrhots.dll"))) != 0)
      {
         typedef VOID (WINAPI *SCREENSAVERCHANGED)(void);
         SCREENSAVERCHANGED changedproc;

         if ((changedproc = (SCREENSAVERCHANGED) GetProcAddress (sagedll, "Screen_Saver_Changed")) != NULL)
            changedproc();
         FreeLibrary(sagedll);
      }
   }
}



LRESULT CALLBACK SaverWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   TSaverWindow *sav;
   int id;
   HWND hmain;
   if (msg == WM_CREATE)
   {
      CREATESTRUCT *cs = (CREATESTRUCT *) lParam;
      id = *(int *)cs->lpCreateParams;
      SetWindowLong(hwnd, 0, id);
      sav = TSaverWindow_TSaverWindow(hwnd, id);
      SetWindowLong(hwnd, GWL_USERDATA, (LONG)sav);
      if (SaverWindow.count < dimof(SaverWindow.arr))
         SaverWindow.arr[SaverWindow.count++] = sav;
   }
   else
   {
      sav = (TSaverWindow *)GetWindowLong(hwnd, GWL_USERDATA);
      id = GetWindowLong(hwnd, 0);
   }
   if (id <= 0)
      hmain = hwnd;
   else
      hmain = SaverWindow.arr[0]->hwnd;

   if (msg == WM_TIMER)
      TSaverWindow_OnTimer(sav);
   else if (msg == WM_PAINT)
   {
      PAINTSTRUCT ps;
      RECT rc;

      BeginPaint(hwnd, &ps);
      GetClientRect(hwnd, &rc);
      if (sav != 0)
         TSaverWindow_OnPaint(sav, ps.hdc, &rc);
      EndPaint(hwnd, &ps);
   }
   else if (sav != 0)
      TSaverWindow_OtherWndProc(sav, msg, wParam, lParam);

   switch (msg)
   {
   case WM_ACTIVATE:
   case WM_ACTIVATEAPP:
   case WM_NCACTIVATE:
      {
         TCHAR pn[100];
         BOOL ispeer;

         GetClassName((HWND)lParam, pn, 100);
         ispeer = (_tcsncmp(pn, _T("ScrClass"), 8) == 0);
#if defined(NDEBUG)
         if (ScrMode == smSaver && !IsDialogActive && LOWORD(wParam) == WA_INACTIVE && !ispeer)
         {
            Debug(_T("WM_ACTIVATE: about to inactive window, so sending close"));
            ReallyClose = TRUE;
            PostMessage(hmain, WM_CLOSE, 0, 0);
            return 0;
         }
#endif
      }
      break;
   case WM_SETCURSOR:
#if defined(NDEBUG)
      if (ScrMode == smSaver && !IsDialogActive)
         SetCursor (NULL);
      else
#endif
         SetCursor(LoadCursor (NULL, IDC_ARROW));
      return 0;

   case WM_KEYDOWN:
      /* allow tab and 'a' to make something happen */
      if (wParam == 9 || wParam == 65)
      {
         saver_keystroke(sav->saver_state, wParam);
         break;
      }

   case WM_LBUTTONDOWN:
   case WM_MBUTTONDOWN:
   case WM_RBUTTONDOWN:
      if (ScrMode == smSaver && !IsDialogActive)
      {
         Debug (_T("WM_BUTTONDOWN: sending close"));
         ReallyClose = TRUE;
         PostMessage (hmain, WM_CLOSE, 0, 0);
         return 0;
      }
      break;

   case WM_MOUSEMOVE:
#if defined(NDEBUG)
      if (ScrMode == smSaver && !IsDialogActive)
      {
         POINT pt;
         int dx, dy;

         GetCursorPos(&pt);

         dx = pt.x - InitCursorPos.x;
         if (dx < 0)
            dx = -dx;
         dy = pt.y - InitCursorPos.y;
         if (dy < 0)
            dy = -dy;
         if (dx > (int)MouseThreshold || dy > (int)MouseThreshold)
         {
            Debug(_T("WM_MOUSEMOVE: gone beyond threshold, sending close"));
            ReallyClose = TRUE;
            PostMessage(hmain, WM_CLOSE, 0, 0);
         }
      }
#endif
      return 0;

   case WM_SYSCOMMAND:
      if (ScrMode == smSaver)
      {
         if (wParam == SC_SCREENSAVE)
         {
            Debug(_T("WM_SYSCOMMAND: gobbling up a SC_SCREENSAVE to stop a new saver from running."));
            return 0;
         }
#if defined(NDEBUG)
         if (wParam == SC_CLOSE)
         {
            Debug(_T("WM_SYSCOMMAND: gobbling up a SC_CLOSE"));
            return 0;
         }
#endif
      }
      break;

   case WM_CLOSE:
      if (id > 0)
         return 0;              // secondary windows ignore this message
      if (id == -1)
      {
         DestroyWindow (hwnd);
         return 0;
      }                         // preview windows close obediently
      if (ReallyClose && !IsDialogActive)
      {
         BOOL CanClose = TRUE;

         Debug (_T("WM_CLOSE: maybe we need a password"));
         if (GetTickCount() - InitTime > 1000 * PasswordDelay)
         {
            IsDialogActive = TRUE;
            SendMessage(hwnd, WM_SETCURSOR, 0, 0);
            CanClose = VerifyPassword (hwnd);
            IsDialogActive = FALSE;
            SendMessage(hwnd, WM_SETCURSOR, 0, 0);
            GetCursorPos(&InitCursorPos);
         }
         // note: all secondary monitors are owned by the primary. And we're the primary (id==0)
         // therefore, when we destroy ourself, windows will destroy the others first
         if (CanClose)
         {
            Debug (_T ("WM_CLOSE: doing a DestroyWindow"));
            DestroyWindow (hwnd);
         }
         else
         {
            Debug (_T ("WM_CLOSE: but failed password, so doing nothing"));
         }
      }
      return 0;

   case WM_DESTROY:
      {
         int i;

         Debug (_T ("WM_DESTROY."));
         SetWindowLong (hwnd, 0, 0);
         SetWindowLong (hwnd, GWL_USERDATA, 0);
         for (i = 0; i != SaverWindow.count; i++)
         {
            if (sav == SaverWindow.arr[i])
               SaverWindow.arr[i] = 0;
         }
         TSaverWindow_xTSaverWindow(sav);
         if ((id == 0 && ScrMode == smSaver) || ScrMode == smPreview)
            PostQuitMessage (0);
      }
      break;
   }

   return DefWindowProc(hwnd, msg, wParam, lParam);
}




/* ENUM-MONITOR-CALLBACK is part of DoSaver. Its stuff is in windef.h but
 * requires WINVER>=0x0500. Well, we're doing LoadLibrary, so we know it's
 * safe...
 */
#ifndef SM_CMONITORS
DECLARE_HANDLE (HMONITOR);
#endif

BOOL CALLBACK EnumMonitorCallback(HMONITOR x, HDC y, LPRECT rc, LPARAM z)
{
   if (monitors.count < dimof(monitors.arr))
   {
      if (rc->left == 0 && rc->top == 0)
      {
         memmove(&monitors.arr[1], &monitors.arr[0], sizeof(*monitors.arr) * monitors.count++);
         monitors.arr[0] = *rc;
      }
      else
         monitors.arr[monitors.count++] = *rc;
   }

   return TRUE;
}

void DoSaver(HWND hparwnd, BOOL fakemulti)
{
   HWND hwnd = 0;
   monitors.count = 0;

   if (ScrMode == smPreview)
   {
      RECT rc;
      GetWindowRect(hparwnd, &rc);
      monitors.count = 1;
      monitors.arr[0] = rc;
   }
   else if (fakemulti)
   {
      int w = GetSystemMetrics(SM_CXSCREEN), x1 = w / 4, x2 = w * 2 / 3, h =
         x2 - x1;
      RECT rc;

      rc.left = x1;
      rc.top = x1;
      rc.right = x1 + h;
      rc.bottom = x1 + h;
      monitors.arr[0] = rc;
      rc.left = 0;
      rc.top = x1;
      rc.right = x1;
      rc.bottom = x1 + x1;
      monitors.arr[1] = rc;
      rc.left = x2;
      rc.top = x1 + h + x2 - w;
      rc.right = w;
      rc.bottom = x1 + h;
      monitors.arr[2] = rc;
      monitors.count = 3;
   }
   else
   {
      int num_monitors = GetSystemMetrics(80); // 80=SM_CMONITORS
      if (num_monitors > 1)
      {
         typedef BOOL (CALLBACK *LUMONITORENUMPROC)(HMONITOR, HDC, LPRECT, LPARAM);
         typedef BOOL (WINAPI *LUENUMDISPLAYMONITORS)(HDC, LPCRECT, LUMONITORENUMPROC, LPARAM);
         HINSTANCE husr = LoadLibrary(_T("user32.dll"));
         LUENUMDISPLAYMONITORS pEnumDisplayMonitors = 0;

         if (husr != NULL)
            pEnumDisplayMonitors = (LUENUMDISPLAYMONITORS)GetProcAddress(husr, "EnumDisplayMonitors");
         if (pEnumDisplayMonitors != NULL)
            pEnumDisplayMonitors(NULL, NULL, EnumMonitorCallback, 0);
         if (husr != NULL)
            FreeLibrary(husr);
      }
      if (monitors.count == 0)
      {
         RECT rc;
         rc.left = 0;
         rc.top = 0;
         rc.right = GetSystemMetrics(SM_CXSCREEN);
         rc.bottom = GetSystemMetrics(SM_CYSCREEN);
         monitors.arr[0] = rc;
         monitors.count = 1;
      }
   }

   if (ScrMode == smPreview)
   {
      RECT rc;
      int w, h, id = -1;

      GetWindowRect(hparwnd, &rc);
      w = rc.right - rc.left, h = rc.bottom - rc.top;
      hwnd = CreateWindowEx(0, _T("ScrClass"), _T(""), WS_CHILD | WS_VISIBLE,
                         0, 0, w, h, hparwnd, NULL, hInstance, &id);
   }
   else
   {
      int i;

      GetCursorPos(&InitCursorPos);
      InitTime = GetTickCount();

      for (i = 0; i < monitors.count; i++)
      {
         RECT rc = monitors.arr[i];
         DWORD exstyle = WS_EX_TOPMOST;
         HWND hthis;

#if !defined(NDEBUG)
         exstyle = 0;
#endif
         hthis = CreateWindowEx(exstyle, _T("ScrClass"), _T(""),
                            WS_POPUP | WS_VISIBLE, rc.left, rc.top,
                            rc.right - rc.left, rc.bottom - rc.top, hwnd,
                            NULL, hInstance, &i);
         if (i == 0)
            hwnd = hthis;
      }
   }

   if (hwnd != NULL)
   {
      UINT oldval;
      MSG msg;

      if (ScrMode == smSaver)
         SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, 1, &oldval, 0);

      while (GetMessage(&msg, NULL, 0, 0))
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }

      if (ScrMode == smSaver)
         SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, 0, &oldval, 0);
   }

   SaverWindow.count = 0;
   return;
}



void DoConfig(HWND hpar)
{
   TCHAR title[256];
   TCHAR message[1024];

   title[dimof(title) - 1] = '\0';
   _sntprintf(title, dimof(title) - 1, _T("%s Settings"), saver_name);
   message[dimof(message) - 1] = '\0';
   _sntprintf(message, dimof(message) - 1,
         _T("Some kind of screensaver, by Simon Hosie <gumboot@clear.net.nz>\r\n")
         _T("\r\n")
         _T("Windows integration based on code by Lucian Wischik, http://www.wischik.com/scr/\r\n")
         _T("Some colour maps borrowed from xfractint\r\n")
         _T("\r\n")
         _T("I'm too lazy for a config box.  Go and use RegEdit.exe in:\r\nHKEY_CURRENT_USER\\%s"), reg_path);

   MessageBox(NULL, message, title, MB_OK);
}


void DoInstall(void)
{
   TCHAR msg[1024];
   TCHAR windir[MAX_PATH], target[MAX_PATH], source[MAX_PATH];
   DWORD attr;

   target[dimof(target) - 1] = '\0';
   msg[dimof(msg) - 1] = '\0';

   GetWindowsDirectory(windir, dimof(windir));
   if (GetTempFileName(windir, _T("pst"), 0, target) == FALSE)
   {
      MessageBox(NULL, _T("You must be logged on as system administrator to install screen savers"),
                  _T("Saver Install"), MB_ICONINFORMATION | MB_OK);
      return;
   }
   DeleteFile(target);

   _sntprintf(target, dimof(target) - 1, _T("%s\\%s.scr"), windir, saver_name);
   attr = GetFileAttributes(target);

   _sntprintf(msg, dimof(msg) - 1, _T("Do you want to install '%s'?%s"),
            saver_name,
            (attr != 0xFFFFFFFF)
               ? _T("\r\n\r\n(This will replace the version that you have currently)")
               : _T(""));

   if (MessageBox(NULL, msg, _T("Saver Install"), MB_YESNOCANCEL) != IDYES)
      return;

   GetModuleFileName(hInstance, source, dimof(source));
   SetCursor(LoadCursor (NULL, IDC_WAIT));

   if (CopyFile(source, target, FALSE) == FALSE)
   {
      TCHAR *errbuf;

      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                     NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     (LPSTR /* TODO: fix this cast*/)&errbuf, 0, NULL);
      _sntprintf(msg, dimof(msg) - 1, _T("There was an error installing the saver (%s).\r\n\r\n%s"), target, errbuf);
      LocalFree(errbuf);

      SetCursor(LoadCursor(NULL, IDC_ARROW));
      MessageBox(NULL, msg, _T("Saver Install"), MB_ICONERROR | MB_OK);

      return;
   }


   {
      HKEY skey;
      DWORD disp;
      TCHAR key[1024];

      key[dimof(key) - 1] = '\0';
      _sntprintf(key, dimof(key) - 1, REGSTR_PATH_UNINSTALL _T("\\%s"), saver_name);

      if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, key, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &skey,
                        &disp) == ERROR_SUCCESS)
      {
         _sntprintf(msg, dimof(msg) - 1, _T("%s saver"), saver_name);
         RegSetValueEx(skey, _T("DisplayName"), 0, REG_SZ,
                        (const BYTE *)msg, sizeof(TCHAR) * (_tcslen(msg) + 1));

         _sntprintf(msg, dimof(msg) - 1, _T("\"%s\" /u"), target);
         RegSetValueEx(skey, _T("UninstallString"), 0, REG_SZ,
                        (const BYTE *)msg, sizeof(TCHAR) * (_tcslen(msg) + 1));
         RegSetValueEx(skey, _T("UninstallPath"), 0, REG_SZ,
                        (const BYTE *)msg, sizeof(TCHAR) * (_tcslen(msg) + 1));

         _sntprintf(msg, dimof(msg) - 1, _T("\"%s\""), target);
         RegSetValueEx(skey, _T("ModifyPath"), 0, REG_SZ,
                        (const BYTE *)msg, sizeof(TCHAR) * (_tcslen(msg) + 1));

         RegSetValueEx(skey, _T("DisplayIcon"), 0, REG_SZ,
                        (const BYTE *)target, sizeof(TCHAR) * (_tcslen(target) + 1));

         if (LoadString(hInstance, 2, msg, sizeof(msg)) != 0)
            RegSetValueEx(skey, _T("HelpLink"), 0, REG_SZ,
                        (const BYTE *)msg, sizeof(TCHAR) * (_tcslen(msg) + 1));
         RegCloseKey(skey);

         WriteGeneralRegistry();
      }
   }

   {
      SHELLEXECUTEINFO sex = { 0 };

      sex.cbSize = sizeof(sex);
      sex.fMask = SEE_MASK_NOCLOSEPROCESS;
      sex.lpVerb = _T("install");
      sex.lpFile = target;
      sex.nShow = SW_SHOWNORMAL;

      if (!ShellExecuteEx(&sex))
      {
         SetCursor(LoadCursor(NULL, IDC_ARROW));
         MessageBox(NULL, _T("The saver has been installed"), saver_name, MB_OK);
         return;
      }
      WaitForInputIdle(sex.hProcess, 2000);
      CloseHandle(sex.hProcess);
   }

   SetCursor(LoadCursor(NULL, IDC_ARROW));
}


void DoUninstall(void)
{
   TCHAR buf[1024], result[1024];
   TCHAR fn[MAX_PATH];

   buf[dimof(buf) - 1] = '\0';
   result[dimof(result) - 1] = '\0';

   _sntprintf(buf, dimof(buf) - 1, REGSTR_PATH_UNINSTALL _T("\\%s"), saver_name);
   _tcscpy(result, _T("Uninstall completed. The saver will be removed next time you reboot."));

   RegDeleteKey(HKEY_LOCAL_MACHINE, buf);
   GetModuleFileName(hInstance, fn, MAX_PATH);
   SetFileAttributes(fn, FILE_ATTRIBUTE_NORMAL);       // remove readonly if necessary

   if (MoveFileEx(fn, NULL, MOVEFILE_DELAY_UNTIL_REBOOT) == FALSE)
   {
      TCHAR const *c1;
      TCHAR const *c2;
      
      c1 = strrchr(fn, '\\');
      c2 = strrchr(fn, '/');
      if (c1 == NULL)
            c1 = c2;
      if (c2 != NULL && c1 < c2)
         c1 = c2;
      if (c1 == NULL)
            c1 = fn;

      _sntprintf(result, dimof(result) - 1,
               _T("There was a problem uninstalling.\r\n")
               _T("To complete the uninstall manually, you should go into your Windows ")
               _T("directory and delete the file '%s'"),
               c1);
   }

   _sntprintf(buf, dimof(buf) - 1, _T("%s uninstaller"), saver_name);
   MessageBox(NULL, result, buf, MB_OK);
}



void RegSave_gen(TCHAR const *name, DWORD type, void *buf, int size)
{
   HKEY skey;

   if (RegCreateKeyEx(HKEY_CURRENT_USER, reg_path, 0, 0, 0,
                      KEY_ALL_ACCESS, 0, &skey, 0) != ERROR_SUCCESS)
      return;
   
   RegSetValueEx(skey, name, 0, type, (const BYTE *)buf, size);
   RegCloseKey(skey);
}

BOOL RegLoadDword(TCHAR const *name, DWORD *buf)
{
   HKEY skey;
   LONG result;
   DWORD size = sizeof (DWORD);

   if (RegOpenKeyEx(HKEY_CURRENT_USER, reg_path, 0, KEY_READ, &skey) != ERROR_SUCCESS)
      return FALSE;

   result = RegQueryValueEx(skey, name, 0, 0, (LPBYTE)buf, &size);
   RegCloseKey(skey);

   return (result == ERROR_SUCCESS);
}

void RegSave_int(TCHAR const *name, int val)
{
   DWORD v = val;
   RegSave_gen(name, REG_DWORD, &v, sizeof(v));
}

void RegSave_bool(TCHAR const *name, BOOL val)
{
   RegSave_int(name, val ? 1 : 0);
}

void RegSave_str(TCHAR const *name, TCHAR const *val)
{
   RegSave_gen(name, REG_SZ, (void *)val, sizeof(TCHAR) * (_tcslen(val) + 1));
}

int RegLoad_int(TCHAR const *name, int def)
{
   DWORD val;
   BOOL res = RegLoadDword(name, &val);
   return res ? val : def;
}

BOOL RegLoad_bool(TCHAR const *name, BOOL def)
{
   int b = RegLoad_int(name, def ? 1 : 0);
   return (b != 0);
}

TCHAR const *RegLoad_str(TCHAR const *name, TCHAR const *def)
{
   HKEY skey;
   DWORD size = 0;
   TCHAR *buf;

   if (RegOpenKeyEx(HKEY_CURRENT_USER, reg_path, 0, KEY_READ, &skey) != ERROR_SUCCESS)
      return _tcsdup(def);

   if (RegQueryValueEx(skey, name, 0, 0, 0, &size) != ERROR_SUCCESS)
   {
      RegCloseKey(skey);
      return _tcsdup(def);
   }

   buf = (TCHAR *)malloc(sizeof(TCHAR) * (size + 1));
   RegQueryValueEx(skey, name, 0, 0, (LPBYTE)buf, &size);
   RegCloseKey(skey);

   buf[size] = '\0';

   return buf;
}


#if !defined(Debug)
void Debug(TCHAR const *s)
{
#if defined(SCR_DEBUGFILE)
   FILE *f;

   if ((f = _tfopen(SCR_DEBUGFILE, _T("a+t"))) != NULL)
   {
      _ftprintf(f, _T("%s\n"), s);
      fclose(f);
   }
#else
   TCHAR tmp[1024];

   _sntprintf(tmp, dimof(tmp) - 1, _T("%s\r\n"), s);
   tmp[dimof(tmp) - 1] = '\0';
   OutputDebugString(tmp);
#endif
}
#endif



int WINAPI WinMain(HINSTANCE h, HINSTANCE x, LPSTR y, int z)
{
   HWND hwnd = NULL;
   BOOL fakemulti = FALSE;
   BOOL isexe;

   hInstance = h;

   {
      TCHAR tmp[1024];

      tmp[dimof(tmp) - 1] = '\0';
      _sntprintf(tmp, dimof(tmp) - 1,_T("Software\\Scrplus\\%s"), saver_name);
      reg_path = _tcsdup(tmp);
   }

   {
      TCHAR tmp[MAX_PATH];
      GetModuleFileName(hInstance, tmp, MAX_PATH);
      isexe = (_tcsstr(tmp, _T(".exe")) != NULL || _tcsstr(tmp, _T(".EXE")) != NULL);
   }

   {
      TCHAR *c = GetCommandLine();

      if (*c == '\"')
      {
         c++;
         while (*c != 0 && *c != '\"')
            c++;
         if (*c == '\"')
            c++;
      }
      else
      {
         while (*c != 0 && *c != ' ')
            c++;
      }
      while (*c == ' ')
         c++;

      if (*c == 0)
      {
         if (isexe)
            ScrMode = smInstall;
         else
            ScrMode = smConfig;
         hwnd = NULL;
      }
      else
      {
         if (*c == '-' || *c == '/')
            c++;
         if (*c == 'u' || *c == 'U')
            ScrMode = smUninstall;
         if (*c == 'p' || *c == 'P' || *c == 'l' || *c == 'L')
         {
            c++;
            while (*c == ' ' || *c == ':')
               c++;
            hwnd = (HWND)_ttoi(c);
            ScrMode = smPreview;
         }
         else if (*c == 's' || *c == 'S')
         {
            ScrMode = smSaver;
            fakemulti = (c[1] == 'm' || c[1] == 'M');
         }
         else if (*c == 'c' || *c == 'C')
         {
            c++;
            while (*c == ' ' || *c == ':')
               c++;
            if (*c == 0)
               hwnd = GetForegroundWindow();
            else
               hwnd = (HWND)_ttoi(c);
            ScrMode = smConfig;
         }
         else if (*c == 'a' || *c == 'A')
         {
            c++;
            while (*c == ' ' || *c == ':')
               c++;
            hwnd = (HWND)_ttoi(c);
            ScrMode = smPassword;
         }
      }
   }

   switch (ScrMode)
   {
   case smInstall:
      DoInstall();
      return 0;

   case smUninstall:
      DoUninstall();
      return 0;

   case smPassword:
      ChangePassword(hwnd);
      return 0;

   case smConfig:
      ReadGeneralRegistry();
      DoConfig(hwnd);
      return 0;

   case smSaver:
   case smPreview:
      ReadGeneralRegistry();

      IsDialogActive = FALSE;

      {
         WNDCLASS wc = { 0 };

         wc.hInstance = hInstance;
         wc.hCursor = LoadCursor(NULL, IDC_ARROW);

         wc.lpfnWndProc = SaverWndProc;
         wc.cbWndExtra = 8;
         wc.lpszClassName = _T("ScrClass");
         RegisterClass(&wc);
      }

      DoSaver(hwnd, fakemulti);
      return 0;
   }

   /* shouldn't get here */
   return 1;
}
