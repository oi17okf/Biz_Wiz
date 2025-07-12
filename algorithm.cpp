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
#include <sstream>
#include <iomanip>
#include <fstream>

using namespace tinyxml2;

void log(std::string s)          { std::cout << "LOG: " << s << std::endl; }
void log(char c)                 { std::cout << "LOG: " << c << std::endl; }
void log(std::string s, char c)  { std::cout << "LOG: " << s << c << std::endl; }
void log(std::string s, int i)   { s += std::to_string(i); log(s); }
void log(std::string s, float i) { s += std::to_string(i); log(s); }

#define SIZE 10

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
    std::string shorthand;
    std::vector<std::string> names;
};

struct event_log {

    int events;
    float average_events_per_trace;
    std::vector<std::string> activity_names;
    std::vector<trace> traces;
};

struct unique_trace {

    std::string shorthand;
    int count;
    std::vector<std::string> names;
    std::vector<char> events;
    std::vector<float> times; // these times are relative to base event time. base event time is thus 0.

    bool operator<(const unique_trace &other) const {
        return count > other.count;  // Ascending order
    }
};

struct node {

    int creationID;
    char event_type;
    std::string name;
    int event_count;
    float average_time;
    std::vector<int> next_nodes;
    std::vector<int> next_nodes_counts;
    std::vector<int> prev_nodes;
    int extra_node = -1;
    int is_attempting_merge;
    int extra_event_count;
    float extra_average_time;
    std::vector<std::string> unique_traces;
    int end_count = 0;

    int deleted = 0;

    float time_diff;

    bool operator<(const node &other) const {
        return time_diff > other.time_diff;  
    }

};

/*
    nodes never get removed from the container.
    this is to simplify working with indexes, 
    it also works since we will never run out of memory on any of the examples. (not even close)
    so if anyone build on this you'll need to address this if you plan on using event logs of infinite size.
*/
struct master_trace {

    std::vector<node> nodes_container;
    std::vector<int>  base_nodes;

    int total_node_count;
    int node_to_merge;

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

void generate_shorthand(event_log &e_log, trace &t) {

    t.shorthand = "";

    for (int i = 0; i < t.events.size(); i++) {

        t.events[i].type = -1;

        for (int j = 0; j < e_log.activity_names.size(); j++) {

            if (t.events[i].name == e_log.activity_names[j]) {

                t.events[i].type = 'A' + j;
                break;
            }
        }

        if (t.events[i].type == -1) { 
            log("error generating shorthand"); 
        }
        t.shorthand += t.events[i].type;
    }
}

void fill_event_log(XMLElement* root, event_log &data) {

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
                    if (std::find(data.activity_names.begin(), data.activity_names.end(), e.name) == data.activity_names.end()) {
                        data.activity_names.push_back(e.name);
                    }
                } else if (key && std::string(key) == "org:role") {
                    e.role = std::string(value);
                } else if (key && std::string(key) == "time:timestamp") {
                    e.time = parse_timestamp(std::string(value));
                }
            }
            t.names.push_back(e.name);

            t.events.push_back(e);
        }

        generate_shorthand(data, t);

        data.traces.push_back(t);

    }

    data.average_events_per_trace = (float)data.events / (float)data.traces.size();
}



float merge_time(int event_count1, float average_time1, int event_count2, float average_time2) {

    if (event_count1 + event_count2 == 0) { return 0; }

    float res = ((event_count1 * average_time1) + (event_count2 * average_time2)) / (event_count1 + event_count2);

    return res;

}

std::string seconds_to_timedelta_string(float seconds) {
    int total_seconds = static_cast<int>(round(seconds));

    int days = total_seconds / 86400;
    total_seconds -= days * 86400;
    int hours = total_seconds / 3600;
    total_seconds -= hours * 3600;
    int minutes = total_seconds / 60;
    total_seconds -= minutes * 60;
    int secs = total_seconds;

    // Format: "X days HH:MM:SS"
    std::ostringstream ss;
    ss << days << " days ";
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setw(2) << minutes << ":"
       << std::setw(2) << secs;

    return ss.str();
}

