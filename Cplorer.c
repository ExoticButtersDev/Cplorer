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
#define IDM_COPY    3004
#define IDM_MOVE    3005
#define IDM_NEWF    3006
#define ID_REFRESH  2004
#define IDI_ICON1   101 

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

int sortCol = 0;
BOOL sortAsc = TRUE;
WCHAR curDir[MAX_PATH] = {0};
WCHAR filt[256] = {0};
WCHAR clipPath[MAX_PATH] = {0};
BOOL isCopyOp = FALSE;

typedef struct {
    WCHAR name[MAX_PATH];
    BOOL isDir;
    ULONGLONG size;
    FILETIME modTime;
    FILETIME createTime;
    FILETIME accessTime;
} FILEENT;

// Changed signature to match qsort expectation
int cmpFunc(const void *p1, const void *p2) {
    FILEENT *f1 = (FILEENT *)p1, *f2 = (FILEENT *)p2;
    switch (sortCol) {
        case 0: { // Name
            int r = wcscmp(f1->name, f2->name);
            return sortAsc ? r : -r;
        }
        case 1: // Type (Directory/File) is handled by name
            return cmpFunc(p1, p2);
        case 2: { // Size
            if (f1->isDir && !f2->isDir) return -1;
            if (!f1->isDir && f2->isDir) return 1;
            return sortAsc ? (f1->size - f2->size) : (f2->size - f1->size);
        }
        case 3: { // Date Modified
            return sortAsc ? CompareFileTime(&f1->modTime, &f2->modTime) : 
                           CompareFileTime(&f2->modTime, &f1->modTime);
        }
    }
    return 0;
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
WCHAR* FormatSize(ULONGLONG size);
BOOL CopyDirectory(const WCHAR* src, const WCHAR* dst);
BOOL RemoveDirectoryRecursive(const WCHAR* path);
void OpenItem(HWND hwnd, HWND lv, int sel);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND lv, ed, btn, st, refreshBtn;
    switch (msg) {
    case WM_CREATE:
        GetCurrentDirectoryW(MAX_PATH, curDir);
        {
            INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
            InitCommonControlsEx(&icex);
        }
        ed = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
                             10, 10, WINDOW_WIDTH - 210, 25, hwnd, (HMENU)ID_EDIT, ((LPCREATESTRUCT)lp)->hInstance, NULL);
        btn = CreateWindowW(L"BUTTON", L"Search", WS_TABSTOP|WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON,
                            WINDOW_WIDTH - 190, 10, 80, 25, hwnd, (HMENU)ID_BTN, ((LPCREATESTRUCT)lp)->hInstance, NULL);
        refreshBtn = CreateWindowW(L"BUTTON", L"Refresh", WS_TABSTOP|WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
                                 WINDOW_WIDTH - 100, 10, 80, 25, hwnd, (HMENU)ID_REFRESH, ((LPCREATESTRUCT)lp)->hInstance, NULL);
        lv = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"", WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
                             10, 45, WINDOW_WIDTH - 20, WINDOW_HEIGHT - 100, hwnd, (HMENU)ID_LV, ((LPCREATESTRUCT)lp)->hInstance, NULL);
        {
            LVCOLUMNW col = {0};
            col.mask = LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
            col.pszText = L"Name"; col.cx = 300;
            ListView_InsertColumn(lv, 0, &col);
            col.pszText = L"Type"; col.cx = 100;
            ListView_InsertColumn(lv, 1, &col);
            col.pszText = L"Size"; col.cx = 100;
            ListView_InsertColumn(lv, 2, &col);
            col.pszText = L"Modified"; col.cx = 150;
            ListView_InsertColumn(lv, 3, &col);
        }
        st = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_LEFT, 
                           10, WINDOW_HEIGHT - 45, WINDOW_WIDTH - 20, 40,
                           hwnd, (HMENU)ID_STAT, ((LPCREATESTRUCT)lp)->hInstance, NULL);
        RefreshListView(lv);
        break;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
            case ID_REFRESH:
                RefreshListView(lv);
                break;
            case ID_BTN:
                GetWindowTextW(ed, filt, 256);
                RefreshListView(lv);
                break;
            case IDM_NEWF: {
                WCHAR newPath[MAX_PATH];
                StringCchCopyW(newPath, MAX_PATH, curDir);
                PathAppendW(newPath, L"New Folder");
                int i = 1; WCHAR temp[MAX_PATH];
                while (!CreateDirectoryW(newPath, NULL)) {
                    if (GetLastError() != ERROR_ALREADY_EXISTS) {
                        MessageBoxW(hwnd, L"Failed to create folder", L"Error", MB_ICONERROR);
                        break;
                    }
                    StringCchPrintfW(temp, MAX_PATH, L"New Folder (%d)", i++);
                    StringCchCopyW(newPath, MAX_PATH, curDir);
                    PathAppendW(newPath, temp);
                }
                RefreshListView(lv);
                break;
            }
            case IDM_OPEN:
            case IDM_DEL:
            case IDM_COPY:
            case IDM_MOVE:
            case IDM_PROP: {
                int sel = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
                if (sel == -1) break;

                LVITEMW it = {0};
                WCHAR txt[MAX_PATH] = {0};
                it.mask = LVIF_TEXT|LVIF_PARAM;
                it.iItem = sel; it.iSubItem = 0;
                it.pszText = txt; it.cchTextMax = MAX_PATH;
                if (!ListView_GetItem(lv, &it)) break;

                WCHAR pth[MAX_PATH];
                if (FAILED(StringCchCopyW(pth, MAX_PATH, curDir)) || 
                    (wcscmp(txt, L"..") != 0 && FAILED(PathAppendW(pth, txt)))) {
                    MessageBoxW(hwnd, L"Path error.", L"Error", MB_ICONERROR);
                    break;
                }

                switch (id) {
                    case IDM_OPEN:
                        OpenItem(hwnd, lv, sel);
                        break;
                    case IDM_DEL: {
                        WCHAR msg[MAX_PATH + 50];
                        StringCchPrintfW(msg, MAX_PATH + 50, L"Are you sure you want to delete '%s'?", txt);
                        if (MessageBoxW(hwnd, msg, L"Confirm Delete", MB_YESNO|MB_ICONQUESTION) == IDYES) {
                            BOOL success;
                            if (it.lParam && wcscmp(txt, L"..") != 0) {
                                success = RemoveDirectoryRecursive(pth);
                            } else if (!it.lParam) {
                                success = DeleteFileW(pth);
                            } else {
                                success = TRUE; // ".." doesn't need deletion
                            }
                            if (!success) {
                                WCHAR errMsg[MAX_PATH + 50];
                                StringCchPrintfW(errMsg, MAX_PATH + 50, L"Failed to delete '%s'", txt);
                                MessageBoxW(hwnd, errMsg, L"Error", MB_ICONERROR);
                            }
                        }
                        RefreshListView(lv);
                        break;
                    }
                    case IDM_COPY:
                    case IDM_MOVE: {
                        if (wcscmp(txt, L"..") == 0) {
                            MessageBoxW(hwnd, L"Cannot copy/move parent directory", L"Error", MB_ICONERROR);
                            break;
                        }
                        if (GetFileAttributesW(pth) == INVALID_FILE_ATTRIBUTES) {
                            MessageBoxW(hwnd, L"Source file/directory not found", L"Error", MB_ICONERROR);
                            break;
                        }
                        StringCchCopyW(clipPath, MAX_PATH, pth);
                        isCopyOp = (id == IDM_COPY);
                        WCHAR msg[MAX_PATH + 50];
                        StringCchPrintfW(msg, MAX_PATH + 50, L"'%s' %s to clipboard", 
                                       txt, isCopyOp ? L"copied" : L"cut");
                        MessageBoxW(hwnd, msg, L"Success", MB_ICONINFORMATION);
                        break;
                    }
                    case IDM_PROP: {
                        if (wcscmp(txt, L"..") == 0) {
                            PathRemoveFileSpecW(pth);
                        }
                        SHELLEXECUTEINFOW sei = { 
                            sizeof(sei),            // cbSize
                            SEE_MASK_INVOKEIDLIST,  // fMask
                            hwnd,                   // hwnd
                            L"properties",          // lpVerb
                            pth,                    // lpFile
                            NULL,                   // lpParameters
                            NULL,                   // lpDirectory
                            SW_SHOWNORMAL,          // nShow
                            0,                      // hInstApp
                            NULL                    // lpIDList (and remaining fields)
                        };
                        if (!ShellExecuteExW(&sei)) {
                            MessageBoxW(hwnd, L"Failed to show properties", L"Error", MB_ICONERROR);
                        }
                        break;
                    }
                }
                if (id != IDM_OPEN) RefreshListView(lv);
                break;
            }
        }
        }
        break;
    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lp;
        if (hdr->idFrom == ID_LV) {
            switch (hdr->code) {
                case NM_DBLCLK: {
                    int sel = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
                    if (sel != -1) {
                        OpenItem(hwnd, lv, sel);
                    }
                    break;
                }
                case LVN_COLUMNCLICK: {
                    NMLISTVIEW *lvCol = (NMLISTVIEW *)lp;
                    if (lvCol->iSubItem == sortCol)
                        sortAsc = !sortAsc;
                    else { sortCol = lvCol->iSubItem; sortAsc = TRUE; }
                    RefreshListView(lv);
                    break;
                }
                case LVN_ITEMCHANGED: {
                    NMLISTVIEW *lvItm = (NMLISTVIEW *)lp;
                    if ((lvItm->uChanged & LVIF_STATE) && (lvItm->uNewState & LVIS_SELECTED)) {
                        WCHAR txt[MAX_PATH] = {0}, full[MAX_PATH];
                        LVITEMW it = {0};
                        it.mask = LVIF_TEXT|LVIF_PARAM;
                        it.iItem = lvItm->iItem; it.iSubItem = 0;
                        it.pszText = txt; it.cchTextMax = MAX_PATH;
                        if (ListView_GetItem(lv, &it)) {
                            if (wcscmp(txt, L"..") != 0) {
                                StringCchCopyW(full, MAX_PATH, curDir);
                                PathAppendW(full, txt);
                            } else full[0] = L'\0';
                            ShowPreview(st, full);
                        }
                    }
                    break;
                }
            }
        }
        }
        break;
    case WM_CONTEXTMENU:
        if ((HWND)wp == lv) {
            POINT pt = { LOWORD(lp), HIWORD(lp) };
            if (pt.x == -1 && pt.y == -1) {
                int sel = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
                if (sel != -1) {
                    RECT rc;
                    ListView_GetItemRect(lv, sel, &rc, LVIR_LABEL);
                    pt.x = rc.left; pt.y = rc.bottom;
                    ClientToScreen(lv, &pt);
                }
            }
            HMENU pop = CreatePopupMenu();
            AppendMenuW(pop, MF_STRING, IDM_OPEN, L"Open");
            AppendMenuW(pop, MF_STRING, IDM_DEL, L"Delete");
            AppendMenuW(pop, MF_STRING, IDM_COPY, L"Copy");
            AppendMenuW(pop, MF_STRING, IDM_MOVE, L"Move");
            AppendMenuW(pop, MF_STRING, IDM_NEWF, L"New Folder");
            AppendMenuW(pop, MF_STRING|(clipPath[0]?0:MF_GRAYED), IDM_OPEN, L"Paste");
            AppendMenuW(pop, MF_STRING, IDM_PROP, L"Properties");
            int cmd = TrackPopupMenu(pop, TPM_RIGHTBUTTON|TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);
            if (cmd == IDM_OPEN && clipPath[0]) {
                WCHAR dest[MAX_PATH];
                StringCchCopyW(dest, MAX_PATH, curDir);
                PathAppendW(dest, PathFindFileNameW(clipPath));
                
                if (GetFileAttributesW(dest) != INVALID_FILE_ATTRIBUTES) {
                    WCHAR msg[MAX_PATH + 50];
                    StringCchPrintfW(msg, MAX_PATH + 50, L"'%s' already exists. Overwrite?", PathFindFileNameW(clipPath));
                    if (MessageBoxW(hwnd, msg, L"Confirm Overwrite", MB_YESNO|MB_ICONQUESTION) != IDYES) {
                        DestroyMenu(pop);
                        break;
                    }
                }

                BOOL success;
                DWORD attr = GetFileAttributesW(clipPath);
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                    success = isCopyOp ? CopyDirectory(clipPath, dest) : MoveFileW(clipPath, dest);
                } else {
                    success = isCopyOp ? CopyFileW(clipPath, dest, FALSE) : MoveFileW(clipPath, dest);
                }
                
                if (!success) {
                    MessageBoxW(hwnd, L"Operation failed.", L"Error", MB_ICONERROR);
                } else {
                    clipPath[0] = L'\0';
                    RefreshListView(lv);
                }
            }
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

