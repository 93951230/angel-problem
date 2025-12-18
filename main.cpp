// ===preprocessor===
#pragma region 
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_ttf.h>

using namespace std;

#define WINDOW_W 1500
#define WINDOW_H 1200
#define FPS 60
#define TILE_SIZE 40


// Tile Types (Match your level.txt)
#define TILE_EMPTY 0
#define TILE_WALL  1
#define TILE_GOAL  2
#define TILE_PLAYER 3
#define TILE_VOID 4

#define TILE_SIGN 6

#define TILE_GATE_N 7
#define TILE_GATE_S 8
#define TILE_GATE_E 9
#define TILE_GATE_W 10

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
    int move_range;
    std::vector<std::vector<bool>> validmpp;
    Player(): grid_pos(0,0), selected_pos(0,0), move_range(3) {} 
    bool is_valid_move(int x, int y) const {
        return grid_pos.dist(Vec2(x,y)) <= move_range;
    }
};

struct Level {
    // scaling to fit
    double scale = 1;
    double wildness = 0.9;
    Vec2 deviation;
    int width, height;
    std::vector<std::vector<int>> grid;
    std::vector<std::vector<double>> exist_since;
    Vec2 goal_pos;
    
    Level() : width(0), height(0) {}

    void load_level(const char* filename, Player& player) {
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
                exist_since.assign(width, std::vector<double>(height, 0));
                int tile_id;
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        if (file >> tile_id) {
                            grid[x][y] = tile_id;
                            if (tile_id == TILE_GOAL) {
                                goal_pos = Vec2(x, y);
                            } 
                            else if (tile_id == TILE_PLAYER) {
                                player.grid_pos = Vec2(x, y);
                                player.selected_pos = Vec2(x, y);
                            } 
                        }
                    }
                }
            }
            else if (input_hint == "scale") file >> scale;
            else if (input_hint == "wildness") file >> wildness;
            else if (input_hint == "endl") break;
            else {
                fprintf(stderr, "Error: Invalid file format\n");
                exit(1);
            }
        }
        file.close();
        printf("Level loaded: %d x %d\n", width, height);

        deviation = Vec2((WINDOW_W-(scale*width*TILE_SIZE))/2, (WINDOW_H-(scale*height*TILE_SIZE))/2);
    }

    bool is_valid_move(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        if (find(is_space.begin(),is_space.end(),grid[x][y]) == is_space.end()) return false;
        return true;
    }

    void grow_walls(const Player& p) {
        std::vector<Vec2> new_walls;
        for(int x = 0; x < width; x++) {
            for(int y = 0; y < height; y++) {
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
        for(auto& w : new_walls) {
            exist_since[w.x][w.y] = al_get_time();
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

    void draw(const Player& p) const {
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                if (grid[x][y] == TILE_VOID) continue;
                
                static auto pop = [&](double t) -> double {
                    return (50.0f/21.0f)*t*t*t
                        - (121.0f/21.0f)*t*t
                        + (92.0f/21.0f)*t;
                };
                double factor = (al_get_time() - exist_since[x][y] >= 1)?1:pop(al_get_time() - exist_since[x][y]);

                Vec2 world = {(double)x * TILE_SIZE,(double)y * TILE_SIZE};
                double gap = 0.04;
                Vec2 screen1 = affine(Vec2((x+gap) * TILE_SIZE, (y+gap) * TILE_SIZE));
                Vec2 screen2 = affine(Vec2((x+(1-gap)) * TILE_SIZE, (y+(1-gap)) * TILE_SIZE));

                Vec2 center = (screen1 + screen2) * 0.5;
                screen1 = center + (screen1 - center) * factor;
                screen2 = center + (screen2 - center) * factor;


                ALLEGRO_COLOR color = al_map_rgb(20, 20, 20);
                if (grid[x][y] == TILE_EMPTY) {
                    color = al_map_rgb(20, 20, 20);
                }
                if (grid[x][y] == TILE_WALL) {
                    color = al_map_rgb(255,118,119);
                } 
                else if (grid[x][y] == TILE_GOAL) {
                    color = al_map_rgb(1,178,226);
                }
                else if (grid[x][y] == TILE_SIGN){
					color = al_map_rgb(255, 255, 0);
				}


                al_draw_filled_rectangle(
                    screen1.x,
                    screen1.y,
                    screen2.x,
                    screen2.y,
                    color
                );
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

    #pragma endregion

    // ---our OWN variables---
    #pragma region

    srand(time(NULL)); // for randomness
    bool done = false;
    bool redraw = true;
    ALLEGRO_EVENT event;
    

    // 恆存の Game Object & status
    GameState state = MENU;
    Level level;
    Level title_level;
    Player player;
    title_level.load_level("title_level.txt", player);

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
                    level.load_level("title_level.txt",player);
                }
                else done = true; // Esc at menu to quit
            }
            
            if ((state == PLAYING || state == MENU) && event.keyboard.keycode == ALLEGRO_KEY_SPACE && turn_ready) {
                al_play_sample(move_sound,1.0,0,1.0,ALLEGRO_PLAYMODE_ONCE,NULL);
                player.grid_pos = player.selected_pos;
                turn_ready = false;

                int current_tile = level.grid[(int)player.grid_pos.x][(int)player.grid_pos.y];
                if (state == MENU) {
                    int gx = player.grid_pos.x, gy = player.grid_pos.y;
                    if (gx == 1 && gy == 2) {
                        state = PLAYING;
                        level.load_level("level.txt",player);
                    }
                    if (gx == 3 && gy == 2) {
                        done = true;
                        break;
                    }
                }
                else {
                    if (current_tile == TILE_GOAL) {
                        state = MENU;
                        level.load_level("title_level.txt", player);
                    } 
                    else if(current_tile == TILE_SIGN){
                        state = READING_SIGN;
                    }
                    else {
                        level.grow_walls(player);
                    }
                }
            }
            else if (state == READING_SIGN){
                if(event.keyboard.keycode == ALLEGRO_KEY_ENTER || event.keyboard.keycode == ALLEGRO_KEY_SPACE){
                    state = PLAYING;
                    
                    level.grow_walls(player);
                    player.selected_pos = player.grid_pos;
                }
            }
        }

        if (redraw && al_is_event_queue_empty(queue)) {
            al_clear_to_color(al_map_rgb(0, 0, 0));

            // --- DRAW GAME ---
            if(state == PLAYING || state == READING_SIGN || state == MENU) {
                level.draw(player);

                if (state != MENU) {
                    al_draw_text(info_font, al_map_rgb(255, 255, 255), 10, WINDOW_H - 30, 0, "Press ESC to return to Menu");
                }
                if(state == READING_SIGN){
                    double cx = WINDOW_W / 2;
                    double cy = WINDOW_H / 2;
                    al_draw_filled_rectangle(cx - 200, cy - 60, cx + 200, cy + 40, al_map_rgb(0, 0, 50));
                    al_draw_rectangle(cx - 200, cy - 60, cx + 200, cy + 40, al_map_rgb(255, 255, 255), 4);
                    al_draw_text(info_font, al_map_rgb(255, 255, 0), cx, cy - info_font_height, ALLEGRO_ALIGN_CENTER, "WARNING!");
                    al_draw_text(info_font, al_map_rgb(255, 255, 255), cx, cy + 2*info_font_height, ALLEGRO_ALIGN_CENTER, "Red walls will spread like fire.");
                    al_draw_text(info_font, al_map_rgb(150, 150, 150), cx, cy + 4*info_font_height, ALLEGRO_ALIGN_CENTER, "Press SPACE to continue...");
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

                // draw
                for (int i = 0; i < 4; i++) {
                    al_draw_text(title_font, colors[i], title_x, title_y, 0, parts[i]);
                    title_x += al_get_text_width(title_font, parts[i]);
                }

                // // Draw Start Button
                // al_draw_filled_rectangle(btn_x, btn_y, btn_x + btn_w, btn_y + btn_h, al_map_rgb(100, 100, 100));
                // al_draw_rectangle(btn_x, btn_y, btn_x + btn_w, btn_y + btn_h, al_map_rgb(255, 255, 255), 2);
                // al_draw_text(info_font, al_map_rgb(255, 255, 255), WINDOW_W/2, btn_y + 20, ALLEGRO_ALIGN_CENTER, "START");

                // Draw Instructions (Below Button)
                double inst_y = WINDOW_H / 10;
                if (player.grid_pos.y != 0) {
                    al_draw_text(info_font, al_map_rgb(200, 200, 200), WINDOW_W/2, inst_y + info_font_height, ALLEGRO_ALIGN_CENTER, "Click grid to select move");
                    al_draw_text(info_font, al_map_rgb(200, 200, 200), WINDOW_W/2, inst_y + 2*info_font_height, ALLEGRO_ALIGN_CENTER, "Press SPACE to Teleport");
                }
                
                // Draw Floor and Ceiling
                double h = 22.0f, off = 8.0f;

                al_draw_filled_rectangle(0,  40, WINDOW_W,  40+h, blue);
                al_draw_filled_rectangle(0,  40+off, WINDOW_W,  40+off+h, blue);

                al_draw_filled_rectangle(0, WINDOW_H-40-h, WINDOW_W, WINDOW_H-40, blue);
                al_draw_filled_rectangle(0, WINDOW_H-40-h-off, WINDOW_W, WINDOW_H-40-off, blue);

                // Draw Credit

                // Draw Option
                if (player.grid_pos.y != 0) {
                    Vec2 play_opt_pos = level.affine(Vec2(1.5*TILE_SIZE, 2.5*TILE_SIZE));
                    al_draw_text_bg_center(info_font,al_map_rgb(200, 200, 200), al_map_rgb(0, 0, 0), play_opt_pos.x, play_opt_pos.y, "[ Play ]",20);
                    Vec2 exit_opt_pos = level.affine(Vec2(3.5*TILE_SIZE, 2.5*TILE_SIZE));
                    al_draw_text_bg_center(info_font,al_map_rgb(200, 200, 200), al_map_rgb(0, 0, 0), exit_opt_pos.x, exit_opt_pos.y, "[ Exit ]",20);
                }
                else {
                    al_draw_text_bg_center(info_font,al_map_rgb(200, 200, 200), al_map_rgb(0, 0, 0), WINDOW_W/2, 5*WINDOW_H/6, "Math 27, Yu Wen Kuang, Ceng Qi Ming presents.",20);
                    al_draw_text_bg_center(info_font,al_map_rgb(200, 200, 200), al_map_rgb(0, 0, 0), WINDOW_W/2, 5*WINDOW_H/6 + 60, "~ Dec 19 2025, IP2 ~",20);
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

    #pragma endregion

    return 0;
}
#pragma endregion