void export_data(master_trace& mt, int i) {

    std::string conn_out = "connections" + std::to_string(i) + ".txt";
    std::string time_out = "timestamps"  + std::to_string(i) + ".txt";

    std::ofstream connections(conn_out);
    std::ofstream timestamps(time_out);

    if (!connections || !timestamps) {
        std::cerr << "Error opening file for writing." << std::endl;
        exit(1);
    }

    std::vector<node*> nodes;
    node* parent = nullptr;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());

    std::vector<node*> exit_nodes;

    std::vector<std::string> connection_strings;
    std::vector<std::string> timestamp_strings;

    while(!nodes.empty()) {

        parent = nodes.back();
        nodes.pop_back();

        std::string parent_s = parent->name + std::to_string(parent->creationID);
        std::string timestamp = seconds_to_timedelta_string(parent->average_time);
        std::string timestamp_out = parent_s + " | " + timestamp;

        if(std::find(timestamp_strings.begin(), timestamp_strings.end(), timestamp_out) == timestamp_strings.end()) {
            timestamps << timestamp_out << std::endl;
            timestamp_strings.push_back(timestamp_out);
        }

        

        for (int i = 0; i < parent->next_nodes.size(); i++) {

            node* kid = parent->next_nodes[i];
            
            std::string kid_s    = kid->name + std::to_string(kid->creationID);
            std::string count    = std::to_string(parent->next_nodes_counts[i]);
            std::string connection_out = parent_s + "," + kid_s + "," + count;


            if(std::find(connection_strings.begin(), connection_strings.end(), connection_out) == connection_strings.end()) {
                connections << connection_out << std::endl;
                connection_strings.push_back(connection_out);
            }            

            nodes.insert(nodes.begin(), kid);
        }

        if (parent->end_count != 0) {

            int exists_already = 0;
            for (int i = 0; i < exit_nodes.size(); i++) {

                if (parent->creationID == exit_nodes[i]->creationID) {
                    exists_already = 1;
                }
            }

            if (exists_already == 0) {
                exit_nodes.push_back(parent);
            }

        }
    }

    // Start nodes
    for (int i = 0; i < mt.base_nodes.size(); i++) {

        std::string start_output = mt.base_nodes[i]->name + std::to_string(mt.base_nodes[i]->creationID);
        std::string count    = std::to_string(mt.base_nodes[i]->event_count);
        connections << "Start:" << start_output << "," << count << std::endl;

    }

    // End nodes
    for (int i = 0; i < exit_nodes.size(); i++) {

        std::string output = exit_nodes[i]->name + std::to_string(exit_nodes[i]->creationID);
        std::string count  = std::to_string(exit_nodes[i]->end_count);
        connections << "End:" << output << "," << count << std::endl;

    }


    connections.close();
    timestamps.close();

}

float calc_time_diff_event(node* node, std::vector<trace> &traces) {

    float sum = 0;

    for (int i = 0; i < traces.size(); i++) {

        for (int j = 0; j < node->unique_traces.size(); j++) {

            if (node->unique_traces[j] == traces[i].shorthand) {

                for (int k = 0; k < traces[i].events.size(); k++) {

                    //we finally found a hit!
                    if (node->event_type == traces[i].events[k].type && traces[i].events[k].used == 0) {

                        float rel_time = traces[i].events[k].time - traces[i].events[0].time;
                        float diff = node->average_time - rel_time;
                        float abs_diff = diff < 0 ? -diff : diff;
                        sum =+ abs_diff;

                        traces[i].events[k].used = 1;
                    }
                }
            }
        }
    }

    return sum;
}

float calc_time_diff(master_trace& mt, int event_count, std::vector<trace> &traces) {

    float sum = 0;

    for (trace &t : traces) {
        for (event &e : t.events) {
            e.used = 0;
        }
    }

    std::vector<node*> nodes;
    node* parent = nullptr;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());

    while(!nodes.empty()) {

        parent = nodes.back();
        nodes.pop_back();

        sum += calc_time_diff_event(parent, traces);

        for (int i = 0; i < parent->next_nodes.size(); i++) {
            
            nodes.insert(nodes.begin(), parent->next_nodes[i]);
        }
    }

    for (trace &t : traces) {
        for (event &e : t.events) {
            if (e.used != 1) {
                log("Error - missed event when calculating time diff");
                exit(1);
            }
        }
    }

    sum /= event_count;

    return sum;

}