WCHAR* FormatSize(ULONGLONG size) {
    static WCHAR buf[32];
    if (size >= 1024ULL * 1024 * 1024) {
        StringCchPrintfW(buf, 32, L"%.2f GB", size / (1024.0 * 1024 * 1024));
    } else if (size >= 1024ULL * 1024) {
        StringCchPrintfW(buf, 32, L"%.2f MB", size / (1024.0 * 1024));
    } else if (size >= 1024ULL) {
        StringCchPrintfW(buf, 32, L"%.2f KB", size / 1024.0);
    } else {
        StringCchPrintfW(buf, 32, L"%llu B", size);
    }
    return buf;
}

void ShowPreview(HWND st, const WCHAR *path) {
    if (!path || path[0] == L'\0') { 
        WCHAR buf[64];
        StringCchPrintfW(buf, 64, L"Items: %d", ListView_GetItemCount(GetDlgItem(GetParent(st), ID_LV)));
        SetWindowTextW(st, buf);
        return; 
    }
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) { SetWindowTextW(st, L""); return; }
    ULONGLONG sz = (((ULONGLONG)fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
    SYSTEMTIME stMod, stCreate, stAccess;
    FileTimeToSystemTime(&fad.ftLastWriteTime, &stMod);
    FileTimeToSystemTime(&fad.ftCreationTime, &stCreate);
    FileTimeToSystemTime(&fad.ftLastAccessTime, &stAccess);
    WCHAR buf[256];
    StringCchPrintfW(buf, 256,
        L"Size: %s\nCreated: %02d/%02d/%04d %02d:%02d  Modified: %02d/%02d/%04d %02d:%02d",
        FormatSize(sz),
        stCreate.wMonth, stCreate.wDay, stCreate.wYear, stCreate.wHour, stCreate.wMinute,
        stMod.wMonth, stMod.wDay, stMod.wYear, stMod.wHour, stMod.wMinute);
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
        e->createTime = fd.ftCreationTime;
        e->accessTime = fd.ftLastAccessTime;
        cnt++;
    } while (FindNextFileW(hFind, &fd) != 0);
    FindClose(hFind);
    qsort(ents, cnt, sizeof(FILEENT), cmpFunc);
    for (int i = 0; i < cnt; i++) {
        LVITEMW it = {0};
        it.mask = LVIF_TEXT|LVIF_PARAM;
        it.iItem = ListView_GetItemCount(lv);
        it.pszText = ents[i].name;
        it.lParam = ents[i].isDir;
        int idx = ListView_InsertItem(lv, &it);
        
        WCHAR t[32];
        StringCchCopyW(t, 32, ents[i].isDir ? L"Directory" : L"File");
        ListView_SetItemText(lv, idx, 1, t);
        
        ListView_SetItemText(lv, idx, 2, FormatSize(ents[i].size));
        
        SYSTEMTIME st;
        FileTimeToSystemTime(&ents[i].modTime, &st);
        WCHAR dt[32];
        StringCchPrintfW(dt, 32, L"%02d/%02d/%04d %02d:%02d", 
            st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute);
        ListView_SetItemText(lv, idx, 3, dt);
    }
}

