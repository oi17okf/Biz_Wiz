#include <iostream>
#include "rapidcsv.h"
#include "raylib.h"

float calculate_avg_age(rapidcsv::Document doc) {
    std::vector<int> ages = doc.GetColumn<int>("Age");

    int sum_ages = 0;
    for (size_t i = 0; i < ages.size(); i++) {
        sum_ages += ages[i];
    }

    float avg_age = sum_ages / ages.size();

    return avg_age;
}

int main() {
    // Read CSV file
    rapidcsv::Document doc("example.csv");

    // Access data by column name
    std::vector<std::string> names = doc.GetColumn<std::string>("Name");
    std::vector<int> ages = doc.GetColumn<int>("Age");

    for (size_t i = 0; i < names.size(); ++i) {
        std::cout << "Name: " << names[i] << ", Age: " << ages[i] << std::endl;
    }

    float avg_age = calculate_avg_age(doc);
    std::cout << "Avg Age: " << avg_age << std::endl;

    // Modify and save data
    //doc.SetCell<std::string>(0, 1, "NewName");
    //doc.Save();


    int screenWidth = 800;
    int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Hello Raylib");
    while (!WindowShouldClose()) {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    CloseWindow();   
    return 0;
}