std::vector<unique_trace> step_1_calc_unique_traces(std::vector<trace> &traces) {

    std::vector<unique_trace> unique_traces;

    for (int i = 0; i < traces.size(); i++) {
    //for (int i = 0; i < 500; i++) {

        trace t = traces[i];

        //each unique trace, checking for match
        int new_unique = 1;
        int ut_index_match = -1;
        for (int j = 0; j < unique_traces.size(); j++) {

            if (unique_traces[j].shorthand == t.shorthand) { 
                new_unique = 0;
                ut_index_match = j;
            }
        }

        if (new_unique) {

            unique_trace new_unique;
            new_unique.shorthand   = t.shorthand;
            new_unique.names = t.names;
            new_unique.count = 1;

            time_t base_time = t.events[0].time;

            for (int j = 0; j < t.events.size(); j++) {

                new_unique.events.push_back(t.events[j].type);
                if (j == 0) {
                    new_unique.times.push_back(0);
                } else {
                    new_unique.times.push_back(t.events[j].time - base_time); 
                }
            }

            unique_traces.push_back(new_unique);

        } else {

            time_t base_time = t.events[0].time;

            for (int k = 1; k < t.events.size(); k++) {

                unique_traces[ut_index_match].times[k] = merge_time(1, t.events[k].time - base_time, 
                    unique_traces[ut_index_match].count, unique_traces[ut_index_match].times[k]);    
            }

            unique_traces[ut_index_match].count++;
        }
    }

    return unique_traces;
}

int check_valid_merge(const master_trace& mt, float next_event_time, int recur) {

    std::vector<node*> nodes;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    node* parent = nullptr;

    node* target = nullptr;

    while(!nodes.empty()) {

        parent = nodes.back(); 
        nodes.pop_back(); 

        if (parent->is_attempting_merge) {
            target = parent;
        }

        for (int i = 0; i < parent->next_nodes.size(); i++) {

            node* kid = parent->next_nodes[i];

            float kid_time = kid->average_time;
            if (kid->is_attempting_merge) {

                kid_time = merge_time(kid->event_count, kid->average_time, 
                                      kid->extra_event_count, kid->extra_average_time);
            }

            float parent_time = parent->average_time;
            if (parent->is_attempting_merge) {

                parent_time = merge_time(parent->event_count, parent->average_time, 
                                         parent->extra_event_count, parent->extra_average_time);
            }

            if (kid_time < parent_time) {

                if (parent->is_attempting_merge) {
                    log("           match rejected! - parent attemptin merge");
                    
                    //log("           failed merge - parent is attempting merge - SHOULD NOT OCCUR");
                    log("           Offending nodes:");
                    log("               parent: " + parent->name + " time: ", parent->average_time);
                    log("               kid: " + kid->name + " time: ", kid->average_time);
                    return 0;
                } else if (!kid->is_attempting_merge){
                    log("           failed merge - no merge attempt - SHOULD NOT OCCUR");
                    log("           Offending nodes:");
                    log("               parent: " + parent->name + " time: ", parent->average_time);
                    log("               kid: " + kid->name + " time: ", kid->average_time);
                    log("recur -> ", recur);
                    exit(0);
                }
                log("           match rejected! - kid attemptin merge");
                return 0;
            }
  

            nodes.insert(nodes.begin(), kid);

        }

        if (parent->extra_node != nullptr) {
            nodes.insert(nodes.begin(), parent->extra_node);

            if (!parent->extra_node->is_attempting_merge) {
                log("           error - checkvalidmerge");
                exit(0);
            }

            node* kid = parent->extra_node;
            float kid_time = kid->average_time;
            kid_time = merge_time(kid->event_count, kid->average_time, 
                                  kid->extra_event_count, kid->extra_average_time);

            if (kid_time < parent->average_time) {
                log("           match rejected via extra path - hmm!");
                log("               parent: " + parent->name + " time: ", parent->average_time);
                log("               kid: " + kid->name + " time: ", kid->average_time);
                return 0;
            }
        }
    }

    if (target == nullptr) {

        log("           No node attempting merge found - Exiting");
        exit(0);
    } else {

        float target_time = merge_time(target->event_count, target->average_time, 
                                       target->extra_event_count, target->extra_average_time);

        if (target_time > next_event_time) {

            log("           match rejected! - due to next_event_time");
            log("           next_event_time was: ", next_event_time);
            log("           target_time was: ", target_time);
            return 0;
        }
    }

    //went through entire graph, all good!
    log("           match found!");
    return 1;
}


