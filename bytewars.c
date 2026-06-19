#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#define MAX_ITEMS 6
#define MAX_SECTORS 4

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
    int total_days;
    int current_sector;
    int inventory[MAX_ITEMS];
    int selected_menu_index;
    int current_screen; // 0: HUD, 1: Buy, 2: Sell, 3: Travel, 4: Bank, 5: GameOver, 6: Start, 7: Mugger, 8: Police, 9: WeaponShop
    int selected_sub_index;
    int health;
    int weapon_level;   // 0: None, 1: Plasma Blade, 2: Laser Rifle
    int barter_chips;   // 0 to 3 chips (10% discount per chip on purchases)
} GameState;

static const char* sectors[MAX_SECTORS] = {
    "Neon District", "The Core", "Slum Sector", "Orbital Dock"
};

static MarketItem market[MAX_ITEMS] = {
    {"Synth-Weed",     15,   15},
    {"Speed",          60,   60},
    {"Acid",           150,  150},
    {"Cocaine",        400,  400},
    {"Heroin",         850,  850},
    {"Nanite-Juice",   2500, 2500}
};

static void game_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    FuriMessageQueue* event_queue = context;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

void randomize_prices() {
    for(int i = 0; i < MAX_ITEMS; i++) {
        int variance = (market[i].base_price * 45) / 100; 
        market[i].current_price = market[i].base_price + (rand() % (variance * 2)) - variance;
        if(market[i].current_price < 2) market[i].current_price = 2;
    }
}

