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