import sys
from pathlib import Path

# App directory
project_root = Path.cwd().parent  # Adjust this if necessary
pm4py_path = project_root / "pm4py"
sys.path.insert(0, str(pm4py_path.parent))

import pm4py
from pm4py.visualization.dfg import visualizer as dfg_visualizer
from pm4py.algo.discovery.dfg.variants import clean_time
from pm4py.visualization.dfg.variants import timeline as timeline_gviz_generator

import pandas as pd

if (len(sys.argv) != 4):
    print("Usage: OutputName connections timestamps")
    exit(0)

out_file  = sys.argv[1]
conn_file = sys.argv[2]
time_file = sys.argv[3]

activity_durations = {}

with open(time_file, "r") as file:
    for line in file:
        if '|' in line:
            activity, duration = line.strip().split("|")
            activity = activity.strip()
            duration = duration.strip()
            activity_durations[activity] = pd.to_timedelta(duration)

print(activity_durations)

# Read the DFG data from a text file
dfg = {}
start_activities = {}
end_activities = {}

with open(conn_file, "r") as file:
    for line in file:
        line = line.strip()
        if line.startswith("Start:"):
            activity = line.split(":")[1]
            start_activities[activity] = start_activities.get(activity, 0) + 1
        elif line.startswith("End:"):
            activity = line.split(":")[1]
            end_activities[activity] = end_activities.get(activity, 0) + 1
        else:
            source, target, freq = line.split(",")
            dfg[(source, target)] = int(freq)


print(dfg)
print(start_activities)
print(end_activities)


gviz = timeline_gviz_generator.apply(dfg, activity_durations, parameters={"format": "png", "start_activities": start_activities,
                                                                    "end_activities": end_activities})
dfg_visualizer.save(gviz, out_file)
