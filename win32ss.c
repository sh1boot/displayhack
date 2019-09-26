#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <tchar.h>
#include <regstr.h>

#include "win32ss_hook.h"

#define dimof(x) (sizeof(x) / sizeof(*x))

/* to force debug output to a file, set SCR_DEBUGFILE */
/*#define SCR_DEBUGFILE _T("scroutput.txt")*/

typedef struct /* used to describe the state of each monitor */
{
   HWND hwnd;
   int id;     /* id=-1 for a preview, or 0..n for full-screen on the specified monitor */
   saver_state_t *saver_state;
} saver_window_t;


static struct
{
   saver_window_t *arr[16];
   int count;
} saver_windows;        /* the saver windows, one per monitor. In preview mode there's just one. */

static enum
{
   SM_NONE, SM_CONFIG, SM_PASSWORD, SM_PREVIEW, SM_SAVER, SM_INSTALL, SM_UNINSTALL
} saver_mode = SM_NONE;


static int smoothscaling, ignorecontrol, ignoreshift;
static int previewrenderwidth, previewrenderheight, renderwidth, renderheight;

const saver_config_item_t wrapper_config[] =
{
   { "SmoothScaling", &smoothscaling, 0 },
   { "IgnoreControl", &ignorecontrol, 0 },
   { "IgnoreShift", &ignoreshift, 0 },
   { "PreviewRenderWidth", &previewrenderwidth, 152 },
   { "PreviewRenderHeight", &previewrenderheight, 112 },
   { "RenderWidth", &renderwidth, 640 },
   { "RenderHeight", &renderheight, 480 },
   { NULL, NULL, 0, 0 }
};


/* utility functions */

static void RegSave_int(char const *name, int val);
static void RegSave_str(char const *name, TCHAR const *val);

static int RegLoad_int(char const *name, int def);
static char const *RegLoad_str(char const *name, char const *def);

#if defined(NDEBUG)
# define Debug(x) (void)(x)
#else
static void Debug(TCHAR const *s);
#endif


/*******************************************************************************
*** Wrappers around win32ss_hook.c *********************************************
*******************************************************************************/

/* what it looks like: */
static struct
{
   HANDLE imagehandle;
   int width, height, pitch;
   void *pixels;
} saver_image;


static void CommonInit(void)
{
   static int refcount = 0;

   if (refcount != 0)
      return;
   refcount++;

   {
      saver_config_item_t const * const ptrtab[] = { SaverConfig, wrapper_config };
      int i;

      for (i = 0; i < dimof(ptrtab); i++)
      {
         saver_config_item_t const *ptr = ptrtab[i];

         while (ptr->name != NULL)
         {
            if (ptr->is_string == 0)
               *ptr->dest = RegLoad_int(ptr->name, ptr->def_value);
            else
               *(char const **)ptr->dest = RegLoad_str(ptr->name, (char const *)ptr->def_value);
            ptr++;
         }
      }

      if (previewrenderwidth < 80)     previewrenderwidth = 80;
      if (previewrenderwidth > 400)    previewrenderwidth = 400;
      if (previewrenderheight < 60)    previewrenderheight = 60;
      if (previewrenderheight > 300)   previewrenderheight = 300;
      if (renderwidth < 160)           renderwidth = 160;
      if (renderwidth > 1600)          renderwidth = 1600;
      if (renderheight < 120)          renderheight = 120;
      if (renderheight > 1200)         renderheight = 1200;
   }

   if (saver_mode == SM_PREVIEW)
   {
      saver_image.width = previewrenderwidth;
      saver_image.height = previewrenderheight;
   }
   else
   {
      saver_image.width = renderwidth;
      saver_image.height = renderheight;
   }

   saver_image.pitch = (saver_image.width + 8 + 7) & ~7;

   {
      BITMAPINFO bmi = { { 0 } };
      HDC sdc;

      bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
      bmi.bmiHeader.biWidth = saver_image.pitch;
      bmi.bmiHeader.biHeight = saver_image.height;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 8;
      bmi.bmiHeader.biCompression = BI_RGB;
      bmi.bmiHeader.biSizeImage = ((bmi.bmiHeader.biWidth + 3) & ~3) * bmi.bmiHeader.biHeight;
      bmi.bmiHeader.biXPelsPerMeter = 1000000;
      bmi.bmiHeader.biYPelsPerMeter = 1000000;
      bmi.bmiHeader.biClrUsed = 256;
      bmi.bmiHeader.biClrImportant = 0;

      sdc = GetDC(0);
      saver_image.imagehandle = CreateDIBSection(sdc, &bmi, DIB_PAL_COLORS, &saver_image.pixels, NULL, 0);
      ReleaseDC(0, sdc);
   }
}


