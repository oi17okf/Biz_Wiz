#include <iostream>
#include "rapidcsv.h"
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include "rlgl.h"
#include "tinyxml2.h"
#include <vector>
#include <string>
#include <algorithm>
#include <ctime>
#include <limits>
#include <math.h>


//struct menu;
struct action;
struct graph_data;

using namespace tinyxml2;

std::string map_source = "cubicmap.png"; 
std::string atlas_source = "cubicmap_atlas1.png";

std::string global_msg = "Welcome to Biz_Wiz";

std::vector<std::string> global_log;

void log(std::string s) {
    std::cout << "LOG: " << s << std::endl;

    global_log.insert(global_log.begin(), s);
    
    int size = global_log.size();

    if (size > 5) {
        global_log.pop_back();
    }
}

void log(std::string s, int i) {

    s += std::to_string(i);
    log(s);
}

void log(std::string s, float i) {

    s += std::to_string(i);
    log(s);
}

void assertt(int val, std::string msg) {

    if (val == 0) {

        log("assert failed: " + msg);

    }
}


Model load_generic_model(std::string model_path, std::string texture_path) {

    Model m = LoadModel(model_path.c_str());                
    Texture2D t = LoadTexture(texture_path.c_str()); 
    m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = t;     

    return m;   

}


enum WindowState {
    WINDOW_NULL,
    DATA,  
    SUMMARY,  
    TIMELINE, 
    GRAPH,
    TRACE
};

const std::map<WindowState, std::string> windowstate_names = {

    { WINDOW_NULL, "WINDOW_NULL"}, 
    { DATA,        "DATA" },
    { SUMMARY,     "SUMMARY" },
    { TIMELINE,    "TIMELINE" },
    { GRAPH,       "GRAPH" },
    { TRACE,       "TRACE" },  
};

std::string windowstate_to_string(WindowState w) {

    auto it = windowstate_names.find(w);

    if (it != windowstate_names.end()) { return it->second;       } 
    else                               { return "UNKNOWN WINDOWSTATE"; }
}

WindowState string_to_windowstate(std::string s) {

    for (auto it = windowstate_names.begin(); it != windowstate_names.end(); it++) {

        if (it->second == s) { return it->first; }

    }

    log("WindowState not found, returning data as backup...");
    return DATA;
}

enum NodeType {
    NORMAL,  
    START,  
    END    
};

enum TimeType {

    NONE,
    SECONDS,
    MINUTES,
    HOURS,
    DAYS,
    MONTHS,
    YEARS
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
    SET_END_NODE,
    MAKE_WALL, 
    REMOVE_WALL,
    HIDE_MOUSE,
    CREATE_SCREEN,
    REMOVE_SCREEN,
    CREATE_DISPLAY,
    REMOVE_DISPLAY,
    SAVE_STATE, 
    LOAD_STATE  

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
    { TOGGLE_PROCESSING, "Toggle_Processing" },
    { CLEAR_GRAPH,       "Clear graph" }, 
    { SET_END_NODE,      "Set end node" },
    { SET_START_NODE,    "Set start node" },
    { MAKE_WALL,         "Make wall" },
    { REMOVE_WALL,       "Remove wall" },
    { HIDE_MOUSE,        "Hide mouse" },
    { CREATE_SCREEN,     "Create screen" }, //mash into object later?
    { REMOVE_SCREEN,     "Remove screen" },
    { CREATE_DISPLAY,    "Create display" },
    { REMOVE_DISPLAY,    "Remove display" },
    { SAVE_STATE,        "Save state" },
    { LOAD_STATE,        "Load state" },
    
};

std::string action_to_string(Action a) {

    auto it = action_names.find(a);

    if (it != action_names.end()) { return it->second;       } 
    else                          { return "UNKNOWN ACTION"; }
}

Action string_to_action(std::string s) {

    for (auto it = action_names.begin(); it != action_names.end(); it++) {

        if (it->second == s) { return it->first; }

    }

    log("Action not found, returning CANCEL as backup...");
    return CANCEL;
}




enum DisplayType {

    DISPLAY_NULL,
    SINGLE_TRACE_TIME,
    SINGLE_TRACE_X,
    MULTIPLE_TRACE_TIME,
    MULTIPLE_TRACE_X

};

const std::map<DisplayType, std::string> displaytype_names = {

    { DISPLAY_NULL,        "DISPLAY_NULL" },
    { SINGLE_TRACE_TIME,   "SINGLE_TRACE_TIME" },
    { SINGLE_TRACE_X,      "SINGLE_TRACE_X" }, 
    { MULTIPLE_TRACE_TIME, "MULTIPLE_TRACE_TIME" },
    { MULTIPLE_TRACE_X,    "MULTIPLE_TRACE_X" },
};

std::string displaytype_to_string(DisplayType d) {

    auto it = displaytype_names.find(d);

    if (it != displaytype_names.end()) { return it->second;       } 
    else                               { return "UNKNOWN DISPLAYTYPE"; }
}

DisplayType string_to_displaytype(std::string s) {

    for (auto it = displaytype_names.begin(); it != displaytype_names.end(); it++) {

        if (it->second == s) { return it->first; }

    }

    log("Displaytype not found, returning SINGLE_TRACE_TIME as backup...");
    return SINGLE_TRACE_TIME;
}



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
    int valid;
};

struct event_conn {

    int valid;
    time_t time;
    time_t expected_time;
};

struct trace {
    std::vector<event> events;
    std::vector<event_conn> connections;
    int valid;

};
std::vector<trace> traces;

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
    Vector2 start;
    Vector2 end;
};

struct display_3D {

    Vector3 loc;
    int index;
    Vector3 coords;
    float rot;

    DisplayType type;


};

struct screen_3D {

    Vector3 loc;
    float width;
    float height;
    float rot;
    Color c;
    int focused;
    Vector2 coords;
    float inset = 0.1;

    std::vector<Vector3> points;
    std::vector<Vector3> walls;
    std::vector<Vector3> scale;

    WindowState type;

    screen_3D(Vector3 pos, float w, float h, float r, WindowState t) : 
        loc(pos), width(w), height(h), rot(r), c(WHITE), focused(0), coords({ 0, 0 }), type(t) {

        float x = loc.x;
        float y = loc.y;
        float z = loc.z;

        points.push_back((Vector3) {x - width / 2 + inset, y - height / 2 + inset, z });
        points.push_back((Vector3) {x + width / 2 - inset, y - height / 2 + inset, z });
        points.push_back((Vector3) {x + width / 2 - inset, y + height / 2 - inset, z });
        points.push_back((Vector3) {x - width / 2 + inset, y + height / 2 - inset, z });

        walls.push_back((Vector3) {x - width / 2 + inset / 2, y, z});
        walls.push_back((Vector3) {x + width / 2 - inset / 2, y, z});
        walls.push_back((Vector3) {x, y - height / 2 + inset / 2, z});
        walls.push_back((Vector3) {x, y + height / 2 - inset / 2, z});
        walls.push_back((Vector3) {x, y, z -inset / 2});

        scale.push_back((Vector3) {0.1, 1, 0.1});
        scale.push_back((Vector3) {0.1, 1, 0.1});
        scale.push_back((Vector3) {1, 0.1, 0.1});
        scale.push_back((Vector3) {1, 0.1, 0.1});
        scale.push_back((Vector3) {0.8, 0.8, 0.05});
    }

    
};





struct text_3D {

    Vector3 loc;
    std::string text;
};

std::vector<text_3D> text_3D_list;

void store_3D_text(Vector3 loc, std::string s) {

    text_3D t = { loc, s };
    text_3D_list.push_back(t);

}

void draw_3D_text(Camera c) {

    for (text_3D t : text_3D_list) {

        Vector2 screenPosition = GetWorldToScreen(t.loc, c);
        DrawText(t.text.c_str(), screenPosition.x, screenPosition.y, 20, BLUE);
    }

    text_3D_list.clear();

}

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
    int active;

};



struct connection {

    interactable_node *start;
    interactable_node *end;
    time_t time;
    TimeType active_type;

    connection(interactable_node* s, interactable_node* e) : start(s), end(e) { time = 0; active_type = NONE; }
    connection(interactable_node* s, interactable_node* e, time_t t, TimeType typ) : start(s), end(e), time(t), active_type(typ) {}
};






struct action {
    Action type;
};

struct button_3D {

    Vector3 pos;
    int pressed = 0;
    action* a;
    std::string name;

};

struct menu {

    int active = 0;
    std::vector<action*> action_list;

    int index_hovered  = -1;
    int index_selected = -1;
    int index_height   = 15;
    Rectangle pos;
    Vector2 old_mouse_pos;

    int sub_menu_exists = 0;
    menu* sub_menu;

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

    menu menu;

    int node_conn_index = -1;
    int processing = 0;
    int traces_processed = 0;
    
};


void clear_action_list(std::vector<action*> &al) {

    for (action* a : al) {
        delete a;
    }

    al.clear();
}

struct cubic_map {

    Vector3 loc;
    Vector3 cube_size;
    Mesh mesh;
    Model model;
    Image image;
    Texture2D texture;
    int size_x;
    int size_y;
    std::vector<std::vector<int>> cubes;

};

//Todo add more if needed
enum CollisionType {

    NO_COLLISION,
    SCREEN_INSIDE,
    SCREEN_OUTSIDE,
    DISPLAY,
    BUTTON,
    OTHER,
    CUBICMAP_DEFAULT,
    CUBICMAP_FLOOR,
    CUBICMAP_CEILING

};

struct Collision_Results {

    CollisionType type;
    int index;
    Vector2 index_point;
    Vector3 end_point;
    float distance;

};

struct world {

    std::map<WindowState, RenderTexture2D> render_textures;
    std::vector<screen_3D> screens; 
    std::vector<display_3D> displays; 
    std::vector<button_3D> buttons; 
    cubic_map cubic_map;
    std::vector<trace> traces; //should be moved out?
    Camera camera;

};

struct action_cancel {

    Action type = CANCEL;
};

struct action_create_node {
    Action type;
    int index;
    std::string name;
    graph_data &g;
    menu &menuu;

    action_create_node(int i, graph_data &g_ref, menu &menu_ref) : type(CREATE_NODE), index(i), g(g_ref), menuu(menu_ref) {
        name = g.node_names[index];
    }

};

