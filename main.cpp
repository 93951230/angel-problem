// ===preprocessor===
#pragma region 
#include <bits/stdc++.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_ttf.h>


#define WINDOW_W 1500
#define WINDOW_H 1200
#define FPS 60
#define TILE_SIZE 40

#define TILE_EMPTY 0
#define TILE_WALL  1
#define TILE_GOAL  2
#define TILE_PLAYER 3
#define TILE_VOID 4

#define TILE_SIGN 6
// GATE
#define TILE_GATE_N 7
#define TILE_GATE_S 8
#define TILE_GATE_E 9
#define TILE_GATE_W 10
// Moving Tile
#define TILE_MOVE_H 11
#define TILE_MOVE_V 12
#define TILE_MOVE_REV_H 13  
#define TILE_MOVE_REV_V 14

std::vector<int> is_space = {0,2,3,6};

#pragma endregion 

// ===global variables===
#pragma region 

// Define Game States
enum GameState {
    MENU,
    LEVEL_SELECTING,
    PLAYING,
    READING_SIGN
};
unsigned char key[ALLEGRO_KEY_MAX];

int info_font_height;
ALLEGRO_FONT* info_font = NULL;
int large_info_font_height;
ALLEGRO_FONT* large_info_font = NULL;
int title_font_height;
ALLEGRO_FONT* title_font = NULL;

ALLEGRO_SAMPLE* move_sound;
ALLEGRO_SAMPLE* change_level_sound;

#pragma endregion

// ===helpers===
#pragma region

void must_init(bool test, const char *desc) {
    if (test) return;
    fprintf(stderr,"[Error] Couldn't initialize %s\n", desc);
    exit(1);
}

void al_draw_text_bg_center (
    ALLEGRO_FONT* font,
    ALLEGRO_COLOR text_color,
    ALLEGRO_COLOR bg_color,
    double cx, double cy,
    const char* text,
    double padding = 4.0f
) {
    int tx, ty, tw, th;
    al_get_text_dimensions(font, text, &tx, &ty, &tw, &th);

    // Text top-left so that bounding box is centered at (cx, cy)
    double text_x = cx - (tw / 2.0f) - tx;
    double text_y = cy - (th / 2.0f) - ty;

    // Background rectangle
    double bx = cx - tw / 2.0f - padding;
    double by = cy - th / 2.0f - padding;
    double bw = tw + 2 * padding;
    double bh = th + 2 * padding;

    // Optional: snap to pixel grid for crisp rendering
    bx = floorf(bx);
    by = floorf(by);
    text_x = floorf(text_x);
    text_y = floorf(text_y);

    al_draw_filled_rectangle(bx, by, bx + bw, by + bh, bg_color);
    al_draw_text(font, text_color, text_x, text_y, 0, text);
}


struct Vec2{
    double x, y;
    Vec2(): x(0), y(0) {}
    Vec2(double _x, double _y): x(_x), y(_y) {}
    bool operator==(const Vec2& v) const { return x == v.x && y == v.y; }
    double dist(const Vec2& v) const { return sqrt(pow(x - v.x, 2) + pow(y - v.y, 2)); }
    Vec2 operator-() const {return Vec2(-x, -y);}
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2 operator*(double s) const { return Vec2(x * s, y * s); }
    Vec2 operator/(double s) const { return Vec2(x / s, y / s); }
    Vec2& operator*=(double s) { x *= s; y *= s; return *this; }
    Vec2& operator/=(double s) { x /= s; y /= s; return *this; }
};
#pragma endregion

// ===game classes===
#pragma region

struct Player {
    Vec2 grid_pos;
    Vec2 selected_pos;
    Vec2 selecting_pos;
    int power;
    std::vector<std::vector<bool>> validmpp;
    Player(): grid_pos(0,0), selected_pos(0,0), power(3) {} 
    bool is_valid_move(int x, int y) const {
        return grid_pos.dist(Vec2(x,y)) <= power;
    }
};

struct Level {
    double scale = 1;
    double wildness = 0.9;
    Vec2 deviation;
    int width, height;
    std::vector<std::vector<int>> grid;
    std::vector<std::vector<int>> movdir;
    std::vector<std::vector<double>> anim_since;