static saver_window_t *TSaverWindow_Create(HWND hwnd, int id)
{
   saver_window_t *state;
   
   if ((state = malloc(sizeof(*state))) == NULL)
      return state;

   CommonInit();

   state->hwnd = hwnd;
   state->id = id;
   state->saver_state = saver_init(saver_image.width, saver_image.height, saver_image.pitch);

   SetTimer(state->hwnd, 1, SaverTickPeriod, NULL);

   return state;
}


static void TSaverWindow_Destroy(saver_window_t *state)
{
   KillTimer(state->hwnd, 1);

   saver_shutdown(state->saver_state);
   if (saver_image.imagehandle != 0)
      DeleteObject(saver_image.imagehandle);
   saver_image.imagehandle = 0;
   saver_image.pixels = NULL;

   free(state);
}


static void TSaverWindow_OnTimer(saver_window_t *state)
{
   if (saver_image.imagehandle == 0)
      return;

   InvalidateRect(state->hwnd, NULL, TRUE);
}


static void TSaverWindow_OtherWndProc(saver_window_t *that, UINT x, WPARAM y, LPARAM z)
{
}


static void TSaverWindow_OnPaint(saver_window_t *state, HDC hdc, const RECT *rc)
{
   HDC bufdc;
   saver_rgbquad const *palette;
   int start, count;

   if (state->saver_state != NULL && saver_image.pixels != NULL)
      saver_advance(state->saver_state, saver_image.pixels);

   if (saver_image.imagehandle != 0)
   {
      bufdc = CreateCompatibleDC(hdc);
      SelectObject(bufdc, saver_image.imagehandle);

      if (saver_checkpalette(state->saver_state, &palette, &start, &count) != 0)
         SetDIBColorTable(bufdc, start, count, (RGBQUAD *)palette);

      if (smoothscaling)
         SetStretchBltMode(hdc, HALFTONE);
      StretchBlt(hdc, 0, 0, rc->right, rc->bottom, bufdc, (saver_image.pitch - saver_image.pitch) >> 1, 0, saver_image.width, saver_image.height, SRCCOPY);
      DeleteDC(bufdc);
   }
}


/*******************************************************************************
*** Silly Windows shit *********************************************************
*******************************************************************************/

typedef struct /* used to collect monitor parameters through a callback */
{
   int count;
   RECT arr[16];
} monitors_t;

/* Some other minor global variables */
static HINSTANCE hInstance = 0;        /* what instance we are (whatever that means) */
static TCHAR const *saver_name;        /* TCHAR version of SaverName */
static TCHAR const *reg_path;          /* where in the registry our bits are (under HKCU) */

/* These global variables are loaded at the start of WinMain */
static DWORD mouse_threshold;          /* In pixels */
static DWORD password_delay;           /* In seconds. Doesn't apply to NT/XP/Win2k. */


