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
    std::string shorthand;
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
    std::vector<node*> next_nodes;
    std::vector<int> next_nodes_counts;
    std::vector<node*> prev_nodes;
    node* extra_node = nullptr;
    int is_attempting_merge;
    int extra_event_count;
    float extra_average_time;
    std::vector<std::string> unique_traces;

    int deleted = 0;

    float time_diff;

    bool operator<(const Node &other) const {
        return time_diff > other.time_diff;  
    }

};

struct master_trace {

    std::vector<node*> base_nodes;
    int total_node_count;
    //List of Each event Type and their count
    std::vector<node*> closest_time_nodes;
    node* node_to_merge;

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

        generate_shorthand(data, t);

        data.traces.push_back(t);

    }

    data.average_events_per_trace = (float)data.events / (float)data.traces.size();
}

void generate_shorthand(event_log &log, trace &t) {

    t.shorthand = "";

    for (int i = 0; i < t.events.size(); i++) {

        t.events[i].type = -1

        for (int j = 0; j < log.activity_names.size(); j++) {

            if (t.events[i].name == log.activity_names[j]) {

                t.events[i].type = 'A' + i;
                break;
            }
        }

        if (t.events[i].type == -1) { log("error generating shorthand"); }
        t.shorthand += t.events[i].type;
    }
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

void export_data(master_trace& mt) {

    std::ofstream connections("connections.txt");
    std::ofstream timestamps("timestamps.txt");

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

        if (parent->next_nodes.size() == 0) {

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
        connections << "Start:" << start_output << std::endl;

    }

    // End nodes
    for (int i = 0; i < exit_nodes.size(); i++) {

        std::string output = exit_nodes[i]->name + std::to_string(exit_nodes[i]->creationID);
        connections << "End:" << output << std::endl;

    }


    connections.close();
    timestamps.close();

}

float calc_time_diff_event(node* node) {

    float sum = 0;

    for (int i = 0; i < traces.size(); i++) {

        for (int j = 0; j < node->unique_traces.size(); j++) {

            if (node->unique_traces[j] == traces[i].type) {

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

void calc_time_diff(master_trace& mt, int event_count) {

    float sum = 0;

    for (trace &t : mt.traces) {
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

        sum += calc_time_diff_event(parent);

        for (int i = 0; i < parent->next_nodes.size(); i++) {

            node* kid = parent->next_nodes[i];
            
            nodes.insert(nodes.begin(), kid);
        }
    }

    for (trace &t : mt.traces) {
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

std::vector<unique_trace> step_1_calc_unique_traces(std::vector<traces> &traces) {

    std::vector<unique_trace> unique_traces;

    for (int i = 0; i < data.traces.size(); i++) {

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
            new_unique.rep   = rep;
            new_unique.names = names;
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

            d.UniqueTraces[index].count++;
        }
    }
}

int check_valid_merge(master_trace& mt) {

    std::vector<node*> nodes;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    Node* parent = nullptr;

    while(!nodes.empty()) {

        parent = nodes.back(); 
        nodes.pop_back(); 

        for (int i = 0; i < parent->next_nodes.size(); i++) {

            Node* kid = parent->next_nodes[i];

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
                    log("failed merge - parent is attempting merge - SHOULD NOT OCCUR");
                } else if (!kid->is_attempting_merge){
                    log("failed merge - no merge attempt - SHOULD NOT OCCUR");
                }

                return 0;
            }
  
            nodes.insert(nodes.begin(), kid);
        }

        if (parent->extra_node != nullptr) {
            nodes.insert(nodes.begin(), parent->extra_node);

            if (!parent->extra_node->is_attempting_merge) {
                log("error - checkvalidmerge");
                exit(0);
            }

            node* kid = parent->extra_node;
            float kid_time = kid->average_time;
            kid_time = merge_time(kid->event_count, kid->average_time, 
                                  kid->extra_event_count, kid->extra_average_time);

            if (kid_time < parent->average_time) {
                return 0;
            }
        }
    }

    //went through entire graph, all good!
    return 1;
}


//prev_node is used when creating a new node, since it needs to be tied in to the structure somehow
Node* merge_letter(master_trace &mt, char event_type, std::string name, float event_time, int event_count, node* prev_node, std::string shorthand) {


    // break out into closest time node function
    // BFS:
    //     if node_event_type == event_type:
    //     node.time_diff = abs(node.average_time - event_time)
    //     closest_time_node.insert(node)

    std::vector<node*> nodes;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    node* parent = nullptr;

    mt.closest_time_nodes.clear();

    int count = 0;
    while(!nodes.empty()) {
        count++;
        if (count > 100) { log("mergeletter error"); exit(0); }


        parent = nodes.back();
        nodes.pop_back();

        if (parent->event_type == event_type) {
            parent->time_diff = parent->average_time - event_time;
            parent->time_diff = parent->time_diff < 0 ? parent->time_diff * -1 : parent->time_diff;
            mt.closest_time_nodes.insert(mt.closest_time_nodes.begin(), parent);
        }

        for (int i = 0; i < parent->next_nodes.size(); i++) {
            nodes.insert(nodes.begin(), parent->next_nodes[i]);
        }
    }

    std::sort(mt.closest_time_nodes.begin(), mt.closest_time_nodes.end());


    /*
      A              A(10)    A(100)
     / \             |        |
    B   C            B(10)    C(100)
     \ /             |        |
      D              D(10)    D(100)

          */

    node* merged_node = nullptr;
    count = 0;
    int merged = 0;
    node* node_ptr = nullptr;
    while(!mt.closest_time_nodes.empty()) {

        count++;

        node_ptr = mt.closest_time_nodes.back();
        mt.closest_time_nodes.pop_back();
        node_ptr->is_attempting_merge = 1;
        node_ptr->extra_event_count   = event_count;
        node_ptr->extra_average_time  = event_time;

        if (prev_node != nullptr) {
            prev_node->extra_node = node_ptr;
        }

        merged = check_valid_merge(mt);

        if (prev_node != nullptr) {
            prev_node->extra_node = nullptr;
        }

        if (merged) {  
            //combine times TODO
            float prev_avg = node_ptr->average_time;
            node_ptr->average_time = merge_time(node_ptr->event_count, node_ptr->average_time,
                                                node_ptr->extra_event_count, node_ptr->extra_average_time);
            node_ptr->event_count += node_ptr->extra_event_count;
            merged_node = node_ptr;


            node_ptr->is_attempting_merge = 0;
            node_ptr->extra_event_count   = 0;
            node_ptr->extra_average_time  = 0;
            node_ptr->unique_traces.push_back(rep);
            log("successfully merging node: " + name);

            
            if (prev_node != nullptr) {

                //add check so a node does not point to same node twice or more
                int already_exists = 0;
                int exists_index = -1;

                for (int i = 0; i < prev_node->next_nodes.size(); i++) {

                    if (prev_node->next_nodes[i]->name == name) {
                        already_exists = 1;
                        exists_index = i;
                    }
                }

                if (already_exists) {
                    log("prev node exists and has this name already. only updating edge count");
                    prev_node->next_nodes_counts[exists_index] += event_count;
                } else {
                    prev_node->next_nodes.push_back(merged_node);
                    prev_node->next_nodes_counts.push_back(event_count);
                    log("MERGE - prev node existed. added " + name + " to " + prev_node->name + "'s next nodes");
                }

            } else {

                // just an assert
                int base_node_found = 0;
                for (int i = 0; i < mt.base_nodes.size(); i++) {
                    if (merged_node->name == mt.base_nodes[i]->name) {
                        base_node_found == 1;
                    } 
                }

                if (base_node_found == 0) {
                    log("ERROR - no prev node found but merged node is not a base node. NAME: " + name);
                    exit(0);
                }

            }

            break;
        }

        node_ptr->is_attempting_merge = 0;
        node_ptr->extra_event_count   = 0;
        node_ptr->extra_average_time  = 0;
    }

    if (merged == 0) {

        merged_node = new node;
        merged_node->creationID = mt.total_node_count;
        if (merged_node->creationID > 1000) {
            log("WTF");
            exit(0);
        }   
        mt.total_node_count++;
        merged_node->event_type          = event_type;
        merged_node->name                = name;
        merged_node->event_count         = event_count;
        merged_node->average_time        = event_time;
        merged_node->is_attempting_merge = 0;
        merged_node->unique_traces.push_back(rep);

        if (prev_node != nullptr) {
            prev_node->next_nodes.push_back(merged_node);
            prev_node->next_nodes_counts.push_back(merged_node->event_count);
            log("NO MERGE - prev node existed. added " + name + " to " + prevNode->name + "'s next nodes");
        } else {
            mt.base_nodes.push_back(merged_node);
            log("Added alternate start with name:" + name);
        }

    }

    return merged_node;
}


void merge_master_trace(master_trace& mt, unique_trace t) {

    node* prev_node = nullptr;

    for (int i = 0; i < t.events.size(); i++) {

        log("merging mastertrace with " + t.names[i]);
        prev_node = merge_letter(mt, t.events[i], t.names[i], t.times[i], t.count, prev_node, t.shorthand);

    }
}

master_trace step_2_build_graph(std::vector<unique_trace> &unique_traces) {

    //init mastertrace
    master_trace mt;
    mt.base_nodes.clear();
    mt.total_node_count = 0;
    mt.closest_time_nodes.clear();
    mt.node_to_merge = nullptr;

    std::sort(unique_traces.begin(), unique_traces.end());

    //setup first trace in master trace
    unique_trace base_trace = unique_traces[0];

    node* last_node = nullptr;

    for (int i = base_trace.events.size() - 1; i >= 0; i--) {

        node* node = new node;
        node->creationID = i;

        node->event_count  = base_trace.count;
        node->name         = base_trace.names[i];
        node->event_type   = base_trace.events[i];
        node->average_time = base_trace.times[i];

        node->is_attempting_merge = 0;
        node->extra_event_count   = 0;
        node->extra_average_time  = 0;
        node->unique_traces.push_back(base_trace.shorthand);
        d.mt.total_node_count++;
        
        if (last_node != nullptr) {
            node->next_nodes.push_back(last_node);
            node->next_nodes_counts.push_back(node->event_count);
        }

        last_node = node;

        if (i == 0) {
            mt.base_nodes.push_back(node);
        }

    }

    for (int i = 1; i < unique_traces.size(); i++) {

        merge_master_trace(mt, unique_traces[i]);
    }

    return mt;
      
}


void swap_node(node* node, node* pot_node) {

    for (int i = 0; i < node->prev_nodes.size(); i++) {
        node* prev_node = node->prev_nodes[i];
        
        int count = 0;
        for (int j = 0; j < prev_node->next_nodes.size(); j++) {

            if (prev_node->next_nodes[j]->creationID == node->creationID) {
                kid->next_nodes.erase(kid->next_nodes.begin() + j);
                count = prev_node->next_nodes_counts[j];
                kid->next_nodes_counts.erase(kid->next_nodes_counts.begin() + j);
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
int inner_merge_pass(node* node, master_trace& mt) {

    std::vector<node*> nodes;
    nodes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    node* parent = nullptr;

    std::string name = node->name;

    while(!nodes.empty()) {

        parent = nodes.back();
        nodes.pop_back();
        
        //If two nodes have the same name but are not the same node.
        if (parent->name == name && node->creationID != parent->creationID) {

            node* pot_node         = new node;
            pot_node->event_type   = node->event_type;
            pot_node->name         = node->name;
            pot_node->extra_node   = nullptr;
            pot_node->creationID   = mt.total_node_count;
            pot_node->event_count  = parent->event_count + node->event_count;
            pot_node->average_time = merge_time(parent->event_count, parent->average_time,
                                                node->event_count, node->average_time);


            pot_node->is_attempting_merge = 1;
            pot_node->extra_event_count   = 0;
            pot_node->extra_average_time  = 0;

            for (int i = 0; i < node->prev_nodes.size();   i++) { node->prev_nodes[i]->extra_node   = pot_node; }
            for (int i = 0; i < parent->prev_nodes.size(); i++) { parent->prev_nodes[i]->extra_node = pot_node; }

            pot_node->next_nodes.insert(pot_node->next_nodes.begin(), 
                                        parent->next_nodes.begin(), parent->next_nodes.end());
            pot_node->next_nodes_counts.insert(pot_node->next_nodes_counts.begin(), 
                                               parent->next_nodes_counts.begin(), parent->next_nodes_counts.end());
            pot_node->next_nodes.insert(pot_node->next_nodes.begin(), 
                                        node->next_nodes.begin(), node->next_nodes.end());
            pot_node->next_nodes_counts.insert(pot_node->next_nodes_counts.begin(), 
                                               node->next_nodes_counts.begin(), node->next_nodes_counts.end());
            pot_node->unique_traces.insert(pot_node->unique_traces.begin(), 
                                           parent->unique_traces.begin(), parent->unique_traces.end());
            pot_node->unique_traces.insert(pot_node->unique_traces.begin(), 
                                           node->unique_traces.begin(), node->unique_traces.end());
            log("CheckingValidMerge");
            int valid = CheckValidMerge(mt);

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
                swap_node(node, pot_node);
                delete node;
                swap_node(parent, pot_node);
                delete parent;

                //double check this then remove comment. pls
                pot_node->is_attempting_merge = 0;
                for (int i = 0; i < node->prev_nodes.size();   i++) { node->prev_nodes[i]->extra_node   = nullptr; }
                for (int i = 0; i < parent->prev_nodes.size(); i++) { parent->prev_nodes[i]->extra_node = nullptr; }

                return 1;

            } else {

                delete pot_node;
                for (int i = 0; i < node->prev_nodes.size();   i++) { node->prev_nodes[i]->extra_node   = nullptr; }
                for (int i = 0; i < parent->prev_nodes.size(); i++) { parent->prev_nodes[i]->extra_node = nullptr; }
            
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

            kid->prev_nodes.insert(kid->prev_nodes.begin(), parent);

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

void main_algorithm(event_log &data) {

    traces &traces = data.traces;
    //Step 1
    std::vector<unique_trace> unique_traces = step_1_calc_unique_traces(traces);

    //Step 2
    master_trace mt = step_2_build_graph(unique_traces);

    //Step 3
    step_3_clean_graph(mt);

    calc_time_diff(mt, data.events);

    export_data(mt);
    
}
       

int main(int argc, char* argv[]) {

    std::string event_log_filename = "Exempel/RequestForPayment.xes_";

    if (argc >= 2) { event_log_filename = argv[1]; }

    log("Running algorithm on: " + xes_filename);

    XMLDocument xes_doc;
    XMLElement* root_log = open_xes(xes_filename, xes_doc);
    xes_data log_data;
    fill_event_log(root_log, log_data);
    main_algorithm(log_data);
    
}