#include <iostream>
#include "rapidcsv.h"
#include "raylib.h"
#include "tinyxml2.h"
#include <vector>
#include <string>
#include <algorithm>
#include <ctime>
#include <limits>

using namespace tinyxml2;

std::string global_msg = "Welcome to Biz_Wiz";

void log(std::string s) {
    std::cout << "LOG: " << s << std::endl;
}

enum WindowState {
    DATA,  
    SUMMARY,  
    TIMELINE, 
    GRAPH    
};



struct summary_data {
    int traces;
    int events;
    float events_per_trace;
};

struct event {

    std::string id;
    std::string resource;
    std::string name;
    std::string role;
    time_t time;
};

struct trace {
    std::vector<event> events;
};

struct xes_data {

    int events;
    float events_per_trace;
    std::vector<std::string> names;
    std::vector<trace> traces;
};

struct data_data {
    std::vector<trace> &traces;
    int scroll_index = 0;
    int length = 0;
    int item_height = 20;
    int hover_index = -1;
    int selected_index  = -1;

    data_data(std::vector<trace> &t) : traces(t) {}
};

struct timeline_data {

    std::vector<trace> &traces;
    int trace_index = 0;
    time_t start = 0;
    time_t end = 0;

    timeline_data(std::vector<trace> &t) : traces(t) {}
};

struct window {
    int x_start;
    int x_end;
    int y_start;
    int y_end;
    int border = 0;
    int focus = 0;
    WindowState state;
};

struct interactable_node {

    Vector2 pos;
    std::string type;
};

struct mouse {

    Vector2 pos;
    Vector2 end;
    Vector2 delta;
    float wheel_delta;
    int left_click;
    int right_click;
    int left_down;

};

struct connection {

    interactable_node *start;
    interactable_node *end;
    int value = 0;

    connection(interactable_node* s, interactable_node* e, int v) : start(s), end(e), value(v) {}
};

enum Action {

    CANCEL,  
    CREATE_NODE,  
    CREATE_CONNECTION, 
    DELETE_NODE,
    DELETE_CONNECTION,
    RESET_CAMERA   

};


struct action {

    Action type;
    int index;
    int index2;

    std::string name;

};

struct graph_data {

    std::vector<interactable_node> nodes;
    std::vector<connection>        connections;
    float node_radius = 10;
    Vector2 offset = { 0, 0 };
    int hover_node_index = -1;
    int active_node_index = -1;
    std::vector<std::string> node_names;

    int menu_active = 0;
    std::vector<action> action_list;
    int item_hovered = -1;
    int item_selected = -1;
    int item_height = 20;
    Rectangle menu_loc;
    Vector2 old_mouse_pos;
    
};

Vector2 AddVector2(Vector2 a, Vector2 b) {

    Vector2 c;
    c.x = a.x + b.x;
    c.y = a.y + b.y;
    return c;
}

Vector2 SubVector2(Vector2 a, Vector2 b) {

    Vector2 c;
    c.x = a.x - b.x;
    c.y = a.y - b.y;
    return c;
}





graph_data create_graph_data(xes_data &log) {

    graph_data g;
    g.node_radius = 10.0;
    g.node_names = log.names;

    interactable_node n1 = { 0, 0, "n1" }; //Todo remove
    interactable_node n2 = { 50, 10, "n2" };
    interactable_node n3 = { 0, 50, "n3" };
    interactable_node n4 = { -20, -20, "n4" };
    g.nodes.push_back(n1);
    g.nodes.push_back(n2);
    g.nodes.push_back(n3);
    g.nodes.push_back(n4);

    return g;
}

XMLElement* open_xes(std::string filename, XMLDocument &doc) {

    XMLError eResult = doc.LoadFile(filename.c_str());
    if (eResult != XML_SUCCESS) {
        std::cerr << "Error loading file: " << eResult << std::endl;
        return nullptr;
    }

    XMLElement* root = doc.FirstChildElement("log");
    if (root == nullptr) {
        std::cerr << "No <log> element found in XES file" << std::endl;
        return nullptr;
    }

    return root;

}