static BOOL VerifyPassword(HWND hwnd)
{
   typedef BOOL (WINAPI *VERIFYSCREENSAVEPWD)(HWND hwnd);
   HINSTANCE hpwdcpl;
   OSVERSIONINFO osv;
   VERIFYSCREENSAVEPWD VerifyScreenSavePwd;
   BOOL bres;

   /* Under NT, we return TRUE immediately. This lets the saver quit, and the
    * system manages passwords.
    */
   /* Under '95, we call VerifyScreenSavePwd. This checks the appropriate
    * registry key and, if necessary, pops up a verify dialog
    */
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


static void ChangePassword(HWND hwnd)
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


static void ReadGeneralRegistry(void)
{
   HKEY skey;
   DWORD valtype, valsize, val;

   password_delay = 15;
   mouse_threshold = 50;

   if (RegOpenKeyEx(HKEY_CURRENT_USER, REGSTR_PATH_SETUP _T("\\Screen Savers"), 0, KEY_READ, &skey) != ERROR_SUCCESS)
      return;

   valsize = sizeof(val);
   if (RegQueryValueEx(skey, _T("Password Delay"), 0, &valtype, (LPBYTE)&val, &valsize) == ERROR_SUCCESS)
      password_delay = val;

   valsize = sizeof(val);
   if (RegQueryValueEx(skey, _T("Mouse Threshold"), 0, &valtype, (LPBYTE)&val, &valsize) == ERROR_SUCCESS)
   {
      mouse_threshold = val;
      if (mouse_threshold > 0x3FFFFFFFl)
         mouse_threshold = 0x3FFFFFFFl;
   }

   RegCloseKey(skey);
}


static LRESULT CALLBACK SaverWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   static POINT init_cursor_pos;
   static DWORD init_time = (DWORD)-1;
   static BOOL really_close = FALSE;      /* for NT, so we know if a WM_CLOSE came from us or it. */
   static BOOL dialogue_is_active = FALSE;

   HWND hmain;
   int id;
   saver_window_t *state;

   if (msg == WM_CREATE)
   {
      CREATESTRUCT *cs = (CREATESTRUCT *)lParam;

      id = *(int *)cs->lpCreateParams;
      SetWindowLong(hwnd, 0, id);
      state = TSaverWindow_Create(hwnd, id);
      SetWindowLong(hwnd, GWL_USERDATA, (LONG)state);
      if (saver_windows.count < dimof(saver_windows.arr))
         saver_windows.arr[saver_windows.count++] = state;

      if (init_time == (DWORD)-1)
      {
         init_time = GetTickCount();
         GetCursorPos(&init_cursor_pos);
      }
   }
   else
   {
      state = (saver_window_t *)GetWindowLong(hwnd, GWL_USERDATA);
      id = GetWindowLong(hwnd, 0);
   }
   if (id <= 0)
      hmain = hwnd;
   else
      hmain = saver_windows.arr[0]->hwnd;

   if (msg == WM_TIMER)
      TSaverWindow_OnTimer(state);
   else if (msg == WM_PAINT)
   {
      PAINTSTRUCT ps;
      RECT rc;

      BeginPaint(hwnd, &ps);
      GetClientRect(hwnd, &rc);
      if (state != 0)
         TSaverWindow_OnPaint(state, ps.hdc, &rc);
      EndPaint(hwnd, &ps);
   }
   else if (state != 0)
      TSaverWindow_OtherWndProc(state, msg, wParam, lParam);

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
         if (saver_mode == SM_SAVER && !dialogue_is_active && LOWORD(wParam) == WA_INACTIVE && !ispeer)
         {
            Debug(_T("WM_ACTIVATE: about to inactive window, so sending close"));
            really_close = TRUE;
            PostMessage(hmain, WM_CLOSE, 0, 0);
            return 0;
         }
#endif
      }
      break;

   case WM_SETCURSOR:
#if defined(NDEBUG)
      if (saver_mode == SM_SAVER && !dialogue_is_active)
         SetCursor(NULL);
      else
#endif
         SetCursor(LoadCursor (NULL, IDC_ARROW));
      return 0;

   case WM_KEYDOWN:
      /* allow tab and 'a' to make something happen */
      if (wParam == '\t' || (wParam >= '0' && wParam <= '9'))
      {
         saver_keystroke(state->saver_state, wParam);
         break;
      }

      /* allow control key to be pressed because I use that for some global hotkeys */
      if ((ignorecontrol != 0 && wParam == 17) || (ignoreshift != 0 && wParam == 16))
         break;

   case WM_LBUTTONDOWN:
   case WM_MBUTTONDOWN:
   case WM_RBUTTONDOWN:
      if (saver_mode == SM_SAVER && !dialogue_is_active)
      {
         Debug (_T("WM_BUTTONDOWN: sending close"));
         really_close = TRUE;
         PostMessage (hmain, WM_CLOSE, 0, 0);
         return 0;
      }
      break;

   case WM_MOUSEMOVE:
