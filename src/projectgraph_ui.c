/*
 * Rufus: ProjectGraph visual adapter for the Wine development UI.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Keep the original controls and input behavior. Prepared PNG slices draw antialiased surfaces;
 * the standard non-client hit-test roles retain window drag and system commands.
 */
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#include <wchar.h>
#include "rufus.h"
#include "ui.h"
#include "darkmode.h"
#include "settings.h"
#include "missing.h"
#include "projectgraph_ui.h"

/* Reuse Wine/Windows PNG icon decoding and the system AlphaBlend implementation. */
PF_TYPE_DECL(WINAPI, BOOL, AlphaBlend, (HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION));
static HBITMAP surfaces[800];
static const WCHAR frame_property[] = L"RufusProjectGraphUI";
#define UI_FRAME_SUBCLASS 0x5047
#define UI_CONTROL_SUBCLASS 0x5048

typedef struct {
	HFONT title_font;
	HFONT body_font;
	HICON minimize_icon;
	HICON close_icon;
	HICON chevron_icon;
	int header_height;
	int hot_hit;
	int pressed_hit;
} FrameData;

typedef struct {
	FrameData* frame;
	int kind; /* 1 = push button, 2 = dropdown, 3 = native text/progress */
	BOOL hot;
} ControlData;

static int Scale(int value)
{
	return (int)(value * fScale + 0.5f);
}

static BOOL InitGraphics(void)
{
	PF_INIT_OR_OUT(AlphaBlend, msimg32);
	return TRUE;
out:
	return FALSE;
}

static void FillColor(HDC dc, const RECT* rect, COLORREF color)
{
	HBRUSH brush = CreateSolidBrush(color);
	FillRect(dc, rect, brush);
	DeleteObject(brush);
}

static void Surface(HDC dc, RECT rect, int kind, int state)
{
	int density = fScale >= 1.75f ? 2 : (fScale >= 1.25f ? 1 : 0);
	int size = density == 2 ? 48 : (density == 1 ? 36 : 24);
	int resource = kind + (GetRValue(GetSysColor(COLOR_WINDOW)) < 128 ? 0 : 400) + state * 10 + density;
	int index = resource - 2000;
	if (rect.right <= rect.left || rect.bottom <= rect.top) return;
	HDC source = CreateCompatibleDC(dc);
	if (source == NULL) return;
	if (surfaces[index] == NULL) {
		DWORD length;
		unsigned char* bytes = GetResource(hMainInstance, MAKEINTRESOURCEA(resource),
			_RT_RCDATA, "ProjectGraph surface", &length, FALSE);
		HICON icon = bytes == NULL ? NULL : CreateIconFromResourceEx(bytes, length, TRUE,
			0x30000, size, size, 0);
		BITMAPINFO info = { 0 };
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = size;
		info.bmiHeader.biHeight = -size;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		void* pixels = NULL;
		if (icon != NULL) {
			surfaces[index] = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
			if (surfaces[index] != NULL) {
				HGDIOBJ previous = SelectObject(source, surfaces[index]);
				ZeroMemory(pixels, size * size * 4);
				DrawIconEx(source, 0, 0, icon, size, size, 0, NULL, DI_NORMAL);
				SelectObject(source, previous);
			}
			DestroyIcon(icon);
		}
	}
	if (surfaces[index] != NULL) {
		HGDIOBJ previous = SelectObject(source, surfaces[index]);
		int corner = size * 3 / 8;
		int dx = min(Scale(9), (rect.right - rect.left) / 2);
		int dy = min(Scale(9), (rect.bottom - rect.top) / 2);
		int sx[4] = { 0, corner, size - corner, size };
		int sy[4] = { 0, corner, size - corner, size };
		int tx[4] = { rect.left, rect.left + dx, rect.right - dx, rect.right };
		int ty[4] = { rect.top, rect.top + dy, rect.bottom - dy, rect.bottom };
		BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
		for (int y = 0; y < 3; y++)
			for (int x = 0; x < 3; x++)
				if (tx[x + 1] > tx[x] && ty[y + 1] > ty[y])
					pfAlphaBlend(dc, tx[x], ty[y], tx[x + 1] - tx[x], ty[y + 1] - ty[y],
						source, sx[x], sy[y], sx[x + 1] - sx[x], sy[y + 1] - sy[y], blend);
		SelectObject(source, previous);
	}
	DeleteDC(source);
}