    std::string sign_title = "NOTICE";
    std::string sign_body = "...";
    std::string next;
    
    Level() : width(0), height(0) {}

    void load_level(const char* filename, Player& player) {
        scale = 1;
        wildness = 0.9;
        next = "";
        player.power = 3;
        sign_title = "NOTICE";
        sign_body = "No message.";
        std::ifstream file(filename);
        if (!file.is_open()) {
            fprintf(stderr, "Error: Could not open file %s\n", filename);
            exit(1);
        }
        std::string input_hint;
        while (file >> input_hint) {
            if (input_hint.size() == 1 && 10 <= input_hint[0] && input_hint[0] <= 15) continue;
            if (input_hint == "tilemap") {
                file >> width >> height;

                grid.assign(width, std::vector<int>(height, 0));
                player.validmpp.assign(width, std::vector<bool>(height, 0));
                movdir.assign(width, std::vector<int>(height, 0));
                anim_since.assign(width, std::vector<double>(height, 0));

                int tile_id;
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        if (file >> tile_id) {
                            grid[x][y] = tile_id;
                            if (tile_id == TILE_PLAYER) {
                                player.grid_pos = Vec2(x, y);
                                player.selected_pos = Vec2(x, y);
                            }
                            else if (tile_id == TILE_MOVE_H || tile_id == TILE_MOVE_V) {
                                movdir[x][y] = 1;
                            } 
                            else if (tile_id == TILE_MOVE_REV_H || tile_id == TILE_MOVE_REV_V) {
                                movdir[x][y] = -1;
                                grid[x][y] -= 2;
                            } 
                        }
                    }
                }
            }
            else if (input_hint == "scale") file >> scale;
            else if (input_hint == "power") file >> player.power;
            else if (input_hint == "wildness") file >> wildness;
            else if (input_hint == "msg_title") {
                file >> sign_title;
                std::replace(sign_title.begin(), sign_title.end(), '_', ' ');
            }
            else if (input_hint == "msg_body") {
                file >> sign_body;
                std::replace(sign_body.begin(), sign_body.end(), '_', ' ');
            }
            else if (input_hint == "next") file >> next;
            else if (input_hint == "endl") break;
            else {
                fprintf(stderr, "Error: Invalid file format\n");
                exit(1);
            }
        }
        file.close();

        printf("Level loaded: %d x %d, name: %s\n", width, height, filename);

        deviation = Vec2((WINDOW_W-(scale*width*TILE_SIZE))/2, (WINDOW_H-(scale*height*TILE_SIZE))/2);
    }

    bool is_valid_move(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        if (find(is_space.begin(),is_space.end(),grid[x][y]) == is_space.end()) return false;
        return true;
    }

    void update_movers(const Player &player) {
        static int next_grid[500][500];
        static int next_movdir[500][500];

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                next_grid[x][y] = grid[x][y];
                next_movdir[x][y] = 0;
            }
        }

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                if (!movdir[x][y]) continue;

                int t = grid[x][y];
                int dx = 0, dy = 0;

                if (t == TILE_MOVE_H) dx = movdir[x][y];
                else if (t == TILE_MOVE_V) dy = movdir[x][y];
                else continue;

                int new_x = x + dx;
                int new_y = y + dy;

                bool blocked = false;

                if (!is_valid_move(new_x, new_y) || next_movdir[new_x][new_y])
                    blocked = true;

                if (new_x == (int)player.grid_pos.x &&
                    new_y == (int)player.grid_pos.y)
                    blocked = true;

                if (blocked) {
                    next_grid[x][y] = t;
                    next_movdir[x][y] = -movdir[x][y];
                } 
                else {
                    next_grid[new_x][new_y] = t;
                    next_movdir[new_x][new_y] = movdir[x][y];
                    next_grid[x][y] = TILE_EMPTY;
                }
            }
        }

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                grid[x][y] = next_grid[x][y];
                movdir[x][y] = next_movdir[x][y];
            }
        }
    }

    void grow_walls(const Player& p) {
        std::vector<Vec2> new_walls;
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                if (grid[x][y] != TILE_WALL) continue;
                if ((rand() % 100) >= 100*wildness) continue; // chance!!
                int nx = x + (rand() % 3) - 1;
                int ny = y + (rand() % 3) - 1;
                if (!(nx >= 0 && nx < width && ny >= 0 && ny < height)) continue;
                if (nx == p.grid_pos.x && ny == p.grid_pos.y) continue;
                if (!(grid[nx][ny] == TILE_EMPTY && grid[nx][ny] != TILE_GOAL)) continue;
                new_walls.emplace_back(Vec2(nx, ny));
            }
        }
        for (auto& w : new_walls) {
            anim_since[w.x][w.y] = al_get_time();
            grid[w.x][w.y] = TILE_WALL;
        }
    }

    // return the affine map that makes tilemap centered
    Vec2 affine(Vec2 world) const {
        return {
            world.x * scale + deviation.x,
            world.y * scale + deviation.y
        };
    }

    // for mouse cursor detection
    Vec2 invaffine(Vec2 aff) const {
        return {
            (aff.x - deviation.x) / scale,
            (aff.y - deviation.y) / scale
        };
    }

    void trigger_gate(Player& p) {
        for (int x = 0; x < width; x++){
            for (int y = 0; y < height; y++){
                int t = grid[x][y];

                // skip if not gates
                if (!(7 <= t && t <= 10)) continue;
                bool isnot = (13 <= t && t <= 16);
                if (isnot) t -= 6;
                int dx = 0, dy = 0; // front dir
                int bx = 0, by = 0; // back dir
                if (t == TILE_GATE_N) { dy = -1; by = 1;}
                else if (t == TILE_GATE_S) { dy = 1; by = -1;}
                else if (t == TILE_GATE_E) { dx = 1; bx = -1;}
                else if (t == TILE_GATE_W) { dx = -1; bx = 1;}

                // pos of tile to copy from
                int to_copy_x = x + bx;
                int to_copy_y = y + by;

                if (to_copy_x < 0 || to_copy_x >= width || to_copy_y < 0 || to_copy_y >= height) continue;
                int tile_to_copy = grid[to_copy_x][to_copy_y];
                if ((tile_to_copy >= 7 && tile_to_copy <= 10) || tile_to_copy == 4) continue;
                int cur_x = x + dx;
                int cur_y = y + dy;
                double tt = 0;
                while(cur_x >= 0 && cur_x < width && cur_y >= 0 && cur_y < height) {
                    tt += 0.05;
                    if (grid[cur_x][cur_y] == tile_to_copy || (cur_x == p.grid_pos.x && cur_y == p.grid_pos.y)) break;
                    grid[cur_x][cur_y] = tile_to_copy;
                    if (movdir[cur_x][cur_y]) movdir[cur_x][cur_y] = 0;
                    if (11 <= tile_to_copy && tile_to_copy <= 12) movdir[cur_x][cur_y] = movdir[to_copy_x][to_copy_y];
                    anim_since[cur_x][cur_y] = al_get_time() + tt;
                    cur_x += dx;
                    cur_y += dy;
                }
            }
        }
    }

    // the majesty draw function
    void draw(const Player& p) const {
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                if (grid[x][y] == TILE_VOID) continue;
                
                static auto pop = [&](double t) -> double {
                    return (50.0f/21.0f)*t*t*t
                        - (121.0f/21.0f)*t*t
                        + (92.0f/21.0f)*t;
                };
                double factor = std::clamp(pop(al_get_time() - anim_since[x][y]),0.,1.);

                Vec2 world = {(double)x * TILE_SIZE,(double)y * TILE_SIZE};
                double gap = 0.04;
                Vec2 screen1 = affine(Vec2((x+gap) * TILE_SIZE, (y+gap) * TILE_SIZE));
                Vec2 screen2 = affine(Vec2((x+(1-gap)) * TILE_SIZE, (y+(1-gap)) * TILE_SIZE));

                Vec2 center = (screen1 + screen2) * 0.5;
                screen1 = center + (screen1 - center) * factor;
                screen2 = center + (screen2 - center) * factor;


                ALLEGRO_COLOR color = al_map_rgb(50, 50, 50);
                if (grid[x][y] == TILE_WALL) {
                    color = al_map_rgb(255,118,119);
                } 
                else if (grid[x][y] == TILE_GOAL) {
                    color = al_map_rgb(1,178,226);
                }
                else if (grid[x][y] == TILE_SIGN) {
                    color = al_map_rgb(255, 255, 0);
                }
                else if (grid[x][y] >= TILE_GATE_N && grid[x][y] <= TILE_GATE_W) { 
                    color = al_map_rgb(170, 150, 226);
                }
                else if (grid[x][y] == TILE_MOVE_H) {
                    color = al_map_rgb(255, 165, 0);
                } 
                else if (grid[x][y] == TILE_MOVE_V) {
                    color = al_map_rgb(50, 205, 50);
                }

                al_draw_filled_rectangle(
                    screen1.x,
                    screen1.y,
                    screen2.x,
                    screen2.y,
                    color
                );

                if (grid[x][y] >= TILE_GATE_N && grid[x][y] <= TILE_GATE_W) {

                    Vec2 tl = screen1;
                    Vec2 tr(screen2.x, screen1.y);
                    Vec2 bl(screen1.x, screen2.y);
                    Vec2 br = screen2;

                    Vec2 center = (screen1 + screen2) * 0.5;
                    double off = (screen2.x - screen1.x) / 3.0;

                    Vec2 dir(0,0);
                    Vec2 e1, e2;

                    switch (grid[x][y]) {
                        case TILE_GATE_N:
                            dir = Vec2(0,-1);
                            e1 = tl; e2 = tr;
                            break;
                        case TILE_GATE_S:
                            dir = Vec2(0, 1);
                            e1 = bl; e2 = br;
                            break;
                        case TILE_GATE_W:
                            dir = Vec2(-1,0);
                            e1 = tl; e2 = bl;
                            break;
                        case TILE_GATE_E:
                            dir = Vec2(1, 0);
                            e1 = tr; e2 = br;
                            break;
                    }

                    Vec2 p = center + dir * off;

                    // fill darker
                    Vec2 re1 = e1 - dir * (2 * off);
                    Vec2 re2= e2 - dir * (2 * off);
                    double left   = std::min({e1.x, e2.x, re1.x, re2.x});
                    double right  = std::max({e1.x, e2.x, re1.x, re2.x});
                    double top    = std::min({e1.y, e2.y, re1.y, re2.y});
                    double bottom = std::max({e1.y, e2.y, re1.y, re2.y});
                    al_draw_filled_rectangle(
                        left, top,
                        right, bottom,
                        al_map_rgb(150, 130, 196)
                    );

                    // carve the notch
                    al_draw_filled_triangle(
                        e1.x, e1.y,
                        e2.x, e2.y,
                        p.x,  p.y,
                        al_map_rgb(20, 20, 20)
                    );
                }
                else if (11 <= grid[x][y] && grid[x][y] <= 12) {
                    double edge_thickness = 0.12 * (screen2.x - screen1.x);
                    ALLEGRO_COLOR edge_dark = al_map_rgb(120, 90, 40);   // for H
                    ALLEGRO_COLOR edge_dark_v = al_map_rgb(30, 140, 30); // for V

                    if (grid[x][y] == TILE_MOVE_H) {
                        color = al_map_rgb(255, 165, 0);
                        al_draw_filled_rectangle(
                            screen1.x,
                            screen1.y,
                            screen2.x,
                            screen1.y + edge_thickness,
                            edge_dark
                        );

                        al_draw_filled_rectangle(
                            screen1.x,
                            screen2.y - edge_thickness,
                            screen2.x,
                            screen2.y,
                            edge_dark
                        );
                    } 
                    else if (grid[x][y] == TILE_MOVE_V) {
                        color = al_map_rgb(50, 205, 50);
                        al_draw_filled_rectangle(
                            screen1.x,
                            screen1.y,
                            screen1.x + edge_thickness,
                            screen2.y,
                            edge_dark_v
                        );

                        al_draw_filled_rectangle(
                            screen2.x - edge_thickness,
                            screen1.y,
                            screen2.x,
                            screen2.y,
                            edge_dark_v
                        );
                    }
                }
            }
        }

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                if (grid[x][y] == TILE_VOID) continue;

                Vec2 world = {(double)x * TILE_SIZE,(double)y * TILE_SIZE};
                Vec2 screen1 = affine(Vec2((double)x * TILE_SIZE, (double)y * TILE_SIZE));
                Vec2 screen2 = affine(Vec2((double)(x+1) * TILE_SIZE, (double)(y+1) * TILE_SIZE));

                if (p.selected_pos.x == x && p.selected_pos.y == y) {
                    al_draw_rectangle(
                        screen1.x,
                        screen1.y,
                        screen2.x,
                        screen2.y,
                        al_map_rgb(255,232,105),
                        3 * scale
                    );
                }
                else if (p.selecting_pos.x == x && p.selecting_pos.y == y) {
                    double t = al_get_time();
                    double offset = abs(fmod(t,M_PI));
                    double fac = 0.4 + 0.3 * sin(offset);
                    double drop = 5;

                    ALLEGRO_COLOR temp =  al_map_rgb(fac*255,fac*232,fac*105);
                    if (!p.is_valid_move(x,y) || !is_valid_move(x,y)) temp = al_map_rgb(fac*255/drop,fac*232/drop,fac*105/drop);
                    al_draw_rectangle(
                        screen1.x,
                        screen1.y,
                        screen2.x,
                        screen2.y,
                        temp,
                        3 * scale
                    );
                }
            }
        }

        // ---draw player---
        #pragma region

        // Player center in world space
        Vec2 player_world = {
            p.grid_pos.x * TILE_SIZE + TILE_SIZE / 2.0,
            p.grid_pos.y * TILE_SIZE + TILE_SIZE / 2.0
        };

        Vec2 player_screen = affine(player_world);

        double r = (TILE_SIZE / 4) * scale;

        double bar_w = 30 * scale;
        double bar_h = 4 * scale;
        double y_off = -0.3 * r;

        // --- player ---
        al_draw_filled_circle(
            player_screen.x,
            player_screen.y,
            r,
            al_map_rgb(255, 255, 255)
        );

        // glow (larger, softer)
        al_draw_filled_rectangle(
            player_screen.x - bar_w / 2,
            player_screen.y + y_off - bar_h * 1.3,
            player_screen.x + bar_w / 2,
            player_screen.y + y_off + bar_h / 2,
            al_map_rgba(80, 140, 255, 60)
        );
        // core bar
        al_draw_filled_rectangle(
            player_screen.x - bar_w / 2,
            player_screen.y + y_off - bar_h / 2,
            player_screen.x + bar_w / 2,
            player_screen.y + y_off + bar_h / 2,
            al_map_rgba(90, 150, 255, 200)
        );

        #pragma endregion
    }
};

