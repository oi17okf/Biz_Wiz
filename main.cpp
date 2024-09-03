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

enum WindowState {
    DATA,  
    SUMMARY,  
    TIMELINE    
};

typedef struct data_window {
    int scroll_index = 0;
    int length = 0;
} data_window;

typedef struct timeline_window {
    int scroll_index = 0;
    int min = 0;
    int max = 0;
} timeline_window;

typedef struct window {
    int x_start;
    int x_end;
    int y_start;
    int y_end;
    int border = 0;
    int focus = 0;
    WindowState state;
    data_window dw;
    timeline_window tw;
} window;

rapidcsv::Document doc("example.csv"); //temporary

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

int inside_border(const window &w, int x, int y) {

    if (x > w.x_start + w.border && x < w.x_end - w.border &&
        y > w.y_start + w.border && y < w.y_end - w.border) {
        return 1;
    } else {
        return 0;
    }
}

//Todo - make it so only 1 window can be focused (z axis needed for good solution)
void update_window_focus(std::vector<window> &windows, int mouse_x, int mouse_y) {

    for (int i = 0; i < windows.size(); i++) {
        if (inside_border(windows[i], mouse_x, mouse_y)) {
            windows[i].focus = 1;

        } else {
            windows[i].focus = 0;
        }
    }

}

//Todo - make size size_percent
DrawTextPercent(const window &w, const std::string &s, float x_percent, float y_percent, int size, Color c) {

    int x_len = w.x_end - w.x_start;
    int y_len = w.y_end - w.y_start;

    int x = w.x_start + (x_len * x_percent);
    int y = w.y_start + (y_len * y_percent);


    int smallest_size = x_len < y_len ? x_len : y_len; // not used yet

    if (inside_border(w, x, y)) {
        DrawText(s.c_str(), x, y, size, c);
    }

}

void render_border(const window &w, Color c) {

    DrawRectangleLinesEx(Rectangle{(float)w.x_start, (float)w.y_start, (float)w.x_end - w.x_start, (float)w.y_end - w.y_start}, w.border, c);

}

void render_csv(const window &w) {

    if (w.focus) {
        render_border(w, BLACK);
    } else {
        render_border(w, WHITE);
    }


    int csv_len = doc.GetRowCount();
    std::vector<std::string> csv_names = doc.GetColumnNames();

    std::string s1 = "Csv contains " + std::to_string(csv_len) + " rows!";
    DrawTextPercent(w, s1, 0.025, 0.02, 30, GREEN);

    std::string s2 = csv_names[0] + " - " + csv_names[1] + " - " + csv_names[2] + " - " + csv_names[3];
    DrawTextPercent(w, s2, 0.025, 0.1, 20, BLUE);

    for (int i = w.dw.scroll_index; i < csv_len; i++) {
        std::vector<std::string> row = doc.GetRow<std::string>(i);
        std::string s3 = row[0] + " - " + row[1] + " - " + row[2] + " - " + row[3];
        DrawTextPercent(w, s3, 0.025, 0.15 + (0.04 * (i - w.dw.scroll_index)), 18, SKYBLUE);
    }

}

void calculate_min_max(const std::vector<int> &values, int &min, int &max) {

    min = 99999; //todo fix
    max = 0;

    for (int val : values) {
        if (val < min) { min = val; }
        if (val > max) { max = val; }
    }

}

float calculate_avg(const std::vector<int>& values) {

    float sum = 0;
    for (int val : values) {
        sum += val;
    }

    return sum / values.size();
}

float calculate_median(std::vector<int> values) {

    std::sort(values.begin(), values.end());

    int n = values.size();
    if (n % 2 == 0) {
        return (values[n / 2 - 1] + values[n / 2]) / 2.0;
    } else {
        return values[n / 2];
    }
}

int calculate_mode(const std::vector<int>& values) {

    std::map<int, int> frequencyMap;
    for (int val : values) {
        frequencyMap[val]++;
    }

    int mode = values[0];
    int maxCount = 0;

    for (const auto& pair : frequencyMap) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            mode = pair.first;
        }
    }

    return mode;
}

render_stats(const window &w) {

    if (w.focus) {
        render_border(w, BLACK);
    } else {
        render_border(w, WHITE);
    }

    
    int len = doc.GetRowCount();
    std::string s1 = "File contains " + std::to_string(len) + " rows.";
    DrawTextPercent(w, s1, 0.025, 0.05, 20, BLUE);

    std::vector<std::string> csv_names = doc.GetColumnNames();
    std::string s2 = csv_names[0] + " - " + csv_names[1] + " - " + csv_names[2] + " - " + csv_names[3];
    DrawTextPercent(w, s2, 0.025, 0.1, 20, BLUE);


    std::vector<int> ages = doc.GetColumn<int>("Age");
    float avg    = calculate_avg(ages);
    float median = calculate_median(ages);
    int mode     = calculate_mode(ages);
    std::string s3 = "Age avg value is:    " + std::to_string(avg);
    std::string s4 = "Age median value is: " + std::to_string(median);
    std::string s5 = "Age mode value is:   " + std::to_string(mode);
    DrawTextPercent(w, s3, 0.025, 0.15, 20, BLUE);
    DrawTextPercent(w, s4, 0.025, 0.20, 20, BLUE);
    DrawTextPercent(w, s5, 0.025, 0.25, 20, BLUE);

}

