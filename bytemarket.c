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

// Update market prices randomly based on base price
void randomize_prices() {
    for(int i = 0; i < MAX_ITEMS; i++) {
        int variance = (market[i].base_price * 40) / 100; // 40% variance
        market[i].current_price = market[i].base_price + (rand() % (variance * 2)) - variance;
        if(market[i].current_price < 2) market[i].current_price = 2;
    }
}

// Render the UI screens
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

    // Top Status Bar (All active screens)
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

// Input handler for controls
static void handle_input(InputEvent* input, GameState* state) {
    if(input->type != InputTypePress) return;

    if(state->current_screen == 5) { // Game Over
        if(input->key == InputKeyOk) { // Reset game
            state->cash = 1000;
            state->debt = 2000;
            state->bank = 0;
            state->day = 1;
            state->current_sector = 0;
            state->current_screen = 0;
            memset(state->inventory, 0, sizeof(state->inventory));
            randomize_prices();
        }
        return;
    }

    if(state->current_screen == 0) { // Main Menu navigation
        if(input->key == InputKeyUp) state->selected_menu_index = (state->selected_menu_index - 1 + 4) % 4;
        if(input->key == InputKeyDown) state->selected_menu_index = (state->selected_menu_index + 1) % 4;
        if(input->key == InputKeyOk) {
            state->current_screen = state->selected_menu_index + 1;
            state->selected_sub_index = 0;
        }
    } 
    else { // Sub-menus (Buy, Sell, Travel, Bank)
        if(input->key == InputKeyBack) {
            state->current_screen = 0; // Go back to main HUD
            return;
        }
        
        int max_index = (state->current_screen == 4) ? 3 : 4; // Bank has 3 options, others 4
        if(input->key == InputKeyUp) state->selected_sub_index = (state->selected_sub_index - 1 + max_index) % max_index;
        if(input->key == InputKeyDown) state->selected_sub_index = (state->selected_sub_index + 1) % max_index;

        if(input->key == InputKeyOk) {
            int idx = state->selected_sub_index;
            if(state->current_screen == 1) { // Buy Logic
                if(state->cash >= market[idx].current_price) {
                    state->cash -= market[idx].current_price;
                    state->inventory[idx]++;
                }
            } 
            else if(state->current_screen == 2) { // Sell Logic
                if(state->inventory[idx] > 0) {
                    state->cash += market[idx].current_price;
                    state->inventory[idx]--;
                }
            } 
            else if(state->current_screen == 3) { // Travel Logic
                if(state->current_sector != idx) {
                    state->current_sector = idx;
                    state->day++;
                    state->debt = (state->debt * 115) / 100; // 15% debt interest daily
                    randomize_prices();
                    state->current_screen = 0; // Return to main menu
                    
                    if(state->day > TOTAL_DAYS) {
                        state->current_screen = 5; // Trigger Game Over
                    }
                }
            }
            else if(state->current_screen == 4) { // Bank Logic
                if(idx == 0 && state->cash >= 500 && state->debt >= 500) { // Pay Debt
                    state->cash -= 500;
                    state->debt -= 500;
                } else if(idx == 1 && state->cash >= 500) { // Deposit
                    state->cash -= 500;
                    state->bank += 500;
                } else if(idx == 2 && state->bank >= 500) { // Withdraw
                    state->bank -= 500;
                    state->cash += 500;
                }
            }
        }
    }
}

// Application entry point
int32_t bytemarket_app(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    
    GameState* state = malloc(sizeof(GameState));
    state->cash = 1000;
    state->debt = 2000;
    state->bank = 0;
    state->day = 1;
    state->current_sector = 0;
    state->current_screen = 0;
    state->selected_menu_index = 0;
    state->selected_sub_index = 0;
    memset(state->inventory, 0, sizeof(state->inventory));
    srand(furi_get_tick());
    randomize_prices();

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, state);
    view_port_input_callback_set(view_port, input_viewport_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    while(1) {
        FuriStatus status = furi_message_queue_get(event_queue, &event, FuriWaitForever);
        if(status == FuriStatusOk) {
            if(event.key == InputKeyBack && state->current_screen == 0) {
                break; // Exit app completely if pressing Back on Main HUD
            }
            handle_input(&event, state);
            view_port_update(view_port);
        }
    }

    // Clean up memory
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    free(state);

    return 0;
}