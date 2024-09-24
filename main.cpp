#include <iostream>
#include "rapidcsv.h"
#include "raylib.h"
#include "tinyxml2.h"
#include <vector>
#include <string>
#include <algorithm>
#include <ctime>
#include <limits>
#include <math.h>

using namespace tinyxml2;

std::string global_msg = "Welcome to Biz_Wiz";

void log(std::string s) {
    std::cout << "LOG: " << s << std::endl;
}

enum WindowState {
    DATA,  
    SUMMARY,  
    TIMELINE, 
    GRAPH,
    TRACE
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
    int valid;
};

struct xes_data {

    int events;
    float events_per_trace;
    std::vector<std::string> names;
    std::vector<trace> traces;
};

struct trace_data {
    std::vector<trace> &traces;
    int selected_trace;
    int current_valid;
    int invalid_traces;

    trace_data(std::vector<trace> &t) : traces(t) {}
};

struct data_data {
    std::vector<trace> &traces;
    int scroll_index = 0;
    int length = 0;
    int item_height = 10;
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

enum NodeType {
    NORMAL,  
    START,  
    END    
};

struct interactable_node {

    Vector2 pos;
    std::string type;
    int id;
    NodeType node_type;
};

struct mouse {

    Vector2 pos;
    Vector2 end;
    Vector2 delta;
    float wheel_delta;
    int left_click;
    int right_click;
    int left_down;
    int left_released;

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
    RESET_CAMERA,
    TOGGLE_DIRECTION,
    TOGGLE_PROCESSING,
    CLEAR_GRAPH,
    SET_START_NODE,
    SET_END_NODE   

};

//TODO, change value to a struct that allows for better names than 'toggle'
const std::map<Action, std::string> action_names = {

    { CANCEL,            "Cancel" },
    { CREATE_NODE,       "Create node" }, //split into two?
    { CREATE_CONNECTION, "Create connection" },
    { DELETE_NODE,       "Delete node" },
    { DELETE_CONNECTION, "Delete connection" },
    { RESET_CAMERA,      "Reset camera" },
    { TOGGLE_DIRECTION,  "Toggle direction" },
    { TOGGLE_PROCESSING, "Toggle processing" },
    { CLEAR_GRAPH,       "Clear graph" }, 
    { SET_END_NODE,      "Set end node" },
    { SET_START_NODE,    "Set start node" }
    
};

std::string action_to_string(Action a) {

    auto it = action_names.find(a);

    if (it != action_names.end()) { return it->second;       } 
    else                          { return "UNKNOWN ACTION"; }
}

struct action {

    Action type;
    int index;

    std::string name;

};

struct graph_data {

    std::vector<interactable_node> nodes;
    int nodes_created = 0;
    std::vector<connection>        connections;
    float node_radius = 10;
    Vector2 offset = { 0, 0 };
    int hover_node_index = -1;
    int active_node_index = -1;
    std::vector<std::string> node_names;
    int hover_conn_index = -1;
    int active_conn_index = -1;

    int menu_active = 0;
    std::vector<action> action_list;
    int item_hovered = -1;
    int item_selected = -1;
    int item_height = 15;
    Rectangle menu_loc;
    Vector2 old_mouse_pos;
    int node_conn_index = -1;
    int processing = 0;
    int traces_processed = 0;
    
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
    g.nodes.reserve(100); //temp solution.
    g.connections.reserve(100);

    interactable_node n1 = { 0, 0, "n1", g.nodes_created }; //Todo remove
    g.nodes_created++;
    interactable_node n2 = { 50, 10, "n2", g.nodes_created };
    g.nodes_created++;
    interactable_node n3 = { 0, 50, "n3", g.nodes_created };
    g.nodes_created++;
    interactable_node n4 = { -20, -20, "n4", g.nodes_created };
    g.nodes_created++;

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