std::vector<node*> get_closest_nodes(const master_trace &mt, char event_type) {

    std::vector<node*> nodes;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    node* parent = nullptr;

    std::vector<node*> closest_time_nodes;

    int count = 0;
    while(!nodes.empty()) {
        //log("!!!");
        
        if (count > 1000) { 

            log("        ERROR - added to many closest nodes, something must be wrong - Exiting"); 
            exit(0); 
        }

        
        parent = nodes.back();
        nodes.pop_back();
        //log(parent->event_type);

        if (parent->event_type == event_type) {
            parent->time_diff = parent->average_time - event_time;
            parent->time_diff = parent->time_diff < 0 ? parent->time_diff * -1 : parent->time_diff;

            int already_exists = 0;

            for (node *n : closest_time_nodes) {

                if (n->creationID == parent->creationID) { 
                    already_exists = 1; 
                }
            }

            if (!already_exists) { 
                count++;

                closest_time_nodes.insert(closest_time_nodes.begin(), parent);
            }

        }

        for (int i = 0; i < parent->next_nodes.size(); i++) {
            nodes.insert(nodes.begin(), parent->next_nodes[i]);
        }

    }

    
    std::sort(closest_time_nodes.begin(), closest_time_nodes.end(), [](const node* a, const node* b) {

        return a->time_diff > b->time_diff;
    });

    return closest_time_nodes;
}


//prev_node is used when creating a new node, since it needs to be tied in to the structure somehow
node* merge_letter(master_trace &mt, char event_type, std::string name, float event_time, int event_count, node* prev_node, std::string shorthand, 
    float next_event_time, int end_node, unique_trace t) {

    std::vector<node*> closest_time_nodes = get_closest_nodes(mt, event_type);



    node* merged_node = nullptr;
    count = 0;
    int merged = 0;
    node* node_ptr = nullptr;

    while(!closest_time_nodes.empty()) {
        log("K?");

        count++;

        node_ptr = closest_time_nodes.back();
        closest_time_nodes.pop_back();
        node_ptr->is_attempting_merge = 1;
        node_ptr->extra_event_count   = event_count;
        node_ptr->extra_average_time  = event_time;

        if (prev_node != nullptr) {
            prev_node->extra_node = node_ptr;
        }

        merged = check_valid_merge(mt, next_event_time);
        //log("merge done. result", merged);
        // do not try this at home kids
        if (merged == 0) {
            log("------");

            if(ii+1 == (int)t.events.size()) {

                if (prev_node != nullptr) {

                    prev_node->extra_node = nullptr;
                }

                return nullptr;
            }


            float next_event_timee = std::numeric_limits<float>::max();

            int end_nodee = 0;
            if (ii+1 != t.events.size() - 1) {

                next_event_timee = t.times[ii+2];
            } else {

                end_nodee = 1;
            }

            //Here we need to fake update graph, due to only allowing for 1 extra node.
            //We need to save old data incase merge still fails.
            node_ptr->is_attempting_merge = 0;
            int saved_extra_count  = node_ptr->extra_event_count;
            float saved_extra_time = node_ptr->extra_average_time;
            int saved_count  = node_ptr->event_count;
            float saved_time = node_ptr->average_time;
            node_ptr->average_time = merge_time(node_ptr->event_count, node_ptr->average_time,
                                                node_ptr->extra_event_count, node_ptr->extra_average_time);
            node_ptr->event_count += node_ptr->extra_event_count;
            node_ptr->extra_average_time = 0;
            node_ptr->extra_event_count  = 0;


            node* res = merge_letter(mt, t.events[ii+1], t.names[ii+1], t.times[ii+1], t.count, node_ptr, t.shorthand, next_event_timee, end_nodee, t, ii+1, recursiondepth+1);

            if (res == nullptr) {
                merged = 0;
            } else {
                merged = 1;
            }

        }

        if (prev_node != nullptr) {
            prev_node->extra_node = nullptr;
        }

        if (merged == 1) {  

            merged_node = merge_node()

            break;

        } 

        node_ptr->is_attempting_merge = 0;
        node_ptr->extra_event_count   = 0;
        node_ptr->extra_average_time  = 0;
    }

    if (merged == 0) {

        merged_node = add_new_node(mt, ut, INDEX, prev_node, end_node);
    }

    return merged_node;
}

