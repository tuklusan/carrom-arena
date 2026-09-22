#include "renderer.h"
#include "physics.h"
#include "game/board.h"
#include "common/types.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    // Initialize renderer in capture mode
    Renderer* r = renderer_create(1280, 720, "Capture Pocket", true, true, false, 0.1f);
    
    // Setup a board with a piece in a pocket
    BoardState board;
    board_state_init(&board);
    board.pieces[0].position = (Vec2){ POCKET_CENTERS[0].x, POCKET_CENTERS[0].y };
    board.pieces[0].on_board = true;
    board.pieces[0].color = PIECE_WHITE;
    board.white_on_board = 1;
    
    // Capture frame 0
    renderer_capture_frame(r, "captures/pocket_test", 0, 0, 0.0, 0.1f, &board);
    
    printf("Captured frame to captures/pocket_test/frame_0000.png\n");
    
    // Cleanup (though we are exiting)
    // renderer_destroy(r); // If renderer_destroy exists
    return 0;
}