    for (XMLElement* log_trace = root->FirstChildElement("trace"); 
        log_trace != nullptr; log_trace = log_trace->NextSiblingElement("trace")) {

        trace t;
        t.valid = 0;

        for (XMLElement* log_event = log_trace->FirstChildElement("event"); 
            log_event != nullptr; log_event = log_event->NextSiblingElement("event")) {

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

void render_graph_node(const window &w, const graph_data &g, interactable_node n, int hover, int active) {

    int x = g.offset.x + w.x_start + n.pos.x;
    int y = g.offset.y + w.y_start + n.pos.y;
    Color c = GREEN;
    c = n.node_type == START ? BLUE : c;
    c = n.node_type == END ? YELLOW : c;
    c = hover ? DARKGREEN : c;
    c = active ? MAROON : c;
    DrawCircleLines(x, y, g.node_radius, c);
    DrawText(n.type.c_str(), x, y, 8, BLUE);
}

void render_graph_conn(const window &w, const graph_data &g, connection c, int hover, int active) {

    Vector2 start = { g.offset.x + w.x_start + c.start->pos.x , g.offset.y + w.y_start + c.start->pos.y };
    Vector2 end   = { g.offset.x + w.x_start + c.end->pos.x   , g.offset.y + w.y_start + c.end->pos.y   };
    Color col = GREEN;
    col = hover ? DARKGREEN : col;
    col = active ? MAROON : col;

    DrawLineEx(start, end, 3, col);

    int mid_x = (start.x + end.x) / 2;
    int mid_y = (start.y + end.y) / 2;

    float dx = end.x - start.x;
    float dy = end.y - start.y;

    float angle = atan2f(dy, dx);
    int arrowSize = 20;

    float arrowAngle = PI / 6; // 30 degrees
    int arrow_x1 = mid_x - arrowSize * cosf(angle - arrowAngle);
    int arrow_y1 = mid_y - arrowSize * sinf(angle - arrowAngle);
    int arrow_x2 = mid_x - arrowSize * cosf(angle + arrowAngle);
    int arrow_y2 = mid_y - arrowSize * sinf(angle + arrowAngle);

    DrawLineEx({ mid_x, mid_y }, { arrow_x1, arrow_y1 }, 3, col);
    DrawLineEx({ mid_x, mid_y }, { arrow_x2, arrow_y2 }, 3, col);

    std::string val = std::to_string(c.value);

    DrawText(val.c_str(), mid_x, mid_y, 8, BLUE);
}

Rectangle calc_menu_pos(const mouse &m, const graph_data &g) {

    int l = 0;
    for (action a : g.action_list) {

        std::string s = action_to_string(a.type);
        int action_len = MeasureText(s.c_str(), 10);
        action_len += 10; //add buffer space
        int name_len   = MeasureText(a.name.c_str(), 10);
        if (name_len > 0) { name_len++; } //add space
        if (action_len  + name_len > l) { l = action_len + name_len; }
    }

    Vector2 m_pos = g.old_mouse_pos;
    int items = g.action_list.size();
    int height = (items + 1) * g.item_height;
    int width = l < 60 ? 60 : l;

    int x = m_pos.x - width / 2;
    x = x < 0 ? 0 : x;
    x = x + width > m.end.x ? x - (x + width - m.end.x) : x;

    int y = m_pos.y;
    y = y + height > m.end.y ? y - (y + height - m.end.y) : y;

    Rectangle rec = { (float)x, (float)y, (float)width, (float)height };
    return rec;
}

void render_graph_menu(const window &w, const graph_data &g) {

    Rectangle menu_loc = { w.x_start + g.menu_loc.x, w.y_start + g.menu_loc.y, g.menu_loc.width, g.menu_loc.height };

    DrawRectangleRec(menu_loc, BROWN);
    DrawRectangle(menu_loc.x + 1, menu_loc.y + 1, menu_loc.width - 2, g.item_height - 2, BLACK);     
    DrawRectangleLines(menu_loc.x + 1, menu_loc.y + g.item_height, menu_loc.width - 2, menu_loc.height - g.item_height - 2, BLACK);

    DrawText("Choose option", menu_loc.x + 4, menu_loc.y + 3, 10, BROWN);
    int i = 0;  
    for (action a : g.action_list) {

        Color c = WHITE;
        c = i == g.item_hovered ? YELLOW : c;
        std::string s = action_to_string(a.type);
        DrawText(s.c_str(), menu_loc.x + 4, menu_loc.y + g.item_height * (i+1) + 3, 10, c);

        if (a.name != "") {
            int len = MeasureText(s.c_str(), 10);
            DrawText(a.name.c_str(), menu_loc.x + 4 + len + 3, menu_loc.y + g.item_height * (i+1) + 3, 10, SKYBLUE);
        } 
        i++;
    }
    
   

}

void render_graph(const window &w, const graph_data &g) {

    //BeginScissorMode(w.x_start, w.y_start, w.x_end - w.x_start, w.y_end - w.y_start);

    render_border(w);

    int i = 0;
    for (interactable_node n : g.nodes) {

        render_graph_node(w, g, n, g.hover_node_index == i, g.active_node_index == i);
        i++;
    }

    i = 0;
    for (connection c : g.connections) {

        render_graph_conn(w, g, c, g.hover_conn_index == i, g.active_conn_index == i);
        i++;
    }


    if (g.menu_active) {

        render_graph_menu(w, g);
    }

    if (g.processing) {

        std::string trace_status = "Traces processed: " + std::to_string(g.traces_processed);
        DrawText(trace_status.c_str(), w.x_start + 10, w.y_start + 10 , 10, YELLOW);
    }

    //EndScissorMode();

}

void render_active_trace(const window &w, trace t) {

    int x = (w.x_start + w.x_end) / 2;

    int y = w.y_start + 20;

    Color c = t.valid ? GREEN : RED;

    for (size_t i = 0; i < t.events.size(); i++) {

        event &e = t.events[i];

        DrawCircleLines(x, y, 10, c);

        if (i != t.events.size() - 1) { 
            DrawLineEx( {x, y}, {x, y + 30}, 3, c); 

        }

        DrawText(e.name.c_str(), x, y, 8, BLUE);
        if (i != t.events.size() - 1) { 
            y += 30;    
        }
    }
}

void render_trace(const window &w, const trace_data &t) {

    std::string s1 = "Invalid traces: " + std::to_string(t.invalid_traces) + " Total traces: " + std::to_string(t.traces.size() - 1);
    DrawText(s1.c_str(), w.x_start + 10, w.y_start + 10, 10, YELLOW);
    std::string s2 = "Rendering trace " + std::to_string(t.selected_trace) + " out of " + std::to_string(t.traces.size() - 1);
    Color c = t.current_valid ? GREEN : RED;
    DrawText(s2.c_str(), w.x_start + 10, w.y_start + 20, 10, c);

    trace active_trace = t.traces[t.selected_trace];

    render_active_trace(w, active_trace);
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

trace_data create_trace_data(xes_data &log) {

    trace_data t = { log.traces };
    t.selected_trace = 0;
    t.current_valid = 0;
    t.invalid_traces = 0;

    return t;
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

void logic_trace(mouse &m, trace_data &t) {

    t.selected_trace -= m.wheel_delta;
    t.selected_trace = clamp(t.selected_trace, 0, t.traces.size() - 1);

    trace ct = t.traces[t.selected_trace];

    t.current_valid = ct.valid;

}

void create_node(graph_data &g, action a) {

    if (a.index == -1) { 

        g.menu_active = 1;

        g.action_list.clear();
        int i = 0;
        for (std::string s : g.node_names) {


            action create_node = { CREATE_NODE, i, s };
            g.action_list.push_back(create_node);

            i++;
        }
        action cancel = { CANCEL, -1, "" }; 
        g.action_list.push_back(cancel);

    } else {

        interactable_node n;
        n.pos = g.old_mouse_pos;
        n.pos.x -= g.offset.x;
        n.pos.y -= g.offset.y;
        n.type = a.name;
        n.id = g.nodes_created;

        g.nodes.push_back(n);

        g.nodes_created++;

    }
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

    if (g.node_conn_index == -1) {

        g.node_conn_index = a.index;

    } else {

        interactable_node *n1 = &(g.nodes[g.node_conn_index]);
        interactable_node *n2 = &(g.nodes[a.index]);

        connection c = connection(n1, n2, 0);

        g.connections.push_back(c);

        g.node_conn_index = -1;

    }

}

void delete_connection(graph_data &g, action a) {

    g.connections.erase(g.connections.begin() + a.index);

}


void set_start_node(graph_data &g, action a) {

    //TODO make better solution then clearing old nodes
    for (interactable_node &n : g.nodes) {
        if (n.node_type == START) {
            n.node_type = NORMAL;
        }
    }

    g.nodes[a.index].node_type = START;

}

void toggle_conn_dir(graph_data &g, action a) {

    connection &c = g.connections[a.index];
    interactable_node *tmp = c.start;
    c.start = c.end;
    c.end = tmp;
}

void set_end_node(graph_data &g, action a) {

    //TODO make better solution then clearing old nodes
    for (interactable_node &n : g.nodes) {
        if (n.node_type == END) {
            n.node_type = NORMAL;
        }
    }

    g.nodes[a.index].node_type = END;

}

void clear_graph(graph_data &g) {

    g.connections.clear();
    g.nodes.clear();
    g.nodes_created = 0;

}

void toggle_processing(graph_data &g) {

    if (g.processing == 0) {
        g.processing = 1;
        g.traces_processed = 0;
    } else {
        g.processing = 0;
    }

}
// in which i create the most convoluted graph search possible...
int check_trace_validity(graph_data &g, trace &t) {

    std::vector<interactable_node> node_list;

    for (interactable_node n : g.nodes) {
        if (n.node_type == START) { 
            node_list.push_back(n);
        }
    }
   
    for (event e : t.events) {

        int event_valid = 0;
        interactable_node valid_node;

        for (interactable_node n : node_list) {

            if (e.name == n.type) { 
                event_valid = 1; 
                valid_node = n;
            }    
        }

        if (!event_valid) { 
            t.valid = 0;
            return 0; 
        }

        node_list.clear();
        for (connection c : g.connections) {
            if (c.start->id == valid_node.id) {
                node_list.push_back(*(c.end));
            }
        }
    }
    log("YES");
    t.valid = 1;
    return 1;
}

int process_traces(xes_data &d, graph_data &g, int start) {

    int traces = 0;
    int i;
    for (i = start; (i < start + 100) && (i < (int)d.traces.size()); i++) {

        check_trace_validity(g, d.traces[i]);

        traces++;
    }

    if (i == (int)d.traces.size()) {
        g.processing = 0;
    }

    return traces;
}

std::vector<trace> get_invalid_traces(xes_data &d) {

    std::vector<trace> invalid_traces;

    for (trace t : d.traces) {

        if (!t.valid) {
            invalid_traces.push_back(t);
        }
    }

    return invalid_traces;
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
        case TOGGLE_DIRECTION:  { toggle_conn_dir(g, a);   break; }
        case TOGGLE_PROCESSING: { toggle_processing(g);    break; }
        case CLEAR_GRAPH:       { clear_graph(g);          break; }
        case SET_START_NODE:    { set_start_node(g, a);    break; }
        case SET_END_NODE:      { set_end_node(g, a);      break; }
  
    }

}



int intersecting_node(Vector2 pos, graph_data &g, int i) {

    interactable_node n = g.nodes[i];
    Vector2 node_pos = AddVector2(n.pos, g.offset);
    return CheckCollisionPointCircle(pos, node_pos, g.node_radius);

}

int intersecting_conn(Vector2 pos, graph_data &g, int i) {

    connection c = g.connections[i];
    Vector2 n1_pos = AddVector2(c.start->pos, g.offset);
    Vector2 n2_pos = AddVector2(c.end->pos, g.offset);

    return CheckCollisionPointLine(pos, n1_pos, n2_pos, 5); 

}


void add_action(std::vector<action> &l, Action action_type, int i) {

    action a = { action_type, i, "" };
    l.push_back(a);

}
//Any item that overlaps with the click gets added
//Do this by simply looping through every single item for now...
void update_action_list(Vector2 pos, graph_data &g) {

    
    g.action_list.clear();
    //For each new item sort created, simply add new for loop and set interaction actions.
    //TODO - differentiate between different items

    //NODES
    for (size_t i = 0; i < g.nodes.size(); i++) {

        //Add node actions
        if (intersecting_node(pos, g, i)) {

            add_action(g.action_list, CREATE_CONNECTION, (int)i);
            add_action(g.action_list, SET_START_NODE, (int)i);
            add_action(g.action_list, SET_END_NODE, (int)i);
            add_action(g.action_list, DELETE_NODE, (int)i);

        }

    }

    //CONNECTIONS
    for (size_t i = 0; i < g.connections.size(); i++) {

        //Add connection actions
        if (intersecting_conn(pos, g, i)) {

            add_action(g.action_list, TOGGLE_DIRECTION, (int)i);
            add_action(g.action_list, DELETE_CONNECTION, (int)i);

        }

    }

    //Always available actions.
    add_action(g.action_list, CREATE_NODE, -1);
    add_action(g.action_list, TOGGLE_PROCESSING, -1);
    add_action(g.action_list, CLEAR_GRAPH, -1);
    add_action(g.action_list, RESET_CAMERA, -1);
    add_action(g.action_list, CANCEL, -1);
 
}

void logic_graph(mouse &m, graph_data &g, xes_data &d) {

    if (g.processing) {
        std::string s = "processing: " + std::to_string(g.traces_processed);
        log(s);
        g.traces_processed += process_traces(d, g, g.traces_processed);
    }

    if (g.menu_active) { g.menu_active = CheckCollisionPointRec(m.pos, g.menu_loc); }

    if (g.menu_active) {

        g.menu_loc = calc_menu_pos(m, g);
        int menu_y = m.pos.y - g.menu_loc.y;
        g.item_hovered = menu_y / g.item_height -1;

        if (m.left_click) {
            g.item_selected = g.item_hovered;
            g.item_hovered  = -1;
            g.menu_active = 0;
            if (g.item_selected != -1 && g.item_selected < (int)g.action_list.size()) {
                do_menu_action(g);
            } 
        }

    } else {

        g.hover_node_index = -1;
        for (size_t i = 0; i < g.nodes.size(); i++) {
            if (intersecting_node(m.pos, g, i)) {
                g.hover_node_index = i;
                //log("hover node");
            }
        }

        g.hover_conn_index = -1;
        for (size_t i = 0; i < g.connections.size(); i++) {
            if (intersecting_conn(m.pos, g, i)) {
                g.hover_conn_index = i;
                //log("hover conn");
            }
        }

        

        if (m.left_click && (g.hover_node_index != -1 || g.hover_conn_index != -1)) {
            if (g.hover_node_index != -1) {
                g.active_node_index = g.hover_node_index;
                log("active conn set");
            } else {
                g.active_conn_index = g.hover_conn_index;
                log("active node set");
            }

        }

        if (m.right_click) {
            log("menu activated");
            g.menu_active = 1;
            g.old_mouse_pos = m.pos;

            update_action_list(m.pos, g);

            g.menu_loc = calc_menu_pos(m, g);

        } else if (m.left_down) {

            if (g.active_node_index == -1 && g.active_conn_index == -1) {
                g.offset = AddVector2(g.offset, m.delta);
            } else {
                if (g.active_node_index != -1) {
                    g.nodes[g.active_node_index].pos = AddVector2(g.nodes[g.active_node_index].pos, m.delta);
                } else {
                    connection c = g.connections[g.active_conn_index];
                    c.start->pos = AddVector2(c.start->pos, m.delta);
                    c.end->pos = AddVector2(c.end->pos, m.delta);
                }
            }
        
        } else if (m.left_released) {

            g.active_node_index = -1;
            g.active_conn_index = -1;
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
    trace_data td    = create_trace_data(log_data);
 

    while (!WindowShouldClose()) {

        // &&&&&&&&&&&&&&&&&&&&&&&&&&&&&& GLOBAL INPUT &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

        if (IsKeyPressed(KEY_F11)) { ToggleFullscreen();                }
        if (IsKeyPressed(KEY_C))   { create_subwindow(windows, DATA);   }
        if (IsKeyPressed(KEY_D))   { delete_focused_subwindow(windows); }

        screen_size.width  = GetScreenWidth();
        screen_size.height = GetScreenHeight();

        update_subwindow_sizes(windows, screen_size);

        m.pos           = GetMousePosition();
        m.delta         = GetMouseDelta();
        m.wheel_delta   = GetMouseWheelMove();
        m.left_click    = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        m.right_click   = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        m.left_down     = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        m.left_released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);  

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
        else if (IsKeyPressed(KEY_FIVE))   { w.state = TRACE;    } 

        // &&&&&&&&&&&&&&&&&&&&& LOGIC &&&&&&&&&&&&&&&&&&

        switch (w.state) {

            case DATA:     { logic_data    (m, d);        break; }
            case SUMMARY:  { logic_summary (m, s);        break; }
            case TIMELINE: { logic_timeline(m, t);        break; }
            case GRAPH:    { logic_graph(m, g, log_data); break; }
            case TRACE:    { logic_trace(m, td);          break; }

        } 
        

        //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Draw @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        BeginDrawing();

        for (window &w : windows) {

            switch (w.state) {
                case DATA:     { render_data    (w, d); break; }
                case SUMMARY:  { render_summary (w, s); break; } 
                case TIMELINE: { render_timeline(w, t); break; }
                case GRAPH:    { render_graph   (w, g); break; }
                case TRACE:    { render_trace   (w, td); break; }


            }
        }

        DrawText(global_msg.c_str(), 10, 10, 8, ORANGE);

        ClearBackground(RAYWHITE);
        EndDrawing();
    }

    CloseWindow();   
    return 0;
}