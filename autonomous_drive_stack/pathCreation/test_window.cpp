#include "raylib.h"

int main() {
    InitWindow(1000, 700, "Autonomy Simulator");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Raylib works!", 30, 30, 30, BLACK);
        DrawText("Next: planner + car simulation", 30, 70, 20, DARKGRAY);

        // Temporary stand-in for the vehicle.
        DrawCircle(500, 350, 20, BLUE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