node* merge_node(node* n, node* prev_node) {

    n->average_time = merge_time(n->event_count, n->average_time,
                                 n->extra_event_count, n->extra_average_time);
    n->event_count += n->extra_event_count;
    merged_node = node_ptr;


    n->is_attempting_merge = 0;
    n->extra_event_count   = 0;
    n->extra_average_time  = 0;
    n->unique_traces.push_back(shorthand);
  
    if (prev_node != nullptr) {

        //add check so a node does not point to same node twice or more
        int already_exists = 0;
        int exists_index = -1;

        for (int i = 0; i < prev_node->next_nodes.size(); i++) {

            if (prev_node->next_nodes[i]->creationID == node_ptr->creationID) {
                already_exists = 1;
                exists_index = i;
            }
        }

        if (already_exists) {
            log("       prev node exists and has this name already. only updating edge count");
            prev_node->next_nodes_counts[exists_index] += event_count;
        } else {
            prev_node->next_nodes.push_back(merged_node);
            prev_node->next_nodes_counts.push_back(event_count);
            log("       MERGE - prev node existed. added " + name + " to " + prev_node->name + "'s next nodes");
        }
    } 

    if (end_node) {
        node_ptr->end_count += event_count;
    }

    return n;
}

int add_new_node(master_trace& mt, const unique_trace& ut, int i, int prev_node_index, int end_node) {

    node new_node;
    node& prev_node;

    new_node->creationID = mt.total_node_count;
    if (merged_node->creationID > 1000) {
        log("CreationID over 1000. Something must be wrong. Aborting");
        exit(0);
    }   
    mt.total_node_count++;

    new_node.event_type          = ut.events[i];
    new_node.name                = ut.names[i];
    new_node.average_time        = ut.times[i];
    new_node.event_count         = ut.count;
    new_node.unique_traces.push_back(ut.shorthand);
    new_node.is_attempting_merge = 0;
    new_node.end_count = 0;
    new_node.deleted = 0;
    new_node.time_diff = 0;
    new_node.extra_event_count = 0;
    new_node.extra_average_time = 0;
    new_node.extra_node = -1;

    if (prev_node != -1) {
        mt.nodes_container[prev_node_index].next_nodes.push_back(new_node);
        mt.nodes_container[prev_node_index].next_nodes_counts.push_back(new_node.event_count);
        log("       NO MERGE - prev node existed. added " + name + " to " + prev_node->name + "'s next nodes");
    } else {
        mt.base_nodes.push_back(new_node.creationID);
        log("       Added alternate start with name:" + new_node.name);
    }

    if (end_node) {
        new_node.end_count = new_node.event_count;
    }

    return new_node.creationID;
}


void merge_master_trace(master_trace& mt, unique_trace t) {

    node* prev_node = nullptr;

    for (int i = 0; i < t.events.size(); i++) {

        float next_event_time = std::numeric_limits<float>::max();

        int end_node = 0;
        if (i != t.events.size() - 1) {

            next_event_time = t.times[i + 1];
        } else {

            end_node = 1;
        }

        log("   merging mastertrace with " + t.names[i]);

        prev_node = merge_letter(mt, t.events[i], t.names[i], t.times[i], t.count, prev_node, t.shorthand, next_event_time, end_node, t, i, 0);

    }
}

master_trace step_2_build_graph(std::vector<unique_trace> &unique_traces) {

    //init mastertrace
    master_trace mt;
    mt.base_nodes.clear();
    mt.total_node_count = 0;
    mt.node_to_merge = -1;

    //std::sort(unique_traces.begin(), unique_traces.end());

    //setup first trace in master trace
    unique_trace base_trace = unique_traces[0];

    node* prev_node = nullptr;

    for (int i = 0; i < base_trace.events.size(); i++) {

        int end_node = i == base_trace.events.size() - 1 ? 1 : 0;    
        prev_node = add_new_node(mt, base_trace, i, prev_node, end_node);

    }

    export_data(mt, 0);
    //for (int i = 1; i < unique_traces.size(); i++) {
    for (int i = 1; i < SIZE; i++) {
        std::string msg = "Mergine trace: " + unique_traces[i].shorthand + " NUMBER: " + std::to_string(i) + " COUNT: " + std::to_string(unique_traces[i].count);
        log(msg);
        merge_master_trace(mt, unique_traces[i]);
        log("Trace merge done, exporting");
        export_data(mt, i);

    }

    return mt;
      
}