BOOL CopyDirectory(const WCHAR* src, const WCHAR* dst) {
    WCHAR srcPath[MAX_PATH], dstPath[MAX_PATH];
    StringCchCopyW(srcPath, MAX_PATH, src);
    StringCchCopyW(dstPath, MAX_PATH, dst);
    PathAppendW(srcPath, L"*");

    if (!CreateDirectoryW(dst, NULL)) return FALSE;

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(srcPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return FALSE;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        WCHAR srcFile[MAX_PATH], dstFile[MAX_PATH];
        StringCchCopyW(srcFile, MAX_PATH, src);
        StringCchCopyW(dstFile, MAX_PATH, dst);
        PathAppendW(srcFile, fd.cFileName);
        PathAppendW(dstFile, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!CopyDirectory(srcFile, dstFile)) {
                FindClose(hFind);
                return FALSE;
            }
        } else {
            if (!CopyFileW(srcFile, dstFile, FALSE)) {
                FindClose(hFind);
                return FALSE;
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return TRUE;
}

BOOL RemoveDirectoryRecursive(const WCHAR* path) {
    WCHAR searchPath[MAX_PATH];
    StringCchCopyW(searchPath, MAX_PATH, path);
    PathAppendW(searchPath, L"*");

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return FALSE;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        WCHAR fullPath[MAX_PATH];
        StringCchCopyW(fullPath, MAX_PATH, path);
        PathAppendW(fullPath, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!RemoveDirectoryRecursive(fullPath)) {
                FindClose(hFind);
                return FALSE;
            }
        } else {
            if (!DeleteFileW(fullPath)) {
                FindClose(hFind);
                return FALSE;
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return RemoveDirectoryW(path);
}

void OpenItem(HWND hwnd, HWND lv, int sel) {
    LVITEMW it = {0};
    WCHAR txt[MAX_PATH] = {0};
    it.mask = LVIF_TEXT|LVIF_PARAM;
    it.iItem = sel; 
    it.iSubItem = 0;
    it.pszText = txt; 
    it.cchTextMax = MAX_PATH;
    
    if (ListView_GetItem(lv, &it)) {
        if (wcscmp(txt, L"..") == 0) {
            if (!PathIsRootW(curDir)) {
                PathRemoveFileSpecW(curDir);
                if (GetFileAttributesW(curDir) == INVALID_FILE_ATTRIBUTES) {
                    MessageBoxW(hwnd, L"Cannot access parent directory", L"Error", MB_ICONERROR);
                    StringCchCopyW(curDir, MAX_PATH, L"C:\\");
                }
            }
        } else {
            WCHAR pth[MAX_PATH];
            StringCchCopyW(pth, MAX_PATH, curDir);
            PathAppendW(pth, txt);
            
            if (it.lParam) { // Directory
                if (GetFileAttributesW(pth) == INVALID_FILE_ATTRIBUTES) {
                    MessageBoxW(hwnd, L"Directory not accessible", L"Error", MB_ICONERROR);
                } else {
                    StringCchCopyW(curDir, MAX_PATH, pth);
                }
            } else { // File
                HINSTANCE result = ShellExecuteW(hwnd, L"open", pth, NULL, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)result <= 32) {
                    WCHAR errMsg[MAX_PATH + 50];
                    StringCchPrintfW(errMsg, MAX_PATH + 50, L"Failed to open '%s'", txt);
                    MessageBoxW(hwnd, errMsg, L"Error", MB_ICONERROR);
                }
            }
        }
        RefreshListView(lv);
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
    HWND hwnd = CreateWindowExW(0, cls, L"Cplorer", 
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT, 
        NULL, NULL, hInst, NULL);
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