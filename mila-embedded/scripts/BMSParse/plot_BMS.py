import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import tkinter as tk
from tkinter import ttk

csv_file = input("give name of BMS.csv file\n")

print("--- REC BMS Live Visualizer ---")
start_input = input("Enter START time in seconds (or press Enter for 0): ").strip()
end_input = input("Enter END time in seconds (or press Enter for LIVE updating): ").strip()

try:
    start_ts_ms = int(float(start_input) * 1000) if start_input else 0
    live_mode = False if end_input else True
    end_ts_ms = int(float(end_input) * 1000) if end_input else None
except ValueError:
    print("Invalid input. Defaulting to show all data in Live Mode.")
    start_ts_ms = 0
    live_mode = True
    end_ts_ms = None

# --- UI WINDOW SETUP ---
root = tk.Tk()
root.title("REC BMS Telemetry Dashboard")
root.geometry("2200x1000")

# Create a canvas and scrollbar for the scrollable area
main_frame = tk.Frame(root)
main_frame.pack(fill=tk.BOTH, expand=1)

canvas_ui = tk.Canvas(main_frame)
canvas_ui.pack(side=tk.LEFT, fill=tk.BOTH, expand=1)

scrollbar = ttk.Scrollbar(main_frame, orient=tk.VERTICAL, command=canvas_ui.yview)
scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

canvas_ui.configure(yscrollcommand=scrollbar.set)
canvas_ui.bind('<Configure>', lambda e: canvas_ui.configure(scrollregion=canvas_ui.bbox("all")))

# Bind mouse wheel scrolling for Linux (X11)
def _on_mousewheel_linux(event):
    if event.num == 4:
        canvas_ui.yview_scroll(-1, "units")
    elif event.num == 5:
        canvas_ui.yview_scroll(1, "units")
root.bind_all('<Button-4>', _on_mousewheel_linux)
root.bind_all('<Button-5>', _on_mousewheel_linux)

# Create a frame inside the canvas to hold the actual plot
scrollable_frame = tk.Frame(canvas_ui)
canvas_ui.create_window((0, 0), window=scrollable_frame, anchor="nw")

# --- MATPLOTLIB SETUP ---
# Create a very tall figure (24 inches) to hold 6 spacious subplots
fig = Figure(figsize=(18, 32), dpi=100)
fig.subplots_adjust(hspace=0.3, right=0.85, left=0.08)

if live_mode:
    fig.suptitle(f'REC BMS Live Telemetry (Starting at {start_ts_ms / 1000.0}s)', fontsize=18, y=0.99)
else:
    fig.suptitle(f'REC BMS Telemetry ({start_ts_ms / 1000.0}s to {end_ts_ms / 1000.0}s)', fontsize=18, y=0.99)

ax1 = fig.add_subplot(611)
ax2 = fig.add_subplot(612, sharex=ax1)
ax3 = fig.add_subplot(613, sharex=ax1)
ax3_twin = ax3.twinx()
ax4 = fig.add_subplot(614, sharex=ax1)
ax4_twin = ax4.twinx()
ax5 = fig.add_subplot(615, sharex=ax1)
ax6 = fig.add_subplot(616, sharex=ax1)

# Embed the figure into the Tkinter scrollable frame
canvas_fig = FigureCanvasTkAgg(fig, scrollable_frame)
canvas_fig.get_tk_widget().pack(fill=tk.BOTH, expand=1)

