#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "resource.h"

#define NUM_SUITS 4
#define NUM_RANKS 13
#define NUM_CARDS 52
#define TABLEAU_COLS 7
#define FOUNDATION_PILES 4
#define STOCK_X 20
#define STOCK_Y 20
#define WASTE_X 90
#define WASTE_Y 20
#define FOUNDATION_X 230
#define FOUNDATION_Y 20
#define TABLEAU_X 20
#define TABLEAU_Y 100
#define CARD_W 32
#define CARD_H 32
#define TABLEAU_DY 22
#define SAVE_FILE "solitaire_save.dat"

typedef enum { DIFF_EASY = 0, DIFF_HARD = 1 } Difficulty;

typedef struct {
    int rank;
    int suit;
    int faceUp;
} Card;

typedef struct {
    Card cards[NUM_CARDS];
    int count;
} Pile;

typedef struct {
    int active;
    int fromType; /*0 tableau, 1 waste*/
    int fromIndex;
    int cardIndex;
} Selection;

typedef struct {
    Pile stock;
    Pile waste;
    Pile tableau[TABLEAU_COLS];
    Pile foundation[FOUNDATION_PILES];
    Difficulty difficulty;
    int stockPasses;
    Selection selection;
} GameState;

static HINSTANCE g_hInst;
static HWND g_hWnd;
static HICON g_cardIcons[NUM_CARDS];
static HICON g_backIcon;
static HICON g_emptyIcon;
static GameState g_game;

static int CardColor(const Card* c) { return (c->suit == 1 || c->suit == 2) ? 1 : 0; }
static int CardId(const Card* c) { return c->suit * NUM_RANKS + c->rank; }

static void Push(Pile* p, Card c) { p->cards[p->count++] = c; }
static Card Pop(Pile* p) { return p->cards[--p->count]; }
static Card* Peek(Pile* p) { return p->count ? &p->cards[p->count - 1] : NULL; }

static void ResetDeck(Card deck[NUM_CARDS]) {
    int i = 0;
    for (int s = 0; s < NUM_SUITS; ++s) {
        for (int r = 0; r < NUM_RANKS; ++r) {
            deck[i].rank = r;
            deck[i].suit = s;
            deck[i].faceUp = 0;
            ++i;
        }
    }
}

static void ShuffleDeck(Card deck[NUM_CARDS]) {
    for (int i = NUM_CARDS - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        Card t = deck[i];
        deck[i] = deck[j];
        deck[j] = t;
    }
}

static void InitGame(GameState* g) {
    Card deck[NUM_CARDS];
    ZeroMemory(g, sizeof(*g));
    g->difficulty = DIFF_EASY;
    ResetDeck(deck);
    ShuffleDeck(deck);

    int idx = 0;
    for (int col = 0; col < TABLEAU_COLS; ++col) {
        g->tableau[col].count = 0;
        for (int row = 0; row <= col; ++row) {
            Card c = deck[idx++];
            c.faceUp = (row == col);
            Push(&g->tableau[col], c);
        }
    }

    while (idx < NUM_CARDS) {
        Push(&g->stock, deck[idx++]);
    }
}

static int CanMoveToFoundation(const Card* c, const Pile* dest) {
    if (!dest->count) return c->rank == 0;
    const Card* top = &dest->cards[dest->count - 1];
    return c->suit == top->suit && c->rank == top->rank + 1;
}

static int CanMoveToTableau(const Card* c, const Pile* dest) {
    if (!dest->count) return c->rank == 12;
    const Card* top = &dest->cards[dest->count - 1];
    if (!top->faceUp) return 0;
    return CardColor(c) != CardColor(top) && c->rank + 1 == top->rank;
}

static void RecycleWaste(GameState* g) {
    if (!g->waste.count) return;
    if (g->difficulty == DIFF_HARD && g->stockPasses >= 1) return;
    while (g->waste.count) {
        Card c = Pop(&g->waste);
        c.faceUp = 0;
        Push(&g->stock, c);
    }
    g->stockPasses++;
}

static void DrawFromStock(GameState* g) {
    int drawCount = (g->difficulty == DIFF_EASY) ? 1 : 3;
    if (!g->stock.count) {
        RecycleWaste(g);
        return;
    }
    for (int i = 0; i < drawCount && g->stock.count; ++i) {
        Card c = Pop(&g->stock);
        c.faceUp = 1;
        Push(&g->waste, c);
    }
}

static void RevealTableauTop(Pile* p) {
    if (p->count && !p->cards[p->count - 1].faceUp) {
        p->cards[p->count - 1].faceUp = 1;
    }
}

static void ClearSelection(GameState* g) { g->selection.active = 0; }

static void SaveGame(const GameState* g) {
    FILE* f = fopen(SAVE_FILE, "wb");
    if (!f) return;
    fwrite(g, sizeof(*g), 1, f);
    fclose(f);
}