static HICON LoadSymbol(int resource)
{
	DWORD size;
	int density = fScale >= 1.75f ? 2 : (fScale >= 1.25f ? 1 : 0);
	unsigned char* bytes = GetResource(hMainInstance, MAKEINTRESOURCEA(resource + density),
		_RT_RCDATA, "Lucide window symbol", &size, FALSE);
	HICON icon = bytes == NULL ? NULL : CreateIconFromResourceEx(bytes, size, TRUE, 0x30000,
		Scale(16), Scale(16), 0);
	ChangeIconColor(&icon, GetSysColor(COLOR_BTNTEXT));
	return icon;
}

static RECT CaptionButton(HWND window, int hit)
{
	RECT rect;
	GetWindowRect(window, &rect);
	int right = rect.right - rect.left - Scale(8);
	if (hit == HTMINBUTTON)
		right -= Scale(36);
	rect.left = right - Scale(32);
	rect.right = right;
	rect.top = Scale(8);
	rect.bottom = rect.top + Scale(28);
	return rect;
}

static int FrameHit(HWND window, POINT point, FrameData* data)
{
	RECT rect;
	GetWindowRect(window, &rect);
	point.x -= rect.left;
	point.y -= rect.top;
	if (point.y < 0 || point.y >= data->header_height)
		return HTCLIENT;
	rect = CaptionButton(window, HTCLOSE);
	if (PtInRect(&rect, point)) return HTCLOSE;
	rect = CaptionButton(window, HTMINBUTTON);
	if (PtInRect(&rect, point)) return HTMINBUTTON;
	return HTCAPTION;
}