time_t parse_timestamp(const std::string& timestamp) {

    std::tm tm = {};
    std::stringstream ss(timestamp);
    
    //cuts of timestamp from "2023-09-06T09:34:00.000+00:00" to 2023-09-06T09:34:00
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        log("error parsing time");
    }

    return mktime(&tm);
}

void parse_xes(XMLElement* root, xes_data &data) {

    for (XMLElement* log_trace = root->FirstChildElement("trace"); log_trace != nullptr; log_trace = log_trace->NextSiblingElement("trace")) {

        trace t;

        for (XMLElement* log_event = log_trace->FirstChildElement("event"); log_event != nullptr; log_event = log_event->NextSiblingElement("event")) {

            data.events++;

            event e;

            for (XMLElement* attribute = log_event->FirstChildElement(); attribute; attribute = attribute->NextSiblingElement()) {

                const char* key   = attribute->Attribute("key");
                const char* value = attribute->Attribute("value");
                
                if        (key && std::string(key) == "id") {
                    e.id = std::string(value);
                } else if (key && std::string(key) == "org:resource") {
                    e.resource = std::string(value);
                } else if (key && std::string(key) == "concept:name") {
                    e.name = std::string(value);
                    if (std::find(data.names.begin(), data.names.end(), e.name) == data.names.end()) {
                        data.names.push_back(e.name);
                    }
                } else if (key && std::string(key) == "org:role") {
                    e.role = std::string(value);
                } else if (key && std::string(key) == "time:timestamp") {
                    e.time = parse_timestamp(std::string(value));
                }
            }

            t.events.push_back(e);
        }

        data.traces.push_back(t);

    }

    data.events_per_trace = (float)data.events / (float)data.traces.size();
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


window& get_focused_window(std::vector<window> &windows, int mouse_x, int mouse_y) {

    int index = 0;

    for (size_t i = 0; i < windows.size(); i++) {
        if (inside_border(windows[i], mouse_x, mouse_y)) {
            windows[i].focus = 1;
            index = i;
        } else {
            windows[i].focus = 0;
        }
    }

    return windows[index];

}

//Todo - make size size_percent
void DrawTextPercent(const window &w, const std::string &s, float x_percent, float y_percent, int size, Color c) {

    int x_len = w.x_end - w.x_start;
    int y_len = w.y_end - w.y_start;

    int x = w.x_start + (x_len * x_percent);
    int y = w.y_start + (y_len * y_percent);


    //int smallest_size = x_len < y_len ? x_len : y_len; // not used yet

    if (inside_border(w, x, y)) {
        DrawText(s.c_str(), x, y, size, c);
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

float calculate_variance(const std::vector<int> &values) {

    float avg = calculate_avg(values);
    float variance = 0.0;

    for (int val : values) {
        variance += (val - avg) * (val - avg);
    }

    variance = variance / values.size();

    return variance;

}



float Interpolate(float percent, int min, int max) {

    int len = max - min;
    int val = min + (len * percent);
    return val;

}

float GetPercent(float value, float min, float max) {

    return (value - min) / (max - min);

}

void DrawCirclePercent(const window &w, float x_percent, float y_percent, Color c) {

    float x = Interpolate(x_percent, w.x_start, w.x_end);
    float y = Interpolate(y_percent, w.y_start, w.y_end);
    DrawCircleLines(x, y, 20, c);

}

void DrawLinePercent(const window &w, float x_start_p, float y_start_p, float x_end_p, float y_end_p, int size, Color c) {

    float start_x = Interpolate(x_start_p, w.x_start, w.x_end);
    float end_x   = Interpolate(x_end_p,   w.x_start, w.x_end);
    float start_y = Interpolate(y_start_p, w.y_start, w.y_end);
    float end_y   = Interpolate(y_end_p  , w.y_start, w.y_end);

    DrawLineEx(Vector2{start_x, start_y}, Vector2{end_x, end_y}, size, c);  
    
}

void DrawTimelineNode(const window &w, float x_pos, float y_timeline, std::string name, int age) {

    float node_y = y_timeline - 0.5;
    name += " " + std::to_string(age);
    //std::cout << name << std::endl;
    DrawCirclePercent(w, x_pos, node_y, BROWN);
    DrawLinePercent(w, x_pos, node_y + 0.05, x_pos, y_timeline, 1, BROWN);

    DrawTextPercent(w, name, x_pos - 0.04, node_y - 0.01, 6, GREEN);

}

void render_border(const window &w) {

    if (w.focus) {
        DrawRectangleLinesEx(Rectangle{(float)w.x_start, (float)w.y_start, (float)w.x_end - w.x_start, (float)w.y_end - w.y_start}, w.border, BLACK);
    } else {
        DrawRectangleLinesEx(Rectangle{(float)w.x_start, (float)w.y_start, (float)w.x_end - w.x_start, (float)w.y_end - w.y_start}, w.border, WHITE);
    }
}

void render_summary(const window &w, const summary_data &s) {

    render_border(w);

}

void render_timeline(const window &w, const timeline_data &d) {

    render_border(w);

    // Drawtimeline

    // Draw min/max values

    // Draw nodes
   

}



void render_data(const window &w, const data_data &d) {

    render_border(w);

}

void render_graph(const window &w, const graph_data &g) {

    //BeginScissorMode(w.x_start, w.y_start, w.x_end - w.x_start, w.y_end - w.y_start);

    render_border(w);


    for (interactable_node n : g.nodes) {

        int x = g.offset.x + w.x_start + n.pos.x;
        int y = g.offset.y + w.y_start + n.pos.y;
        DrawCircleLines(x, y, g.node_radius, GREEN);
        DrawText(n.type.c_str(), x, y, 8, BLUE);
    }

    //EndScissorMode();

}


struct screen {
    int width;
    int height;
};

void update_subwindow_sizes(std::vector<window> &windows, screen s) {

    int size = windows.size();
    int subwindow_width = s.width / size;

    for (int i = 0; i < size; i++) {
        windows[i].y_start = 0;
        windows[i].y_end = s.height;

        windows[i].x_start = subwindow_width * i;
        windows[i].x_end   = subwindow_width * i + subwindow_width;

    }

}

void create_subwindow(std::vector<window> &windows, WindowState state) {

    window w;
    w.state = state;
    w.border = 2;

    windows.push_back(w);
}



void delete_focused_subwindow(std::vector<window> &windows) {

    if (windows.size() == 1) {
        log("Attempt to delete final subwindow - ignored");
        return;
    }

    for (auto it = windows.begin(); it != windows.end(); ) {
        if (it->focus) { 
            it = windows.erase(it);
        } else {
            it++;  
        }
    }
}

summary_data create_summary_data(xes_data &log) {

    summary_data s;
    s.traces = log.traces.size();
    s.events = log.events;
    s.events_per_trace = log.events_per_trace;

    return s;
}

data_data create_data_data(xes_data &log) {

    data_data d = { log.traces };

    d.length = log.traces.size() + log.events * 6; // each event has 5 fields

    return d;
}

void update_timeline_bounds(timeline_data &timeline) {

    trace t = timeline.traces[timeline.trace_index];

    timeline.start = std::numeric_limits<time_t>::max();
    timeline.end   = std::numeric_limits<time_t>::min();

    for (event e : t.events) {
        timeline.start = e.time < timeline.start ? e.time : timeline.start;
        timeline.end   = e.time > timeline.end   ? e.time : timeline.end;
    }
}

timeline_data create_timeline_data(xes_data &log) {

    timeline_data d = { log.traces };
    update_timeline_bounds(d);

    return d;
}

int clamp(int i, int min, int max) {

    i = i < min ? min : i;
    i = i > max ? max : i;
    return i;
}


void logic_data(mouse &m, data_data &d) {

    d.scroll_index += -1 * m.wheel_delta;
    d.scroll_index = clamp(d.scroll_index, 0, d.length);

    d.hover_index = m.pos.y / d.item_height;

    if (m.left_click) { 
        d.selected_index = d.hover_index;
    }

    if (m.left_down == 0 && d.selected_index != -1) {
        //swappppp
        d.selected_index = -1;
    }
}

void logic_summary(mouse &m, summary_data &s) {

    //todo?
}

void logic_timeline(mouse &m, timeline_data &t) {

    //todoi
}

void create_node(graph_data &g, action a) {

    interactable_node n;
    n.pos = g.old_mouse_pos;
    n.type = a.name;

    g.nodes.push_back(n);
}

void delete_node(graph_data &g, action a) {

    interactable_node* ptr = &(g.nodes[a.index]);

    for (std::vector<connection>::iterator it = g.connections.begin(); it != g.connections.end();) {
        if (it->start == ptr || it->end == ptr) { 
            it = g.connections.erase(it); 
            log("deleted connection while deleting node");
        } else { 
            it++; 
        }
    }

    g.nodes.erase(g.nodes.begin() + a.index);

}

void create_connection(graph_data &g, action a) {

    interactable_node *n1 = &(g.nodes[a.index]);
    interactable_node *n2 = &(g.nodes[a.index2]);

    connection c = connection(n1, n2, 0);

    g.connections.push_back(c);

}

void delete_connection(graph_data &g, action a) {

    g.connections.erase(g.connections.begin() + a.index);

}

void reset_camera(graph_data &g) {
    g.offset = { 0, 0 };
}

void do_menu_action(graph_data &g) {

    action a = g.action_list[g.item_selected];

    switch (a.type) {

        case CANCEL:            {                          break; }
        case CREATE_NODE:       { create_node(g, a);       break; }
        case DELETE_NODE:       { delete_node(g, a);       break; }
        case CREATE_CONNECTION: { create_connection(g, a); break; }
        case DELETE_CONNECTION: { delete_connection(g, a); break; }
        case RESET_CAMERA:      { reset_camera(g);         break; }
  
    }

}

int intersecting_node(Vector2 pos, graph_data &g, int i) {

    interactable_node n = g.nodes[i];
    Vector2 node_pos = AddVector2(n.pos, g.offset);
    return CheckCollisionPointCircle(m.pos, node_pos, g.node_radius);

}



//Any item that overlaps with the click gets added
//Do this by simply looping through every single item for now...
void update_action_list(Vector2 pos, graph_data &g) {

    //For each new item sort created, simply add new for loop and set interaction actions.

    //NODES
    for (size_t i = 0; i < g.nodes.size(); i++) {

        //Add node actions
        if (intersecting_node(pos, g, i)) {
            
            
            action delete_node = { DELETE_NODE, i, 0, "" };
            g.actions_list.push_back(delete_node);
        }

    }

    //CONNECTIONS
    for (size_t i = 0; i < g.connections.size(); i++) {

        //Add connection actions
        if (intersecting_connection(pos, g, i)) {
            action delete_connection = { DELETE_CONNECTION, i, 0, "" };
            g.actions_list.push_back(delete_connection);

        }

    }



    //Add reset camera for funsies
    action reset_cam = { RESET_CAMERA, 0, 0, "" }; //only type is relevant here.
    g.action_list.push_back(reset_cam);
    //Finish by adding a cancel
    action cancel = { CANCEL, 0, 0, "" }; 
    g.action_list.push_back(reset_cam);
}

void logic_graph(mouse &m, graph_data &g) {

    if (g.menu_active) { g.menu_active = CheckCollisionPointRec(m.pos, g.menu_loc); }

    if (g.menu_active) {
        int menu_y = m.pos.y - g.menu_loc.y;
        g.item_hovered = menu_y / g.item_height -1;

        if (m.left_click) {
            g.item_selected = g.item_hovered;
            if (g.item_selected != -1) {
                do_menu_action(g);
            }
            g.item_hovered  = -1;
            g.menu_active = 0;
        }

    } else {

        g.hover_node_index = -1;
        for (size_t i = 0; i < g.nodes.size(); i++) {
            interactable_node n = g.nodes[i];
            Vector2 node_pos = AddVector2(n.pos, g.offset);
            if (CheckCollisionPointCircle(m.pos, node_pos, g.node_radius)) {
                g.hover_node_index = i;
                log("hooooooovering");
            }
        }

        if (m.right_click) {
            g.menu_active = 1;

            update_action_list(m.pos, g);

        }

        if (m.left_click && g.hover_node_index != -1) {
            g.active_node_index = g.hover_node_index;
            log("active set");

        }

        //to handle connection collision
        //bool CheckCollisionPointLine(Vector2 point, Vector2 p1, Vector2 p2, int threshold); 



        if (m.left_down) {
            if (g.active_node_index == -1) {
                g.offset = AddVector2(g.offset, m.delta);
            } else {
                g.nodes[g.active_node_index].pos = AddVector2(g.nodes[g.active_node_index].pos, m.delta);
            }
        
        } else {
            g.active_node_index = -1;
            log("active un-set");
        }
    }
}

            
           


int main() {

    std::string xes_filename = "RequestForPayment.xes_";

    SetTraceLogLevel(LOG_ALL);

    screen screen_size = { 800, 450 };

    window w;
    w.border = 5;
    w.state = DATA;

    window w1;
    w1.border = 2;
    w1.state = TIMELINE;

    std::vector<window> windows;
    windows.push_back(w);
    windows.push_back(w1);

    update_subwindow_sizes(windows, screen_size);


    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screen_size.width, screen_size.height, "Biz Wiz");


    struct mouse m;
    
    XMLDocument xes_doc;
    XMLElement* root_log = open_xes(xes_filename, xes_doc);
    xes_data log_data;
    parse_xes(root_log, log_data);

    summary_data s  = create_summary_data(log_data);
    data_data d     = create_data_data(log_data);
    timeline_data t = create_timeline_data(log_data);
    graph_data g    = create_graph_data(log_data);
 

    while (!WindowShouldClose()) {

        // &&&&&&&&&&&&&&&&&&&&&&&&&&&&&& GLOBAL INPUT &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

        if (IsKeyPressed(KEY_F11)) { ToggleFullscreen();                }
        if (IsKeyPressed(KEY_C))   { create_subwindow(windows, DATA);   }
        if (IsKeyPressed(KEY_D))   { delete_focused_subwindow(windows); }

        screen_size.width  = GetScreenWidth();
        screen_size.height = GetScreenHeight();

        update_subwindow_sizes(windows, screen_size);

        m.pos         = GetMousePosition();
        m.delta       = GetMouseDelta();
        m.wheel_delta = GetMouseWheelMove();
        m.left_click  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        m.right_click = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        m.left_down   = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        window &w = get_focused_window(windows, m.pos.x, m.pos.y);
        m.pos.x -= w.x_start;
        m.pos.y -= w.y_start;
        m.end.x =  w.x_end - w.x_start;
        m.end.y =  w.y_end - w.y_start;

        // &&&&&&&&&&&&&&&&&&&&&&&&&&&&&& LOCAL INPUT &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

        if      (IsKeyPressed(KEY_ONE))    { w.state = DATA;     } 
        else if (IsKeyPressed(KEY_TWO))    { w.state = SUMMARY;  } 
        else if (IsKeyPressed(KEY_THREE))  { w.state = TIMELINE; } 
        else if (IsKeyPressed(KEY_FOUR))   { w.state = GRAPH;    } 

        // &&&&&&&&&&&&&&&&&&&&& LOGIC &&&&&&&&&&&&&&&&&&

        switch (w.state) {

            case DATA:     { logic_data    (m, d); break; }
            case SUMMARY:  { logic_summary (m, s); break; }
            case TIMELINE: { logic_timeline(m, t); break; }
            case GRAPH:    { logic_graph   (m, g); break; }

        } 
        

        //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Draw @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        BeginDrawing();

        for (window &w : windows) {

            switch (w.state) {
                case DATA:     { render_data    (w, d); break; }
                case SUMMARY:  { render_summary (w, s); break; } 
                case TIMELINE: { render_timeline(w, t); break; }
                case GRAPH:    { render_graph   (w, g); break; }

            }
        }

        DrawText(global_msg.c_str(), 2, 2, 8, YELLOW);

        ClearBackground(RAYWHITE);
        EndDrawing();
    }

    CloseWindow();   
    return 0;
}