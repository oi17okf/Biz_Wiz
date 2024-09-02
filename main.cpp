#include <iostream>
#include "rapidcsv.h"
#include "raylib.h"

// Note - RapidCSV doesn't support retrieving mixed data types directly.

// Current example data - Alice,30,New York,Engineer

typedef struct data {
    std::string name;
    int age;
    std::string loc;
    std::string work;
} data; 

//Filling a list of structs is fine for testdata, but will have performance problems later.

void fill_example(const std::vector<std::string> &row, data &d) {

    d.name = row[0];
    d.age  = std::stoi(row[1]);
    d.loc  = row[2];
    d.work = row[3];

}

void create_example_list(rapidcsv::Document doc, std::vector<data> &list) {

    int len = doc.GetRowCount();
    list.resize(len);

    for (int i = 0; i < len; i++) {

        std::vector<std::string> rowData = doc.GetRow<std::string>(i);
        fill_example(rowData, list[i]);

    }
}

void print_example_list(const std::vector<data> &list) {

    int len = list.size();
    std::cout << "Example data contains " << len << " rows" << std::endl;
 

    for (int i = 0; i < len; i++) {
        //std::cout << "Row " << i << list[i].name << list[i].age << list[i].loc << list[i].work << std::endl;
        printf("Row %d: %s, %d, %s, %s\n", i, list[i].name.c_str(), list[i].age, list[i].loc.c_str(), list[i].work.c_str());
    }
    std::cout << "Example data printed" << std::endl;
}

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

    InitWindow(screenWidth, screenHeight, "Biz Wiz");

    std::vector<data> list;
    create_example_list(doc, list);
    print_example_list(list);

    int csv_len = doc.GetRowCount();
    std::vector<std::string> csv_names = doc.GetColumnNames();

    while (!WindowShouldClose()) {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
        std::string s1 = "Csv contains " + std::to_string(csv_len) + " rows!";
        DrawText(s1.c_str(), 20, 50, 30, GREEN);
        std::string s2 = csv_names[0] + " - " + csv_names[1] + " - " + csv_names[2] + " - " + csv_names[3];
        DrawText(s2.c_str(), 20, 100, 20, BLUE);
        for (int i = 0; i < csv_len; i++) {
            std::vector<std::string> row = doc.GetRow<std::string>(i);
            std::string s3 = row[0] + " - " + row[1] + " - " + row[2] + " - " + row[3];
            DrawText(s3.c_str(), 20, 130 + i * 20, 18, SKYBLUE);
        }

        ClearBackground(RAYWHITE);



        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    CloseWindow();   
    return 0;
}