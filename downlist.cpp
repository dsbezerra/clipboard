#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

#include "downlist.h"
#include "downlist_text_util.cpp"

#define Win32TrayIconMessage (WM_USER + 1)

static Replace_Strings replace_strings;

static void
free(void *memory) {
    if (!memory) {
        return;
    }
    
    VirtualFree(memory, 0, MEM_RELEASE);
}

static void *
alloc(u64 size) {
    if (size == 0) {
        return 0;
    }
    
    return VirtualAlloc(0, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

// TODO(diego): Convert to C strings
static bool
check_substring(std::string &full, std::string substring) {
    if (substring.size() > full.size()) return false;
    
    return full.find(substring) != std::string::npos;
}

// TODO(diego): Convert to C strings
static bool
replace_substring(std::string &full, std::string from, std::string to) {
    size_t position = full.find(from);
    if (position == std::string::npos) return false;
    
    full.replace(position, from.length(), to);
    
    return true;
}

static void
process_text(HWND window, LPTSTR text) {
    if (!text) return;
    
    std::string string = std::string(text);
    
    bool processed = false;
    if (check_substring(string, replace_strings.match_text)) {
        size_t size = replace_strings.replace_text.size();
        if (size > 0) {
            processed = true;
            for (size_t replace_index = 0; replace_index < size; ++replace_index) {
                Replace_Text it = replace_strings.replace_text[replace_index];
                processed &= replace_substring(string, it.from, it.to);
            }
        }
    }
    
    if (processed && OpenClipboard(window)) {
        EmptyClipboard();
        
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, string.length() + 1 * sizeof(TCHAR)); 
        if (!handle) { 
            CloseClipboard();
            return; 
        }
        
        LPTSTR new_data = (LPTSTR) GlobalLock(handle);
        CopyMemory(new_data, string.c_str(), string.length() * sizeof(TCHAR)); 
        new_data[string.length()] = 0; 
        GlobalUnlock(handle); 
        
        SetClipboardData(CF_TEXT, handle);
        CloseClipboard();
    }
}

static void
handle_clipboard_change(HWND window) {
    if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(window)) {
        LPTSTR contents = 0;
        HANDLE data_handle = GetClipboardData(CF_TEXT);
        if (data_handle) {
            contents = (LPTSTR) GlobalLock(data_handle);
            if (contents) {
                GlobalUnlock(data_handle);
            }
        }
        CloseClipboard();
        
        if (contents) {
            process_text(window, contents);
        }
    }
}

static void
add_separator(HMENU menu_handle) {
    MENUITEMINFO menu_item;
    menu_item.cbSize = sizeof(menu_item);
    menu_item.fMask = MIIM_ID | MIIM_DATA | MIIM_TYPE;
    menu_item.fType = MFT_SEPARATOR;
    menu_item.wID = 0;
    menu_item.dwItemData = 0;
    
    InsertMenuItem(menu_handle, GetMenuItemCount(menu_handle), true, &menu_item);
}

static int
add_menu_item(HMENU menu_handle, char *item_text, bool checked, bool enabled, void *extra_data) {
    UINT flags = MF_STRING;
    if (checked) {
        flags |= MF_CHECKED;
    }
    
    MENUITEMINFO menu_item;
    menu_item.cbSize = sizeof(menu_item);
    menu_item.fMask = MIIM_ID | MIIM_STATE | MIIM_DATA | MIIM_TYPE;
    menu_item.fType = MFT_STRING;
    menu_item.fState = ((checked ? MFS_CHECKED : MFS_UNCHECKED) | 
                        (enabled ? MFS_ENABLED : MFS_DISABLED));
    menu_item.wID = GetMenuItemCount(menu_handle) + 1;
    menu_item.dwItemData = (LPARAM) extra_data;
    menu_item.dwTypeData = (char *) item_text;
    
    InsertMenuItem(menu_handle, GetMenuItemCount(menu_handle), true, &menu_item);
    
    return menu_item.wID;
}

static void *
get_menu_extra_data(HMENU menu_handle, int index) {
    MENUITEMINFO menu_item_info;
    
    menu_item_info.cbSize = sizeof(menu_item_info);
    menu_item_info.fMask = MIIM_DATA;
    menu_item_info.dwItemData = 0;
    
    GetMenuItemInfo(menu_handle, index, false, &menu_item_info);
    
    return (void *) menu_item_info.dwItemData;
}

