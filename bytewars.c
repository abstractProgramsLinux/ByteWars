#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#define MAX_ITEMS 4
#define MAX_SECTORS 4
#define TOTAL_DAYS 30

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
    int current_screen; 
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

void randomize_prices() {
    for(int i = 0; i < MAX_ITEMS; i++) {
        int variance = (market[i].base_price * 40) / 100; 
        market[i].current_price = market[i].base_price + (rand() % (variance * 2)) - variance;
        if(market[i].current_price < 2) market[i].current_price = 2;
    }
}

static void render_callback(Canvas* canvas, void* ctx) {
    GameState* state = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    char buffer[64];

    if (state->current_screen == 5) { 
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

    snprintf(buffer, sizeof(buffer), "D:%d/%d | $%d | Debt:$%d", state->day, TOTAL_DAYS, state->cash, state->debt);
    canvas_draw_str(canvas, 2, 10, buffer);
    canvas_draw_line(canvas, 0, 12, 128, 12);

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
    else if(state->current_screen == 1 || state->current_screen == 2) {
        canvas_draw_str(canvas, 2, 21, state->current_screen == 1 ? "BUY DATA:" : "SELL DATA:");
        for(int i = 0; i < MAX_ITEMS; i++) {
            if(state->selected_sub_index == i) canvas_draw_str(canvas, 2, 31 + (i * 8), ">");
            snprintf(buffer, sizeof(buffer), "%s: $%d [%d]", market[i].name, market[i].current_price, state->inventory[i]);
            canvas_draw_str(canvas, 10, 31 + (i * 8), buffer);
        }
    }
    else if(state->current_screen == 3) {
        canvas_draw_str(canvas, 2, 21, "JUMP TO SECTOR:");
        for(int i = 0; i < MAX_SECTORS; i++) {
            if(state->selected_sub_index == i) canvas_draw_str(canvas, 2, 31 + (i * 8), ">");
            snprintf(buffer, sizeof(buffer), "%s %s", sectors[i], (state->current_sector == i) ? "(Here)" : "");
            canvas_draw_str(canvas, 10, 31 + (i * 8), buffer);
        }
    }
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
    
    // Direct firmware view-port subscribe strategy 
    view_port_input_callback_set(view_port, (ViewPortInputCallback)furi_message_queue_put, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    while(1) {
        if(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type != InputTypePress) continue;

            // Clear execution processing for navigation back out
            if(event.key == InputKeyBack) {
                if(state->current_screen == 0) {
                    break; 
                } else {
                    state->current_screen = 0;
                    view_port_update(view_port);
                    continue;
                }
            }

            if(state->current_screen == 5) { 
                if(event.key == InputKeyOk) {
                    state->cash = 1000;
                    state->debt = 2000;
                    state->bank = 0;
                    state->day = 1;
                    state->current_sector = 0;
                    state->current_screen = 0;
                    memset(state->inventory, 0, sizeof(state->inventory));
                    randomize_prices();
                }
                view_port_update(view_port);
                continue;
            }

            if(state->current_screen == 0) { 
                if(event.key == InputKeyUp) state->selected_menu_index = (state->selected_menu_index - 1 + 4) % 4;
                if(event.key == InputKeyDown) state->selected_menu_index = (state->selected_menu_index + 1) % 4;
                if(event.key == InputKeyOk) {
                    state->current_screen = state->selected_menu_index + 1;
                    state->selected_sub_index = 0;
                }
            } 
            else { 
                int max_index = (state->current_screen == 4) ? 3 : 4; 
                if(event.key == InputKeyUp) state->selected_sub_index = (state->selected_sub_index - 1 + max_index) % max_index;
                if(event.key == InputKeyDown) state->selected_sub_index = (state->selected_sub_index + 1) % max_index;

                if(event.key == InputKeyOk) {
                    int idx = state->selected_sub_index;
                    if(state->current_screen == 1) { 
                        if(state->cash >= market[idx].current_price) {
                            state->cash -= market[idx].current_price;
                            state->inventory[idx]++;
                        }
                    } 
                    else if(state->current_screen == 2) { 
                        if(state->inventory[idx] > 0) {
                            state->cash += market[idx].current_price;
                            state->inventory[idx]--;
                        }
                    } 
                    else if(state->current_screen == 3) { 
                        if(state->current_sector != idx) {
                            state->current_sector = idx;
                            state->day++;
                            state->debt = (state->debt * 115) / 100; 
                            randomize_prices();
                            state->current_screen = 0; 
                            
                            if(state->day > TOTAL_DAYS) {
                                state->current_screen = 5; 
                            }
                        }
                    }
                    else if(state->current_screen == 4) { 
                        if(idx == 0 && state->cash >= 500 && state->debt >=
