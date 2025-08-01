import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime, timedelta
import argparse
import matplotlib.dates as mdates
import sys

# --------------------------
# Custom CLI argument parser
# --------------------------
def parse_args():
    parser = argparse.ArgumentParser(description="Plot CO₂ and Photoresistivity per cell with flexible input.")
    parser.add_argument('--file', required=True, help="Path to JSONL file")
    parser.add_argument('--cells', action='store_true', help="Plot all cells with both C and P")
    parser.add_argument('--cell', type=int, action='append', help="Specify individual cellId (repeatable)")
    parser.add_argument('--remove', choices=['C', 'P'], action='append', help="Remove CO2 (C) or Photoresistivity (P) for the last --cell")
    return parser.parse_args()

def build_cell_config(args):
    config = {}

    if args.cells:
        df = pd.read_json(args.file, lines=True)
        all_ids = df['cellId'].unique()
        for cid in all_ids:
            config[cid] = {"CO2": True, "Photoresistivity": True}
        return config

    if not args.cell:
        print("Error: You must provide at least --cells or one --cell")
        sys.exit(1)

    current_cell = None
    for arg in sys.argv:
        if arg == '--cell':
            current_cell = None  # Reset before getting the next value
        elif current_cell is None and arg.isdigit():
            current_cell = int(arg)
            config[current_cell] = {"CO2": True, "Photoresistivity": True}
        elif arg == '--remove':
            continue
        elif arg in ['C', 'P']:
            if current_cell is None:
                print("Error: --remove must follow a --cell")
                sys.exit(1)
            if current_cell not in config:
                config[current_cell] = {"CO2": True, "Photoresistivity": True}
            if arg == 'C':
                config[current_cell]["CO2"] = False
            elif arg == 'P':
                config[current_cell]["Photoresistivity"] = False

    return config

# --------------------------
# Main Plotting Code
# --------------------------
args = parse_args()
cell_config = build_cell_config(args)

# Load and prepare data
df = pd.read_json(args.file, lines=True)
data_fields = df['data'].apply(pd.Series)
df = pd.concat([df.drop(columns=['data']), data_fields], axis=1)
df['datetime'] = pd.to_datetime(df['ts'], unit='s') - timedelta(hours=7)

fig, ax1 = plt.subplots(figsize=(12, 6))
ax2 = ax1.twinx()
color_map = plt.cm.get_cmap("tab10", len(cell_config))

for idx, (cell_id, features) in enumerate(sorted(cell_config.items())):
    bucket_df = df[df['cellId'] == cell_id]
    colors = bucket_df['state'].map({0: 'blue', 1: 'red'})

    if features["CO2"]:
        ax1.plot(bucket_df['datetime'], bucket_df['CO2'], label=f"CO₂ (Cell {cell_id})", color=color_map(idx))
        ax1.scatter(bucket_df['datetime'], bucket_df['CO2'], c=colors, s=20)

    if features["Photoresistivity"]:
        ax2.plot(bucket_df['datetime'], bucket_df['Photoresistivity'], linestyle='--',
                 label=f"Photoresistivity (Cell {cell_id})", color=color_map(idx))

# Formatting
ax1.set_xlabel("Time")
ax1.set_ylabel("CO₂ (ppm)", color='black')
ax2.set_ylabel("Photoresistivity (V)", color='gray')
ax1.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))

# Combine legends
lines1, labels1 = ax1.get_legend_handles_labels()
lines2, labels2 = ax2.get_legend_handles_labels()
ax1.legend(lines1 + lines2, labels1 + labels2, loc='upper left')

plt.title("Sensor Data (CO₂ and Photoresistivity by Cell)")
plt.grid(True)
fig.tight_layout()
plt.show()