def update_plot(frame):
    try:
        df = pd.read_csv(csv_file)
    except Exception:
        return

    if live_mode:
        mask = df['Timestamp_ms'] >= start_ts_ms
    else:
        mask = (df['Timestamp_ms'] >= start_ts_ms) & (df['Timestamp_ms'] <= end_ts_ms)
        
    filtered_df = df.loc[mask]
    if filtered_df.empty: return

    time_sec = filtered_df['Timestamp_ms'] / 1000.0

    # Clear all axes
    for ax in [ax1, ax2, ax3, ax3_twin, ax4, ax4_twin, ax5, ax6]:
        ax.clear()

    # --- Plot 1: Temperatures ---
    temp_cols = [f'Temp{i}_C' for i in range(1, 9)] + ['BMS_Temp_C']
    for col in temp_cols:
        if col in filtered_df.columns and filtered_df[col].any(): 
            ax1.plot(time_sec, filtered_df[col], label=col)
    ax1.set_ylabel('Temperature (°C)', weight='bold')
    ax1.legend(loc='upper left', bbox_to_anchor=(1.02, 1))
    ax1.grid(True, linestyle='--', alpha=0.7)

    # --- Plot 2: Current & Limits ---
    ax2.plot(time_sec, filtered_df['Pack_A'], label='Actual Pack Amps', color='darkorange', linewidth=2)
    ax2.plot(time_sec, filtered_df['MaxCharge_A'], label='Max Charge Limit', color='green', linestyle='--')
    ax2.plot(time_sec, filtered_df['MaxDischarge_A'], label='Max Discharge Limit', color='red', linestyle='--')
    ax2.set_ylabel('Current (A)', weight='bold')
    ax2.legend(loc='upper left', bbox_to_anchor=(1.02, 1))
    ax2.grid(True, linestyle='--', alpha=0.7)

    # --- Plot 3: Voltage & Cell Levels ---
    color_pack = 'tab:red'
    ax3.plot(time_sec, filtered_df['Pack_V'], label='Actual Pack Volts', color=color_pack, linewidth=2.5)
    ax3.plot(time_sec, filtered_df['MaxCharge_V'], label='Max Pack V Limit', color='green', linestyle='--')
    ax3.plot(time_sec, filtered_df['MinDischarge_V'], label='Min Pack V Limit', color='purple', linestyle='--')
    ax3.set_ylabel('Pack Voltage (V)', color=color_pack, weight='bold')
    ax3.tick_params(axis='y', labelcolor=color_pack)
    ax3.grid(True, linestyle='--', alpha=0.7)

    color_cells = 'tab:blue'
    ax3_twin.set_ylabel('Cell Voltage (V)', color=color_cells, weight='bold')
    ax3_twin.yaxis.set_label_position("right")
    ax3_twin.yaxis.tick_right()
    for i in range(1, 17):
        col = f'Cell{i}_V'
        if col in filtered_df.columns and filtered_df[col].any():
            ax3_twin.plot(time_sec, filtered_df[col], linestyle='-', alpha=0.7, label=col)
    ax3_twin.tick_params(axis='y', labelcolor=color_cells)
    
    l1, lab1 = ax3.get_legend_handles_labels()
    l2, lab2 = ax3_twin.get_legend_handles_labels()
    ax3_twin.legend(l1 + l2, lab1 + lab2, loc='upper left', bbox_to_anchor=(1.08, 1))

    # --- Plot 4: Capacity (SOC, SOH, Ah) ---
    ax4.plot(time_sec, filtered_df['SOC_%'], label='SOC %', color='tab:green', linewidth=2)
    ax4.plot(time_sec, filtered_df['SOH_%'], label='SOH %', color='tab:purple', linewidth=2)
    ax4.set_ylabel('Percentage (%)', weight='bold')
    ax4.set_ylim(-5, 105)
    ax4.grid(True, linestyle='--', alpha=0.7)

    ax4_twin.yaxis.set_label_position("right")
    ax4_twin.yaxis.tick_right()
    ax4_twin.plot(time_sec, filtered_df['Ah'], label='Available Ah', color='tab:brown', linewidth=2, linestyle='-.')
    ax4_twin.set_ylabel('Capacity (Ah)', color='tab:brown', weight='bold')
    ax4_twin.tick_params(axis='y', labelcolor='tab:brown')
    
    l3, lab3 = ax4.get_legend_handles_labels()
    l4, lab4 = ax4_twin.get_legend_handles_labels()
    ax4_twin.legend(l3 + l4, lab3 + lab4, loc='upper left', bbox_to_anchor=(1.08, 1))

    # --- Plot 5: Status & Errors ---
    ax5.step(time_sec, filtered_df['ErrorNum'], label='Error Code', where='post')
    ax5.step(time_sec, filtered_df['ErrorCellTemp'], label='Error Source', where='post')
    ax5.step(time_sec, filtered_df['Contactor_Status'], label='Contactor Status', where='post', linestyle='--')
    ax5.step(time_sec, filtered_df['IO_Status'], label='I/O Status', where='post', linestyle=':')
    ax5.set_ylabel('Status / Codes', weight='bold')
    ax5.legend(loc='upper left', bbox_to_anchor=(1.02, 1))
    ax5.grid(True, linestyle='--', alpha=0.7)

    # --- Plot 6: Balancing Mask ---
    # Convert '0x0000' strings safely to integers for plotting
    bal_int = filtered_df['Balancing_Status_Hex'].astype(str).apply(lambda x: int(x, 16) if x.startswith('0x') else 0)
    ax6.step(time_sec, bal_int, label='Balancing Mask (Int)', where='post', color='tab:red')
    ax6.set_ylabel('Balancing Status', weight='bold')
    ax6.set_xlabel('Time (Seconds)', weight='bold', fontsize=12)
    ax6.legend(loc='upper left', bbox_to_anchor=(1.02, 1))
    ax6.grid(True, linestyle='--', alpha=0.7)

print("\nStarting visualizer... Close the pop-up window to exit.")

# Attach animation to the figure
ani = FuncAnimation(fig, update_plot, interval=1000, cache_frame_data=False)

# Safe closing protocol
def on_closing():
    root.quit()
    root.destroy()
root.protocol("WM_DELETE_WINDOW", on_closing)

# Start the Tkinter loop
root.mainloop()