#pragma endregion

// ===animation===
#pragma region

double lastAnimTime = 0.0;
bool in_transition = false;
bool map_swapped   = false;
const double COVER_TIME = 0.35; 
const double HOLD_TIME = 0.10;
const double UNCOVER_TIME = 0.35;
bool draw_black_swipe(double lastAnimTime) {
    double t = al_get_time() - lastAnimTime;

    if (t >= COVER_TIME + HOLD_TIME + UNCOVER_TIME)
        return false; 

    double left = 0.0;
    double right = 0.0;

    if (t < COVER_TIME) {
        double u = t / COVER_TIME; 
        left  = 0.0;
        right = WINDOW_W * u;
    }
    else if (t < COVER_TIME + HOLD_TIME) {
        left  = 0.0;
        right = WINDOW_W;
    }
    else {
        double u = (t - COVER_TIME - HOLD_TIME) / UNCOVER_TIME; 
        left  = WINDOW_W * u;
        right = WINDOW_W;
    }

    al_draw_filled_rectangle(
        left, 0,
        right, WINDOW_H,
        al_map_rgb(0, 0, 0)
    );

    return true;
}


#pragma endregion

// ===main===
#pragma region
int main(int, char**) {
    // ---init assets for allergo 5--- 
    #pragma region

    must_init(al_init(), "allegro");
    must_init(al_install_keyboard(), "keyboard");
    must_init(al_install_mouse(), "mouse");
    must_init(al_init_primitives_addon(), "primitives");
    must_init(al_init_font_addon(), "font");
    must_init(al_init_ttf_addon(), "ttf");

    auto timer = al_create_timer(1.0 / FPS);
    auto queue = al_create_event_queue();
    auto disp = al_create_display(WINDOW_W, WINDOW_H);
    
    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_mouse_event_source());
    al_register_event_source(queue, al_get_display_event_source(disp));
    al_register_event_source(queue, al_get_timer_event_source(timer));

    info_font = al_load_font("src/QuinqueFive.ttf", 24, 0);
    must_init(info_font, "info font");
    info_font_height = al_get_font_line_height(info_font);

    large_info_font = al_load_font("src/QuinqueFive.ttf", 32, 0);
    must_init(large_info_font, "info font");
    large_info_font_height = al_get_font_line_height(large_info_font);

    title_font = al_load_font("src/Round9x13.ttf", 96, 0);
    must_init(title_font, "title font");
    title_font_height = al_get_font_line_height(title_font);

    must_init(al_install_audio(),"al_install_audio");
    must_init(al_init_acodec_addon(),"al_init_acodec_addon");
    must_init(al_reserve_samples(8),"al_reserve_samples");

    move_sound = al_load_sample("src/move_sound.wav");
    must_init(move_sound, "move_sound");
    change_level_sound = al_load_sample("src/change_level_sound.wav");
    must_init(change_level_sound, "change_level_sound");

    #pragma endregion

    // ---our OWN variables---
    #pragma region

    srand(time(NULL));
    int deathCnt = 0;
    bool done = false;
    bool redraw = true;
    ALLEGRO_EVENT event;
    

    // 恆存の Game Object & status
    GameState state = MENU;
    Level level;
    Level title_level;
    Player player;
    title_level.load_level("levels/title_level.txt", player);
    std::string curr_filename = "title_level.txt";
    bool retried = false;

    level = title_level;
    bool turn_ready = false;

    al_start_timer(timer);
    memset(key, 0, sizeof(key));

    #pragma endregion

    // ---game loop---
    while (!done) {
        al_wait_for_event(queue, &event);

        if (event.type == ALLEGRO_EVENT_TIMER) {
            redraw = true;
        }
        else if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            done = true;
        }
        else if (event.type == ALLEGRO_EVENT_MOUSE_AXES) {
            // to draw preview
            Vec2 pull_back_mouse = level.invaffine(Vec2(event.mouse.x,event.mouse.y));
            int grid_x = pull_back_mouse.x / TILE_SIZE;
            int grid_y = pull_back_mouse.y / TILE_SIZE;
            player.selecting_pos = Vec2(grid_x,grid_y);
        }
        else if (event.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
            if (state == PLAYING || state == MENU) { 
                // mouse
                Vec2 pull_back_mouse = level.invaffine(Vec2(event.mouse.x,event.mouse.y));
                int grid_x = pull_back_mouse.x / TILE_SIZE;
                int grid_y = pull_back_mouse.y / TILE_SIZE;
        
                if (level.is_valid_move(grid_x, grid_y) && player.is_valid_move(grid_x,grid_y)) {
                    Vec2 target(grid_x, grid_y);
                    player.selected_pos = target;
                    turn_ready = true;
                }
            }
        }
        else if (event.type == ALLEGRO_EVENT_KEY_DOWN) {
            if (event.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                if (state == PLAYING) {
                    state = MENU; // Esc to go back to menu
                    level.load_level("levels/title_level.txt",player);
                }
                else done = true; // Esc at menu to quit
            }
            else if (event.keyboard.keycode == ALLEGRO_KEY_TAB) {
                if (state == PLAYING) {
                    const std::string s = level.next;
                    curr_filename = level.next;
                    if (s.empty() || (s.size() == 1 && 10 <= s[0] && s[0] <= 15)) {
                        level.load_level("levels/title_level.txt",player);
                        state = MENU;
                    }
                    else {
                        char filename[100] = "levels/";
                        for (size_t i=0;i<level.next.size();i++) filename[i+7] = level.next[i];
                        level.load_level(filename, player);
                    }
                }
            }
    
            if ((state == PLAYING || state == MENU) && event.keyboard.keycode == ALLEGRO_KEY_SPACE && turn_ready) {
                al_play_sample(move_sound,1.0,0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);
                player.grid_pos = player.selected_pos;
                turn_ready = false;

                int current_tile = level.grid[(int)player.grid_pos.x][(int)player.grid_pos.y];
                if (state == MENU) {
                    int gx = player.grid_pos.x, gy = player.grid_pos.y;
                    if (gx == 1 && gy == 2) {
                        lastAnimTime = al_get_time();
                        in_transition = true;
                        map_swapped = false;
                    }
                    if (gx == 3 && gy == 2) {
                        done = true;
                        break;
                    }
                }
                else if (state == PLAYING){
                    if (current_tile == TILE_GOAL) {
                        lastAnimTime = al_get_time();
                        in_transition = true;
                        map_swapped = false;
                    } 
                    else if (current_tile == TILE_SIGN){
                        state = READING_SIGN;
                    }
                    else {
                        level.update_movers(player);
                        level.trigger_gate(player);
                        level.grow_walls(player);
                    }
                }
            }
            else if (state == READING_SIGN){
                if (event.keyboard.keycode == ALLEGRO_KEY_ENTER || event.keyboard.keycode == ALLEGRO_KEY_SPACE){
                    state = PLAYING;
                    
                    level.update_movers(player);
                    level.trigger_gate(player);
                    level.grow_walls(player);
                    
                    player.selected_pos = player.grid_pos;
                }
            }

            if (state == PLAYING && event.keyboard.keycode == ALLEGRO_KEY_R) {
                lastAnimTime = al_get_time();
                in_transition = true;
                map_swapped = false;
                retried = true;
                deathCnt++;
            }
        }

        // change level
        if (in_transition) {
            double t = al_get_time() - lastAnimTime;
            if (!map_swapped && t >= COVER_TIME) {

                std::string s;
                if (retried) {
                    retried = false;
                    s = curr_filename;
                }
                else s = level.next;

                if (s.empty() || (s.size() == 1 && 10 <= s[0] && s[0] <= 15)) {
                    level.load_level("levels/title_level.txt",player);
                    state = MENU;
                }
                else {
                    char filename[100] = "levels/";
                    for (size_t i=0;i<level.next.size();i++) filename[i+7] = s[i];
                    curr_filename = s;
                    level.load_level(filename, player);
                }

                map_swapped = true;
                if (state == MENU) state = PLAYING;

                al_play_sample(change_level_sound,1.0,0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);
            }
        }

        if (redraw && al_is_event_queue_empty(queue)) {
            al_clear_to_color(al_map_rgb(0, 0, 0));

            // --- DRAW GAME ---
            if (state == PLAYING || state == READING_SIGN || state == MENU) {
                level.draw(player);

                if (state != MENU) {
                    al_draw_text(info_font, al_map_rgb(255, 255, 255), 10, WINDOW_H - 30, 0, "Press ESC to return to Menu");
                    al_draw_textf(info_font, al_map_rgb(200, 200, 200), 10, 10, ALLEGRO_ALIGN_LEFT, "Death Count : %d", deathCnt);
                }
                if (state == READING_SIGN){
                    double cx = WINDOW_W / 2;
                    double cy = WINDOW_H / 2;
                    al_draw_filled_rectangle(0, cy - 80, WINDOW_W, cy + 110, al_map_rgb(0, 0, 50));
                    al_draw_rectangle(0, cy - 80, WINDOW_W, cy + 110, al_map_rgb(255, 255, 255), 4);
                    al_draw_text(info_font, al_map_rgb(255, 255, 0), cx, cy - info_font_height - 10, ALLEGRO_ALIGN_CENTER, level.sign_title.c_str());
                    al_draw_text(info_font, al_map_rgb(255, 255, 255), cx, cy + 10, ALLEGRO_ALIGN_CENTER, level.sign_body.c_str());
                    al_draw_text(info_font, al_map_rgb(150, 150, 150), cx, cy + 40, ALLEGRO_ALIGN_CENTER, "Press [SPACE] to continue ...");
                }
            }
            // --- DRAW MENU ---
            if (state == MENU) {
                // Draw Title
                const char *parts[] = {
                    "A", " N G E L     ", "P", " R O B L E M"
                };

                ALLEGRO_COLOR blue = al_map_rgba(80,140,255,120);
                ALLEGRO_COLOR red = al_map_rgba(255, 90, 90, 120);
                ALLEGRO_COLOR colors[] = {  
                    blue,
                    al_map_rgb(255, 255, 255),
                    red,
                    al_map_rgb(255, 255, 255)
                };
                
                double title_y = WINDOW_H / 4;
                if (player.grid_pos.y == 0) title_y = WINDOW_H - title_y - title_font_height;

                // total width
                double total_w = 0;
                for (int i = 0; i < 4; i++)
                    total_w += al_get_text_width(title_font, parts[i]);

                double title_x = WINDOW_W / 2 - total_w / 2;

                for (int i = 0; i < 4; i++) {
                    al_draw_text(title_font, colors[i], title_x, title_y, 0, parts[i]);
                    title_x += al_get_text_width(title_font, parts[i]);
                }

                // Draw Instructions (Below Button)
                double inst_y = WINDOW_H / 10;
                if (player.grid_pos.y != 0) {
                    al_draw_text(info_font, al_map_rgb(200, 200, 200), WINDOW_W/2, inst_y + info_font_height, ALLEGRO_ALIGN_CENTER, "Click grey grid to select move");
                    al_draw_text(info_font, al_map_rgb(200, 200, 200), WINDOW_W/2, inst_y + 2*info_font_height, ALLEGRO_ALIGN_CENTER, "Press [SPACE] to Teleport");
                }
                
                // Draw Floor and Ceiling
                double h = 22.0f, off = 8.0f;

                al_draw_filled_rectangle(0,  40, WINDOW_W,  40+h, blue);
                al_draw_filled_rectangle(0,  40+off, WINDOW_W,  40+off+h, blue);

                al_draw_filled_rectangle(0, WINDOW_H-40-h, WINDOW_W, WINDOW_H-40, blue);
                al_draw_filled_rectangle(0, WINDOW_H-40-h-off, WINDOW_W, WINDOW_H-40-off, blue);

                // Draw Option + Credit
                if (player.grid_pos.y != 0) {
                    Vec2 play_opt_pos = level.affine(Vec2(1.5*TILE_SIZE, 2.5*TILE_SIZE));
                    al_draw_text_bg_center(info_font,al_map_rgb(200, 200, 200), al_map_rgb(0, 0, 0), play_opt_pos.x, play_opt_pos.y, "[ Play ]",20);
                    Vec2 exit_opt_pos = level.affine(Vec2(3.5*TILE_SIZE, 2.5*TILE_SIZE));
                    al_draw_text_bg_center(info_font,al_map_rgb(200, 200, 200), al_map_rgb(0, 0, 0), exit_opt_pos.x, exit_opt_pos.y, "[ Exit ]",20);
                }
                else {
                    // 
                    al_draw_text_bg_center(info_font,al_map_rgb(200, 200, 200), al_map_rgb(0, 0, 0), WINDOW_W/2, 5*WINDOW_H/6, "Math 27, Yu Wen Kuang, Zeng Qi Ming presents.",20);
                    al_draw_text_bg_center(info_font,al_map_rgb(200, 200, 200), al_map_rgb(0, 0, 0), WINDOW_W/2, 5*WINDOW_H/6 + 60, "~ Dec 19 2025, IP2 ~",20);
                }
            }

            if (in_transition) {
                if (!draw_black_swipe(lastAnimTime)) {
                    in_transition = false; // animation finished
                }
            }

            al_flip_display();
            redraw = false;
        }
    }

    // ---unnecearily deallocate objects---
    #pragma region

    al_destroy_display(disp);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);

    al_destroy_font(large_info_font);
    al_destroy_font(info_font);
    al_destroy_font(title_font);

    al_destroy_sample(move_sound);
    al_destroy_sample(change_level_sound);

    #pragma endregion

    return 0;
}
#pragma endregion