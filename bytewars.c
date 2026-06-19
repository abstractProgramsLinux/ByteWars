#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#define MAX_ITEMS 4
#define MAX_SECTORS 4
#define TOTAL_DAYS 30

// Game State Structures
typedef struct {
    const char* name;
    int base_price;
    int current_price;
} MarketItem;

typedef struct {
    int cash;
    int debt;
    int bank;
    int day;
    int current_sector;
    int inventory[MAX_ITEMS];
    int selected_menu_index;
    int current_screen; // 0: Main, 1: Buy, 2: Sell, 3: Travel, 4: Bank, 5: GameOver
    int selected_sub_index;
} GameState;

static const char* sectors[MAX_SECTORS] = {
    "Neon District", "The Core", "Slum Sector", "Orbital Dock"
};

static MarketItem market[MAX_ITEMS] = {
    {"Quantum Keys", 500, 500},
    {"AI Cores", 200, 200},
    {"Giga-RAM", 50, 50},
    {"Bio-Firmware", 15, 15}
};

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

void randomize_prices() {
    for(int i = 0; i < MAX_ITEMS; i++) {
        int variance = (market[i].base_price * 40) / 100; // 40% variance
        market[i].current_price = market[i].base_price + (rand() % (variance * 2)) - variance;
        if(market[i].current_price < 2) market[i].current_price = 2;
    }
}

static void render_callback(Canvas* canvas, void* ctx) {
    GameState* state = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    char buffer[64];

    if (state->current_screen == 5) { // Game Over Screen
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 30, 15, "GAME OVER");
        canvas_set_font(canvas, FontSecondary);
        snprintf(buffer, sizeof(buffer), "Final Cash: $%d", state->cash);
        canvas_draw_str(canvas, 10, 32, buffer);
        int net = state->cash - state->debt + state->bank;
        snprintf(buffer, sizeof(buffer), "Net Worth: $%d", net);
        canvas_draw_str(canvas, 10, 45, buffer);
        canvas_draw_str(canvas, 15, 58, "Press OK to Restart");
        return;
    }

    // Top Status Bar
    snprintf(buffer, sizeof(buffer), "D:%d/%d | $%d | Debt:$%d", state->day, TOTAL_DAYS, state->cash, state->debt);
    canvas_draw_str(canvas, 2, 10, buffer);
    canvas_draw_line(canvas, 0, 12, 128, 12);

    // Screen 0: Main HUD
    if(state->current_screen == 0) {
        snprintf(buffer, sizeof(buffer), "Loc: %s", sectors[state->current_sector]);
        canvas_draw_str(canvas, 2, 22, buffer);

        const char* menu_opts[] = {"1. Buy Tech", "2. Sell Tech", "3. Travel", "4. Local Bank"};
        for(int i = 0; i < 4; i++) {
            if(state->selected_menu_index == i) {
                canvas_draw_str(canvas, 5, 34 + (i * 9), ">");
            }
            canvas_draw_str(canvas, 15, 34 + (i * 9), menu_opts[i]);
        }
    }
    // Screen 1 & 2: Buy / Sell Market
    else if(state->current_screen == 1 || state->current_screen == 2) {
        canvas_draw_str(canvas, 2, 21, state->current_screen == 1 ? "BUY DATA:" : "SELL DATA:");
        for(int i = 0; i < MAX_ITEMS; i++) {
            if(state->selected_sub_index == i) canvas_draw_str(canvas, 2, 31 + (i * 8), ">");
            snprintf(buffer, sizeof(buffer), "%s: $%d [%d]", market[i].name, market[i].current_price, state->inventory[i]);
            canvas_draw_str(canvas, 10, 31 + (i * 8), buffer);
        }
    }
    // Screen 3: Travel Selection
    else if(state->current_screen == 3) {
        canvas_draw_str(canvas, 2, 21, "JUMP TO SECTOR:");
        for(int i = 0; i < MAX_SECTORS; i++) {
            if(state->selected_sub_index == i) canvas_draw_str(canvas, 2, 31 + (i * 8), ">");
            snprintf(buffer, sizeof(buffer), "%s %s", sectors[i], (state->current_sector == i) ? "(Here)" : "");
            canvas_draw_str(canvas, 10, 31 + (i * 8), buffer);
        }
    }
    // Screen 4: Bank
    else if(state->current_screen == 4) {
        canvas_draw_str(canvas, 2, 22, "LOAN SHARK TERMINAL");
        snprintf(buffer, sizeof(buffer), "Bank Balance: $%d", state->bank);
        canvas_draw_str(canvas, 10, 33, buffer);
        
        const char* bank_opts[] = {"Pay $500 Debt", "Deposit $500", "Withdraw $500"};
        for(int i = 0; i < 3; i++) {
            if(state->selected_sub_index == i) canvas_draw_str(canvas, 5, 44 + (i * 7), ">");
            canvas_draw_str(canvas, 15, 44 + (i * 7), bank_opts[i]);
        }
    }
}

// Fixed Input Handler Logic
static void handle_input(InputEvent* input, GameState* state, bool* should_exit) {
    if(input->type != InputTypePress) return;

    // Global check: if Back is pressed
    if(input->key == InputKeyBack) {
        if(state->current_screen == 0) {
            *should_exit = true; // Only exit app completely if we are already on the Main HUD
        } else {
            state->current_screen = 0; // Otherwise, pop back to the Main HUD
        }
        return;
    }

    if(state->current_screen == 5) { // Game Over Restart
        if(input->key == InputKeyOk) {
            state->cash = 1000;
            state->debt = 2000;
            state->bank = 0;
            state->day = 1;
            state->current_sector = 0;
            state->current_screen = 0;
            memset(state->inventory,
