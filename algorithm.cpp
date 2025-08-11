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

void log(std::string s)          { std::cerr << "LOG: " << s << std::endl; }
void log(char c)                 { std::cerr << "LOG: " << c << std::endl; }
void log(std::string s, char c)  { std::cerr << "LOG: " << s << c << std::endl; }
void log(std::string s, int i)   { s += std::to_string(i); log(s); }
void log(std::string s, float i) { s += std::to_string(i); log(s); }

#define SIZE 89

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

    //bool operator<(const unique_trace &other) const {
        //return shorthand.size() > other.shorthand.size();
    //}
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

    int used = 1;

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
    int last_count;
    int recursion = 0;
    int has_recursed = 0;

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

//done
void export_data(master_trace& mt, std::string ii) {
    std::string conn_out = "connections" + ii + ".txt";
    std::string time_out = "timestamps"  + ii + ".txt";

    std::ofstream connections(conn_out);
    std::ofstream timestamps(time_out);

    if (!connections || !timestamps) {
        std::cerr << "Error opening file for writing." << std::endl;
        exit(1);
    }

    std::vector<int> node_indexes;
    int parent_index = -1;
    node_indexes.insert(node_indexes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    std::vector<int> visited;

    std::vector<int> exit_node_indexes;

    std::vector<std::string> connection_strings;
    std::vector<std::string> timestamp_strings;

    while(!node_indexes.empty()) {

        parent_index = node_indexes.back();
        node_indexes.pop_back();
	    node& parent = mt.nodes_container[parent_index];
        visited.push_back(parent_index);

        std::string parent_s = parent.name + std::to_string(parent.creationID);
        std::string timestamp = seconds_to_timedelta_string(parent.average_time);
        std::string timestamp_out = parent_s + " | " + timestamp;

        if (std::find(timestamp_strings.begin(), timestamp_strings.end(), timestamp_out) == timestamp_strings.end()) {
            timestamps << timestamp_out << std::endl;
            timestamp_strings.push_back(timestamp_out);
        }

        

        for (int i = 0; i < parent.next_nodes.size(); i++) {

      	    int kid_index = parent.next_nodes[i];
    	    node& kid = mt.nodes_container[kid_index];
            
            std::string kid_s    = kid.name + std::to_string(kid.creationID);
            std::string count    = std::to_string(parent.next_nodes_counts[i]);
            std::string connection_out = parent_s + "," + kid_s + "," + count;


            if (std::find(connection_strings.begin(), connection_strings.end(), connection_out) == connection_strings.end()) {
                connections << connection_out << std::endl;
                connection_strings.push_back(connection_out);
            }

            if (std::find(visited.begin(), visited.end(), kid_index) == visited.end()) {

                node_indexes.insert(node_indexes.end(), kid_index);
                visited.push_back(kid_index);
            }            

        }

        if (parent.end_count != 0) {

            int exists_already = 0;
            for (int e_i :  exit_node_indexes) {

                if (parent_index == e_i) {
                    exists_already = 1;
                }
            }

            if (exists_already == 0) {
                exit_node_indexes.push_back(parent_index);
            }

        }
    }

    // Start nodes
    for (int i : mt.base_nodes) {

	   node& n = mt.nodes_container[i];
        std::string start_output = n.name + std::to_string(n.creationID);
        std::string count    = std::to_string(n.event_count);
        connections << "Start:" << start_output << "," << count << std::endl;

    }

    // End nodes
    for (int i : exit_node_indexes) {

	    node& n = mt.nodes_container[i];
        std::string output = n.name + std::to_string(n.creationID);
        std::string count  = std::to_string(n.end_count);
        connections << "End:" << output << "," << count << std::endl;

    }


    connections.close();
    timestamps.close();

}

void export_data(master_trace& mt, int ii) {

    std::string s = std::to_string(ii);
    export_data(mt, s);

}

//done
float calc_time_diff_event(node& node, std::vector<trace> &traces) {

    float sum = 0;

    for (int i = 0; i < traces.size(); i++) {

        for (int j = 0; j < node.unique_traces.size(); j++) {

            if (node.unique_traces[j] == traces[i].shorthand) {

                for (int k = 0; k < traces[i].events.size(); k++) {

                    //we finally found a hit!
                    if (node.event_type == traces[i].events[k].type && traces[i].events[k].used == 0) {

                        float rel_time = traces[i].events[k].time - traces[i].events[0].time;
                        float diff = node.average_time - rel_time;
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

//done
float calc_time_diff(master_trace& mt, int event_count, std::vector<trace> &traces) {

    float sum = 0;

    for (trace &t : traces) {
        for (event &e : t.events) {
            e.used = 0;
        }
    }

    std::vector<int> node_indexes;
    int parent_index = -1;
    node_indexes.insert(node_indexes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());

    while(!node_indexes.empty()) {

        parent_index = node_indexes.back();
        node_indexes.pop_back();
	node& parent = mt.nodes_container[parent_index];

        sum += calc_time_diff_event(parent, traces);

        node_indexes.insert(node_indexes.begin(), parent.next_nodes.begin(), parent.next_nodes.end());
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



int loop(std::vector<int> &node_indexes, std::vector<int> &visited, master_trace &mt) {

    int parent_index = node_indexes.back();
    visited.insert(visited.begin(), parent_index);
    
    const node& parent = mt.nodes_container[parent_index];

    for (int i : parent.next_nodes) {

        //loop found
        if (std::find(node_indexes.begin(), node_indexes.end(), i) != node_indexes.end()) {

            return 1;
        }

        if (std::find(visited.begin(), visited.end(), i) == visited.end()) {

            node_indexes.insert(node_indexes.end(), i);
            if (loop(node_indexes, visited, mt)) {
                return 1;
            }
        }

    }

    if (parent.extra_node != -1) {

        if (std::find(node_indexes.begin(), node_indexes.end(), parent.extra_node) != node_indexes.end()) {

            return 1;
        }

        if (std::find(visited.begin(), visited.end(), parent.extra_node) == visited.end()) {

            node_indexes.insert(node_indexes.end(), parent.extra_node);

            if (loop(node_indexes, visited, mt)) {
                return 1;
            }
        }
            
    }

    node_indexes.pop_back();

    return 0;

}

//1 if loop exists
int check_for_loops(master_trace &mt) {

    std::vector<int> node_indexes;
    std::vector<int> visited;
    node_indexes.insert(node_indexes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());

    while(!node_indexes.empty()) {

        if (loop(node_indexes, visited, mt)) {

            return 1;
        }
    }

    return 0;
}

//extra node is solved by check
//done
int check_valid_merge(master_trace& mt, float next_time, unique_trace t, int trace_index, int skip_loop) {

    if (!skip_loop) {
        if (check_for_loops(mt)) { return -1; }
    }

    int check_next_time = 1;
    if (next_time < 0) { check_next_time = 0; }

    std::vector<int> visited;
    std::vector<int> node_indexes;
    node_indexes.insert(node_indexes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    int parent_index = -1;

    while(!node_indexes.empty()) {

        parent_index = node_indexes.back(); 
        node_indexes.pop_back();
        const node& parent = mt.nodes_container[parent_index];	
        visited.push_back(parent_index);

        float parent_time = parent.average_time;
        if (parent.is_attempting_merge) {

            parent_time = merge_time(parent.event_count, parent.average_time, 
                                     parent.extra_event_count, parent.extra_average_time);
        }

        if (check_next_time && parent.is_attempting_merge && parent_time > next_time) {
            //log("rejected because next time");
            return 0;
        }

        for (int i : parent.next_nodes) {

            const node& kid = mt.nodes_container[i];

            float kid_time = kid.average_time;
            if (kid.is_attempting_merge) {

                kid_time = merge_time(kid.event_count, kid.average_time, 
                                      kid.extra_event_count, kid.extra_average_time);
            }

            if (kid_time < parent_time) {

                if (parent.is_attempting_merge) {

                    if (kid.used) { return -1; }
                    int found = 0;
                    for (int j = trace_index + 1; j < t.shorthand.size(); j++) {
                        if (kid.event_type == t.shorthand[j]) { 
                            float new_time = merge_time(kid.event_count, kid.average_time,
                                                    t.count, t.times[j]);

                            if (new_time > parent_time) {
                                found = 1;
                            } 
                        }
                    }


                    if (!found) { return -1; }

                    //log("           match rejected! - parent attemptin merge");
                    
                    //log("           Offending nodes:");
                    //log("               parent: " + parent.name + " time: ", parent_time);
                    //log("               kid: " + kid.name + " time: ", kid_time);
                    return 0;

                } else if (!kid.is_attempting_merge){

		            if (mt.recursion || mt.has_recursed) {
    			        //log("		match rejected. Would have failed but we are recursing");
                        if (kid.used && parent.used) { return -1; }
    			        return 0;
		            }

                    log("           failed merge - no merge attempt - SHOULD NOT OCCUR");
                    log("           Offending nodes:");
                    log("               parent: " + parent.name + std::to_string(parent.creationID) + " time: ", parent_time);
                    log("               kid: " + kid.name + std::to_string(kid.creationID) + " time: ", kid_time);
                    export_data(mt, "END");
                    exit(0);
                }
                if (parent.used) { return -1; }
                int found = 0;
                for (int j = trace_index + 1; j < t.shorthand.size(); j++) {
                    if (parent.event_type == t.shorthand[j]) { 
                        float new_time = merge_time(parent.event_count, parent.average_time,
                                                    t.count, t.times[j]);
                        if (new_time < kid_time) {

                            found = 1;
                        } 
                    }
                }

                if (!found) { return -1; }
                //log("           match rejected! - kid attemptin merge");
                return 0;
            }

            
            if (std::find(visited.begin(), visited.end(), i) == visited.end()) {
                node_indexes.insert(node_indexes.end(), i);
                visited.push_back(i);
            }
        }

	//this assumes that 1 node is only ever 
        if (parent.extra_node != -1) {
            
	        const node& kid = mt.nodes_container[parent.extra_node];

            if (!kid.is_attempting_merge) {
                log("           error - checkvalidmerge");
                exit(0);
            }

            float kid_time = merge_time(kid.event_count, kid.average_time, 
                                  kid.extra_event_count, kid.extra_average_time);

            if (kid_time < parent.average_time) {
                //log("           match rejected via extra path - hmm!");
                //log("               parent: " + parent.name + " time: ", parent.average_time);
                //log("               kid: " + kid.name + " time: ", kid_time);
                return 0;
            }

            if (std::find(visited.begin(), visited.end(), parent.extra_node) == visited.end()) {
                node_indexes.insert(node_indexes.end(), parent.extra_node);
                visited.push_back(parent.extra_node);
            }
        }
    }

    //went through entire graph, all good!
    log("           match found!");
    return 1;
}

//done
std::vector<int> get_closest_nodes(master_trace &mt, char event_type, float event_time) {

    std::vector<int> closest_indexes;

    int j = 0;
    for (node& n : mt.nodes_container) {
        
        if (n.event_type == event_type && n.used == 0) {
            n.time_diff = n.average_time - event_time;
            n.time_diff = n.time_diff < 0 ? n.time_diff * -1 : n.time_diff;

            int already_exists = 0;

            for (int i : closest_indexes) {

                if (i == n.creationID) { 
                    already_exists = 1; 
                }
            }

            if (!already_exists) { 

		        int place = 0;
		        for (int i : closest_indexes) {

        			float other_diff = mt.nodes_container[i].time_diff;			
        			if (n.time_diff > other_diff) {
        				break;
        			}
			
			        place++;
		        }

                closest_indexes.insert(closest_indexes.begin() + place, j);
            }

        }

        j++;

    }

    return closest_indexes;
}


//done
int merge_node(master_trace& mt, int merge_index, int prev_index, std::string shorthand, int end_node) {

    node& n = mt.nodes_container[merge_index];

    n.average_time = merge_time(n.event_count, n.average_time,
                                 n.extra_event_count, n.extra_average_time);
    n.event_count += n.extra_event_count;
    
    if (end_node) {
        n.end_count += n.extra_event_count;
    }

    if (prev_index != -1) {

    	node& prev_n = mt.nodes_container[prev_index];
        //add check so a node does not point to same node twice or more
        int already_exists = 0;
        int exists_index = -1;

        for (int i = 0; i < prev_n.next_nodes.size(); i++) {

            if (prev_n.next_nodes[i] == merge_index) {
                already_exists = 1;
                exists_index = i;
            }
        }

        if (already_exists) {
            //log("       prev node exists and has this name already. only updating edge count");
            prev_n.next_nodes_counts[exists_index] += n.extra_event_count;
        } else {
            prev_n.next_nodes.push_back(merge_index);
            prev_n.next_nodes_counts.push_back(n.extra_event_count);
            //log("       MERGE - prev node existed. added to " + prev_n.name + "'s next nodes");
        }
    } 
    
    if (prev_index != -1) {
        mt.nodes_container[prev_index].extra_node = -1;
    }

    n.is_attempting_merge = 0;
    n.extra_event_count   = 0;
    n.extra_average_time  = 0;
    n.unique_traces.push_back(shorthand);

    n.used = 1;

    //log("Merged node: " + n.name, n.creationID);

    return merge_index;
}

//done
int add_new_node(master_trace& mt, const unique_trace& ut, int i, int prev_node_index, int end_node, int recursion) {

    //log("NEW NODE CREATED - NAME: " + ut.names[i], mt.total_node_count);
	
    node new_node;
    new_node.creationID = mt.total_node_count;
    if (new_node.creationID > 1000) {
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

    if (end_node) {
        new_node.end_count = new_node.event_count;
    }

    mt.nodes_container.push_back(new_node);

    if (prev_node_index != -1) {
        mt.nodes_container[prev_node_index].next_nodes.push_back(new_node.creationID);
        mt.nodes_container[prev_node_index].next_nodes_counts.push_back(new_node.event_count);
    } else {
        mt.base_nodes.push_back(new_node.creationID);
        //log("       Added alternate start with name:" + new_node.name);
    }

    if (recursion == 0) { log("Node created - " + new_node.name + std::to_string(new_node.creationID) + " ", new_node.average_time); }

    return new_node.creationID;
}


//prev_node is used when creating a new node, since it needs to be tied in to the structure somehow
int merge_letter(master_trace &mt, unique_trace t, int prev_node_index, int trace_index, int size) {

    char event_type  = t.events[trace_index];
    std::string name = t.names[trace_index];
    float event_time = t.times[trace_index];
    int event_count  = t.count;
    std::string shorthand = t.shorthand;

    int end_node = trace_index == t.events.size() - 1 ? 1 : 0;
    float next_time = -1;
    if (!end_node) { next_time = t.times[trace_index + 1]; }

    std::vector<int> closest_indexes = get_closest_nodes(mt, event_type, event_time);

    int merged_index = -1;
    int can_merge = 0;
    int has_merged = 0;
    int node_index = -1;

    //log("prev_node_index: ", prev_node_index);

    int no_matching_nodes = closest_indexes.size();
    size *= no_matching_nodes;
    std::string p = "";
    for (int i = 0; i < trace_index; i++) {
        p += " ";
    }
    p += event_type;
    p += std::to_string(size);
    //log(p);
    //if (size > 10000) { return -1; }

    

    while(!closest_indexes.empty()) {

        node_index = closest_indexes.back();
        closest_indexes.pop_back();
	    node& node_ptr = mt.nodes_container[node_index];
        node_ptr.is_attempting_merge = 1;
        node_ptr.extra_event_count   = event_count;
        node_ptr.extra_average_time  = event_time;

        int skip_loop = 0;
        if (prev_node_index != -1) {
	       mt.nodes_container[prev_node_index].extra_node = node_index;

           for (int i : mt.nodes_container[prev_node_index].next_nodes) {
                if (i == mt.nodes_container[prev_node_index].extra_node) {
                    skip_loop = 1;
                }
            }
        }

        if (prev_node_index == -1 || end_node) {
            skip_loop = 1;
            
        }

        //if (skip_loop) { log("skippin"); }

                  

        can_merge = check_valid_merge(mt, next_time, t, trace_index, skip_loop);

    	//log("can merge: ", can_merge);

    	if (can_merge == 1) {

           merged_index = merge_node(mt, node_index, prev_node_index, shorthand, end_node);
    	   has_merged = 1;

    	   break;
    	
    	} else if (can_merge == 0 && !end_node) {
           
    	    //copy mastertrace
    	    master_trace mt_copy = mt;
    	    mt_copy.recursion = 1;
    	    //merge node
    	    merge_node(mt_copy, node_index, prev_node_index, shorthand, end_node);
    	    //recursion
    	    //log("R E C U R S I O N !-----------------------------------------------------------------");
            int res_index = merge_letter(mt_copy, t, node_index, trace_index + 1, size);
    	    //log("E N D OF R E C U R S I O N&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&"); 

            if (res_index != -1) {
                
                if (mt.recursion == 0) {
                    
                    log("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
                    log("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
                    log("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
                    log("RECURSION SUCCESS!!!!!!!!!!!!!!!!!!!!!!");
                    log("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
                    log("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
                    log("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
                    
                }

                merged_index = merge_node(mt, node_index, prev_node_index, shorthand, end_node);
    		    has_merged = 1;
                mt.has_recursed = 1;

    		    break;
            }

        }

        if (prev_node_index != -1) {
	       mt.nodes_container[prev_node_index].extra_node = -1;
        }

        node_ptr.is_attempting_merge = 0;
        node_ptr.extra_event_count   = 0;
        node_ptr.extra_average_time  = 0;
    }

    if (has_merged == 0) {

        merged_index = add_new_node(mt, t, trace_index, prev_node_index, end_node, mt.recursion);

    	if (mt.recursion) { 
    		merged_index = -1; 
    		//log("recursion failed");
    	}

    }

    return merged_index;
}


void clear_used_nodes(master_trace& mt) {

    for (node& n : mt.nodes_container) {
        n.used = 0;
    }
}

//done
void merge_master_trace(master_trace& mt, unique_trace t, int detailed) {

    int prev_node_index = -1;
    mt.has_recursed = 0;
    for (int i = 0; i < t.events.size(); i++) {

        clear_used_nodes(mt);
        log("   merging mastertrace with " + t.names[i]);

        prev_node_index = merge_letter(mt, t, prev_node_index, i, 1);

        if (detailed) {
            std::string s = "STEP" + std::to_string(i + 1);
            export_data(mt, s);
        }

    }
}

//done
master_trace step_2_build_graph(std::vector<unique_trace> &unique_traces) {

    log("starting step 2!!!");
    log(" ");
    //init mastertrace
    master_trace mt;
    mt.base_nodes.clear();
    mt.total_node_count = 0;
    mt.node_to_merge = -1;

    //std::sort(unique_traces.begin(), unique_traces.end());

    //setup first trace in master trace
    unique_trace base_trace = unique_traces[0];

    int prev_node_index = -1;
    log("setting up base trace");
    for (int i = 0; i < base_trace.events.size(); i++) {

        int end_node = i == base_trace.events.size() - 1 ? 1 : 0;    
        prev_node_index = add_new_node(mt, base_trace, i, prev_node_index, end_node, 0);

    }

    mt.last_count = base_trace.events.size();

    export_data(mt, 0);
    //for (int i = 1; i < unique_traces.size(); i++) {
    log("going through remaining traces");
    for (int i = 1; i < SIZE; i++) {
        int detailed = 0;
        if (i == 58) { detailed = 1; }
        log("");
        log("");
        log("");
        std::string msg = "Mergine trace: " + unique_traces[i].shorthand + " NUMBER: " + std::to_string(i) + " COUNT: " + std::to_string(unique_traces[i].count);
        log(msg);
        merge_master_trace(mt, unique_traces[i], detailed);
        log("Trace merge done, exporting");
        export_data(mt, i);

    }

    return mt;
      
}



//skip for now
/*
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


//skip for now
//
// Goes through graph looking for another node with the same name as node. 
// If found, creates a merged node with those two with the correct edges. 
// Then it checks if the graph is still valid with the new merged node.
// If so, delete the old two nodes and keep the merged one. And exit function.
// otherwise, delete the new merged node and try another combination.
int inner_merge_pass(int node_index, master_trace& mt) {

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

            

            A     D - prev_nodes  layer
            | \ / |
            B  B  B - node/parent layer
            | / \ |
            C     E - next_nodes  layer

            

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

    std::vector<int> node_indexes;
    node_indexes.insert(nodes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    int parent_index = -1;

    while(!node_indexes.empty()) {

        parent_index = node_indexes.back();
        node_indexes.pop_back();
	node& parent = mt.nodes_container[parent_index];

        int merge = inner_merge_pass(parent_index, mt);

        if (merge) { return 1; }

    	node_indexes.insert(node_indexes.begin(), parent.next_nodes.begin(), parent.next_nodes.end());

    }

    return 0;

}
*/
//done
void clear_prev_nodes(master_trace& mt) {

    std::vector<int> node_indexes;
    int parent_index = -1;
    node_indexes.insert(node_indexes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());
    
    while(!node_indexes.empty()) {

        parent_index = node_indexes.back();
        node_indexes.pop_back();
	    node& parent = mt.nodes_container[parent_index];

        parent.prev_nodes.clear();

    	node_indexes.insert(node_indexes.begin(), parent.next_nodes.begin(), parent.next_nodes.end());
    }
}


void set_prev_nodes(master_trace& mt) {

    std::vector<int> node_indexes;
    int parent_index = -1;
    node_indexes.insert(node_indexes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());

    while(!node_indexes.empty()) {

        parent_index = node_indexes.back();
	    node& parent = mt.nodes_container[parent_index];
        node_indexes.pop_back();

        for (int kid_index : parent.next_nodes) {

            node& kid = mt.nodes_container[kid_index];

            int already_exists = 0;

            for (int prev_index : kid.prev_nodes) {
		        node& prev_node = mt.nodes_container[prev_index];
                if (prev_node.creationID == parent.creationID) {
                    already_exists = 1;
                }
            }

            if (!already_exists) {
                kid.prev_nodes.insert(kid.prev_nodes.begin(), parent_index);
            }

            node_indexes.insert(node_indexes.begin(), kid_index);
        }
    }
}

//done
/*
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

*/
//done
void main_algorithm(event_log &data, std::string name) {

    //Step 1
    std::vector<unique_trace> unique_traces = step_1_calc_unique_traces(data.traces);
    std::sort(unique_traces.begin(), unique_traces.end());

    log("Unique Traces created: ", (int)unique_traces.size());

    for (int i = 0; i < SIZE; i++) {
    

        unique_trace &ut = unique_traces[i];

        log("ShortHand: " + ut.shorthand + " i: ", i);

        //for (int i = 0; i < ut.names.size(); i++) {

          //  std::string name = ut.names[i];
            //float time = ut.times[i];
            //log(name + ": ", time);
        //}

    }
 

    //Step 2
    master_trace mt = step_2_build_graph(unique_traces);
    log("step2 done");

   // set_prev_nodes(mt);

    //error checking
    std::vector<int> node_indexes;
    std::vector<int> visited;
    int parent_index = -1;
    node_indexes.insert(node_indexes.begin(), mt.base_nodes.begin(), mt.base_nodes.end());

    while(!node_indexes.empty()) {



        parent_index = node_indexes.back();
	    node& parent = mt.nodes_container[parent_index];
        node_indexes.pop_back();
        visited.push_back(parent_index);

        int sum = 0;
        for (int prev_index : parent.prev_nodes) {
	        node& prev = mt.nodes_container[prev_index];
            //log(prev->name);
            for (int i = 0; i < prev.next_nodes.size(); i++) {
                int prev_next_index = prev.next_nodes[i];
		        node& prev_next = mt.nodes_container[prev_next_index];
                //log("?: " + prev_next->name);
                if (prev_next.creationID == parent.creationID) {
                    sum += prev.next_nodes_counts[i];
                }
            }
        }

        if (sum != parent.event_count) {
            log("");
            log("ERROR: " + parent.name);
            log("SUM: ", sum);
            log("eventcount: ", parent.event_count);
        }

        for (int i : parent.next_nodes) {

            if (std::find(visited.begin(), visited.end(), i) == visited.end()) {

                node_indexes.insert(node_indexes.end(), i);
                visited.push_back(i);
            }
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
      
//done
int main(int argc, char* argv[]) {

    std::string event_log_filename = "Exempel/DomesticDeclarations.xes_";

    if (argc >= 2) { event_log_filename = argv[1]; }

    log("Running algorithm on: " + event_log_filename);

    XMLDocument xes_doc;
    XMLElement* root_log = open_xes(event_log_filename, xes_doc);
    event_log log_data;
    fill_event_log(root_log, log_data);
    main_algorithm(log_data, event_log_filename);
    
}
