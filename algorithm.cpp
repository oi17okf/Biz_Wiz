/*
    Script to calculate graph with property X, follow the algorithm in the thesis.

*/

#include "tinyxml2.h"

#include <vector>
#include <string>
#include <algorithm>
#include <ctime>
#include <limits>
#include <math.h>
#include <iostream>

using namespace tinyxml2;

void log(std::string s)          { std::cout << "LOG: " << s << std::endl; }
void log(std::string s, int i)   { s += std::to_string(i); log(s); }
void log(std::string s, float i) { s += std::to_string(i); log(s); }

struct event {

    std::string id;
    std::string resource;
    std::string name;
    std::string role;
    time_t time;
    int valid;
    char type;
    int used;
};

struct trace {

    std::vector<event> events;
    int valid;
    std::string type;
};

struct event_log {

    int events;
    int traces;
    float average_events_per_trace;
    std::vector<std::string> activity_names;
    std::vector<trace> traces;
};

time_t parse_timestamp(const std::string& timestamp) {

    std::tm tm = {};
    std::stringstream ss(timestamp);
    
    //cuts of timestamp from "2023-09-06T09:34:00.000+00:00" to 2023-09-06T09:34:00
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) { log("error parsing time"); }

    return mktime(&tm);
}

std::string time_to_string(time_t t) {

    std::tm* t_i = std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(t_i, "%Y-%m-%d %H:%M:%S");

    return oss.str();
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

void generate_event_log(XMLElement* root, event_log &data) {

    for (XMLElement* log_trace = root->FirstChildElement("trace"); 
        log_trace != nullptr; log_trace = log_trace->NextSiblingElement("trace")) {

        trace t;
        t.type = "";
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

            e.type = -1;
            for (int i = 0; i < (int)data.names.size(); i++) {
                if (e.name == data.names[i]) {

                    e.type = 'A' + i;
                    break;
                }
            }
            if (e.type == -1) { log("error parsing log"); }
            t.type += e.type;
            t.events.push_back(e);
        }

        data.traces.push_back(t);

    }

    data.events_per_trace = (float)data.events / (float)data.traces.size();
}

int main(int argc, char* argv[]) {


    std::string xes_filename = "RequestForPayment.xes_";
    log(xes_filename);
    if (argc >= 2) {

        xes_filename = argv[1];
    }
    log(xes_filename);

    XMLDocument xes_doc;
    XMLElement* root_log = open_xes(xes_filename, xes_doc);
    xes_data log_data;
    parse_xes(root_log, log_data);
    traces = log_data.traces;
    names = log_data.names;


    calc_stuff


}