#if defined(NDEBUG)
      if (saver_mode == SM_SAVER && !dialogue_is_active)
      {
         POINT pt;
         int dx, dy;

         GetCursorPos(&pt);
         dx = pt.x - init_cursor_pos.x;
         dy = pt.y - init_cursor_pos.y;
         if ((unsigned)(dx + mouse_threshold) > 2u * mouse_threshold ||
             (unsigned)(dy + mouse_threshold) > 2u * mouse_threshold)
         {
            Debug(_T("WM_MOUSEMOVE: gone beyond threshold, sending close"));
            really_close = TRUE;
            PostMessage(hmain, WM_CLOSE, 0, 0);
         }
      }
#endif
      return 0;

   case WM_SYSCOMMAND:
      if (saver_mode == SM_SAVER)
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
         return 0;              /* secondary windows ignore this message */
      if (id == -1)
      {
         DestroyWindow(hwnd);
         return 0;
      }                         /* preview windows close obediently */
      if (really_close && !dialogue_is_active)
      {
         BOOL CanClose = TRUE;

         Debug (_T("WM_CLOSE: maybe we need a password"));
         if (GetTickCount() - init_time > 1000 * password_delay)
         {
            dialogue_is_active = TRUE;
            SendMessage(hwnd, WM_SETCURSOR, 0, 0);
            CanClose = VerifyPassword(hwnd);
            dialogue_is_active = FALSE;
            SendMessage(hwnd, WM_SETCURSOR, 0, 0);
            GetCursorPos(&init_cursor_pos);
         }
         /* note: all secondary monitors are owned by the primary. And we're
          * the primary (id==0) therefore, when we destroy ourself, windows
          * will destroy the others first.
          */
         if (CanClose)
         {
            Debug(_T("WM_CLOSE: doing a DestroyWindow"));
            DestroyWindow(hwnd);
         }
         else
         {
            Debug(_T("WM_CLOSE: but failed password, so doing nothing"));
         }
      }
      return 0;

   case WM_DESTROY:
      {
         int i;

         Debug(_T("WM_DESTROY."));
         SetWindowLong (hwnd, 0, 0);
         SetWindowLong (hwnd, GWL_USERDATA, 0);
         for (i = 0; i != saver_windows.count; i++)
         {
            if (state == saver_windows.arr[i])
               saver_windows.arr[i] = 0;
         }
         TSaverWindow_Destroy(state);
         if ((id == 0 && saver_mode == SM_SAVER) || saver_mode == SM_PREVIEW)
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
DECLARE_HANDLE(HMONITOR);
#endif

static BOOL CALLBACK EnumMonitorCallback(HMONITOR x, HDC y, LPRECT rc, LPARAM arg)
{
   monitors_t *monptr = (monitors_t *)arg;

   if (monptr->count < dimof(monptr->arr))
   {
      if (rc->left == 0 && rc->top == 0)
      {
         memmove(&monptr->arr[1], &monptr->arr[0], sizeof(*monptr->arr) * monptr->count++);
         monptr->arr[0] = *rc;
      }
      else
         monptr->arr[monptr->count++] = *rc;
   }

   return TRUE;
}


static void DoSaver(HWND hparwnd, BOOL fakemulti)
{
   monitors_t monitors = { 0 };
   HWND hwnd = 0;

   if (saver_mode == SM_PREVIEW)
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
      int num_monitors = GetSystemMetrics(80); /* 80=SM_CMONITORS */
      if (num_monitors > 1)
      {
         typedef BOOL (CALLBACK *LUMONITORENUMPROC)(HMONITOR, HDC, LPRECT, LPARAM);
         typedef BOOL (WINAPI *LUENUMDISPLAYMONITORS)(HDC, LPCRECT, LUMONITORENUMPROC, LPARAM);
         HINSTANCE husr = LoadLibrary(_T("user32.dll"));
         LUENUMDISPLAYMONITORS pEnumDisplayMonitors = 0;

         if (husr != NULL)
            pEnumDisplayMonitors = (LUENUMDISPLAYMONITORS)GetProcAddress(husr, "EnumDisplayMonitors");
         if (pEnumDisplayMonitors != NULL)
            pEnumDisplayMonitors(NULL, NULL, EnumMonitorCallback, (LPARAM)&monitors);
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

   if (saver_mode == SM_PREVIEW)
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

      if (saver_mode == SM_SAVER)
         SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, 1, &oldval, 0);

      while (GetMessage(&msg, NULL, 0, 0))
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }

      if (saver_mode == SM_SAVER)
         SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, 0, &oldval, 0);
   }

   saver_windows.count = 0;
   return;
}


