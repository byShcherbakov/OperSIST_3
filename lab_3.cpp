#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwctype>
#include <clocale>
#include <cstdlib>
#include <filesystem>


using namespace std;


// Одна запись: пять ключевых полей и текст.
struct Record {
    wstring field[5];
    wstring text;
};

vector<Record> records;

// Номера записей, отображаемых в таблице.
vector<int> shown;

int current = -1;
bool updating = false;

HWND window;
HWND table;
HWND fields[5];
HWND textBox;
HWND sortBox;

HWND searchField1, searchField2;
HWND searchValue1, searchValue2;
HWND operation;

const wchar_t* names[] = {
    L"Заголовок", L"Категория", L"Имя", L"Телефон", L"Метки"
};

// Номера кнопок.
enum {
    NEW_RECORD = 1,
    APPLY,
    REMOVE,
    PREVIOUS,
    NEXT,
    SAVE_FILE,
    LOAD_FILE,
    SORT,
    SEARCH,
    SHOW_ALL
};

// Получить текст из элемента окна.
wstring GetText(HWND element) {
    int length = GetWindowTextLengthW(element);

    vector<wchar_t> buffer(length + 1);
    GetWindowTextW(element, buffer.data(), length + 1);

    return buffer.data();
}

// Нижний регистр для поиска без учёта регистра.
wstring Lower(wstring text) {
    for (wchar_t& letter : text) {
        letter = towlower(letter);
    }

    return text;
}

// Создать кнопку, поле ввода или подпись.
HWND Create(
    const wchar_t* type,
    const wchar_t* text,
    DWORD style,
    int x, int y, int width, int height,
    int id = 0)
{
    return CreateWindowW(
        type, text,
        WS_CHILD | WS_VISIBLE | style,
        x, y, width, height,
        window,
        (HMENU)(INT_PTR)id,
        GetModuleHandleW(nullptr),
        nullptr
    );
}

void Message(const wchar_t* text) {
    MessageBoxW(window, text, L"Сообщение", MB_OK);
}

// Показать запись в полях справа.
// -1 означает очистить поля для новой записи.
void ShowRecord(int index) {
    current = index;

    for (int i = 0; i < 5; i++) {
        SetWindowTextW(
            fields[i],
            index == -1 ? L"" : records[index].field[i].c_str()
        );
    }

    SetWindowTextW(
        textBox,
        index == -1 ? L"" : records[index].text.c_str()
    );
}

// Заполнить таблицу по списку shown.
void UpdateTable(int selected = -1) {
    updating = true;

    ListView_DeleteAllItems(table);

    for (int row = 0; row < (int)shown.size(); row++) {
        int index = shown[row];

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.pszText = records[index].field[0].data();

        ListView_InsertItem(table, &item);

        for (int column = 1; column < 5; column++) {
            ListView_SetItemText(
                table, row, column,
                records[index].field[column].data()
            );
        }

        if (index == selected) {
            ListView_SetItemState(
                table, row,
                LVIS_SELECTED | LVIS_FOCUSED,
                LVIS_SELECTED | LVIS_FOCUSED
            );

            ListView_EnsureVisible(table, row, FALSE);
        }
    }

    updating = false;
}

// Отменить поиск и показать все записи.
void ShowAll(int selected = -1) {
    shown.clear();

    for (int i = 0; i < (int)records.size(); i++) {
        shown.push_back(i);
    }

    UpdateTable(selected);
}

// Сохранить содержимое полей в список records.
void ApplyRecord() {
    Record record;

    for (int i = 0; i < 5; i++) {
        record.field[i] = GetText(fields[i]);
    }

    record.text = GetText(textBox);

    if (record.field[0].find_first_not_of(L" \t\r\n") == wstring::npos) {
        Message(L"Введите заголовок.");
        return;
    }

    // В Windows один wchar_t занимает 2 байта.
    if (record.text.size() * sizeof(wchar_t) > 4096) {
        Message(L"Текст должен занимать не более 4 Кб в UTF-16.");
        return;
    }

    if (current == -1) {
        records.push_back(record);
        current = (int)records.size() - 1;
    } else {
        records[current] = record;
    }

    ShowAll(current);
    ShowRecord(current);
}

void DeleteRecord() {
    if (current == -1) {
        Message(L"Выберите запись.");
        return;
    }

    if (MessageBoxW(
            window, L"Удалить запись?", L"Удаление",
            MB_YESNO) != IDYES) {
        return;
    }

    records.erase(records.begin() + current);

    ShowAll();
    ShowRecord(-1);
}