float Interpolate(float percent, int min, int max) {

    int len = max - min;
    int val = min + (len * percent);
    return val;

}

DrawLinePercent(const window &w, float x_start_p, float y_start_p, float x_end_p, float y_end_p) {

    float start_x = Interpolate(x_start_p, w.x_start, w.x_end);
    float end_x   = Interpolate(x_end_p,   w.x_start, w.x_end);
    float start_y = Interpolate(y_start_p, w.y_start, w.y_end);
    float end_y   = Interpolate(y_end_p  , w.y_start, w.y_end);

    DrawLineEx(Vector2{start_x, start_y}, Vector2{end_x, end_y}, 3, BLACK);  
    
}

render_timeline(const window &w) {

    if (w.focus) {
        render_border(w, BLACK);
    } else {
        render_border(w, WHITE);
    }

    DrawLinePercent(w, 0.1, 0.8, 0.9, 0.8);
    DrawLinePercent(w, 0.1, 0.75, 0.1, 0.85);
    DrawLinePercent(w, 0.9, 0.75, 0.9, 0.85);

   // w.tw.min
    int len = doc.GetRowCount();
    std::string s1 = "File contains " + std::to_string(len) + " rows.";
    DrawTextPercent(w, s1, 0.025, 0.05, 20, BLUE);

}

/* 
void render_status_bar(std::string state) {
    std::string s = "Current state: " + state;
    DrawText(s.c_str(), 5, 5, 16, BROWN);
    std::string s1 = "Press 1-4 to change state";
    DrawText(s1.c_str(), 5, 20, 10, BROWN);
}

*/



int main() {

    // How to change/save data in csv
    //doc.SetCell<std::string>(0, 1, "NewName");
    //doc.Save();

    int screenWidth = 800;
    int screenHeight = 450;

    window w;
    w.x_start = 0;
    w.x_end = 400;
    w.y_start = 50;
    w.y_end = 450;
    w.border = 5;
    w.state = SUMMARY;

    window w1;
    w1.x_start = 400;
    w1.x_end = 800;
    w1.y_start = 50;
    w1.y_end = 450;
    w1.border = 2;
    w.state = DATA;

    std::vector<window> windows;
    windows.push_back(w);
    windows.push_back(w1);

    InitWindow(screenWidth, screenHeight, "Biz Wiz");

    std::vector<data> list;
    create_example_list(doc, list);
    print_example_list(list);

    int mouse_x = 0;
    int mouse_y = 0;

    while (!WindowShouldClose()) {

        // &&&&&&&&&&&&&&&&&&&&&&&&&&&&&& Update Logic &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

        mouse_x = GetMouseX();                              
        mouse_y = GetMouseY();  

        update_window_focus(windows, mouse_x, mouse_y);

        for (window &w : windows) {

            if (w.focus == 0) { continue; }

            if (IsKeyPressed(KEY_ONE)) {
                w.state = DATA;
            } else if (IsKeyPressed(KEY_TWO)) {
                w.state = SUMMARY;
            } else if (IsKeyPressed(KEY_THREE)) {
                w.state = TIMELINE;
            } 
     
            switch (w.state) {

                case DATA: {

                    w.dw.length = doc.GetRowCount(); //should be moved to some kind of init function

                    w.dw.scroll_index += -1 * GetMouseWheelMove();

                    if (w.dw.scroll_index < 0) { w.dw.scroll_index = 0; }
                    if (w.dw.scroll_index > w.dw.length - 1) { w.dw.scroll_index = w.dw.length - 1; }

                    break;
                }

                case SUMMARY: {

                    break;    
                }

                case TIMELINE: {

                    int min;
                    int max;
                    calculate_min_max(doc.GetColumn<int>("Age"), min, max);
                    w.tw.min = min < 5 ? 0 : min - 5;
                    w.tw.max = max + 5;

                    break;
                }

            }
        }

        //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Draw @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        BeginDrawing();

        //render_status_bar(state);

        for (window &w : windows) {

            switch (w.state) {

                case DATA: {

                    render_csv(w);
                    break;
                }

                case SUMMARY: {

                    render_stats(w);
                    break;    
                }

                case TIMELINE: {
                    render_timeline(w);
                    break;
                }
            
            }
        }

        ClearBackground(RAYWHITE);
        EndDrawing();
    }

    CloseWindow();   
    return 0;
}