static void DoConfig(HWND hpar)
{
   TCHAR title[256];
   TCHAR message[1024];
   saver_config_item_t const * const ptrtab[] = { SaverConfig, wrapper_config };
   int i;

   for (i = 0; i < dimof(ptrtab); i++)
   {
      saver_config_item_t const *ptr = ptrtab[i];

      while (ptr->name != NULL)
      {
         if (ptr->is_string == 0)
            RegSave_int(ptr->name, ptr->def_value);
         else
            RegSave_str(ptr->name, (TCHAR const *)ptr->def_value);
         ptr++;
      }
   }

   title[dimof(title) - 1] = '\0';
   _sntprintf(title, dimof(title) - 1, _T("%s Settings"), saver_name);
   message[dimof(message) - 1] = '\0';
   _sntprintf(message, dimof(message) - 1,
         _T("Some kind of screensaver, by Simon Hosie <gumboot@clear.net.nz>\r\n")
         _T("\r\n")
         _T("Windows integration based on code by Lucian Wischik, http://www.wischik.com/scr/\r\n")
         _T("Some colour maps borrowed from xfractint\r\n")
         _T("\r\n")
         _T("I'm too lazy for a config box.  Go and use RegEdit.exe in:\r\n[HKCU]\\%s"), reg_path);

   MessageBox(NULL, message, title, MB_OK);
}


static void DoInstall(void)
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
                     (TCHAR *)&errbuf, 0, NULL);
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