static void PaintFrame(HWND window, FrameData* data)
{
	RECT rect;
	WCHAR title[256];
	HDC dc = GetWindowDC(window);
	if (dc == NULL) return;
	GetWindowRect(window, &rect);
	rect.right -= rect.left;
	rect.left = rect.top = 0;
	rect.bottom = data->header_height;
	FillColor(dc, &rect, GetSysColor(COLOR_BTNFACE));
	GetWindowTextW(window, title, ARRAYSIZE(title));
	HFONT previous = (HFONT)SelectObject(dc, data->title_font);
	SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
	SetBkMode(dc, TRANSPARENT);
	rect.left = Scale(16);
	rect.right -= Scale(88);
	DrawTextW(dc, title, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
	SelectObject(dc, previous);
	for (int i = 0; i < 2; i++) {
		int hit = i == 0 ? HTMINBUTTON : HTCLOSE;
		rect = CaptionButton(window, hit);
		if (data->hot_hit == hit || data->pressed_hit == hit)
			Surface(dc, rect, 2000, 1);
		DrawIconEx(dc, rect.left + Scale(8), rect.top + Scale(6),
			i == 0 ? data->minimize_icon : data->close_icon, Scale(16), Scale(16), 0, NULL, DI_NORMAL);
	}
	ReleaseDC(window, dc);
}

static void RoundRegion(HWND window, int radius)
{
	RECT rect;
	GetWindowRect(window, &rect);
	HRGN region = CreateRoundRectRgn(0, 0, rect.right - rect.left + 1,
		rect.bottom - rect.top + 1, Scale(radius * 2), Scale(radius * 2));
	if (!SetWindowRgn(window, region, TRUE))
		DeleteObject(region);
}

static void PaintControl(HWND window, HDC target, ControlData* data)
{
	RECT rect, text_rect;
	WCHAR text[512];
	GetClientRect(window, &rect);
	HDC dc = CreateCompatibleDC(target);
	HBITMAP bitmap = CreateCompatibleBitmap(target, rect.right, rect.bottom);
	HGDIOBJ old_bitmap = SelectObject(dc, bitmap);
	FillColor(dc, &rect, GetSysColor(COLOR_BTNFACE));
	BOOL enabled = IsWindowEnabled(window), focused = GetFocus() == window;
	BOOL pressed = data->kind == 1 && (SendMessage(window, BM_GETSTATE, 0, 0) & BST_PUSHED);
	BOOL primary = GetDlgCtrlID(window) == IDC_START && enabled;
	int state = !enabled ? 3 : (primary ? ((data->hot || pressed || focused) ? 5 : 4) :
		(focused ? 2 : ((data->hot || pressed) ? 1 : 0)));
	Surface(dc, rect, data->kind == 1 ? 2100 : 2000, state);
	GetWindowTextW(window, text, ARRAYSIZE(text));
	HFONT old_font = (HFONT)SelectObject(dc, data->frame->body_font);
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, enabled ? (primary ? GetSysColor(COLOR_BTNFACE) : GetSysColor(COLOR_WINDOWTEXT)) : GetSysColor(COLOR_GRAYTEXT));
	text_rect = rect;
	UINT flags = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
	if (data->kind == 2) {
		text_rect.left += Scale(10);
		text_rect.right -= Scale(28);
		DrawIconEx(dc, rect.right - Scale(24), (rect.bottom - Scale(16)) / 2,
			data->frame->chevron_icon, Scale(16), Scale(16), 0, NULL, DI_NORMAL);
		flags |= DT_LEFT | DT_NOPREFIX;
	} else {
		flags |= DT_CENTER;
		if (SendMessage(window, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL)
			flags |= DT_HIDEPREFIX;
	}
	DrawTextW(dc, text, -1, &text_rect, flags);
	BitBlt(target, 0, 0, rect.right, rect.bottom, dc, 0, 0, SRCCOPY);
	SelectObject(dc, old_font);
	SelectObject(dc, old_bitmap);
	DeleteObject(bitmap);
	DeleteDC(dc);
}

void PaintProjectGraphFrame(HWND window, HDC dc)
{
	if (GetPropW(GetParent(window), frame_property) == NULL || dc == NULL) return;
	RECT rect;
	GetClientRect(window, &rect);
	/* Clear the native square perimeter before composing the rounded frame. */
	RECT edge = rect;
	edge.bottom = Scale(1);
	FillColor(dc, &edge, GetSysColor(COLOR_BTNFACE));
	edge = rect; edge.top = rect.bottom - Scale(1);
	FillColor(dc, &edge, GetSysColor(COLOR_BTNFACE));
	edge = rect; edge.right = Scale(1);
	FillColor(dc, &edge, GetSysColor(COLOR_BTNFACE));
	edge = rect; edge.left = rect.right - Scale(1);
	FillColor(dc, &edge, GetSysColor(COLOR_BTNFACE));
	Surface(dc, rect, 2200, !IsWindowEnabled(window) ? 3 : (GetFocus() == window ? 2 : 0));
}

static LRESULT CALLBACK ControlSubclass(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
	UINT_PTR id, DWORD_PTR ref)
{
	ControlData* data = (ControlData*)ref;
	if (message == WM_NCDESTROY) {
		RemoveWindowSubclass(window, ControlSubclass, id);
		free(data);
		return DefSubclassProc(window, message, wparam, lparam);
	}
	if (message == WM_MOUSEMOVE && !data->hot) {
		TRACKMOUSEEVENT tracking = { sizeof(tracking), TME_LEAVE, window, 0 };
		TrackMouseEvent(&tracking);
		data->hot = TRUE;
		InvalidateRect(window, NULL, FALSE);
	} else if (message == WM_MOUSELEAVE) {
		data->hot = FALSE;
		InvalidateRect(window, NULL, FALSE);
	}
	if (data->kind != 3 && (message == WM_PAINT || message == WM_PRINTCLIENT)) {
		PAINTSTRUCT ps;
		HDC dc = message == WM_PAINT ? BeginPaint(window, &ps) : (HDC)wparam;
		PaintControl(window, dc, data);
		if (message == WM_PAINT) EndPaint(window, &ps);
		return 0;
	}
	if (data->kind != 3 && message == WM_ERASEBKGND)
		return 1;
	LRESULT result = DefSubclassProc(window, message, wparam, lparam);
	if (message == WM_ENABLE || message == WM_SETFOCUS || message == WM_KILLFOCUS ||
		message == WM_SETTEXT || message == CB_SETCURSEL || message == BM_SETSTATE ||
		message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_KEYUP)
		InvalidateRect(window, NULL, FALSE);
	if (data->kind == 3 && message == WM_PAINT && GetDlgCtrlID(window) != IDC_PROGRESS) {
		HDC dc = GetDC(window);
		PaintProjectGraphFrame(window, dc);
		ReleaseDC(window, dc);
	}
	return result;
}

static BOOL CALLBACK StyleChild(HWND window, LPARAM ref)
{
	WCHAR name[32];
	FrameData* frame = (FrameData*)ref;
	GetClassNameW(window, name, ARRAYSIZE(name));
	int control_id = GetDlgCtrlID(window);
	if (control_id != IDS_DRIVE_PROPERTIES_TXT && control_id != IDS_FORMAT_OPTIONS_TXT &&
		control_id != IDS_STATUS_TXT)
		SendMessage(window, WM_SETFONT, (WPARAM)frame->body_font, TRUE);
	int kind = 0;
	if (wcscmp(name, L"Button") == 0) {
		LONG_PTR type = GetWindowLongPtr(window, GWL_STYLE) & BS_TYPEMASK;
		if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON) kind = 1;
	} else if (wcscmp(name, L"ComboBox") == 0 &&
		(GetWindowLongPtr(window, GWL_STYLE) & 3) == CBS_DROPDOWNLIST) {
		kind = 2;
	} else if (wcscmp(name, L"Edit") == 0 || wcscmp(name, PROGRESS_CLASSW) == 0) {
		kind = 3;
	}
	if (kind != 0) {
		ControlData* data = (ControlData*)calloc(1, sizeof(*data));
		if (data == NULL) return TRUE;
		data->kind = kind;
		data->frame = frame;
		if (!SetWindowSubclass(window, ControlSubclass, UI_CONTROL_SUBCLASS, (DWORD_PTR)data)) {
			free(data);
			return TRUE;
		}
		if (kind == 3) {
			SetWindowLongPtr(window, GWL_STYLE, GetWindowLongPtr(window, GWL_STYLE) & ~WS_BORDER);
			SetWindowLongPtr(window, GWL_EXSTYLE, GetWindowLongPtr(window, GWL_EXSTYLE) & ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
			SetWindowPos(window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		}
	}
	return TRUE;
}

static LRESULT CALLBACK FrameSubclass(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
	UINT_PTR id, DWORD_PTR ref)
{
	FrameData* data = (FrameData*)ref;
	switch (message) {
	case WM_NCCALCSIZE:
		if (wparam) ((NCCALCSIZE_PARAMS*)lparam)->rgrc[0].top += data->header_height;
		else ((RECT*)lparam)->top += data->header_height;
		return 0;
	case WM_NCPAINT:
	case WM_NCACTIVATE:
		PaintFrame(window, data);
		return message == WM_NCACTIVATE ? TRUE : 0;
	case WM_NCHITTEST: {
		POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		return FrameHit(window, point, data);
	}
	case WM_NCMOUSEMOVE: {
		POINT point = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		int hit = FrameHit(window, point, data);
		if (hit != data->hot_hit) {
			data->hot_hit = hit;
			PaintFrame(window, data);
		}
		TRACKMOUSEEVENT tracking = { sizeof(tracking), TME_NONCLIENT | TME_LEAVE, window, 0 };
		TrackMouseEvent(&tracking);
		break;
	}
	case WM_NCMOUSELEAVE:
		data->hot_hit = 0;
		PaintFrame(window, data);
		break;
	case WM_NCLBUTTONDBLCLK:
		if (wparam == HTCAPTION) return 0; /* This dialog is not resizable. */
		break;
	case WM_NCLBUTTONDOWN:
		if (wparam == HTMINBUTTON || wparam == HTCLOSE) {
			data->pressed_hit = (int)wparam;
			SetCapture(window);
			PaintFrame(window, data);
			return 0;
		}
		break;
	case WM_LBUTTONUP:
		if (data->pressed_hit != 0) {
			POINT point;
			GetCursorPos(&point);
			int hit = data->pressed_hit;
			data->pressed_hit = 0;
			ReleaseCapture();
			PaintFrame(window, data);
			if (FrameHit(window, point, data) == hit)
				PostMessage(window, WM_SYSCOMMAND, hit == HTCLOSE ? SC_CLOSE : SC_MINIMIZE, 0);
			return 0;
		}
		break;
	case WM_CAPTURECHANGED:
		data->pressed_hit = 0;
		PaintFrame(window, data);
		break;
	case WM_SIZE:
		if (wparam != SIZE_MINIMIZED) RoundRegion(window, 12);
		break;
	case WM_NCDESTROY:
		RemovePropW(window, frame_property);
		RemoveWindowSubclass(window, FrameSubclass, id);
		DeleteObject(data->title_font);
		DeleteObject(data->body_font);
		DestroyIcon(data->minimize_icon);
		DestroyIcon(data->close_icon);
		DestroyIcon(data->chevron_icon);
		free(data);
		for (int i = 0; i < ARRAYSIZE(surfaces); i++) {
			DeleteObject(surfaces[i]);
			surfaces[i] = NULL;
		}
		break;
	}
	return DefSubclassProc(window, message, wparam, lparam);
}

void InitProjectGraphUI(HWND window)
{
	RECT client;
	if (!use_system_colors || !ReadSettingBool(SETTING_PROJECTGRAPH_UI) ||
		GetPropW(window, frame_property) != NULL || !InitGraphics())
		return;
	FrameData* data = (FrameData*)calloc(1, sizeof(*data));
	if (data == NULL) return;
	data->header_height = Scale(44);
	LOGFONTW font;
	HFONT dialog_font = (HFONT)SendMessage(window, WM_GETFONT, 0, 0);
	if (dialog_font == NULL || GetObjectW(dialog_font, sizeof(font), &font) == 0) {
		free(data);
		return;
	}
	font.lfHeight = -Scale(16);
	font.lfWeight = FW_NORMAL;
	font.lfQuality = CLEARTYPE_QUALITY;
	data->title_font = CreateFontIndirectW(&font);
	font.lfHeight = -Scale(14);
	data->body_font = CreateFontIndirectW(&font);
	data->minimize_icon = LoadSymbol(IDI_PG_MINIMIZE);
	data->close_icon = LoadSymbol(IDI_PG_CLOSE);
	data->chevron_icon = LoadSymbol(IDI_PG_CHEVRON);
	if (!SetWindowSubclass(window, FrameSubclass, UI_FRAME_SUBCLASS, (DWORD_PTR)data)) {
		DeleteObject(data->title_font);
		DeleteObject(data->body_font);
		DestroyIcon(data->minimize_icon);
		DestroyIcon(data->close_icon);
		DestroyIcon(data->chevron_icon);
		free(data);
		return;
	}
	GetClientRect(window, &client);
	SetPropW(window, frame_property, data);
	EnumChildWindows(window, StyleChild, (LPARAM)data);
	SetWindowLongPtr(window, GWL_STYLE, GetWindowLongPtr(window, GWL_STYLE) & ~(WS_CAPTION | WS_BORDER | WS_MAXIMIZEBOX));
	SetWindowLongPtr(window, GWL_EXSTYLE, GetWindowLongPtr(window, GWL_EXSTYLE) & ~WS_EX_DLGMODALFRAME);
	SetWindowPos(window, NULL, 0, 0, client.right, client.bottom + data->header_height,
		SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
	RoundRegion(window, 12);
	PaintFrame(window, data);
	RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}