struct action_create_connection {
    Action type;
    int index;
    graph_data &g;

    action_create_connection(int i, graph_data &g) : type(CREATE_CONNECTION), index(i), g(g) {}
};

struct action_delete_node {
    Action type;
    int index;
    graph_data &g;

    action_delete_node(int i, graph_data &g) : type(DELETE_NODE), index(i), g(g) {}
};

struct action_delete_connection {
    Action type;
    int index;
    graph_data &g;

    action_delete_connection(int i, graph_data &g) : type(DELETE_CONNECTION), index(i), g(g) {}
};

struct action_reset_camera {
    Action type;
    graph_data &g;

    action_reset_camera(graph_data &g) : type(RESET_CAMERA), g(g) {}
};

struct action_toggle_direction {
    Action type;
    int index;
    graph_data &g;

    action_toggle_direction(int i, graph_data &g) : type(TOGGLE_DIRECTION), index(i), g(g) {}
};

struct action_toggle_processing {
    Action type;
    graph_data &g;

    action_toggle_processing(graph_data &g) : type(TOGGLE_PROCESSING), g(g) {}
};

struct action_clear_graph {
    Action type;
    graph_data &g;

    action_clear_graph(graph_data &g) : type(CLEAR_GRAPH), g(g) {}
};

struct action_set_end_node {
    Action type;
    int index;
    graph_data &g;

    action_set_end_node(int i, graph_data &g) : type(SET_END_NODE), index(i), g(g) {}
};

struct action_set_start_node {
    Action type;
    int index;
    graph_data &g;

    action_set_start_node(int i, graph_data &g) : type(SET_START_NODE), index(i), g(g) {}
};

struct action_make_wall {
    Action type;
    Vector2 index;
    world &w;

    action_make_wall(world &w, Vector2 i) : type(MAKE_WALL), index(i), w(w) {}
};

struct action_remove_wall {
    Action type;
    Vector2 index;
    world &w;


    action_remove_wall(world &wref, Vector2 i) : type(REMOVE_WALL), index(i), w(wref) {}
};

struct action_hide_mouse {
    Action type;
    mouse &m;

    action_hide_mouse(mouse &mouse_ref) : type(HIDE_MOUSE), m(mouse_ref) {}
};

struct action_create_screen {
    Action type;
    Vector3 pos;
    float width;
    float height;
    float rot;
    WindowState windowtype;

    world &w;
    menu &m;

    action_create_screen(world &world, menu &m, Vector3 p, float w, float h, float r, WindowState wt) : 
        type(CREATE_SCREEN), w(world), m(m), pos(p), width(w), height(h), rot(r), windowtype(wt) {}
};

struct action_remove_screen {
    Action type;
    int index;
    world &w;

    action_remove_screen(world &world, int i) : type(REMOVE_SCREEN), index(i), w(world) {}
};

struct action_create_display {
    Action type;
    Vector3 pos;
    float rot;
    DisplayType displaytype;

    world &w;
    menu &m;
 
    action_create_display(world &world, menu &m, Vector3 p, float r, DisplayType dt) : 
        type(CREATE_DISPLAY), w(world), m(m), pos(p), rot(r), displaytype(dt) {}
};

struct action_remove_display {
    Action type;
    int index;
    world &w;

    action_remove_display(world &world, int i) : type(REMOVE_DISPLAY), index(i), w(world) {}
};

struct action_save_state {
    Action type;
    int confirm;
    std::string name;

    world &w;
    menu &m;
    

    action_save_state(world &world, menu &m, int c) : type(SAVE_STATE), w(world), m(m), confirm(c) {
             if (confirm == 0) { name = "NO"; }
        else if (confirm == 1) { name = "YES"; }
    }
};

struct action_load_state {
    Action type;
    int confirm;
    std::string name;

    world &w;
    menu &m;

    action_load_state(world &world, menu &m, int c) : type(LOAD_STATE), w(world), m(m), confirm(c) { 
             if (confirm == 0) { name = "NO"; }
        else if (confirm == 1) { name = "YES"; }}
};

void add_action(Action type, std::vector<action*> &l, action* a) {

    assertt((type == a->type), "in add_action, mismatching types");

    l.push_back(a);
}


void create_display(world &w, Vector3 pos, float rot, DisplayType type) {

    display_3D display;
    display.loc = pos;
    display.rot = rot;
    display.type = type;

    w.displays.push_back(display);
}


void create_display(action_create_display* a) {

    if (a->displaytype == DISPLAY_NULL) {

        a->m.active = 1;
        clear_action_list(a->m.action_list);
        for (auto w : displaytype_names) {
            if (w.first == DISPLAY_NULL) { continue; }
            
            action* a1 = (action*) new action_create_display(a->w,
                a->m, a->pos, a->rot, w.first);
            a->m.action_list.push_back(a1);
        }
        add_action(CANCEL, a->m.action_list, (action*) new action_cancel());

    } else {

        create_display(a->w, a->pos, a->rot, a->displaytype);

    }

}



void remove_display(action_remove_display* a) {
   
    a->w.displays.erase(a->w.displays.begin() + a->index);

}

void create_screen(world &w, Vector3 pos, float width, float height, float rot, WindowState type) {

        screen_3D s(pos, width, height, rot, type);

        auto search = w.render_textures.find(type);
        if (search == w.render_textures.end()) {
            RenderTexture2D t = LoadRenderTexture(512, 512);
            w.render_textures.insert(w.render_textures.begin(), {type, t} );
        } 

        s.loc.y = 0.5;
        s.loc.x = std::round(s.loc.x);
        s.loc.z = std::round(s.loc.z);

        w.screens.insert(w.screens.begin(), s);
}

void create_screen(action_create_screen* a) {

    if (a->windowtype == WINDOW_NULL) {

        a->m.active = 1;
        clear_action_list(a->m.action_list);
        for (auto w : windowstate_names) {
            if (w.first == WINDOW_NULL) { continue; }
            
            action* a1 = (action*) new action_create_screen(a->w,
                a->m, a->pos, a->width, a->height, a->rot, w.first);
            a->m.action_list.push_back(a1);
        }
        add_action(CANCEL, a->m.action_list, (action*) new action_cancel());

    } else {

        create_screen(a->w, a->pos, a->width, a->height, a->rot, a->windowtype);

    }
}



void remove_screen(action_remove_screen* a) {

    WindowState type = a->w.screens.at(a->index).type;

    int count = 0;
    for (auto it = a->w.screens.begin(); it != a->w.screens.end(); it++) {
        if (it->type == type) { count++; }
    }

    if (count == 1) {
        a->w.render_textures.erase(type);
    }

    a->w.screens.erase(a->w.screens.begin() + a->index);

}




/*
void press_button(std::vector<button_3D> &buttons, action a) {

    button_3D &b = buttons[a.index];
    b.pressed = 60;
    do_action(b.action)

}
*/

void add_button(std::vector<button_3D> &buttons, Action a, Vector3 loc) {

    button_3D b;
    b.pos = loc;
    b.pressed = 0;

    b.a = (action*) new action_cancel();
    b.name = action_to_string(b.a->type);

    buttons.push_back(b);

}

void DrawButton(const button_3D &b) {

    Color c = GREEN;
    if (b.pressed) { c = RED; }

    DrawCube(b.pos, 0.5, 0.5, 0.1, BROWN);  
    DrawCube(b.pos, 0.1, 0.1, 0.5, c);  


}

void DrawButtons(std::vector<button_3D> &buttons) {

    for (button_3D &b : buttons) {
        if (b.pressed > 0) { b.pressed--; }
        DrawButton(b);
    }
}


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

Vector3 AddVector3(Vector3 a, Vector3 b) {

    Vector3 c;
    c.x = a.x + b.x;
    c.y = a.y + b.y;
    c.z = a.z + b.z;
    return c;

}

Vector3 SubVector3(Vector3 a, Vector3 b) {

    Vector3 c;
    c.x = a.x - b.x;
    c.y = a.y - b.y;
    c.z = a.z - b.z;
    return c;

}

Vector3 ScaleVector3(Vector3 a, float scale) {

    Vector3 c;
    c.x = a.x * scale;
    c.y = a.y * scale;
    c.z = a.z * scale;
    return c;

}


//assumes cube_size 1,1,1
int is_outside_map(Vector3 p, cubic_map &c) {

    Vector3 normed_p = SubVector3(p, c.loc);

    if (normed_p.x < 0) { return 1; }
    if (normed_p.z < 0) { return 1; }
    if (normed_p.x >= c.size_x) { return 1; }
    if (normed_p.z >= c.size_y) { return 1; }

    return 0;
}