static void DoUninstall(void)
{
   TCHAR buf[1024], result[1024];
   TCHAR fn[MAX_PATH];

   buf[dimof(buf) - 1] = '\0';
   result[dimof(result) - 1] = '\0';

   _sntprintf(buf, dimof(buf) - 1, REGSTR_PATH_UNINSTALL _T("\\%s"), saver_name);
   _tcscpy(result, _T("Uninstall completed. The saver will be removed next time you reboot."));

   RegDeleteKey(HKEY_LOCAL_MACHINE, buf);

   /* TODO: remove our own registry settings as well as the uninstall info */

   GetModuleFileName(hInstance, fn, MAX_PATH);
   SetFileAttributes(fn, FILE_ATTRIBUTE_NORMAL);       /* remove readonly if necessary */

   if (MoveFileEx(fn, NULL, MOVEFILE_DELAY_UNTIL_REBOOT) == FALSE)
   {
      TCHAR const *c1;
      TCHAR const *c2;
      
      c1 = _tcsrchr(fn, '\\');
      c2 = _tcsrchr(fn, '/');
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



static void RegSave_gen(char const *name, DWORD type, void *buf, int size)
{
   HKEY skey;

   if (RegCreateKeyEx(HKEY_CURRENT_USER, reg_path, 0, 0, 0,
                      KEY_ALL_ACCESS, 0, &skey, 0) != ERROR_SUCCESS)
      return;

   if (RegQueryValueExA(skey, name, 0, NULL, NULL, NULL) != ERROR_SUCCESS)
      RegSetValueExA(skey, name, 0, type, (const BYTE *)buf, size);
   RegCloseKey(skey);
}

static void RegSave_int(char const *name, int val)
{
   DWORD v = val;
   RegSave_gen(name, REG_DWORD, &v, sizeof(v));
}

static void RegSave_str(char const *name, TCHAR const *val)
{
   RegSave_gen(name, REG_SZ, (void *)val, sizeof(TCHAR) * (_tcslen(val) + 1));
}


static BOOL RegLoad_gen(char const *name, DWORD *buf)
{
   HKEY skey;
   LONG result;
   DWORD size = sizeof (DWORD);

   if (RegOpenKeyEx(HKEY_CURRENT_USER, reg_path, 0, KEY_READ, &skey) != ERROR_SUCCESS)
      return FALSE;

   result = RegQueryValueExA(skey, name, 0, 0, (LPBYTE)buf, &size);
   RegCloseKey(skey);

   return (result == ERROR_SUCCESS);
}

static int RegLoad_int(char const *name, int def)
{
   DWORD val;
   BOOL res = RegLoad_gen(name, &val);
   return res ? val : def;
}

static char const *RegLoad_str(char const *name, char const *def)
{
   HKEY skey;
   DWORD size = 0;
   char *buf;

   if (RegOpenKeyEx(HKEY_CURRENT_USER, reg_path, 0, KEY_READ, &skey) != ERROR_SUCCESS)
      return strdup(def);

   if (RegQueryValueExA(skey, name, 0, 0, 0, &size) != ERROR_SUCCESS)
   {
      RegCloseKey(skey);
      return strdup(def);
   }

   if ((buf = malloc(size + 1)) != NULL)
   {
      RegQueryValueExA(skey, name, 0, 0, (LPBYTE)buf, &size);
      RegCloseKey(skey);
      buf[size] = '\0';
   }

   return buf;
}


#if !defined(Debug)
static void Debug(TCHAR const *s)
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


int WINAPI WinMain(HINSTANCE h, HINSTANCE prev_h, LPSTR commandline, int z)
{
   HWND hwnd = NULL;
   BOOL fakemulti = FALSE;
   BOOL isexe;

   hInstance = h;

#if defined(_UNICODE)
   {
      int len = strlen(SaverName);
      WCHAR *tmp;

      if ((tmp = calloc(len + 1, sizeof(*tmp))) != NULL)
      {
         MultiByteToWideChar(CP_UTF8, 0, SaverName, -1, tmp, len + 1);
         tmp[len] = '\0';
      }
      saver_name = tmp;
   }
#else
   saver_name = SaverName;
#endif

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
      char const *c = commandline;

      while (*c == ' ')
         c++;

      if (*c == 0)
      {
         if (isexe)
            saver_mode = SM_INSTALL;
         else
            saver_mode = SM_CONFIG;
         hwnd = NULL;
      }
      else
      {
         if (*c == '-' || *c == '/')
            c++;
         if (*c == 'u' || *c == 'U')
            saver_mode = SM_UNINSTALL;
         if (*c == 'p' || *c == 'P' || *c == 'l' || *c == 'L')
         {
            c++;
            while (*c == ' ' || *c == ':')
               c++;
            hwnd = (HWND)atoi(c);
            saver_mode = SM_PREVIEW;
         }
         else if (*c == 's' || *c == 'S')
         {
            saver_mode = SM_SAVER;
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
               hwnd = (HWND)atoi(c);
            saver_mode = SM_CONFIG;
         }
         else if (*c == 'a' || *c == 'A')
         {
            c++;
            while (*c == ' ' || *c == ':')
               c++;
            hwnd = (HWND)atoi(c);
            saver_mode = SM_PASSWORD;
         }
      }
   }

   switch (saver_mode)
   {
   case SM_INSTALL:
      DoInstall();
      return 0;

   case SM_UNINSTALL:
      DoUninstall();
      return 0;

   case SM_PASSWORD:
      ChangePassword(hwnd);
      return 0;

   case SM_CONFIG:
      ReadGeneralRegistry();
      DoConfig(hwnd);
      return 0;

   case SM_SAVER:
   case SM_PREVIEW:
      ReadGeneralRegistry();

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

   default:
      break;
   }

   /* shouldn't get here */
   return 1;
}