// Переход по записям текущего списка.
void MoveRecord(int step) {
    if (shown.empty()) {
        return;
    }

    int row = -1;

    for (int i = 0; i < (int)shown.size(); i++) {
        if (shown[i] == current) {
            row = i;
            break;
        }
    }

    if (row == -1) {
        row = step > 0 ? 0 : (int)shown.size() - 1;
    } else {
        row += step;
    }

    if (row >= 0 && row < (int)shown.size()) {
        ShowRecord(shown[row]);
        UpdateTable(current);
    }
}

// Пользователь задаёт порядок полей:
// 2 1 — категория, потом заголовок.
// -2 1 — категория по убыванию, потом заголовок.
void SortRecords() {
    wistringstream input(GetText(sortBox));

    vector<int> order;
    int number;

    while (input >> number) {
        if (number == 0 || number < -5 || number > 5) {
            Message(L"Введите номера полей от 1 до 5. Пример: 2 1");
            return;
        }

        for (int previous : order) {
            if (abs(previous) == abs(number)) {
                Message(L"Не повторяйте номера полей.");
                return;
            }
        }

        order.push_back(number);
    }

    if (order.empty() || !input.eof()) {
        Message(L"Введите номера через пробел. Пример: 2 1");
        return;
    }

    stable_sort(shown.begin(), shown.end(),
        [&](int a, int b) {
            for (int number : order) {
                int field = abs(number) - 1;

                wstring first = Lower(records[a].field[field]);
                wstring second = Lower(records[b].field[field]);

                if (first != second) {
                    if (number > 0) {
                        return first < second;
                    }

                    return first > second;
                }
            }

            return false;
        }
    );

    UpdateTable(current);
}

// Проверить наличие подстроки в ключевом поле.
bool Contains(int index, int field, const wstring& value) {
    return Lower(records[index].field[field]).find(Lower(value))
        != wstring::npos;
}

void SearchRecords() {
    int field1 = (int)SendMessageW(
        searchField1, CB_GETCURSEL, 0, 0
    );

    int field2 = (int)SendMessageW(
        searchField2, CB_GETCURSEL, 0, 0
    );

    int logic = (int)SendMessageW(
        operation, CB_GETCURSEL, 0, 0
    );

    wstring value1 = GetText(searchValue1);
    wstring value2 = GetText(searchValue2);

    if (value1.empty()) {
        Message(L"Введите первое условие поиска.");
        return;
    }

    shown.clear();

    for (int i = 0; i < (int)records.size(); i++) {
        bool first = Contains(i, field1, value1);
        bool result = first;

        if (!value2.empty()) {
            bool second = Contains(i, field2, value2);

            if (logic == 0) result = first && second;
            if (logic == 1) result = first || second;
            if (logic == 2) result = first && !second;
        }

        if (result) {
            shown.push_back(i);
        }
    }

    ShowRecord(-1);
    UpdateTable();
}

// Записать строку: сначала длину, затем символы.
void WriteString(ofstream& file, const wstring& text) {
    unsigned int length = (unsigned int)text.size();

    file.write((const char*)&length, sizeof(length));
    file.write(
        (const char*)text.data(),
        length * sizeof(wchar_t)
    );
}

// Прочитать строку с проверкой допустимой длины.
bool ReadString(ifstream& file, wstring& text, unsigned int maximum) {
    unsigned int length = 0;

    if (!file.read((char*)&length, sizeof(length))) {
        return false;
    }

    if (length > maximum) {
        return false;
    }

    text.resize(length);

    if (length > 0) {
        file.read((char*)text.data(), length * sizeof(wchar_t));
    }

    return (bool)file;
}

void SaveRecords() {
    ofstream file("notebook.dat", ios::binary);

    if (!file) {
        Message(L"Не удалось создать файл.");
        return;
    }

    unsigned int count = (unsigned int)records.size();

    file.write((const char*)&count, sizeof(count));

    for (const Record& record : records) {
        for (int i = 0; i < 5; i++) {
            WriteString(file, record.field[i]);
        }

        WriteString(file, record.text);
    }

    file.close();

    Message(file ? L"Файл сохранён." : L"Ошибка сохранения.");
}

