#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <wchar.h>
#include <strsafe.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define ID_LV       1001
#define ID_EDIT     2001
#define ID_BTN      2002
#define ID_STAT     2003
#define IDM_OPEN    3001
#define IDM_DEL     3002
#define IDM_PROP    3003
#define IDI_ICON1 101 

int sortCol = 0;
BOOL sortAsc = TRUE;
WCHAR curDir[MAX_PATH] = {0};
WCHAR filt[256] = {0};

typedef struct {
    WCHAR name[MAX_PATH];
    BOOL isDir;
    ULONGLONG size;
    FILETIME modTime;
} FILEENT;

int CALLBACK cmpFunc(LPARAM p1, LPARAM p2, LPARAM pSort) {
    (void)pSort;
    FILEENT *f1 = (FILEENT *)p1, *f2 = (FILEENT *)p2;
    int r = wcscmp(f1->name, f2->name);
    return sortAsc ? r : -r;
}

BOOL matchFilter(const WCHAR *n, const WCHAR *f) {
    if (!f || f[0] == L'\0') return TRUE;
    WCHAR *upN = _wcsdup(n), *upF = _wcsdup(f);
    _wcsupr(upN);
    _wcsupr(upF);
    BOOL r = (wcsstr(upN, upF) != NULL);
    free(upN); free(upF);
    return r;
}