int is_outside_map(Vector2 p, cubic_map &c) {

    Vector3 v = { p.x, 0.5, p.y };
    return is_outside_map(v, c);
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


std::string time_to_string(time_t t) {

    std::tm* t_i = std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(t_i, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

void parse_xes(XMLElement* root, xes_data &data) {

    for (XMLElement* log_trace = root->FirstChildElement("trace"); 
        log_trace != nullptr; log_trace = log_trace->NextSiblingElement("trace")) {

        trace t;
        t.valid = 0;
        int first = 1;

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

            if (!first) {
                event_conn c;
                c.valid = 0;
                c.expected_time = 0;
                c.time = e.time - t.events.back().time;
                t.connections.push_back(c);
            }
            first = 0;

            t.events.push_back(e);
        }

        data.traces.push_back(t);

    }

    data.events_per_trace = (float)data.events / (float)data.traces.size();
}

void DrawScreen(screen_3D s, Texture2D texture) {
    
    float x = s.loc.x;
    float y = s.loc.y;
    float z = s.loc.z;

    /*
    Vector3 p1 = { -s.width / 2 + s.inset, -s.height / 2 + s.inset, 0 };
    Vector3 p2 = {  s.width / 2 - s.inset, -s.height / 2 + s.inset, 0 };
    Vector3 p3 = {  s.width / 2 - s.inset,  s.height / 2 - s.inset, 0 };
    Vector3 p4 = { -s.width / 2 + s.inset,  s.height / 2 - s.inset, 0 };

    Vector3 c1 = { -s.width / 2 + s.inset / 2, 0, 0};
    Vector3 c2 = {  s.width / 2 - s.inset / 2, 0, 0 };
    Vector3 c3 = { 0, -s.height / 2 + s.inset / 2, 0 };
    Vector3 c4 = { 0,  s.height / 2 - s.inset / 2, 0 };
    Vector3 wall = { 0, 0, -s.inset / 2 };

    */
    rlSetTexture(texture.id);

    rlPushMatrix();
    rlTranslatef(x,y,z);
    rlRotatef(s.rot, 0, 1, 0);
    //rlScalef(screen.width, 1, screen.height);
    rlTranslatef(-x,-y,-z);

    rlBegin(RL_QUADS);
    rlColor4ub(s.c.r, s.c.g, s.c.b, s.c.a);

    rlNormal3f(0.0f, 0.0f, 1.0f);      
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(s.points[0].x, s.points[0].y, s.points[0].z);  
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(s.points[1].x, s.points[1].y, s.points[1].z);  
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(s.points[2].x, s.points[2].y, s.points[2].z);  
    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(s.points[3].x, s.points[3].y, s.points[3].z); 

    rlEnd();

    DrawCubeV(s.walls[0], s.scale[0], LIGHTGRAY);
    DrawCubeV(s.walls[1], s.scale[1], LIGHTGRAY);
    DrawCubeV(s.walls[2], s.scale[2], LIGHTGRAY);
    DrawCubeV(s.walls[3], s.scale[3], LIGHTGRAY);
    DrawCubeV(s.walls[4], s.scale[4], GRAY);
    rlPopMatrix();

    rlSetTexture(0);
}

void DrawScreens(std::vector<screen_3D> &screens, std::map<WindowState, RenderTexture2D> &render_textures) {

    for (screen_3D screen : screens) {

        RenderTexture2D texture = render_textures[screen.type]; 

        DrawScreen(screen, texture.texture);

    }
}

//Raylib example
static void DrawTextCodepoint3D(Font font, int codepoint, Vector3 position, float fontSize, bool backface, Color tint, int debug)
{
    // Character index position in sprite font
    // NOTE: In case a codepoint is not available in the font, index returned points to '?'
    int index = GetGlyphIndex(font, codepoint);
    float scale = fontSize/(float)font.baseSize;

    // Character destination rectangle on screen
    // NOTE: We consider charsPadding on drawing
    position.x += (float)(font.glyphs[index].offsetX - font.glyphPadding)/(float)font.baseSize*scale;
    position.z += (float)(font.glyphs[index].offsetY - font.glyphPadding)/(float)font.baseSize*scale;

    // Character source rectangle from font texture atlas
    // NOTE: We consider chars padding when drawing, it could be required for outline/glow shader effects
    Rectangle srcRec = { font.recs[index].x - (float)font.glyphPadding, font.recs[index].y - (float)font.glyphPadding,
                         font.recs[index].width + 2.0f*font.glyphPadding, font.recs[index].height + 2.0f*font.glyphPadding };

    float width = (float)(font.recs[index].width + 2.0f*font.glyphPadding)/(float)font.baseSize*scale;
    float height = (float)(font.recs[index].height + 2.0f*font.glyphPadding)/(float)font.baseSize*scale;

    if (font.texture.id > 0)
    {
        const float x = 0.0f;
        const float y = 0.0f;
        const float z = 0.0f;

        // normalized texture coordinates of the glyph inside the font texture (0.0f -> 1.0f)
        const float tx = srcRec.x/font.texture.width;
        const float ty = srcRec.y/font.texture.height;
        const float tw = (srcRec.x+srcRec.width)/font.texture.width;
        const float th = (srcRec.y+srcRec.height)/font.texture.height;

        if (debug) DrawCubeWiresV((Vector3){ position.x + width/2, position.y, position.z + height/2}, (Vector3){ width, 10, height }, GREEN);

        rlCheckRenderBatchLimit(4 + 4*backface);
        rlSetTexture(font.texture.id);

        rlPushMatrix();
            rlTranslatef(position.x, position.y, position.z);

            rlBegin(RL_QUADS);
                rlColor4ub(tint.r, tint.g, tint.b, tint.a);

                // Front Face
                rlNormal3f(0.0f, 1.0f, 0.0f);                                   // Normal Pointing Up
                rlTexCoord2f(tx, ty); rlVertex3f(x,         y, z);              // Top Left Of The Texture and Quad
                rlTexCoord2f(tx, th); rlVertex3f(x,         y, z + height);     // Bottom Left Of The Texture and Quad
                rlTexCoord2f(tw, th); rlVertex3f(x + width, y, z + height);     // Bottom Right Of The Texture and Quad
                rlTexCoord2f(tw, ty); rlVertex3f(x + width, y, z);              // Top Right Of The Texture and Quad

                if (backface)
                {
                    // Back Face
                    rlNormal3f(0.0f, -1.0f, 0.0f);                              // Normal Pointing Down
                    rlTexCoord2f(tx, ty); rlVertex3f(x,         y, z);          // Top Right Of The Texture and Quad
                    rlTexCoord2f(tw, ty); rlVertex3f(x + width, y, z);          // Top Left Of The Texture and Quad
                    rlTexCoord2f(tw, th); rlVertex3f(x + width, y, z + height); // Bottom Left Of The Texture and Quad
                    rlTexCoord2f(tx, th); rlVertex3f(x,         y, z + height); // Bottom Right Of The Texture and Quad
                }
            rlEnd();
        rlPopMatrix();

        rlSetTexture(0);
    }
}

//Raylib example
static void DrawText3D(Font font, std::string s, Vector3 position, float fontSize, float fontSpacing, float lineSpacing, bool backface, Color tint, int debug)
{
    const char* text = s.c_str();
    int length = TextLength(text);          // Total length in bytes of the text, scanned by codepoints in loop

    float textOffsetY = 0.0f;               // Offset between lines (on line break '\n')
    float textOffsetX = 0.0f;               // Offset X to next character to draw

    float scale = fontSize/(float)font.baseSize;

    for (int i = 0; i < length;)
    {
        // Get next codepoint from byte string and glyph index in font
        int codepointByteCount = 0;
        int codepoint = GetCodepoint(&text[i], &codepointByteCount);
        int index = GetGlyphIndex(font, codepoint);

        // NOTE: Normally we exit the decoding sequence as soon as a bad byte is found (and return 0x3f)
        // but we need to draw all of the bad bytes using the '?' symbol moving one byte
        if (codepoint == 0x3f) codepointByteCount = 1;

        if (codepoint == '\n')
        {
            // NOTE: Fixed line spacing of 1.5 line-height
            // TODO: Support custom line spacing defined by user
            textOffsetY += scale + lineSpacing/(float)font.baseSize*scale;
            textOffsetX = 0.0f;
        }
        else
        {
            if ((codepoint != ' ') && (codepoint != '\t'))
            {
                DrawTextCodepoint3D(font, codepoint, (Vector3){ position.x + textOffsetX, position.y, position.z + textOffsetY }, fontSize, backface, tint, debug);
            }

            if (font.glyphs[index].advanceX == 0) textOffsetX += (float)(font.recs[index].width + fontSpacing)/(float)font.baseSize*scale;
            else textOffsetX += (float)(font.glyphs[index].advanceX + fontSpacing)/(float)font.baseSize*scale;
        }

        i += codepointByteCount;   // Move text bytes counter to next codepoint
    }
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


void render_summary(const window &w, const summary_data &s) {

  
 

   // render_border(w);


}

void render_timeline(const window &w, const timeline_data &d) {



    //render_border(w);


    // Drawtimeline

    // Draw min/max values

    // Draw nodes
   

}



void render_data(const window &w, const data_data &d) {



    //render_border(w);



}

void render_graph_node(const window &w, const graph_data &g, interactable_node n, int hover, int active) {

    int x = g.offset.x + w.start.x + n.pos.x;
    int y = g.offset.y + w.start.y + n.pos.y;
    Color c = GREEN;

    c = hover ? DARKGREEN : c;
    c = active ? MAROON : c;
    DrawCircleLines(x, y, g.node_radius, c);
    DrawText(n.type.c_str(), x, y, 8, BLUE);

    if (n.node_type == START) {
        DrawText("S", x, y - 10, 12, RED);
    }

    if (n.node_type == END) {
        DrawText("E", x, y - 10, 12, RED);
    }
}

void render_graph_conn(const window &w, const graph_data &g, connection c, int hover, int active) {

    Vector2 start = { g.offset.x + w.start.x + c.start->pos.x , g.offset.y + w.start.y + c.start->pos.y };
    Vector2 end   = { g.offset.x + w.start.x + c.end->pos.x   , g.offset.y + w.start.y + c.end->pos.y   };
    Color col = GREEN;
    col = hover ? DARKGREEN : col;
    col = active ? MAROON : col;

    DrawLineEx(start, end, 3, col);

    int mid_x = (start.x + end.x) / 2;
    int mid_y = (start.y + end.y) / 2;

    float dx = end.x - start.x;
    float dy = end.y - start.y;

    float angle = atan2f(dy, dx);
    int size = 20;

    float arrow_angle = PI / 6; // 30 degrees
    int arrow_x1 = mid_x - size * cosf(angle - arrow_angle);
    int arrow_y1 = mid_y - size * sinf(angle - arrow_angle);
    int arrow_x2 = mid_x - size * cosf(angle + arrow_angle);
    int arrow_y2 = mid_y - size * sinf(angle + arrow_angle);

    DrawLine( mid_x, mid_y ,  arrow_x1, arrow_y1 ,  col);
    DrawLine( mid_x, mid_y ,  arrow_x2, arrow_y2 ,  col);

    std::string val = time_to_string(c.time);

    DrawText(val.c_str(), mid_x, mid_y, 8, BLUE);
}

Rectangle calc_menu_pos(const mouse &m, const menu &menu) {

    int l = 0;
    for (action* a : menu.action_list) {

        std::string s = action_to_string(a->type);
        int action_len = MeasureText(s.c_str(), 10);
        action_len += 10; //add buffer space
        int name_len = 0;
        if (a->type == CREATE_NODE) {
            action_create_node* a1 = (action_create_node*) a;
            name_len = MeasureText(a1->name.c_str(), 10);
        }

        if (name_len > 0) { name_len++; } //add space
        if (action_len + name_len > l) { l = action_len + name_len; }
    }

    Vector2 m_pos = menu.old_mouse_pos;
    int items = menu.action_list.size();
    int height = (items + 1) * menu.index_height;
    int width = l < 60 ? 60 : l;

    int x = m_pos.x - width / 2;
    x = x < 0 ? 0 : x;
    x = x + width > m.end.x ? x - (x + width - m.end.x) : x;

    int y = m_pos.y;
    y = y + height > m.end.y ? y - (y + height - m.end.y) : y;

    Rectangle rec = { (float)x, (float)y, (float)width, (float)height };

    std::cout << x << y << width << height << std::endl;

    return rec;



}

void render_menu(const window &w, const menu &menu) {

    Rectangle menu_loc = { w.start.x + menu.pos.x, w.start.y + menu.pos.y, menu.pos.width, menu.pos.height };

    DrawRectangleRec(menu_loc, BROWN);
    DrawRectangle(menu_loc.x + 1, menu_loc.y + 1, menu_loc.width - 2, menu.index_height - 2, BLACK);     
    DrawRectangleLines(menu_loc.x + 1, menu_loc.y + menu.index_height, menu.pos.width - 2, menu.pos.height - menu.index_height - 2, BLACK);

    DrawText("Choose option", menu_loc.x + 4, menu_loc.y + 3, 10, BROWN);
    int i = 0;  

    for (action* a : menu.action_list) {

        Color c = WHITE;
        c = i == menu.index_hovered ? YELLOW : c;
        std::string s = action_to_string(a->type);
        DrawText(s.c_str(), menu_loc.x + 4, menu_loc.y + menu.index_height * (i+1) + 3, 10, c);

        int len = MeasureText(s.c_str(), 10);

        if (a->type == CREATE_NODE) {
            action_create_node* a1 = (action_create_node*) a;
            DrawText(a1->name.c_str(),  menu_loc.x + 4 + len + 3, menu_loc.y + menu.index_height * (i+1) + 3, 10, SKYBLUE);

        } else if (a->type == SAVE_STATE) {
            action_save_state* a1 = (action_save_state*) a;
            DrawText(a1->name.c_str(),  menu_loc.x + 4 + len + 3, menu_loc.y + menu.index_height * (i+1) + 3, 10, SKYBLUE);

        } else if (a->type == LOAD_STATE) {
            action_load_state* a1 = (action_load_state*) a;
            DrawText(a1->name.c_str(),  menu_loc.x + 4 + len + 3, menu_loc.y + menu.index_height * (i+1) + 3, 10, SKYBLUE);

        } else if (a->type == CREATE_SCREEN) {
            action_create_screen* a1 = (action_create_screen*) a;
            std::string screen_name = windowstate_to_string(a1->windowtype);
            DrawText(screen_name.c_str(),  menu_loc.x + 4 + len + 3, menu_loc.y + menu.index_height * (i+1) + 3, 10, SKYBLUE);
        } else if (a->type == CREATE_DISPLAY) {
            action_create_display* a1 = (action_create_display*) a;
            std::string screen_name = displaytype_to_string(a1->displaytype);
            DrawText(screen_name.c_str(),  menu_loc.x + 4 + len + 3, menu_loc.y + menu.index_height * (i+1) + 3, 10, SKYBLUE);


        }

        i++;
    }
    
   

}

void render_graph(const window &w, const graph_data &g) {


    //render_border(w);

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


    if (g.menu.active) {

        render_menu(w, g.menu);
    }

    if (g.processing) {

        std::string trace_status = "Traces processed: " + std::to_string(g.traces_processed);
        DrawText(trace_status.c_str(), w.start.x + 10, w.start.y + 10 , 10, YELLOW);
    }

}



void render_active_trace(const window &w, trace t) {

    int x = (w.start.x + w.end.x) / 2;

    int y = w.start.y + 20;

    for (size_t i = 0; i < t.events.size(); i++) {

        event &e = t.events[i];

        Color c = e.valid ? GREEN : RED;

        DrawCircleLines(x, y, 10, c);

        if (i != t.events.size() - 1) { 

            Color c1 = t.connections[i].valid ? GREEN : RED;
            DrawLine( x, y, x, y + 30, c1); 
            std::string s = time_to_string(t.connections[i].time);
            DrawText(s.c_str(), x + 10, y + 15, 8, c1);


        }

        DrawText(e.name.c_str(), x, y, 8, BLUE);
        if (i != t.events.size() - 1) { 
            y += 30;    
        }
    }
}

void render_active_trace_3D(Vector3 loc, trace t) {

    float x = loc.x;
    float y = loc.y;
    float z = loc.z;

    for (size_t i = 0; i < t.events.size(); i++) {

        event &e = t.events[i];

        Color c = e.valid ? GREEN : RED;

        DrawSphere( {x, y, z}, 10, c);  

        if (i != t.events.size() - 1) { 

            Color c1 = t.connections[i].valid ? GREEN : RED;
            DrawLine3D( {x, y, z }, {x, y + 50, z}, c1);
            //std::string s = time_to_string(t.connections[i].time);
            //DrawText(s.c_str(), x + 10, y + 15, 8, c1);

        }

        store_3D_text( {x, y, z}, e.name);

        if (i != t.events.size() - 1) { 
            y += 50;    
        }
    }

}



void render_active_trace_3D_cube(Vector3 loc, trace &t) {

    float x = loc.x;
    float y = loc.y;
    float z = loc.z;

    Color c = (Color){ 255, 0, 0, 255 };

    for (size_t i = 0; i < t.events.size() - 1; i++) {

        event &e = t.events[i];
        event &next_e = t.events[i + 1];

        float hour_diff = (next_e.time - e.time) / 3600.0f + 0.1f;
        hour_diff *= 0.1f;

        DrawCube( {x, y + hour_diff / 2, z}, 1, hour_diff, 1, c);  

        y += hour_diff;

        c.r -= 25;
        c.g += 25;
        

    }

}

void render_all_traces(std::vector<trace> &traces) {

    float x = 50;
    float y = 0;
    float z = 0;

    for (size_t i = 0; i < traces.size(); i++) {

        render_active_trace_3D_cube( {x, y, z}, traces[i]);
        z += 1;

    }

    DrawCube( {x, y, z}, 4, 50000, 4, BLUE); 
}

void render_trace(const window &w, const trace_data &t) {



    std::string s1 = "Invalid traces: " + std::to_string(t.invalid_traces) + " Total traces: " + std::to_string(t.traces.size() - 1);
    DrawText(s1.c_str(), w.start.x + 10, w.start.y + 10, 10, YELLOW);
    std::string s2 = "Rendering trace " + std::to_string(t.selected_trace) + " out of " + std::to_string(t.traces.size() - 1);
    Color c = t.current_valid ? GREEN : RED;
    DrawText(s2.c_str(), w.start.x + 10, w.start.y + 20, 10, c);

    trace active_trace = t.traces[t.selected_trace];

    render_active_trace(w, active_trace);

}


/*
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
*/
/*
void create_subwindow(std::vector<window> &windows, WindowState state) {

    window w;
    w.state = state;
    w.border = 2;

    windows.push_back(w);
}
*/

/*
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
*/

//todo - research if memeory leak w/ debugger
void refresh_cubic_map(cubic_map &c) {

    UnloadModel(c.model);

    c.mesh = GenMeshCubicmap(c.image, c.cube_size);
    c.model = LoadModelFromMesh(c.mesh);
    c.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = c.texture;  

    Color *map_pixels = LoadImageColors(c.image);

    c.cubes.clear();
    for (int i = 0; i < c.size_y; i++) {

        std::vector<int> cc;
        c.cubes.insert(c.cubes.end(), cc);

        for (int j = 0; j < c.size_x; j++) {

            Color col = map_pixels[i * c.size_x + j];
            int cube = col.r == 255 ? 1 : 0; //if color is white, insert wall, otherwise openspace.
            c.cubes[i].insert(c.cubes[i].end(), cube);

        }
    }      

    UnloadImageColors(map_pixels);
}

void clear_cubic_map(cubic_map &c) {

    if (IsImageReady(c.image)) { UnloadImage(c.image); }
    if (IsModelReady(c.model)) { UnloadModel(c.model); }
    if (IsTextureReady(c.texture)) { UnloadTexture(c.texture); }
    c.cubes.clear();
}

void save_cubic_map(const cubic_map &c) {

    if (IsImageReady(c.image)) { 


        bool success = ExportImage(c.image, map_source.c_str());   
        if (!success) { log("failed to export cubic map"); }
    } else {
        log("error, called savecubicmap on invalid cubicmap");
    }
}

cubic_map generate_cubic_map(cubic_map &c) {

    clear_cubic_map(c);
    c.loc = { 0, 0, 0 };
    c.cube_size = { 1, 1, 1 };

    c.image = LoadImage(map_source.c_str()); 

    Texture2D cubic_map_tex = LoadTextureFromImage(c.image);  //is this needed?
    c.size_x = cubic_map_tex.width;  
    c.size_y = cubic_map_tex.height;
    UnloadTexture(cubic_map_tex);

    c.mesh = GenMeshCubicmap(c.image, c.cube_size);
    c.model = LoadModelFromMesh(c.mesh);

    c.texture = LoadTexture(atlas_source.c_str());    
    c.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = c.texture;    

    Color *map_pixels = LoadImageColors(c.image);

    c.cubes.clear();
    for (int i = 0; i < c.size_y; i++) {

        std::vector<int> cc;
        c.cubes.insert(c.cubes.end(), cc);

        for (int j = 0; j < c.size_x; j++) {

            Color col = map_pixels[i * c.size_x + j];
            int cube = col.r == 255 ? 1 : 0; //if color is white, insert open space, otherwise wall.
            c.cubes[i].insert(c.cubes[i].end(), cube);

        }
    }      

    UnloadImageColors(map_pixels);

    return c;
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

    if (m.active) {
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
}

void logic_summary(mouse &m, summary_data &s) {

    //todo?
}

void logic_timeline(mouse &m, timeline_data &t) {

    //todoi
}

void logic_trace(mouse &m, trace_data &t) {

    if (m.active) {

        t.selected_trace -= m.wheel_delta;
        t.selected_trace = clamp(t.selected_trace, 0, t.traces.size() - 1);

        trace ct = t.traces[t.selected_trace];

        t.current_valid = ct.valid;
    }

}

void create_node(action_create_node* a) {

    graph_data &g = a->g;

    if (a->index == -1) { 

        g.menu.active = 1;

        clear_action_list(g.menu.action_list);
        int i = 0;
        for (std::string s : g.node_names) {

            add_action(CREATE_NODE, g.menu.action_list, (action*) new action_create_node(i, g, g.menu));

            i++;
        }

        add_action(CANCEL, g.menu.action_list, (action*) new action_cancel());

    } else {

        interactable_node n;
        n.pos = g.menu.old_mouse_pos;
        n.pos.x -= g.offset.x;
        n.pos.y -= g.offset.y;
        n.type = g.node_names[a->index];
        n.id = g.nodes_created;

        g.nodes.push_back(n);

        g.nodes_created++;

    }
}

void delete_node(action_delete_node* a) {

    graph_data &g = a->g;

    interactable_node* ptr = &(g.nodes[a->index]);

    for (std::vector<connection>::iterator it = g.connections.begin(); it != g.connections.end();) {
        if (it->start == ptr || it->end == ptr) { 
            it = g.connections.erase(it); 
            log("deleted connection while deleting node");
        } else { 
            it++; 
        }
    }

    g.nodes.erase(g.nodes.begin() + a->index);

}

void create_connection(action_create_connection* a) {

    graph_data &g = a->g;

    if (g.node_conn_index == -1) {

        g.node_conn_index = a->index;

    } else {

        interactable_node *n1 = &(g.nodes[g.node_conn_index]);
        interactable_node *n2 = &(g.nodes[a->index]);

        connection c = connection(n1, n2);

        g.connections.push_back(c);

        g.node_conn_index = -1;

    }

}

void delete_connection(action_delete_connection* a) {

    a->g.connections.erase(a->g.connections.begin() + a->index);

}


void set_start_node(action_set_start_node* a) {

    //TODO make better solution then clearing old nodes
    for (interactable_node &n : a->g.nodes) {
        if (n.node_type == START) {
            n.node_type = NORMAL;
        }
    }

    a->g.nodes[a->index].node_type = START;

}

void toggle_conn_dir(action_toggle_direction* a) {

    connection &c = a->g.connections[a->index];
    interactable_node *tmp = c.start;
    c.start = c.end;
    c.end = tmp;
}

void set_end_node(action_set_end_node* a) {

    //TODO make better solution then clearing old nodes
    for (interactable_node &n : a->g.nodes) {
        if (n.node_type == END) {
            n.node_type = NORMAL;
        }
    }

    a->g.nodes[a->index].node_type = END;

}

void clear_graph(action_clear_graph* a) {

    a->g.connections.clear();
    a->g.nodes.clear();
    a->g.nodes_created = 0;

}

void toggle_processing(action_toggle_processing* a) {

    if (a->g.processing == 0) {
        a->g.processing = 1;
        a->g.traces_processed = 0;
    } else {
        a->g.processing = 0;
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
   
    for (event &e : t.events) { e.valid = 0; }
    for (event_conn &c : t.connections) { c.valid = 0; }

    std::vector<time_t> expected_times;

    int conn_index = -1;
    for (event &e : t.events) {

        interactable_node valid_node;

        int time_index = 0;
        for (interactable_node n : node_list) {

            if (e.name == n.type) { 
                e.valid = 1; 
                valid_node = n;
                if (n.node_type != START) {
                    t.connections[conn_index].expected_time = expected_times[time_index];
                    if (t.connections[conn_index].time > t.connections[conn_index].expected_time) {
                        t.connections[conn_index].valid = 1;
                    } else {
                        t.connections[conn_index].valid = 0;
                    }
                }
            }  
            time_index++;  
        }

        if (!e.valid) { 
            t.valid = 0;
            return 0; 
        }

        node_list.clear();
        expected_times.clear();

        for (connection c : g.connections) {
            if (c.start->id == valid_node.id) {
                node_list.push_back(*(c.end));
                expected_times.push_back(c.time);
            }
        }
        conn_index++;
    }

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

void reset_camera(action_reset_camera* a) {
    a->g.offset = { 0, 0 };
}

void hide_mouse(mouse &m) {

    if (m.active == 0) { log("hide mouse called when it should not have been"); }

    m.active = 0;
    DisableCursor();  
}

void hide_mouse(action_hide_mouse* a) {

    hide_mouse(a->m);

}

void make_wall(action_make_wall* a) {

    cubic_map &c = a->w.cubic_map;

    if (is_outside_map(a->index, c)) {

        log("make_wall error");
        return;
    }

    Vector2 normed_point = { a->index.x - c.loc.x, a->index.y - c.loc.z };

    ImageDrawPixel(&(c.image), (int)normed_point.x, (int)normed_point.y, WHITE);
    refresh_cubic_map(c);
}

//ImageDrawPixel(Image *dst, int posX, int posY, Color color);  
void remove_wall(action_remove_wall* a) {

    cubic_map &c = a->w.cubic_map;

    if (is_outside_map(a->index, c)) {

        log("remove_wall error");
        return;
    }


    Vector2 normed_point = { a->index.x - c.loc.x, a->index.y - c.loc.z };

    ImageDrawPixel(&(c.image), (int)normed_point.x, (int)normed_point.y, BLACK);
    refresh_cubic_map(c);
    //c.cubes[(int)normed_point.y][(int)normed_point.x] = 0;
}

std::string map_file = "map.txt";

enum ReadMapMode {
    NOTHINGMODE,
    MAPMODE,
    SCREENMODE,
    DISPLAYMODE,
    BUTTONMODE
};

void load_state(world &w) {

    std::ifstream file(map_file);  // Open the file
    if (!file) { log("unable to open map file"); return; }

    w.screens.clear();
    w.displays.clear();
    w.buttons.clear();

    generate_cubic_map(w.cubic_map);

    ReadMapMode mode = NOTHINGMODE;
    std::string line;

    while (std::getline(file, line)) {  // Read line by line

        if      (line.find("MAP")      != std::string::npos && mode == NOTHINGMODE) { mode = MAPMODE;     continue; }
        else if (line.find("SCREENS")  != std::string::npos && mode == NOTHINGMODE) { mode = SCREENMODE;  continue; }
        else if (line.find("DISPLAYS") != std::string::npos && mode == NOTHINGMODE) { mode = DISPLAYMODE; continue; }
        else if (line.find("BUTTONS")  != std::string::npos && mode == NOTHINGMODE) { mode = BUTTONMODE;  continue; }
        else if (line.find("END")      != std::string::npos)                        { mode = NOTHINGMODE; continue; }

        std::stringstream ss(line);
        switch (mode) {

            case NOTHINGMODE: { continue; }

            case MAPMODE: {
                continue;
                //Add row to map - no can do sir
            }

            case SCREENMODE: {

                std::string start;
                std::string end;
                float x;
                float y;
                float z;
                float rot;
                std::string type_string;

                ss >> start >> type_string >> x >> y >> z >> rot >> end;
                if (start != "{") { log("parse error1s"); }
                if (end   != "}") { log("parse error2s"); }

                Vector3 pos = (Vector3) { x, y, z };
                WindowState type = string_to_windowstate(type_string);
                create_screen(w, pos, 1, 1, rot, type);
                break;

            }

            case DISPLAYMODE: {
                std::string start;
                std::string end;
                float x;
                float y;
                float z;
                std::string type_string;
                
                //Add display to displays vector
                ss >> start >> type_string >> x >> y >> z >> end;
                if (start != "{") { log("parse error1d"); }
                if (end   != "}") { log("parse error2d"); }

                Vector3 pos = (Vector3) { x, y, z };
                DisplayType type = string_to_displaytype(type_string);

                create_display(w, pos, 0, type);
                break;
            }

            case BUTTONMODE: {
                std::string start;
                std::string end;
                float x;
                float y;
                float z;
                std::string type_string;
                
                //Add display to displays vector
                ss >> start >> type_string >> x >> y >> z >> end;
                if (start != "{") { log("parse error1b"); }
                if (end   != "}") { log("parse error2b"); }

                Vector3 pos = (Vector3) { x, y, z };
                Action a = string_to_action(type_string);

                //add_button(buttons, a, pos);
                break;
            }

        }

    }

}

void load_state(action_load_state* a) {

    if (a->confirm < 0) {
        a->m.active = 1;
        clear_action_list(a->m.action_list);
        action* yes = (action*) new action_load_state(a->w, a->m, 1);
        action* no  = (action*) new action_load_state(a->w, a->m, 0);
        add_action(LOAD_STATE, a->m.action_list, yes);
        add_action(LOAD_STATE, a->m.action_list, no);

    } else if (a->confirm > 0) {
        load_state(a->w);
    } 

}

void save_state(const world &w) {

    std::ofstream file(map_file);
    if (!file.is_open()) { log("unable to save state"); return; }

    //MAP
    save_cubic_map(w.cubic_map);

    //SCREENS
    file << "SCREENS" << std::endl;
    for (screen_3D screen : w.screens) {

        file << "{ ";
        file << windowstate_to_string(screen.type) << " ";
        file << std::to_string(screen.loc.x) << " " << std::to_string(screen.loc.y) << " " << std::to_string(screen.loc.z) << " ";
        file << std::to_string(screen.rot) << " ";
        file << "}" << std::endl;
    }
    file << "END" << std::endl << std::endl;

    //DISPLAYS
    file << "DISPLAYS" << std::endl;
    for (display_3D display : w.displays) {

        file << "{ ";
        file << displaytype_to_string(display.type) << " ";
        file << std::to_string(display.loc.x) << " " << std::to_string(display.loc.y) << " " << std::to_string(display.loc.z) << " ";
        file << "}" << std::endl;
    }
    file << "END" << std::endl << std::endl;

    //BUTTONS
    file << "BUTTONS" << std::endl;
    for (button_3D button : w.buttons) {

        file << "{ ";
        file << action_to_string(button.a->type) << " ";
        file << std::to_string(button.pos.x) << " " << std::to_string(button.pos.y) << " " << std::to_string(button.pos.z) << " ";
        file << "}" << std::endl;
    }
    file << "END" << std::endl << std::endl;

    file.close();
}

void save_state(action_save_state* a) {

    if (a->confirm < 0) {
        a->m.active = 1;
        clear_action_list(a->m.action_list);
        action* yes = (action*) new action_save_state(a->w, a->m, 1);
        action* no  = (action*) new action_save_state(a->w, a->m, 0);
        add_action(SAVE_STATE, a->m.action_list, yes);
        add_action(SAVE_STATE, a->m.action_list, no);

    } else if (a->confirm > 0) {
        save_state(a->w);
    } 

}

void do_action(action* a) {

    switch (a->type) {

        case CANCEL:            {                                                   break; }
        case CREATE_NODE:       { create_node(      (action_create_node*)       a); break; }
        case DELETE_NODE:       { delete_node(      (action_delete_node*)       a); break; }
        case CREATE_CONNECTION: { create_connection((action_create_connection*) a); break; }
        case DELETE_CONNECTION: { delete_connection((action_delete_connection*) a); break; }
        case RESET_CAMERA:      { reset_camera(     (action_reset_camera*)      a); break; }
        case TOGGLE_DIRECTION:  { toggle_conn_dir(  (action_toggle_direction*)  a); break; }
        case TOGGLE_PROCESSING: { toggle_processing((action_toggle_processing*) a); break; }
        case CLEAR_GRAPH:       { clear_graph(      (action_clear_graph*)       a); break; }
        case SET_START_NODE:    { set_start_node(   (action_set_start_node*)    a); break; }
        case SET_END_NODE:      { set_end_node(     (action_set_end_node*)      a); break; }
        case REMOVE_WALL:       { remove_wall(      (action_remove_wall*)       a); break; }
        case MAKE_WALL:         { make_wall(        (action_make_wall*)         a); break; }
        case HIDE_MOUSE:        { hide_mouse(       (action_hide_mouse*)        a); break; }
        case LOAD_STATE:        { load_state(       (action_load_state*)        a); break; }
        case SAVE_STATE:        { save_state(       (action_save_state*)        a); break; }
        case REMOVE_SCREEN:     { remove_screen(    (action_remove_screen*)     a); break; }
        case CREATE_SCREEN:     { create_screen(    (action_create_screen*)     a); break; }
        case REMOVE_DISPLAY:    { remove_display(   (action_remove_display*)    a); break; } 
        case CREATE_DISPLAY:    { create_display(   (action_create_display*)    a); break; }

    
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





//Any item that overlaps with the click gets added
//Do this by simply looping through every single item for now...
void update_graph_action_list(Vector2 pos, std::vector<action*> &al, graph_data &g, menu &m) {

    clear_action_list(al);
    //For each new item sort created, simply add new for loop and set interaction actions.
    //TODO - differentiate between different items
    int hit = 0;

    //NODES
    for (size_t i = 0; i < g.nodes.size(); i++) {

        //Add node actions
        if (intersecting_node(pos, g, i)) {
            hit++;

            add_action(CREATE_CONNECTION, al, (action*) new action_create_connection(i, g));
            add_action(SET_START_NODE, al, (action*) new action_set_start_node(i, g));
            add_action(SET_END_NODE, al, (action*) new action_set_end_node(i, g));
            add_action(DELETE_NODE, al, (action*) new action_delete_node(i, g));

        }

    }

    //CONNECTIONS
    for (size_t i = 0; i < g.connections.size(); i++) {

        //Add connection actions
        if (intersecting_conn(pos, g, i)) {
            hit++;

            add_action(TOGGLE_DIRECTION, al, (action*) new action_toggle_direction(i, g));
            add_action(DELETE_CONNECTION, al, (action*) new action_delete_connection(i, g));

        }

    }

    if (hit == 0) {

        add_action(CREATE_NODE, al, (action*) new action_create_node(-1, g, m));
        add_action(TOGGLE_PROCESSING, al, (action*) new action_toggle_processing(g));
        add_action(CLEAR_GRAPH, al, (action*) new action_clear_graph(g));
        add_action(RESET_CAMERA, al, (action*) new action_reset_camera(g));
    }

    add_action(CANCEL, al, (action*) new action_cancel());
 
}




// todo - make it add ALL actions, not just 1 type
void update_action_list_3D(std::vector<action*> &al, Collision_Results &res, world &w, menu &m, mouse &mo) {

    clear_action_list(al);

    switch (res.type) {

        case NO_COLLISION:     { break; }
        case SCREEN_INSIDE:           {
            add_action(REMOVE_SCREEN, al, (action*) new action_remove_screen(w, res.index));
            break; 
        }
        case SCREEN_OUTSIDE:           {
            add_action(REMOVE_SCREEN, al, (action*) new action_remove_screen(w, res.index));
            break; 
        }
        case OTHER:            { break; }
        case DISPLAY: {
            add_action(REMOVE_DISPLAY, al, (action*) new action_remove_display(w, res.index));
            break;
        }
        case BUTTON: {


            break;
        }
        case CUBICMAP_DEFAULT: { 
            add_action(REMOVE_WALL, al, (action*) new action_remove_wall(w, res.index_point));
            break; 
        }
        case CUBICMAP_FLOOR:   { }
        case CUBICMAP_CEILING: { 
            add_action(MAKE_WALL,      al, (action*) new action_make_wall(w, res.index_point));
            add_action(CREATE_SCREEN,  al, (action*) new action_create_screen(w, m, res.end_point, 1, 1, 0, WINDOW_NULL));
            add_action(CREATE_DISPLAY, al, (action*) new action_create_display(w, m, res.end_point, 0, DISPLAY_NULL));
            break; 
        }

    }

    

    add_action(HIDE_MOUSE, al, (action*) new action_hide_mouse(mo));

    add_action(SAVE_STATE, al, (action*) new action_save_state(w, m, -1));
    add_action(LOAD_STATE, al, (action*) new action_load_state(w, m, -1));

    add_action(CANCEL,     al, (action*) new action_cancel());
}

int menu_still_active(menu* menu, mouse &mouse) {

    int active = CheckCollisionPointRec(mouse.pos, menu->pos);
    int sub_menu_active = 0;

    if (menu->sub_menu_exists) {
        sub_menu_active = menu_still_active(menu->sub_menu, mouse);
    }

    if (mouse.active == 0) {
        active = 0;
    }

    return active || sub_menu_active;
};

void update_menu_index_hovered(menu* menu, mouse &mouse) {

    //Check y index
    int menu_y = mouse.pos.y - menu->pos.y;
    menu->index_hovered = (menu_y / menu->index_height) - 1;

    //Check if inside on x axis
    if (mouse.pos.x < menu->pos.x || mouse.pos.x > (menu->pos.x + menu->pos.width)) {
        menu->index_hovered = -1;

        if (menu->sub_menu_exists) {
            update_menu_index_hovered(menu->sub_menu, mouse);
        }
    }

}

void left_click_action(menu* menu, mouse& m) {

    if (menu->index_hovered == -1) {
        if (menu->sub_menu_exists) {
            left_click_action(menu->sub_menu, m);
        } else {
            log("menu broken 1");
        }
    } else {
        menu->index_selected = menu->index_hovered;
        menu->active = 0;

        //sanity check
        if (menu->index_selected < (int)menu->action_list.size()) {  
            action* a = menu->action_list[menu->index_selected];         
            do_action(a);
        } else {
            log("menu broken2");
        }

    }

}

void logic_graph(mouse &m, graph_data &g, xes_data &d) {

    if (g.processing) {
        std::string s = "processing: " + std::to_string(g.traces_processed);
        log(s);
        g.traces_processed += process_traces(d, g, g.traces_processed);
    }

    //if (g.menu.active) { g.menu.active = menu_still_active(&(g.menu), m); }

    if (g.menu.active) {

        update_menu_index_hovered(&(g.menu), m);

        if (m.left_click) {

            left_click_action(&(g.menu), m);
        }

    } else if (m.active) {
        std::string s1 = "X:" + std::to_string(m.pos.x) + "Y:" + std::to_string(m.pos.y);
        log(s1);

        g.hover_node_index = -1;
        for (size_t i = 0; i < g.nodes.size(); i++) {
            if (intersecting_node(m.pos, g, i)) {
                g.hover_node_index = i;
                log("hover node");
            }
        }

        g.hover_conn_index = -1;
        for (size_t i = 0; i < g.connections.size(); i++) {
            if (intersecting_conn(m.pos, g, i)) {
                g.hover_conn_index = i;
                log("hover conn");
            }
        }

        

        if (m.left_click && (g.hover_node_index != -1 || g.hover_conn_index != -1)) {
            if (g.hover_node_index != -1) {
                g.active_node_index = g.hover_node_index;
                log("active node set");
            } else {
                g.active_conn_index = g.hover_conn_index;
                log("active conn set");
            }

        }

        update_graph_action_list(m.pos, g.menu.action_list, g, g.menu);

        if (m.right_click) {
            log("menu activated");
            g.menu.active = 1;
            g.menu.old_mouse_pos = m.pos;

            

            g.menu.pos = calc_menu_pos(m, g.menu);

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

RayCollision CheckScreenCollisionInside(Ray ray, screen_3D &s) {

    Vector3 p1 = { -s.width / 2 + s.inset / 2, -s.height / 2 + s.inset / 2, 0 };
    Vector3 p2 = { -s.width / 2 + s.inset / 2,  s.height / 2 - s.inset / 2, 0 };
    Vector3 p3 = {  s.width / 2 - s.inset / 2, -s.height / 2 + s.inset / 2, 0 };
    Vector3 p4 = {  s.width / 2 - s.inset / 2,  s.height / 2 - s.inset / 2, 0 };

    Matrix rotationMatrix = MatrixRotateY(DEG2RAD * s.rot);

    p1 = Vector3Transform(p1, rotationMatrix);
    p2 = Vector3Transform(p2, rotationMatrix);
    p3 = Vector3Transform(p3, rotationMatrix);
    p4 = Vector3Transform(p4, rotationMatrix);

    p1 = Vector3Add(p1, s.loc);
    p2 = Vector3Add(p2, s.loc);
    p3 = Vector3Add(p3, s.loc);
    p4 = Vector3Add(p4, s.loc);

    RayCollision hit = GetRayCollisionQuad(ray, p1, p2, p3, p4);

    return hit;

}

Vector2 CalcScreenHitLoc(screen_3D &s, RayCollision hit) {

    Vector3 local_hit = SubVector3(hit.point, s.loc);
    Matrix rotationMatrix = MatrixRotateY(DEG2RAD * -s.rot);
    local_hit = Vector3Transform(local_hit, rotationMatrix);

    float x_perc = GetPercent(local_hit.x, -s.width / 2 + s.inset / 2, s.width / 2 - s.inset / 2);
    float y_perc = GetPercent(local_hit.y, s.height / 2 + s.inset / 2, -s.height / 2 + s.inset / 2);

    log("x_per", x_perc);
    log("y_per", y_perc);

    Vector2 coords = { x_perc * 512, y_perc * 512 };

    return coords;
}


//assumes cube_size 1,1,1
//todo deal with index_point
void CheckCollidingMap(Ray r, cubic_map &c, Collision_Results &res) {

    Vector3 start = r.position;
    Vector3 dir   = r.direction;

    int steps = 500;
    res.end_point = start;

    for (int i = 0; i < steps; i++) {

        if (is_outside_map(res.end_point, c)) { break; }

        Vector3 normed_point = SubVector3(res.end_point, c.loc);
        res.index_point.x = normed_point.x;
        res.index_point.y = normed_point.z;

        if (normed_point.y < 0) { res.type = CUBICMAP_FLOOR;   break;}
        if (normed_point.y > 1) { res.type = CUBICMAP_CEILING; break;}

        int hit = c.cubes[(int)normed_point.z][(int)normed_point.x];

        if (hit) { res.type = CUBICMAP_DEFAULT; break; }

        res.end_point = AddVector3(res.end_point, ScaleVector3(dir, 0.1));
    }

}

void CheckCollidingMapSingle(Ray r, cubic_map &c, Collision_Results &res) {

    Vector3 start = r.position;

    res.end_point = start;

    if (is_outside_map(res.end_point, c)) { return; }

    Vector3 normed_point = SubVector3(res.end_point, c.loc);
    res.index_point.x = normed_point.x;
    res.index_point.y = normed_point.z;

    if (normed_point.y < 0) { res.type = CUBICMAP_FLOOR;   return;}
    if (normed_point.y > 1) { res.type = CUBICMAP_CEILING; return;}

    int hit = c.cubes[(int)normed_point.z][(int)normed_point.x];

    if (hit) { res.type = CUBICMAP_DEFAULT; return; }

}

BoundingBox get_bb(Vector3 pos, Vector3 size) {

    BoundingBox b;
    b.min = { pos.x - size.x / 2, pos.y - size.y / 2, pos.z - size.z / 2 };
    b.max = { pos.x + size.x / 2, pos.y + size.y / 2, pos.z + size.z / 2 };
    return b;
}

BoundingBox get_bb(Vector3 pos, float size) {
    return get_bb(pos, (Vector3) {size, size, size});
}


//todo - add rotation support
RayCollision CheckScreenCollisionOutside(Ray ray, screen_3D &s) {

    float lowest = 9999999;
    int index = -1;
    for (int i = 0; i < s.walls.size(); i++) {

        BoundingBox b = get_bb(s.walls[i], s.scale[i]);
        RayCollision hit = GetRayCollisionBox(ray, b);  

        if (hit.hit && hit.distance < 0.1 && hit.distance < lowest) {
            index = i;
            lowest = hit.distance;
        }
        
    } 

    RayCollision r;
    r.hit = 0;
    if (index != -1) {
        r.hit = 1;
        r.distance = lowest;
    } 
    return r;

}

Collision_Results CheckMouseCollision(world &w, Ray ray) {

    Collision_Results r = { NO_COLLISION, -1, { 0, 0 }, { 0, 0, 0 }, 9999999.0f };
    Vector3 start = ray.position;

    for (int j = 0; j < 500; j++) {

        for (int i = 0; (size_t)i < w.screens.size(); i++) {
            
            screen_3D &s = w.screens[i];

            RayCollision hit = CheckScreenCollisionInside(ray, s);
            RayCollision hit_out = CheckScreenCollisionOutside(ray, s);

            if (hit.hit && hit.distance < 0.1 && hit_out.hit) {
                if (hit.distance < hit_out.distance) {
                    log("screen hit inside!");
                    r.type = SCREEN_INSIDE;
                    r.index = i;
                    r.end_point = hit.point;
                    r.distance = hit.distance;
                    r.index_point = CalcScreenHitLoc(s, hit);
                    return r;
                } else {
                    r.type = SCREEN_OUTSIDE;
                    r.index = 1;
                    r.distance = hit.distance;
                    return r;
                }
            } else if (hit.hit && hit.distance < 0.1) {
                log("screen hit inside!");
                r.type = SCREEN_INSIDE;
                r.index = i;
                r.end_point = hit.point;
                r.distance = hit.distance;
                r.index_point = CalcScreenHitLoc(s, hit);
                return r;
            } else if (hit_out.hit) {
                r.type = SCREEN_OUTSIDE;
                r.index = 1;
                r.distance = hit.distance;
                return r;
            }
        }

        for (int i = 0; (size_t)i < w.displays.size(); i++) {
            display_3D &d = w.displays[i];
            BoundingBox b = get_bb(d.loc, 0.5);
            if (CheckCollisionBoxSphere(b, ray.position, 0.01)) {
                r.type = DISPLAY;
                r.index = i;
                r.end_point = ray.position;
                r.distance = Vector3Length(SubVector3(ray.position, start));
                return r;
            }
        }

        for (int i = 0; (size_t)i < w.buttons.size(); i++) {
            button_3D &butt = w.buttons[i];
            BoundingBox b = get_bb(butt.pos, 0.5);
            if (CheckCollisionBoxSphere(b, ray.position, 0.01)) {
                r.type = BUTTON;
                r.index = i;
                r.end_point = ray.position;
                r.distance = Vector3Length(SubVector3(ray.position, start));
                return r;
            }
        }

        CheckCollidingMapSingle(ray, w.cubic_map, r);

        if (r.type != NO_COLLISION) { return r; }

        ray.position = AddVector3(ray.position, ScaleVector3(ray.direction, 0.1)); 
    }

    return r;

}

void DrawCursor(Ray ray, Collision_Results res) {

    float size = 0.1f;
    if (res.type == SCREEN_INSIDE) { size = 0.01f; }

    DrawCube(res.end_point, size, size, size, GREEN);
    DrawCubeWires(res.end_point, size, size, size, RED);

    //Vector3 normalEnd = SubVector3(res.end_point, ScaleVector3(ray.direction, 5));

    DrawLine3D(ray.position, res.end_point, RED);

}

void CameraCollision(Camera &curr_c, Camera old_c, cubic_map map) {

    if (is_outside_map(curr_c.position, map)) { log("camera outside map!!!!"); return; }

    Vector3 normed_point = SubVector3(curr_c.position, map.loc);

    int collision = map.cubes[(int)normed_point.z][(int)normed_point.x];

    if (collision) { curr_c = old_c; } // only resets cam to prev tick.

}


void render_map(world& w, Vector2 corner) {

    cubic_map c = w.cubic_map;
    Camera &cam = w.camera;
    //Data setup
    c.loc = SubVector3(c.loc, (Vector3 {0.5, 0, 0.5}));
    int map_size = 40;
    Vector2 player_pos = { cam.position.x, cam.position.z };
    Vector3 cam_forward = GetCameraForward(&cam);

    float rot = atan2f(cam_forward.z, cam_forward.x);
    rot += DEG2RAD * 90;


    //Draw map area
    DrawRectangle     (corner.x - map_size, corner.y, map_size, map_size, BLACK);  
    DrawRectangleLines(corner.x - map_size, corner.y, map_size, map_size, WHITE);      


    for (int x = 0; x < map_size; x++) {
        for (int y = 0; y < map_size; y++) {
            
            int screen_x = corner.x - map_size + x;
            int screen_y = corner.y + y;
            
            int rel_to_player_x = x - map_size / 2;
            int rel_to_player_y = y - map_size / 2;

            float rot_x = rel_to_player_x * cosf(rot) - rel_to_player_y * sinf(rot);
            float rot_y = rel_to_player_x * sinf(rot) + rel_to_player_y * cosf(rot);

            int global_x = player_pos.x + rot_x;
            int global_y = player_pos.y + rot_y;

            int cubemap_x = global_x - c.loc.x;
            int cubemap_y = global_y - c.loc.y;

            if (cubemap_x > 0 && cubemap_x < c.size_x && cubemap_y > 0 && cubemap_y < c.size_y) {
                

                Color col = BROWN;
                if (c.cubes[cubemap_y][cubemap_x] == 1) {
                    col = DARKBLUE;
                }

                DrawPixel(screen_x, screen_y, col);  
            } else { // for debug
                DrawPixel(screen_x, screen_y, PINK);  
            }
        }
    }
  
    
    //draw object pixels
    for (screen_3D s : w.screens) {

        float x = s.loc.x - cam.position.x;
        float y = s.loc.z - cam.position.z;

        float rot_x = x * cosf(-rot) - y * sinf(-rot);
        float rot_y = x * sinf(-rot) + y * cosf(-rot);

        int map_x = corner.x - map_size / 2 + rot_x;
        int map_y = corner.y + map_size / 2 + rot_y;

        DrawPixel(map_x, map_y, GREEN);

    }
    
   
    // again...

    //draw user
    DrawPixel(corner.x - map_size / 2, corner.y + map_size / 2, WHITE);

}

std::string VectorToString(Vector3 v) {

    std::string s = "X:" + std::to_string(v.x) + " Y:" + std::to_string(v.y) + " Z:" + std::to_string(v.z);

    return s;
}

//todo
enum MainState {

    MOVE,
    INSPECT,
    MENU
};

void DrawDisplay(display_3D &d) {

    float size = 1.0;
    float start_x = d.loc.x - size / 2;
    float start_y = d.loc.y + 0.1;

    Vector3 pos = { start_x, start_y + size / 3, d.loc.z };

    trace t = traces[d.index];
    Color c = { 255, 30, 200, 255 };

    if (d.type == SINGLE_TRACE_TIME && t.events.size()) {
        float size = 1.0;
        DrawCubeWires(d.loc, size, size, size, BLUE); 

        size = 0.01;
        for (int j = 0; j < 100; j++) {
            t = traces[d.index + j];
            time_t current_time = t.events[0].time;
            time_t total = 0;

            Vector3 last_pos;
            for (int i = 0; i < t.events.size(); i++) {

                time_t diff = t.events[i].time - current_time;
                total += diff;
                time_t day_offset = total % (3600 * 24);
                float extra_x = (float)day_offset / (3600.0 * 24.0);
                pos.x = start_x + extra_x;
                float extra_y = 0.02 * float(total / (3600 * 24)); 
                pos.y = start_y + extra_y;
                DrawSphere(pos, size, c);

                if (i != 0) {
                    DrawLine3D(last_pos, pos, RED);       
                }

                last_pos = pos;
                current_time = t.events[i].time;

            }
            pos.z += 0.03;
            c.g += 2;
        }


    } else if (d.type == MULTIPLE_TRACE_TIME && t.events.size()) {

        size = 0.5f;

        DrawCube(d.loc, size, size, size, BLUE);
        
    } else {

        size = 0.5f;

        DrawCube(d.loc, size, size, size, BLUE);

    }
}

void DrawDisplays(std::vector<display_3D> displays) {

    for (display_3D &d : displays) {
        DrawDisplay(d);
    }

}

void DrawWorld(world& w) {

    //convert to function
    DrawModel(w.cubic_map.model, w.cubic_map.loc, w.cubic_map.cube_size.x, WHITE); 
    for (int i = 0; i < w.cubic_map.size_y; i++) {
        for (int j = 0; j < w.cubic_map.size_x; j++) {
            if (w.cubic_map.cubes[i][j] == 1) {
                DrawCube((Vector3) {(float)j, 0.5, (float)i}, 0.3f, 0.3f, 0.3f, PINK);
            }
        }
    } 

    DrawScreens(w.screens, w.render_textures);
    DrawDisplays(w.displays);
    DrawButtons(w.buttons);


}

int main() {

    std::string xes_filename = "RequestForPayment.xes_";

    SetTraceLogLevel(LOG_ALL);

    window main_window = { {0, 0}, {800, 450} };

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(main_window.end.x, main_window.end.y, "Biz Wiz");

    SetTargetFPS(60);  

    struct mouse m;
    hide_mouse(m);
    
    XMLDocument xes_doc;
    XMLElement* root_log = open_xes(xes_filename, xes_doc);
    xes_data log_data;
    parse_xes(root_log, log_data);
    traces = log_data.traces;

    
    menu menu;

    //Make generic too?
    summary_data s  = create_summary_data(log_data);
    data_data d     = create_data_data(log_data);
    timeline_data t = create_timeline_data(log_data);
    graph_data g    = create_graph_data(log_data);
    trace_data td   = create_trace_data(log_data);

    world world;
    load_state(world);

    std::string action = "";


    world.camera.position = (Vector3){ 2.0f, 0.5f, 2.0f };
    world.camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    world.camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    world.camera.fovy = 65.0f;
    world.camera.projection = CAMERA_PERSPECTIVE;
    rlSetClipPlanes(0.1f, 50000.0f); // increase far plane dist

    Font font = GetFontDefault();
    float fontSize = 8.0f;
    float fontSpacing = 0.5f;
    float lineSpacing = -1.0f;

    std::string text3D = "hello...";

    Shader alphaDiscard = LoadShader(NULL, "resources/shaders/glsl330/alpha_discard.fs");

    Ray ray;

    Collision_Results col_res = { NO_COLLISION };

    Mesh cubeMesh = GenMeshCube(0.5f, 0.5f, 0.5f);
    Model cubeModel = LoadModelFromMesh(cubeMesh);

    //Shader shader = LoadShader("resources/shaders/glsl330/base_lighting.vs", "resources/shaders/glsl330/lighting.fs");
    //cubeModel.materials[0].shader = shader;

    while (!WindowShouldClose()) {

        // &&&&&&&&&&&&&&&&&&&&&&&&&&&&&& GLOBAL INPUT &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

        if (IsKeyPressed(KEY_F11)) { ToggleFullscreen(); }

        main_window.end.x  = GetScreenWidth();
        main_window.end.y = GetScreenHeight();

        m.pos           = GetMousePosition();
        m.delta         = GetMouseDelta();
        m.wheel_delta   = GetMouseWheelMove();
        m.left_click    = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        m.right_click   = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        m.left_down     = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        m.left_released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

        m.end = main_window.end;  

        if (menu.active) { menu.active = menu_still_active(&menu, m); }
        
        if (menu.active && m.active) {
            //log("state1");
            col_res.type = NO_COLLISION;
            update_menu_index_hovered(&menu, m);

            if (m.left_click) { left_click_action(&menu, m); }

        } else if (menu.active && !m.active) { 
            log("bad state1");
        } else if (m.active) {
            
            ray = GetMouseRay(m.pos, world.camera);

            col_res = CheckMouseCollision(world, ray);

            update_action_list_3D(menu.action_list, col_res, world, menu, m);

            if (menu.action_list.size()) {
                std::string first_action_name = action_to_string(menu.action_list.front()->type);
                std::string no_actions = std::to_string(menu.action_list.size() - 1); //-1 for cancel
                action = first_action_name + " / " + no_actions + " more options";
            }

            if (m.right_click) {
                menu.active = 1;
                menu.old_mouse_pos = m.pos;
                menu.pos = calc_menu_pos(m, menu);

            }

            if (m.left_click) {
                log("did:" + action);
            }

            //remove later
            if (col_res.type == SCREEN_INSIDE) { menu.active = 0; }

        } else {

            col_res.type = NO_COLLISION;

            Camera old_cam = world.camera;        

            if (IsKeyDown(KEY_LEFT_SHIFT)) {

                UpdateCamera(&(world.camera), CAMERA_FREE);
                UpdateCamera(&(world.camera), CAMERA_FREE);
                UpdateCamera(&(world.camera), CAMERA_FREE);
                UpdateCamera(&(world.camera), CAMERA_FREE);
            };

            UpdateCamera(&(world.camera), CAMERA_FREE);

            CameraCollision(world.camera, old_cam, world.cubic_map);

            if (m.left_click || m.right_click) {
                m.active = 1;
                m.pos.x = main_window.end.x;
                m.pos.y = main_window.end.y;
                SetMousePosition(m.pos.x, m.pos.y);
                EnableCursor();  
            }

        }
    
        for (std::map<WindowState, RenderTexture2D>::iterator it = world.render_textures.begin(); it != world.render_textures.end(); it++) {

            mouse mouse_2D = m;
            mouse_2D.active = 0;

            if (col_res.type == SCREEN_INSIDE) { //if ray hits a specific screen, transfer mouse coords to it.
                //log("SCREEEEEEEEEEEEEEEEEEEEEN");
                WindowState win_type = world.screens[col_res.index].type;
                if (win_type == it->first) {
                    mouse_2D.active = 1;
                    mouse_2D.pos = col_res.index_point;
                }
            }

            switch (it->first) {
                case DATA:     logic_data    (mouse_2D, d);  break; 
                case SUMMARY:  logic_summary (mouse_2D, s);  break; 
                case TIMELINE: logic_timeline(mouse_2D, t);  break; 
                case GRAPH:    logic_graph(mouse_2D, g, log_data);   break; 
                case TRACE:    logic_trace(mouse_2D, td);    break; 
                case WINDOW_NULL: log("error53"); break;
            }
        }
       
     

        //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Draw @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        // For all objects of type screen, update render associated function to render texture (todo make ref)

        for (std::map<WindowState, RenderTexture2D>::iterator it = world.render_textures.begin(); it != world.render_textures.end(); it++) {

            RenderTexture2D &tex = it->second;
            BeginTextureMode(tex);
            ClearBackground(BLACK);

            window w = { {0, 0}, {(float)tex.texture.width, (float)tex.texture.height} };

            switch (it->first) {
                case DATA:     render_data    (w, d);  break; 
                case SUMMARY:  render_summary (w, s);  break; 
                case TIMELINE: render_timeline(w, t);  break; 
                case GRAPH:    render_graph   (w, g);  break; 
                case TRACE:    render_trace   (w, td); break; 
                case WINDOW_NULL: log("error53"); break;
            }

            EndTextureMode();
        }

 
        BeginMode3D(world.camera);
        DrawWorld(world);
        DrawCursor(ray, col_res);
        DrawSphere((Vector3){0, 1, 0}, 0.1, PINK);
        DrawModel(cubeModel, Vector3{ 4.0f, 0.5f, 4.0f }, 1.0f, WHITE);
        EndMode3D();



        
        //DrawGrid(10, 1.0f);    

        //DrawRay(ray, GREEN);   
        //BeginShaderMode(alphaDiscard);

        //DrawText3D(font, text3D, { 0,-10, 0 }, fontSize, fontSpacing, lineSpacing, true, RED, false);
        //EndShaderMode();
        
        
    
        //render_active_trace_3D({100, 0, 80}, log_data.traces[0]);
        //render_active_trace_3D_cube({100, 0, 100}, log_data.traces[0]);
        //render_all_traces(log_data.traces);


        

        //draw_3D_text(camera);

        // &&&&&&&&&&& INTERFACE &&&&&&&&&&&&&&

        DrawText(global_msg.c_str(), 10, 10, 8, ORANGE);
        DrawText(action.c_str(), 10, 20, 8, ORANGE);
        DrawFPS(10, 50);
      
        for (size_t i = 0; i < global_log.size(); i++) {
            DrawText(global_log[i].c_str(), 10, 430 - i * 10, 8, ORANGE);
        }

        if (menu.active && m.active) {
            render_menu(main_window, menu);
        }
        
        render_map(world, (Vector2) { main_window.end.x, main_window.start.y });
        
        std::string player_loc = VectorToString(world.camera.position);
        std::string hit_loc    = VectorToString(col_res.end_point);

        DrawText(player_loc.c_str(), 10, 30, 8, ORANGE);
        DrawText(hit_loc.c_str(), 10, 40, 8, ORANGE);
        
        EndDrawing();
        
    }

    CloseWindow();   
    return 0;
}