static int LoadGame(GameState* g) {
    FILE* f = fopen(SAVE_FILE, "rb");
    if (!f) return 0;
    int ok = fread(g, sizeof(*g), 1, f) == 1;
    fclose(f);
    return ok;
}

static void DrawCardIcon(HDC hdc, int x, int y, const Card* c) {
    HICON icon = c->faceUp ? g_cardIcons[CardId(c)] : g_backIcon;
    DrawIconEx(hdc, x, y, icon, CARD_W, CARD_H, 0, NULL, DI_NORMAL);
}

static void DrawPileFrame(HDC hdc, int x, int y) {
    DrawIconEx(hdc, x, y, g_emptyIcon, CARD_W, CARD_H, 0, NULL, DI_NORMAL);
}

static void PaintGame(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    HBRUSH bg = CreateSolidBrush(RGB(0, 100, 0));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    if (g_game.stock.count) {
        DrawIconEx(hdc, STOCK_X, STOCK_Y, g_backIcon, CARD_W, CARD_H, 0, NULL, DI_NORMAL);
    } else {
        DrawPileFrame(hdc, STOCK_X, STOCK_Y);
    }

    if (g_game.waste.count) {
        DrawCardIcon(hdc, WASTE_X, WASTE_Y, &g_game.waste.cards[g_game.waste.count - 1]);
    } else {
        DrawPileFrame(hdc, WASTE_X, WASTE_Y);
    }

    for (int i = 0; i < FOUNDATION_PILES; ++i) {
        int x = FOUNDATION_X + i * 70;
        if (g_game.foundation[i].count) {
            DrawCardIcon(hdc, x, FOUNDATION_Y, &g_game.foundation[i].cards[g_game.foundation[i].count - 1]);
        } else {
            DrawPileFrame(hdc, x, FOUNDATION_Y);
        }
    }

    for (int c = 0; c < TABLEAU_COLS; ++c) {
        int x = TABLEAU_X + c * 70;
        if (!g_game.tableau[c].count) {
            DrawPileFrame(hdc, x, TABLEAU_Y);
            continue;
        }
        for (int i = 0; i < g_game.tableau[c].count; ++i) {
            int y = TABLEAU_Y + i * TABLEAU_DY;
            DrawCardIcon(hdc, x, y, &g_game.tableau[c].cards[i]);
        }
    }

    if (g_game.selection.active) {
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 215, 0));
        HGDIOBJ old = SelectObject(hdc, pen);
        int sx = 0, sy = 0;
        if (g_game.selection.fromType == 1) {
            sx = WASTE_X; sy = WASTE_Y;
        } else {
            sx = TABLEAU_X + g_game.selection.fromIndex * 70;
            sy = TABLEAU_Y + g_game.selection.cardIndex * TABLEAU_DY;
        }
        Rectangle(hdc, sx - 1, sy - 1, sx + CARD_W + 1, sy + CARD_H + 1);
        SelectObject(hdc, old);
        DeleteObject(pen);
    }
}

static int HitTableau(int x, int y, int* colOut, int* cardIdxOut) {
    for (int c = 0; c < TABLEAU_COLS; ++c) {
        int left = TABLEAU_X + c * 70;
        if (x < left || x > left + CARD_W) continue;
        Pile* p = &g_game.tableau[c];
        if (!p->count) {
            if (y >= TABLEAU_Y && y <= TABLEAU_Y + CARD_H) {
                *colOut = c; *cardIdxOut = -1; return 1;
            }
            continue;
        }
        for (int i = p->count - 1; i >= 0; --i) {
            int top = TABLEAU_Y + i * TABLEAU_DY;
            int bottom = top + CARD_H;
            if (y >= top && y <= bottom) {
                *colOut = c;
                *cardIdxOut = i;
                return 1;
            }
        }
    }
    return 0;
}

static int HitFoundation(int x, int y) {
    for (int i = 0; i < FOUNDATION_PILES; ++i) {
        int left = FOUNDATION_X + i * 70;
        if (x >= left && x <= left + CARD_W && y >= FOUNDATION_Y && y <= FOUNDATION_Y + CARD_H) return i;
    }
    return -1;
}

static void TryMoveSelectedToTableau(GameState* g, int col) {
    if (!g->selection.active) return;
    Card moving;
    if (g->selection.fromType == 1) {
        moving = g->waste.cards[g->waste.count - 1];
    } else {
        Pile* from = &g->tableau[g->selection.fromIndex];
        moving = from->cards[g->selection.cardIndex];
        if (g->selection.cardIndex != from->count - 1) return;
    }

    if (!CanMoveToTableau(&moving, &g->tableau[col])) return;

    if (g->selection.fromType == 1) {
        Push(&g->tableau[col], Pop(&g->waste));
    } else {
        Pile* from = &g->tableau[g->selection.fromIndex];
        Push(&g->tableau[col], Pop(from));
        RevealTableauTop(from);
    }
    ClearSelection(g);
}