void swap_node(node* node1, node* pot_node) {

    for (int i = 0; i < node1->prev_nodes.size(); i++) {
        node* prev_node = node1->prev_nodes[i];
        
        int count = 0;
        for (int j = 0; j < prev_node->next_nodes.size(); j++) {

            if (prev_node->next_nodes[j]->creationID == node1->creationID) {
                prev_node->next_nodes.erase(prev_node->next_nodes.begin() + j);
                count = prev_node->next_nodes_counts[j];
                prev_node->next_nodes_counts.erase(prev_node->next_nodes_counts.begin() + j);
                break;
            }
        }

        prev_node->extra_node = nullptr;
        prev_node->next_nodes.insert(prev_node->next_nodes.begin(), pot_node);
        prev_node->next_nodes_counts.insert(prev_node->next_nodes_counts.begin(), count);
    }
}

// Goes through graph looking for another node with the same name as node. 
// If found, creates a merged node with those two with the correct edges. 
// Then it checks if the graph is still valid with the new merged node.
// If so, delete the old two nodes and keep the merged one. And exit function.
// otherwise, delete the new merged node and try another combination.
int inner_merge_pass(node* node1, master_trace& mt) {

    std::vector<node*> nodes;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    node* parent = nullptr;

    while(!nodes.empty()) {

        parent = nodes.back();
        nodes.pop_back();
        
        //If two nodes have the same name but are not the same node.
        if (parent->name == node1->name && node1->creationID != parent->creationID) {

            node* pot_node         = new node;
            pot_node->event_type   = node1->event_type;
            pot_node->name         = node1->name;
            pot_node->extra_node   = nullptr;
            pot_node->creationID   = mt.total_node_count;
            pot_node->event_count  = parent->event_count + node1->event_count;
            pot_node->average_time = merge_time(parent->event_count, parent->average_time,
                                                node1->event_count, node1->average_time);


            pot_node->is_attempting_merge = 1;
            pot_node->extra_event_count   = 0;
            pot_node->extra_average_time  = 0;

            pot_node->end_count = node1->end_count + parent->end_count;

            for (int i = 0; i < node1->prev_nodes.size();   i++) { node1->prev_nodes[i]->extra_node   = pot_node; }
            for (int i = 0; i < parent->prev_nodes.size(); i++)  { parent->prev_nodes[i]->extra_node = pot_node; }

            pot_node->next_nodes.insert(pot_node->next_nodes.begin(), 
                                        parent->next_nodes.begin(), parent->next_nodes.end());
            pot_node->next_nodes_counts.insert(pot_node->next_nodes_counts.begin(), 
                                               parent->next_nodes_counts.begin(), parent->next_nodes_counts.end());
            pot_node->next_nodes.insert(pot_node->next_nodes.begin(), 
                                        node1->next_nodes.begin(), node1->next_nodes.end());
            pot_node->next_nodes_counts.insert(pot_node->next_nodes_counts.begin(), 
                                               node1->next_nodes_counts.begin(), node1->next_nodes_counts.end());
            pot_node->unique_traces.insert(pot_node->unique_traces.begin(), 
                                           parent->unique_traces.begin(), parent->unique_traces.end());
            pot_node->unique_traces.insert(pot_node->unique_traces.begin(), 
                                           node1->unique_traces.begin(), node1->unique_traces.end());
            log("CheckingValidMerge");
            int valid = check_valid_merge(mt, std::numeric_limits<float>::max(), 0);

            /*

            A     D - prev_nodes  layer
            | \ / |
            B  B  B - node/parent layer
            | / \ |
            C     E - next_nodes  layer

            */

            if (valid) {
                log("--- MERGING ---");
                mt.total_node_count++; //not great, since 2 nodes are killed...

                //Swaps prev_nodes layer pointer to pot_node from the one to be deleted. Then deletes first node.
                swap_node(node1, pot_node);
                delete node1;
                swap_node(parent, pot_node);
                delete parent;

                pot_node->is_attempting_merge = 0;

                return 1;

            } else {

                delete pot_node;
                for (int i = 0; i < node1->prev_nodes.size();   i++) { node1->prev_nodes[i]->extra_node  = nullptr; }
                for (int i = 0; i < parent->prev_nodes.size(); i++)  { parent->prev_nodes[i]->extra_node = nullptr; }
            
            }
        }
        

        for (int i = 0; i < parent->next_nodes.size(); i++) {
            nodes.insert(nodes.begin(), parent->next_nodes[i]);
        }
    }

    return 0;

}

