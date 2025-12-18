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
                    screen.x,
                    screen.y,
                    screen.x + size - 2 * scale,
                    screen.y + size - 2 * scale,
                    color
                );