static void TryMoveSelectedToFoundation(GameState* g, int idx) {
    if (!g->selection.active) return;
    Card moving;
    if (g->selection.fromType == 1) {
        moving = g->waste.cards[g->waste.count - 1];
    } else {
        Pile* from = &g->tableau[g->selection.fromIndex];
        if (g->selection.cardIndex != from->count - 1) return;
        moving = from->cards[from->count - 1];
    }

    if (!CanMoveToFoundation(&moving, &g->foundation[idx])) return;

    if (g->selection.fromType == 1) {
        Push(&g->foundation[idx], Pop(&g->waste));
    } else {
        Pile* from = &g->tableau[g->selection.fromIndex];
        Push(&g->foundation[idx], Pop(from));
        RevealTableauTop(from);
    }
    ClearSelection(g);
}

static void HandleClick(int x, int y) {
    if (x >= STOCK_X && x <= STOCK_X + CARD_W && y >= STOCK_Y && y <= STOCK_Y + CARD_H) {
        DrawFromStock(&g_game);
        ClearSelection(&g_game);
        InvalidateRect(g_hWnd, NULL, TRUE);
        return;
    }

    if (x >= WASTE_X && x <= WASTE_X + CARD_W && y >= WASTE_Y && y <= WASTE_Y + CARD_H) {
        if (g_game.waste.count) {
            g_game.selection.active = 1;
            g_game.selection.fromType = 1;
            g_game.selection.fromIndex = -1;
            g_game.selection.cardIndex = g_game.waste.count - 1;
        }
        InvalidateRect(g_hWnd, NULL, TRUE);
        return;
    }

    int f = HitFoundation(x, y);
    if (f >= 0) {
        TryMoveSelectedToFoundation(&g_game, f);
        InvalidateRect(g_hWnd, NULL, TRUE);
        return;
    }

    int col, cardIdx;
    if (HitTableau(x, y, &col, &cardIdx)) {
        if (g_game.selection.active) {
            TryMoveSelectedToTableau(&g_game, col);
        } else if (cardIdx >= 0) {
            Card* c = &g_game.tableau[col].cards[cardIdx];
            if (!c->faceUp && cardIdx == g_game.tableau[col].count - 1) {
                c->faceUp = 1;
            } else if (c->faceUp && cardIdx == g_game.tableau[col].count - 1) {
                g_game.selection.active = 1;
                g_game.selection.fromType = 0;
                g_game.selection.fromIndex = col;
                g_game.selection.cardIndex = cardIdx;
            }
        } else {
            TryMoveSelectedToTableau(&g_game, col);
        }
        InvalidateRect(g_hWnd, NULL, TRUE);
        return;
    }

    ClearSelection(&g_game);
    InvalidateRect(g_hWnd, NULL, TRUE);
}

static void SetDifficulty(GameState* g, Difficulty d) {
    g->difficulty = d;
    g->stockPasses = 0;
}

static void UpdateDifficultyMenu(HMENU menu, Difficulty d) {
    CheckMenuItem(menu, IDM_DIFFICULTY_EASY, MF_BYCOMMAND | (d == DIFF_EASY ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, IDM_DIFFICULTY_HARD, MF_BYCOMMAND | (d == DIFF_HARD ? MF_CHECKED : MF_UNCHECKED));
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hWnd = hwnd;
        for (int i = 0; i < NUM_CARDS; ++i) {
            g_cardIcons[i] = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_CARD_BASE + i));
        }
        g_backIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_CARD_BACK));
        g_emptyIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_CARD_EMPTY));
        srand((unsigned int)time(NULL));
        InitGame(&g_game);
        HMENU menu = GetMenu(hwnd);
        UpdateDifficultyMenu(menu, g_game.difficulty);
        return 0;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDM_GAME_NEW:
            InitGame(&g_game);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        case IDM_GAME_SAVE:
            SaveGame(&g_game);
            break;
        case IDM_GAME_LOAD:
            if (LoadGame(&g_game)) InvalidateRect(hwnd, NULL, TRUE);
            break;
        case IDM_GAME_EXIT:
            DestroyWindow(hwnd);
            break;
        case IDM_DIFFICULTY_EASY:
            SetDifficulty(&g_game, DIFF_EASY);
            UpdateDifficultyMenu(GetMenu(hwnd), g_game.difficulty);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        case IDM_DIFFICULTY_HARD:
            SetDifficulty(&g_game, DIFF_HARD);
            UpdateDifficultyMenu(GetMenu(hwnd), g_game.difficulty);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
        HandleClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintGame(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine;
    g_hInst = hInstance;

    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "KabalSolitaireWnd";

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        "Kabal Solitaire (Petzold Style Win32)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        650,
        520,
        NULL,
        LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MAINMENU)),
        hInstance,
        NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