void LoadRecords() {
    // Буфер для пути выбранного файла.
wchar_t fileName[MAX_PATH] = L"";

OPENFILENAMEW dialog{};
dialog.lStructSize = sizeof(dialog);
dialog.hwndOwner = window;

dialog.lpstrFilter =
    L"Файлы записной книжки (*.dat)\0*.dat\0"
    L"Все файлы (*.*)\0*.*\0";

dialog.lpstrFile = fileName;
dialog.nMaxFile = MAX_PATH;

dialog.lpstrTitle = L"Открыть записную книжку";
dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
               OFN_NOCHANGEDIR;

// Если пользователь нажал «Отмена», ничего не загружаем.
if (!GetOpenFileNameW(&dialog)) {
    return;
}

ifstream file(
    std::filesystem::path(fileName),
    ios::binary
);

    if (!file) {
        Message(L"Не удалось открыть выбранный файл.");
        return;
    }

    unsigned int count = 0;

    if (!file.read((char*)&count, sizeof(count)) || count > 100000) {
        Message(L"Неверный файл.");
        return;
    }

    vector<Record> loaded;

    for (unsigned int i = 0; i < count; i++) {
        Record record;

        for (int field = 0; field < 5; field++) {
            if (!ReadString(file, record.field[field], 255)) {
                Message(L"Файл повреждён.");
                return;
            }
        }

        if (!ReadString(file, record.text, 2048) ||
            record.field[0].find_first_not_of(L" \t\r\n") == wstring::npos) {
            Message(L"Некорректная запись в файле.");
            return;
        }

        loaded.push_back(record);
    }

    records = loaded;

    ShowAll();
    ShowRecord(-1);
}

// Заполнить список выбора названиями полей.
void FillCombo(HWND combo) {
    for (int i = 0; i < 5; i++) {
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)names[i]);
    }

    SendMessageW(combo, CB_SETCURSEL, 0, 0);
}

void CreateInterface() {
    // Список записей слева.
    Create(L"STATIC", L"Список записей",
           0, 10, 10, 200, 20);

    table = Create(
        WC_LISTVIEWW, L"",
        WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        10, 35, 480, 270
    );

    ListView_SetExtendedListViewStyle(
        table,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES
    );

    for (int i = 0; i < 5; i++) {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.pszText = (wchar_t*)names[i];
        column.cx = i == 0 ? 115 : 85;

        ListView_InsertColumn(table, i, &column);
    }

    // Поля выбранной записи справа.
    for (int i = 0; i < 5; i++) {
        Create(
            L"STATIC", names[i],
            0, 510, 35 + i * 30, 95, 20
        );

        fields[i] = Create(
            L"EDIT", L"",
            WS_BORDER | ES_AUTOHSCROLL,
            610, 32 + i * 30, 245, 24
        );

        SendMessageW(fields[i], EM_SETLIMITTEXT, 255, 0);
    }

    Create(L"STATIC", L"Текст записи",
           0, 510, 190, 200, 20);

    textBox = Create(
        L"EDIT", L"",
        WS_BORDER | ES_MULTILINE | WS_VSCROLL |
        ES_AUTOVSCROLL | ES_WANTRETURN,
        510, 215, 345, 90
    );

    SendMessageW(textBox, EM_SETLIMITTEXT, 2048, 0);

    // Основные кнопки.
    Create(L"BUTTON", L"Добавить", 0,
           10, 320, 90, 30, NEW_RECORD);

    Create(L"BUTTON", L"Применить", 0,
           110, 320, 100, 30, APPLY);

    Create(L"BUTTON", L"Удалить", 0,
           220, 320, 85, 30, REMOVE);

    Create(L"BUTTON", L"Назад", 0,
           315, 320, 80, 30, PREVIOUS);

    Create(L"BUTTON", L"Вперёд", 0,
           405, 320, 85, 30, NEXT);

    Create(L"BUTTON", L"Сохранить файл", 0,
           510, 320, 165, 30, SAVE_FILE);

    Create(L"BUTTON", L"Открыть файл", 0,
           690, 320, 165, 30, LOAD_FILE);

    // Сортировка.
    Create(L"STATIC", L"Сортировка:",
           0, 10, 375, 100, 20);

    sortBox = Create(
        L"EDIT", L"1",
        WS_BORDER | ES_AUTOHSCROLL,
        115, 372, 160, 25
    );

    Create(L"BUTTON", L"Сортировать", 0,
           290, 370, 125, 30, SORT);

    Create(
        L"STATIC",
        L"1 — заголовок; 2 — категория; 3 — имя;\n"
        L"4 — телефон; 5 — метки. Пример: 2 1",
        0, 435, 370, 420, 45
    );

    // Первое условие поиска.
    Create(L"STATIC", L"Поиск:",
           0, 10, 430, 90, 20);

    searchField1 = Create(
        L"COMBOBOX", L"",
        CBS_DROPDOWNLIST | WS_VSCROLL,
        115, 427, 150, 180
    );

    searchValue1 = Create(
        L"EDIT", L"",
        WS_BORDER | ES_AUTOHSCROLL,
        280, 427, 300, 25
    );

    // Второе условие поиска.
    operation = Create(
        L"COMBOBOX", L"",
        CBS_DROPDOWNLIST,
        10, 467, 90, 130
    );

    SendMessageW(operation, CB_ADDSTRING, 0, (LPARAM)L"И");
    SendMessageW(operation, CB_ADDSTRING, 0, (LPARAM)L"ИЛИ");
    SendMessageW(operation, CB_ADDSTRING, 0, (LPARAM)L"И НЕ");
    SendMessageW(operation, CB_SETCURSEL, 0, 0);

    searchField2 = Create(
        L"COMBOBOX", L"",
        CBS_DROPDOWNLIST | WS_VSCROLL,
        115, 467, 150, 180
    );

    searchValue2 = Create(
        L"EDIT", L"",
        WS_BORDER | ES_AUTOHSCROLL,
        280, 467, 300, 25
    );

    FillCombo(searchField1);
    FillCombo(searchField2);

    Create(L"BUTTON", L"Найти", 0,
           600, 425, 120, 30, SEARCH);

    Create(L"BUTTON", L"Показать всё", 0,
           735, 425, 120, 30, SHOW_ALL);

    // Короткая инструкция.
    Create(
        L"STATIC",
        L"Добавить → заполнить поля → Применить.\n"
        L"Перед переходом к другой записи нажмите «Применить».\n"
        L"Для сохранения записей на диск нажмите «Сохранить файл».",
        0, 10, 525, 845, 65
    );
}