static void render_callback(Canvas* canvas, void* ctx) {
    GameState* state = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    char buffer[64];

    // Screen 6: Game Mode Title Screen
    if(state->current_screen == 6) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 32, 14, "BYTE WARS");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 26, "Select Game Contract Duration:");
        
        const char* modes[] = {"Classic (30 Days)", "Long (90 Days)", "Long+ (120 Days)"};
        for(int i = 0; i < 3; i++) {
            if(state->selected_menu_index == i) canvas_draw_str(canvas, 10, 39 + (i * 9), ">");
            canvas_draw_str(canvas, 20, 39 + (i * 9), modes[i]);
        }
        return;
    }

    // Screen 5: Game Over Screen
    if (state->current_screen == 5) { 
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 30, 15, "GAME OVER");
        canvas_set_font(canvas, FontSecondary);
        snprintf(buffer, sizeof(buffer), "Final Cash: $%d | HP: %d%%", state->cash, state->health);
        canvas_draw_str(canvas, 10, 30, buffer);
        int net = state->cash - state->debt + state->bank;
        snprintf(buffer, sizeof(buffer), "Net Worth Score: $%d", net);
        canvas_draw_str(canvas, 10, 43, buffer);
        canvas_draw_str(canvas, 15, 58, "Press OK to Boot Menu");
        return;
    }

    // Screen 7: Mugger Encounter Screen
    if(state->current_screen == 7) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "!!! AMBUSH !!!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "Syndicate thugs demand a cut!");
        
        const char* choices[] = {"1. Fight Back", "2. Run Away"};
        for(int i = 0; i < 2; i++) {
            if(state->selected_sub_index == i) canvas_draw_str(canvas, 10, 38 + (i * 10), ">");
            canvas_draw_str(canvas, 22, 38 + (i * 10), choices[i]);
        }
        return;
    }

    // Screen 8: Police Encounter Screen
    if(state->current_screen == 8) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "!!! NETSEC PATROL !!!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "Scanning cargo bay locks...");
        
        const char* choices[] = {"1. Fight Scanners", "2. Punch Throttle", "3. Surrender Cargo"};
        for(int i = 0; i < 3; i++) {
            if(state->selected_sub_index == i) canvas_draw_str(canvas, 10, 36 + (i * 9), ">");
            canvas_draw_str(canvas, 22, 36 + (i * 9), choices[i]);
        }
        return;
    }

    // Screen 9: Weapon & Barter Armory Shop
    if(state->current_screen == 9) {
        canvas_draw_str(canvas, 2, 20, "BLACK MARKET ARMORY:");
        
        // Dynamic labels based on what player owns
        const char* w_name = (state->weapon_level == 0) ? "Plasma Blade ($1k)" : 
                             (state->weapon_level == 1) ? "Laser Rifle ($3.5k)" : "Max Weapons Owned";
        snprintf(buffer, sizeof(buffer), "Barter Chip ($1.5k) [%d/3]", state->barter_chips);

        if(state->selected_sub_index == 0) canvas_draw_str(canvas, 2, 32, ">");
        canvas_draw_str(canvas, 12, 32, w_name);

        if(state->selected_sub_index == 1) canvas_draw_str(canvas, 2, 45, ">");
        canvas_draw_str(canvas, 12, 45, buffer);
        
        snprintf(buffer, sizeof(buffer), "Current Gear: Wpn Lvl %d | Buff: -%d%%", state->weapon_level, state->barter_chips * 10);
        canvas_draw_str(canvas, 2, 59, buffer);
    }

    // Global Top Status Bar (Only on actionable UI states)
    if(state->current_screen <= 4 || state->current_screen == 9) {
        snprintf(buffer, sizeof(buffer), "D:%d/%d | $%d | HP:%d%%", state->day, state->total_days, state->cash, state->health);
        canvas_draw_str(canvas, 2, 10, buffer);
        canvas_draw_line(canvas, 0, 12, 128, 12);
    }

    // Screen 0: Main HUD
    if(state->current_screen == 0) {
        snprintf(buffer, sizeof(buffer), "Loc: %s | Debt: $%d", sectors[state->current_sector], state->debt);
        canvas_draw_str(canvas, 2, 22, buffer);

        const char* menu_opts[] = {"Buy Stash", "Sell Stash", "Jump Sector", "Loan Shark", "Black Market Armory"};
        for(int i = 0; i < 5; i++) {
            int y_pos = 33 + (i * 7);
            if(state->selected_menu_index == i) canvas_draw_str(canvas, 2, y_pos, ">");
            canvas_draw_str(canvas, 10, y_pos, menu_opts[i]);
        }
    }
    // Screen 1 & 2: Buy / Sell Market
    else if(state->current_screen == 1 || state->current_screen == 2) {
        canvas_draw_str(canvas, 2, 20, state->current_screen == 1 ? "BUY (BUFFED PRICE):" : "SELL COMMODITY:");
        for(int i = 0; i < MAX_ITEMS; i++) {
            int row = i % 3;
            int col = i / 3;
            int x = col * 66;
            int y = 30 + (row * 11);
            
            int price = market[i].current_price;
            if(state->current_screen == 1) { // Render price with barter reductions applied
                price = price - ((price * (state->barter_chips * 10)) / 100);
            }

            if(state->selected_sub_index == i) canvas_draw_str(canvas, x + 1, y, ">");
            snprintf(buffer, sizeof(buffer), "%s:$%d(%d)", market[i].name, price, state->inventory[i]);
            canvas_draw_str(canvas, x + 8, y, buffer);
        }
    }
    // Screen 3: Travel Selection
    else if(state->current_screen == 3) {
        canvas_draw_str(canvas, 2, 21, "JUMP COORD DESTINATION:");
        for(int i = 0; i < MAX_SECTORS; i++) {
            if(state->selected_sub_index == i) canvas_draw_str(canvas, 2, 31 + (i * 8), ">");
            snprintf(buffer, sizeof(buffer), "%s %s", sectors[i], (state->current_sector == i) ? "(Active)" : "");
            canvas_draw_str(canvas, 10, 31 + (i * 8), buffer);
        }
    }
    // Screen 4: Bank / Loan Shark Terminal
    else if(state->current_screen == 4) {
        canvas_draw_str(canvas, 2, 22, "UNDERGROUND FINANCIAL TERMINAL");
        snprintf(buffer, sizeof(buffer), "Credit Secure Account: $%d", state->bank);
        canvas_draw_str(canvas, 10, 33, buffer);
        
        const char* bank_opts[] = {"Pay $500 Debt", "Deposit $500 Secure", "Withdraw $500"};
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
    state->total_days = 30;
    state->current_sector = 0;
    state->current_screen = 6; 
    state->selected_menu_index = 0;
    state->selected_sub_index = 0;
    state->health = 100;
    state->weapon_level = 0;
    state->barter_chips = 0;
    memset(state->inventory, 0, sizeof(state->inventory));
    srand(furi_get_tick());
    randomize_prices();

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, state);
    view_port_input_callback_set(view_port, game_input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    while(1) {
        if(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type != InputTypePress) continue;

            if(event.key == InputKeyBack) {
                if(state->current_screen == 6 || state->current_screen == 0) {
                    break; 
                } else if(state->current_screen == 7 || state->current_screen == 8) {
                    continue; 
                } else {
                    state->current_screen = 0;
                    view_port_update(view_port);
                    continue;
                }
            }

            if(state->current_screen == 6) {
                if(event.key == InputKeyUp) state->selected_menu_index = (state->selected_menu_index - 1 + 3) % 3;
                if(event.key == InputKeyDown) state->selected_menu_index = (state->selected_menu_index + 1) % 3;
                if(event.key == InputKeyOk) {
                    if(state->selected_menu_index == 0) state->total_days = 30;
                    if(state->selected_menu_index == 1) state->total_days = 90;
                    if(state->selected_menu_index == 2) state->total_days = 120;
                    state->current_screen = 0;
                    state->selected_menu_index = 0;
                }
                view_port_update(view_port);
                continue;
            }

            if(state->current_screen == 5) { 
                if(event.key == InputKeyOk) {
                    state->cash = 1000;
                    state->debt = 2000;
                    state->bank = 0;
                    state->day = 1;
                    state->health = 100;
                    state->weapon_level = 0;
                    state->barter_chips = 0;
                    state->current_sector = 0;
                    state->current_screen = 6; 
                    memset(state->inventory, 0, sizeof(state->inventory));
                    randomize_prices();
                }
                view_port_update(view_port);
                continue;
            }

            if(state->current_screen == 7) { // Muggers
                if(event.key == InputKeyUp || event.key == InputKeyDown) state->selected_sub_index = (state->selected_sub_index + 1) % 2;
                if(event.key == InputKeyOk) {
                    if(state->selected_sub_index == 0) { // Fight back!
                        // Weapons drop your odds of losing fights significantly
                        int fail_chance = (state->weapon_level == 0) ? 50 : (state->weapon_level == 1) ? 25 : 10;
                        if((rand() % 100) < fail_chance) { 
                            state->health -= 25;
                            state->cash /= 2; 
                        } else {
                            state->cash += 400; // Total victory payout
                        }
                    } else { // Run Away!
                        int dropped_idx = rand() % MAX_ITEMS;
                        if(state->inventory[dropped_idx] > 0) state->inventory[dropped_idx]--; 
                        state->health -= 5;
                    }
                    state->current_screen = 0;
                    if(state->health <= 0) state->current_screen = 5;
                }
                view_port_update(view_port);
                continue;
            }

            if(state->current_screen == 8) { // Police Intercept
                if(event.key == InputKeyUp) state->selected_sub_index = (state->selected_sub_index - 1 + 3) % 3;
                if(event.key == InputKeyDown) state->selected_sub_index = (state->selected_sub_index + 1) % 3;
                if(event.key == InputKeyOk) {
                    if(state->selected_sub_index == 0) { // Fight Scanners
                        int fail_chance = (state->weapon_level == 0) ? 70 : (state->weapon_level == 1) ? 40 : 15;
                        if((rand() % 100) < fail_chance) { 
                            state->health -= 40;
                            state->cash = 0; 
                        }
                    } else if(state->selected_sub_index == 1) { // Flight/Throttle Evasion
                        if(rand() % 2 == 0) {
                            state->health -= 15; 
                        }
                    } else { // Surrender Cargo
                        int total_contraband = 0;
                        for(int i = 0; i < MAX_ITEMS; i++) {
                            total_contraband += state->inventory[i];
                            state->inventory[i] = 0; 
                        }
                        state->cash -= (total_contraband * 100);
                        if(state->cash < 0) state->cash = 0;
                        if(total_contraband > 5) {
                            state->day += 3; 
                        }
                    }
                    state->current_screen = 0;
                    if(state->health <= 0 || state->day > state->total_days) state->current_screen = 5;
                }
                view_port_update(view_port);
                continue;
            }

            // Screen 9: Black Market Weapon and Barter Upgrades Logic Loop
            if(state->current_screen == 9) {
                if(event.key == InputKeyUp || event.key == InputKeyDown) state->selected_sub_index = (state->selected_sub_index + 1) % 2;
                if(event.key == InputKeyOk) {
                    if(state->selected_sub_index == 0) { // Purchase Weapon Upgrade branch
                        if(state->weapon_level == 0 && state->cash >= 1000) {
                            state->cash -= 1000;
                            state->weapon_level = 1; // Plasma Blade unlocked
                        } else if(state->weapon_level == 1 && state->cash >= 3500) {
                            state->cash -= 3500;
                            state->weapon_level = 2; // Laser Rifle unlocked
                        }
                    } else if(state->selected_sub_index == 1) { // Purchase Barter Buff Chip branch
                        if(state->barter_chips < 3 && state->cash >= 1500) {
                            state->cash -= 1500;
                            state->barter_chips++;
                        }
                    }
                }
                view_port_update(view_port);
                continue;
            }

            // Screen 0 HUD Options Controller Update
            if(state->current_screen == 0) { 
                if(event.key == InputKeyUp) state->selected_menu_index = (state->selected_menu_index - 1 + 5) % 5;
                if(event.key == InputKeyDown) state->selected_menu_index = (state->selected_menu_index + 1) % 5;
                if(event.key == InputKeyOk) {
                    state->current_screen = state->selected_menu_index + 1;
                    state->selected_sub_index = 0;
                }
            } 
            else { 
                int max_index = (state->current_screen == 4) ? 3 : MAX_ITEMS; 
                if(event.key == InputKeyUp) state->selected_sub_index = (state->selected_sub_index - 1 + max_index) % max_index;
                if(event.key == InputKeyDown) state->selected_sub_index = (state->selected_sub_index + 1) % max_index;

                if(event.key == InputKeyOk) {
                    int idx = state->selected_sub_index;
                    if(state->current_screen == 1) { // Buy Module with stacking barter percentage reductions
                        int adjusted_cost = market[idx].current_price - ((market[idx].current_price * (state->barter_chips * 10)) / 100);
                        if(state->cash >= adjusted_cost) {
                            state->cash -= adjusted_cost;
                            state->inventory[idx]++;
                            
                            if(rand() % 100 < 15) {
                                state->current_screen = 7;
                                state->selected_sub_index = 0;
                            }
                        }
                    } 
                    else if(state->current_screen == 2) { // Sell Stash item routine module path
                        if(state->inventory[idx] > 0) {
                            state->cash += market[idx].current_price;
                            state->inventory[idx]--;
                        }
                    } 
                    else if(state->current_screen == 3) { // Travel
                        if(state->current_sector != idx) {
                            state->current_sector = idx;
                            state->day++;
                            state->debt = (state->debt * 115) / 100; 
                            randomize_prices();
                            state->current_screen = 0; 
                            
                            if(rand() % 100 < 20) {
                                state->current_screen = 8;
                                state->selected_sub_index = 0;
                            }
                            
                            if(state->day > state->total_days) {
                                state->current_screen = 5; 
                            }
                        }
                    }
                    else if(state->current_screen == 4) { // Secure Financial Bank Link
                        if(idx == 0 && state->cash >= 500 && state->debt >= 500) {
                            state->cash -= 500;
                            state->debt -= 500;
                        } else if(idx == 1 && state->cash >= 500) {
                            state->cash -= 500;
                            state->bank += 500;
                        } else if(idx == 2 && state->bank >= 500) {
                            state->bank -= 500;
                            state->cash += 500;
                        }
                    }
                }
            }
            view_port_update(view_port);
        }
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    free(state);

    return 0;
}