static void
menu_exit_downlist() {
    PostQuitMessage(0);
}

typedef void menu_callback();
static LRESULT CALLBACK
default_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;
    switch (message) {
        case Win32TrayIconMessage: {
            switch (lparam) {
                case WM_LBUTTONDOWN:
                case WM_RBUTTONDOWN: {
                    POINT mouse_position = {0, 0};
                    GetCursorPos(&mouse_position);
                    
                    HMENU window_menu = CreatePopupMenu();
                    
                    add_menu_item(window_menu, "Exit Downlist", false, true, menu_exit_downlist);
                    add_separator(window_menu);
                    add_menu_item(window_menu, "Downlist", false, false, 0);
                    
                    int picked_index = TrackPopupMenu(window_menu, TPM_LEFTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD | TPM_CENTERALIGN|TPM_TOPALIGN,
                                                      mouse_position.x, mouse_position.y, 0, window, 0);
                    
                    menu_callback *callback = (menu_callback *) get_menu_extra_data(window_menu, picked_index);
                    if (callback) callback();
                    
                    DestroyMenu(window_menu);
                } break;
                
                default: {
                    // No-op
                } break;
            }
        } break;
        
        case WM_CLIPBOARDUPDATE: {
            handle_clipboard_change(window);
        } break;
        
        default: {
            result = DefWindowProcA(window, message, wparam, lparam);
        } break;
    }
    return result;
}

static File_Contents
read_entire_file(char *filepath) {
    File_Contents result = {};
    
    HANDLE file_handle = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file_handle == INVALID_HANDLE_VALUE) {
        CloseHandle(file_handle);
        return result;
    }
    
    DWORD file_size = GetFileSize(file_handle, 0);
    result.file_size = file_size;
    result.contents = (u8 *) VirtualAlloc(0, file_size,
                                          MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    
    DWORD bytes_read;
    if (ReadFile(file_handle, result.contents, file_size, &bytes_read, 0) && file_size == bytes_read) {
        // Success;
    }
    
    CloseHandle(file_handle);
    return result;
}

static bool
init_input(LPSTR cmd_line) {
    char *input_file = 0;
    if (cmd_line[0]) {
        input_file = cmd_line;
    } else {
        return false;
    }
    
    File_Contents file = read_entire_file(cmd_line);
    assert(file.file_size > 0);
    
    Text_File_Handler handler = {};
    init_handler(&handler, file.contents);
    
    assert(handler.num_lines > 0);
    
    char *line = consume_next_line(&handler);
    if (!line) return false;
    
    replace_strings.match_text = copy_string(line);
    
    while (1) {
        char *line = consume_next_line(&handler);
        if (!line) break;
        
        Break_String_Result r = break_by_tok(line, '|');
        if (!r.lhs || !r.rhs) {
            break;
        }
        
        Replace_Text text = {};
        text.from = copy_string(r.lhs);
        text.to   = copy_string(r.rhs);
        
        try { replace_strings.replace_text.push_back(text); } catch (const std::bad_alloc &) {} 
    }
    
    return true;
}

int CALLBACK
WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show) {
    
    bool success = init_input(cmd_line);
    if (!success) {
        ExitProcess(0);
        return 0;
    }
    
    WNDCLASSA window_class = {0};
    window_class.lpfnWndProc = default_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = "FakeWindowClass";
    
    if (RegisterClassA(&window_class)) {
        HWND fake_window = CreateWindowEx(0, window_class.lpszClassName, 0, 0, 0, 0, 1, 1, 0, 0, GetModuleHandle(0), 0);
        if (fake_window) {
            static NOTIFYICONDATA tray_icon_data;
            tray_icon_data.cbSize = sizeof(NOTIFYICONDATA);
            tray_icon_data.hWnd = fake_window;
            tray_icon_data.uID = 0;
            tray_icon_data.uFlags = NIF_MESSAGE | NIF_ICON;
            tray_icon_data.uCallbackMessage = Win32TrayIconMessage;
            tray_icon_data.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(101));
            tray_icon_data.szTip[0] = '\0';
            
            Shell_NotifyIcon(NIM_ADD, &tray_icon_data);
            
            AddClipboardFormatListener(fake_window);
            
            MSG message;
            while (GetMessage(&message, 0, 0, 0) > 0) {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            
            RemoveClipboardFormatListener(fake_window);
        }
    }
    
    ExitProcess(0);
}