void RefreshListView(HWND lv);
void ShowPreview(HWND st, const WCHAR *path);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND lv, ed, btn, st;
    switch (msg) {
    case WM_CREATE:
        GetCurrentDirectoryW(MAX_PATH, curDir);
        {
            INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
            InitCommonControlsEx(&icex);
        }
        ed = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
                             10, 10, 300, 25, hwnd, (HMENU)ID_EDIT, ((LPCREATESTRUCT)lp)->hInstance, NULL);
        btn = CreateWindowW(L"BUTTON", L"Search", WS_TABSTOP|WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON,
                             320, 10, 80, 25, hwnd, (HMENU)ID_BTN, ((LPCREATESTRUCT)lp)->hInstance, NULL);
        lv = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"", WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
                             10, 45, 600, 400, hwnd, (HMENU)ID_LV, ((LPCREATESTRUCT)lp)->hInstance, NULL);
        {
            LVCOLUMNW col = {0};
            col.mask = LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
            col.pszText = L"Name";
            col.cx = 400;
            ListView_InsertColumn(lv, 0, &col);
            col.pszText = L"Type";
            col.cx = 100;
            ListView_InsertColumn(lv, 1, &col);
        }
        st = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_LEFT, 10, 450, 600, 25,
                           hwnd, (HMENU)ID_STAT, ((LPCREATESTRUCT)lp)->hInstance, NULL);
        RefreshListView(lv);
        break;
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        MoveWindow(GetDlgItem(hwnd, ID_EDIT), 10, 10, w - 120, 25, TRUE);
        MoveWindow(GetDlgItem(hwnd, ID_BTN), w - 100, 10, 80, 25, TRUE);
        MoveWindow(GetDlgItem(hwnd, ID_LV), 10, 45, w - 20, h - 100, TRUE);
        MoveWindow(GetDlgItem(hwnd, ID_STAT), 10, h - 45, w - 20, 25, TRUE);
        }
        break;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_BTN) {
            GetWindowTextW(GetDlgItem(hwnd, ID_EDIT), filt, 256);
            RefreshListView(GetDlgItem(hwnd, ID_LV));
        } else if (id == IDM_OPEN || id == IDM_DEL || id == IDM_PROP) {
            int sel = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
            if (sel != -1) {
                LVITEMW it = {0};
                WCHAR txt[MAX_PATH] = {0};
                it.mask = LVIF_TEXT|LVIF_PARAM;
                it.iItem = sel; it.iSubItem = 0;
                it.pszText = txt; it.cchTextMax = MAX_PATH;
                if (ListView_GetItem(lv, &it)) {
                    WCHAR pth[MAX_PATH];
                    if (wcscmp(txt, L"..") == 0) {
                        if (!PathIsRootW(curDir))
                            PathRemoveFileSpecW(curDir);
                    } else {
                        if (FAILED(StringCchCopyW(pth, MAX_PATH, curDir)) ||
                            FAILED(PathAppendW(pth, txt))) {
                            MessageBoxW(hwnd, L"Path error.", L"Error", MB_ICONERROR);
                            break;
                        }
                        if (it.lParam) {
                            if (id == IDM_OPEN)
                                StringCchCopyW(curDir, MAX_PATH, pth);
                            else if (id == IDM_DEL)
                                RemoveDirectoryW(pth);
                            else if (id == IDM_PROP)
                                ShellExecuteW(hwnd, L"properties", pth, NULL, NULL, SW_SHOWNORMAL);
                        } else {
                            if (id == IDM_OPEN)
                                ShellExecuteW(hwnd, L"open", pth, NULL, NULL, SW_SHOWNORMAL);
                            else if (id == IDM_DEL)
                                DeleteFileW(pth);
                            else if (id == IDM_PROP) {
                                SHELLEXECUTEINFOW sei = {0};
                                sei.cbSize = sizeof(sei);
                                sei.fMask = SEE_MASK_INVOKEIDLIST;
                                sei.hwnd = hwnd;
                                sei.lpVerb = L"properties";
                                sei.lpFile = pth;
                                sei.nShow = SW_SHOWNORMAL;
                                ShellExecuteExW(&sei);
                            }
                        }
                    }
                    RefreshListView(lv);
                }
            }
        }
        }
        break;
    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lp;
        if (hdr->idFrom == ID_LV) {
            if (hdr->code == NM_DBLCLK) {
                int sel = ListView_GetNextItem((HWND)hdr->hwndFrom, -1, LVNI_SELECTED);
                if (sel != -1) {
                    LVITEMW it = {0};
                    WCHAR txt[MAX_PATH] = {0};
                    it.mask = LVIF_TEXT|LVIF_PARAM;
                    it.iItem = sel; it.iSubItem = 0;
                    it.pszText = txt; it.cchTextMax = MAX_PATH;
                    if (ListView_GetItem((HWND)hdr->hwndFrom, &it)) {
                        WCHAR np[MAX_PATH];
                        if (wcscmp(txt, L"..") == 0) {
                            if (!PathIsRootW(curDir))
                                PathRemoveFileSpecW(curDir);
                        } else if (it.lParam) {
                            if (FAILED(StringCchCopyW(np, MAX_PATH, curDir)) ||
                                FAILED(PathAppendW(np, txt))) {
                                MessageBoxW(hwnd, L"Path error.", L"Error", MB_ICONERROR);
                                break;
                            }
                            StringCchCopyW(curDir, MAX_PATH, np);
                        } else {
                            if (FAILED(StringCchCopyW(np, MAX_PATH, curDir)) ||
                                FAILED(PathAppendW(np, txt))) {
                                MessageBoxW(hwnd, L"Path error.", L"Error", MB_ICONERROR);
                                break;
                            }
                            ShellExecuteW(hwnd, L"open", np, NULL, NULL, SW_SHOWNORMAL);
                        }
                        RefreshListView((HWND)hdr->hwndFrom);
                    }
                }
            } else if (hdr->code == LVN_COLUMNCLICK) {
                NMLISTVIEW *lvCol = (NMLISTVIEW *)lp;
                if (lvCol->iSubItem == sortCol)
                    sortAsc = !sortAsc;
                else { sortCol = lvCol->iSubItem; sortAsc = TRUE; }
                RefreshListView(GetDlgItem(hwnd, ID_LV));
            } else if (hdr->code == LVN_ITEMCHANGED) {
                NMLISTVIEW *lvItm = (NMLISTVIEW *)lp;
                if ((lvItm->uChanged & LVIF_STATE) && (lvItm->uNewState & LVIS_SELECTED)) {
                    int idx = lvItm->iItem;
                    LVITEMW it = {0};
                    WCHAR txt[MAX_PATH] = {0};
                    it.mask = LVIF_TEXT|LVIF_PARAM;
                    it.iItem = idx; it.iSubItem = 0;
                    it.pszText = txt; it.cchTextMax = MAX_PATH;
                    if (ListView_GetItem(GetDlgItem(hwnd, ID_LV), &it)) {
                        WCHAR full[MAX_PATH];
                        if (wcscmp(txt, L"..") != 0) {
                            if (FAILED(StringCchCopyW(full, MAX_PATH, curDir)) ||
                                FAILED(PathAppendW(full, txt)))
                                full[0] = L'\0';
                        } else full[0] = L'\0';
                        ShowPreview(GetDlgItem(hwnd, ID_STAT), full);
                    }
                }
            }
        }
        }
        break;
    case WM_CONTEXTMENU:
        if ((HWND)wp == GetDlgItem(hwnd, ID_LV)) {
            POINT pt;
            pt.x = LOWORD(lp); pt.y = HIWORD(lp);
            if (pt.x == -1 && pt.y == -1) {
                int sel = ListView_GetNextItem(GetDlgItem(hwnd, ID_LV), -1, LVNI_SELECTED);
                if (sel != -1) {
                    RECT rc;
                    ListView_GetItemRect(GetDlgItem(hwnd, ID_LV), sel, &rc, LVIR_LABEL);
                    pt.x = rc.left; pt.y = rc.bottom;
                    ClientToScreen(GetDlgItem(hwnd, ID_LV), &pt);
                }
            }
            HMENU pop = CreatePopupMenu();
            AppendMenuW(pop, MF_STRING, IDM_OPEN, L"Open");
            AppendMenuW(pop, MF_STRING, IDM_DEL, L"Delete");
            AppendMenuW(pop, MF_STRING, IDM_PROP, L"Properties");
            TrackPopupMenu(pop, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(pop);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

void ShowPreview(HWND st, const WCHAR *path) {
    if (!path || path[0] == L'\0') { SetWindowTextW(st, L""); return; }
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) { SetWindowTextW(st, L""); return; }
    ULONGLONG sz = (((ULONGLONG)fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&fad.ftLastWriteTime, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    WCHAR buf[256];
    StringCchPrintfW(
        buf,
        256,
        L"Size: %llu bytes    Modified: %02d/%02d/%04d %02d:%02d",
        (unsigned long long)sz,
        stLocal.wMonth,
        stLocal.wDay,
        stLocal.wYear,
        stLocal.wHour,
        stLocal.wMinute
    );
    SetWindowTextW(st, buf);
}

void RefreshListView(HWND lv) {
    ListView_DeleteAllItems(lv);
    FILEENT ents[1024];
    int cnt = 0;
    if (!PathIsRootW(curDir)) {
        LVITEMW it = {0};
        it.mask = LVIF_TEXT|LVIF_PARAM;
        it.iItem = 0; it.pszText = L".."; it.lParam = TRUE;
        ListView_InsertItem(lv, &it);
    }
    WCHAR sp[MAX_PATH];
    if (FAILED(StringCchCopyW(sp, MAX_PATH, curDir)) || FAILED(PathAppendW(sp, L"*"))) return;
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(sp, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (!matchFilter(fd.cFileName, filt)) continue;
        FILEENT *e = &ents[cnt];
        StringCchCopyW(e->name, MAX_PATH, fd.cFileName);
        e->isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
        e->size = (((ULONGLONG)fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
        e->modTime = fd.ftLastWriteTime;
        cnt++;
    } while (FindNextFileW(hFind, &fd) != 0);
    FindClose(hFind);
    qsort(ents, cnt, sizeof(FILEENT), (int(*)(const void*, const void*))cmpFunc);
    for (int i = 0; i < cnt; i++) {
        LVITEMW it = {0};
        it.mask = LVIF_TEXT|LVIF_PARAM;
        it.iItem = ListView_GetItemCount(lv);
        it.pszText = ents[i].name;
        it.lParam = ents[i].isDir;
        int idx = ListView_InsertItem(lv, &it);
        WCHAR t[32];
        if (ents[i].isDir)
            StringCchCopyW(t, 32, L"Directory");
        else
            StringCchCopyW(t, 32, L"File");
        ListView_SetItemText(lv, idx, 1, t);
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmd, int nCmd) {
    (void)hPrev; (void)lpCmd;
    const WCHAR cls[] = L"FEClass";
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, cls, L"File Explorer", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 520, NULL, NULL, hInst, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, nCmd);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}