// Обработка событий: кнопки, выбор строки, закрытие.
LRESULT CALLBACK WindowProc(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        window = hwnd;
        CreateInterface();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case NEW_RECORD:
            ShowRecord(-1);
            UpdateTable();
            break;

        case APPLY:
            ApplyRecord();
            break;

        case REMOVE:
            DeleteRecord();
            break;

        case PREVIOUS:
            MoveRecord(-1);
            break;

        case NEXT:
            MoveRecord(1);
            break;

        case SAVE_FILE:
            SaveRecords();
            break;

        case LOAD_FILE:
            LoadRecords();
            break;

        case SORT:
            SortRecords();
            break;

        case SEARCH:
            SearchRecords();
            break;

        case SHOW_ALL:
            ShowAll();
            ShowRecord(-1);
            break;
        }
        return 0;

    case WM_NOTIFY: {
        NMHDR* event = (NMHDR*)lParam;

        if (event->hwndFrom == table &&
            event->code == LVN_ITEMCHANGED &&
            !updating) {

            NMLISTVIEW* item = (NMLISTVIEW*)lParam;

            if ((item->uNewState & LVIS_SELECTED) &&
                !(item->uOldState & LVIS_SELECTED) &&
                item->iItem >= 0 &&
                item->iItem < (int)shown.size()) {

                ShowRecord(shown[item->iItem]);
            }
        }

        return 0;
    }

    case WM_CLOSE:
        if (MessageBoxW(
                hwnd,
                L"Закрыть программу? Несохранённые изменения будут потеряны.",
                L"Выход",
                MB_YESNO) == IDYES) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    setlocale(LC_ALL, "");

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&controls);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"Notebook";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    if (!RegisterClassW(&wc)) {
        return 1;
    }

    DWORD style = WS_OVERLAPPED | WS_CAPTION |
                  WS_SYSMENU | WS_MINIMIZEBOX;

    RECT size = {0, 0, 875, 605};
    AdjustWindowRect(&size, style, FALSE);

    HWND hwnd = CreateWindowW(
        L"Notebook",
        L"Записная книжка",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        size.right - size.left,
        size.bottom - size.top,
        nullptr, nullptr, instance, nullptr
    );

    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, show);

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}