int merge_pass(master_trace& mt) {

    std::vector<node*> nodes;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    node* parent = nullptr;

    while(!nodes.empty()) {

        parent = nodes.back();
        nodes.pop_back();

        int merge = inner_merge_pass(parent, mt);

        if (merge) { return 1; }

        for (int i = 0; i < parent->next_nodes.size(); i++) {

            nodes.insert(nodes.begin(), parent->next_nodes[i]);

        }
    }

    return 0;

}

void clear_prev_nodes(master_trace& mt) {

    std::vector<node*> nodes;
    node* parent = nullptr;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    
    while(!nodes.empty()) {

        parent = nodes.back();
        nodes.pop_back();

        for (int i = 0; i < parent->next_nodes.size(); i++) {

            node* kid = parent->next_nodes[i];

            parent->prev_nodes.clear();

            nodes.insert(nodes.begin(), kid);
        }
    }
}

void set_prev_nodes(master_trace& mt) {

    std::vector<node*> nodes;
    node* parent = nullptr;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());

    while(!nodes.empty()) {

        parent = nodes.back();
        nodes.pop_back();

        for (int i = 0; i < parent->next_nodes.size(); i++) {

            node* kid = parent->next_nodes[i];

            int already_exists = 0;

            for (node* n : kid->prev_nodes) {
                if (n->creationID == parent->creationID) {
                    already_exists = 1;
                }
            }

            if (!already_exists) {
                kid->prev_nodes.insert(kid->prev_nodes.begin(), parent);
            }

            nodes.insert(nodes.begin(), kid);
        }
    }
}

void step_3_clean_graph(master_trace &mt) {

    clear_prev_nodes(mt);

    set_prev_nodes(mt);

    log("------------EXTRA-----------");
    while(merge_pass(mt)) { 
        log("Nodes were merged. Attempting again..."); 
        clear_prev_nodes(mt);
        set_prev_nodes(mt);
    }
}

void main_algorithm(event_log &data, std::string name) {

    //Step 1
    std::vector<unique_trace> unique_traces = step_1_calc_unique_traces(data.traces);
    std::sort(unique_traces.begin(), unique_traces.end());

    log("Unique Traces created: ", (int)unique_traces.size());

    for (int i = 0; i < SIZE; i++) {
    

        unique_trace &ut = unique_traces[i];

        log("ShortHand: " + ut.shorthand);

        for (std::string name : ut.names) {

            log(name);
        }

    }
 

    //Step 2
    master_trace mt = step_2_build_graph(unique_traces);
    log("step2 done");

    set_prev_nodes(mt);

    std::vector<node*> nodes;
    node* parent = nullptr;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());

    while(!nodes.empty()) {



        parent = nodes.back();
        nodes.pop_back();
        //log("GOING THROUGH: " +  parent->name + " PREV NODES ARE: ");
        int sum = 0;
        for (node* prev : parent->prev_nodes) {
            //log(prev->name);
            for (int i = 0; i < prev->next_nodes.size(); i++) {
                node* prev_next = prev->next_nodes[i];
                //log("?: " + prev_next->name);
                if (prev->next_nodes[i]->creationID == parent->creationID) {
                    sum += prev->next_nodes_counts[i];
                }
            }
        }

        if (sum != parent->event_count) {
            log("");
            log("ERROR: " + parent->name);
            log("SUM: ", sum);
            log("eventcount: ", parent->event_count);
        }

        for (int i = 0; i < parent->next_nodes.size(); i++) {

            node* kid = parent->next_nodes[i];

            

            nodes.insert(nodes.begin(), kid);
        }
    }

    //Step 3
    //step_3_clean_graph(mt);

    // Not part of algorithm, just outputting data.
    //float time_diff = calc_time_diff(mt, data.events, data.traces);

    //log("-------------------------------");
    //log(name + " TIME_DIFF: ", time_diff);
    //log("-------------------------------");

    export_data(mt, 9999);
    
}
       
int main(int argc, char* argv[]) {

    std::string event_log_filename = "Exempel/RequestForPayment.xes_";

    if (argc >= 2) { event_log_filename = argv[1]; }

    log("Running algorithm on: " + event_log_filename);

    XMLDocument xes_doc;
    XMLElement* root_log = open_xes(event_log_filename, xes_doc);
    event_log log_data;
    fill_event_log(root_log, log_data);
    main_algorithm(log_data, event_log_filename);
    
}