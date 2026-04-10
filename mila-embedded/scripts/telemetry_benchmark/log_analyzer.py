import re
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import xml.etree.ElementTree as ET
from matplotlib.widgets import Slider
import contextily as cx  # <-- Added contextily import for the map background

def parse_log_file(input_path, output_csv_path):
    """
    Parses the raw log file to find 'csv entry' lines, extracts the data,
    and saves it to a CSV file.
    """
    log_pattern = re.compile(
        r"csv entry:\s*(\d+),"      # 1: timestamp_ms (u64)
        r"(\d+),"                  # 2: packetNumber (u64)
        r"(\d+),"                  # 3: packetDataCorrupt (u8)
        r"(\d+),"                  # 4: protocolError (u8)
        r"(\d+),"                  # 5: packetWrongSize (u8)
        r"(-?\d+\.\d+),"            # 6: RSSI (float)
        r"(-?\d+\.\d+),"            # 7: SNR (float)
        r"(\d+),"                  # 8: irqFlags (int)
        r"(\d+)"                   # 9: packetRecvPercent (u8)
    )

    parsed_data = []
    try:
        with open(input_path, 'r') as f:
            for line in f:
                match = log_pattern.search(line)
                if match:
                    parsed_data.append(match.groups())
    except FileNotFoundError:
        print(f"Error: Input file not found at '{input_path}'")
        return pd.DataFrame()

    if not parsed_data:
        return pd.DataFrame()

    columns = [
        'timestamp_ms', 'packetNumber', 'packetDataCorrupt', 'protocolError',
        'packetWrongSize', 'RSSI', 'SNR', 'irqFlags', 'packetRecvPercent'
    ]

    df = pd.DataFrame(parsed_data, columns=columns)
    df = df.astype({
        'timestamp_ms': np.uint64, 'packetNumber': np.uint64, 'packetDataCorrupt': np.uint8,
        'protocolError': np.uint8, 'packetWrongSize': np.uint8, 'RSSI': np.float64,
        'SNR': np.float64, 'irqFlags': np.int32, 'packetRecvPercent': np.uint8
    })

    df['timestamp_s'] = df['timestamp_ms'] / 1000.0
    df.to_csv(output_csv_path, index=False)
    print(f"Successfully parsed {len(df)} entries and saved to '{output_csv_path}'")
    
    return df

def parse_gpx_file(gpx_path):
    """
    Parses a GPX file to extract track points (latitude, longitude, time).
    """
    try:
        tree = ET.parse(gpx_path)
        root = tree.getroot()
        ns = {'gpx': 'http://www.topografix.com/GPX/1/1'}
        
        track_points = []
        for trkpt in root.findall('.//gpx:trkpt', ns):
            lat = trkpt.get('lat')
            lon = trkpt.get('lon')
            time_element = trkpt.find('gpx:time', ns)
            if lat is not None and lon is not None and time_element is not None:
                track_points.append({
                    'latitude': float(lat),
                    'longitude': float(lon),
                    'timestamp': pd.to_datetime(time_element.text)
                })

        if not track_points:
            print(f"Warning: No track points found in '{gpx_path}'")
            return pd.DataFrame()

        gpx_df = pd.DataFrame(track_points)
        gpx_df = gpx_df.sort_values(by='timestamp').reset_index(drop=True)
        gpx_df['time_s'] = (gpx_df['timestamp'] - gpx_df['timestamp'].iloc[0]).dt.total_seconds()
        
        print(f"Successfully parsed {len(gpx_df)} GPX track points from '{gpx_path}'")
        return gpx_df

    except (FileNotFoundError, ET.ParseError) as e:
        print(f"Error parsing GPX file '{gpx_path}': {e}")
        return pd.DataFrame()

def calculate_pps(df):
    """
    Calculates the number of correctly received packets per second over a 
    10-second rolling window.
    """
    if df.empty:
        return pd.DataFrame()
        
    correct_packets_df = df[(df['packetDataCorrupt'] == 0) & (df['packetWrongSize'] == 0)].copy()
    correct_packets_df['datetime'] = pd.to_datetime(correct_packets_df['timestamp_s'], unit='s')
    correct_packets_df = correct_packets_df.set_index('datetime')

    rolling_count = correct_packets_df['packetNumber'].rolling('10s').count()
    pps = rolling_count / 10.0
    
    pps_resampled = pps.resample('1s').last().fillna(0)    
    return pps_resampled.to_frame(name='pps')

def plot_data(df, pps_df, gpx_df, start_time=None, end_time=None):
    """
    Generates and displays five plots. Top 4 are full-width, 
    the GPS Map is centered, large, and square.
    """
    plot_df = df.copy()
    plot_pps_df = pps_df.copy()
    plot_gpx_df = gpx_df.copy()

    if start_time is not None and end_time is not None:
        plot_df = plot_df[(plot_df['timestamp_s'] >= start_time) & (plot_df['timestamp_s'] <= end_time)]
        plot_pps_df = plot_pps_df[(plot_pps_df.index >= pd.to_datetime(start_time, unit='s')) & 
                                  (plot_pps_df.index <= pd.to_datetime(end_time, unit='s'))]

    if plot_df.empty:
        print("No data to plot in the specified time range.")
        return

    # --- GRID RESTRUCTURE ---
    # Figsize increased slightly to handle the larger map.
    fig = plt.figure(figsize=(18, 22)) 
    
    # 5 rows, 5 columns. 
    # height_ratios: Total is 12. The last row (5) gets ~41% of the vertical space.
    gs = fig.add_gridspec(5, 5, height_ratios=[2, 1.5, 1.5, 2, 5])
    
    # Top 4 rows span ALL 5 columns (index 0 to end)
    ax1 = fig.add_subplot(gs[0, :])
    ax_pps = fig.add_subplot(gs[1, :], sharex=ax1)
    ax_proto = fig.add_subplot(gs[2, :], sharex=ax1)
    ax_num = fig.add_subplot(gs[3, :], sharex=ax1)
    
    # The Map plot spans columns 1, 2, and 3 (leaving col 0 and 4 empty as margins)
    # This gives it 60% of the horizontal space to balance its new vertical size.
    ax_gps = fig.add_subplot(gs[4, 1:4]) 

    fig.suptitle('LoRa & GPS Performance Analysis', fontsize=6)
    plt.subplots_adjust(bottom=0.1, top=.98, hspace=0.4)
    # --- Plot 1: Signal Quality ---
    ax1.set_title('Signal Quality')
    color = 'tab:red'
    ax1.set_ylabel('RSSI (dBm)', color=color)
    ax1.plot(plot_df['timestamp_s'], plot_df['RSSI'], color=color, alpha=0.7)
    ax1.grid(True, linestyle='--', alpha=0.5)

    ax1_snr = ax1.twinx()
    ax1_snr.set_ylabel('SNR (dB)', color='tab:blue')
    ax1_snr.plot(plot_df['timestamp_s'], plot_df['SNR'], color='tab:blue', alpha=0.7)

    # --- Plot 2: Throughput ---
    ax_pps.set_title('Throughput')
    ax_pps.plot(plot_pps_df.index.astype(np.int64) // 10**9, plot_pps_df['pps'], color='tab:green', drawstyle='steps-post')
    ax_pps.set_ylabel('Packets / Sec')
    ax_pps.grid(True, linestyle='--', alpha=0.5)

    # --- Plot 3: Burst & Errors ---
    ax_proto.set_title('Burst & Errors')
    ax_proto.plot(plot_df['timestamp_s'], plot_df['packetRecvPercent'], color='purple')
    
    errors = {
        'Data Corrupt': ('packetDataCorrupt', 'orange'),
        'Protocol Error': ('protocolError', 'magenta'),
        'Wrong Size': ('packetWrongSize', 'brown')
    }
    y_offset = 105 
    for label, (col, color) in errors.items():
        error_times = plot_df[plot_df[col] == 1]['timestamp_s']
        if not error_times.empty:
            ax_proto.plot(error_times, [y_offset] * len(error_times), 'x', label=label, color=color, markersize=8)
        y_offset += 7 

    ax_proto.set_ylim(0, y_offset)
    ax_proto.grid(True, linestyle='--', alpha=0.5)

    # --- Plot 4: Packet Number ---
    ax_num.set_title('Packet Sequence')
    ax_num.plot(plot_df['timestamp_s'], plot_df['packetNumber'], color='teal', marker='.', markersize=2)
    ax_num.set_xlabel('Relative Time (s)')
    ax_num.grid(True, linestyle='--', alpha=0.5)

    # --- Plot 5: GPS Map View ---
    ax_gps.set_title('GPS Track')
    ax_gps.set_aspect('equal', adjustable='box') 
    
    annotations = []
    if not plot_gpx_df.empty:
        ax_gps.plot(plot_gpx_df['longitude'], plot_gpx_df['latitude'], color='blue', linewidth=2, zorder=2)
        ax_gps.scatter(plot_gpx_df['longitude'], plot_gpx_df['latitude'], color='red', s=15, zorder=3)
        
        num_points = len(plot_gpx_df)
        step = max(1, num_points // 15)
        for idx in range(0, num_points, step):
            row = plot_gpx_df.iloc[idx]
            ann = ax_gps.annotate("", (row['longitude'], row['latitude']), 
                                  xytext=(5, 5), textcoords="offset points",
                                  bbox=dict(boxstyle="round", fc="white", alpha=0.8), fontsize=8, zorder=4)
            annotations.append((ann, idx))

        try:
            # Add contextily basemap
            cx.add_basemap(ax_gps, crs="EPSG:4326", source=cx.providers.CartoDB.Positron, zoom='auto')
        except Exception as e:
            print(f"Map load failed: {e}")

    # --- Slider for Time Offset ---
    # Shifted slightly left to remain centered relative to the whole figure
    ax_slider = fig.add_axes([0.25, 0.03, 0.5, 0.02])
    time_offset_slider = Slider(ax_slider, 'Offset (s)', -600, 600, valinit=0)

    def format_time_tick(x, pos):
        return f"{x:.0f}s"

    ax_num.xaxis.set_major_formatter(plt.FuncFormatter(format_time_tick))
    
    def update(val):
        offset = time_offset_slider.val
        if not plot_gpx_df.empty:
            for ann, idx in annotations:
                adjusted_time = plot_gpx_df['time_s'].iloc[idx] + offset
                ann.set_text(f"{adjusted_time:.0f}s")
        fig.canvas.draw_idle()

    time_offset_slider.on_changed(update)
    fig.time_offset_slider = time_offset_slider 
    
    update(0)
    plt.show()

def main():
    """Main function to run the log analysis."""
    input_file = 'raw_data.txt'
    output_csv = 'parsed_log_data.csv'
    gpx_file = 'test.gpx'

    print(f"Parsing log data from '{input_file}'...")
    main_df = parse_log_file(input_file, output_csv)
    
    print(f"Parsing GPX data from '{gpx_file}'...")
    gpx_df = parse_gpx_file(gpx_file)
    
    if main_df.empty:
        print("No 'csv entry' lines found in the log file. Exiting.")
        return

    if not main_df['packetNumber'].empty:
        received_packets = sorted(main_df['packetNumber'].unique())
        first_packet = received_packets[0]
        last_packet = received_packets[-1]
        
        print("\n--- Overall Packet Reception Statistics ---")
        print(f"First packet number recorded: {first_packet}")
        print(f"Last packet number recorded:  {last_packet}")
        
        full_set = set(range(int(first_packet), int(last_packet) + 1))
        received_set = set(received_packets)
        missing_packets = sorted(list(full_set - received_set))
        
        if missing_packets:
            print(f"Missing packet numbers: {missing_packets}")
        else:
            print("No missing packets in the sequence.")
            
        total_expected = len(full_set)
        total_received = len(received_set)
        
        if total_expected > 0:
            reception_percent = (total_received / total_expected) * 100
            print(f"Reception Rate: {reception_percent:.2f}% ({total_received} of {total_expected} packets received)")
        print("-----------------------------------------\n")

    print("Calculating packets per second...")
    pps_df = calculate_pps(main_df)

    print("\n--- Initial Plot ---")
    print("Displaying plots for the full time range. Close the plot window to continue.")
    plot_data(main_df, pps_df, gpx_df)

    while True:
        print("\n--------------------")
        print("Enter a time range to zoom into the plots.")
        print("Press 'q' to quit.")
        
        start_str = input("Start time in seconds (e.g., 30): ")
        if start_str.lower() == 'q':
            break
        
        end_str = input("End time in seconds (e.g., 90): ")
        if end_str.lower() == 'q':
            break

        try:
            start_time = float(start_str)
            end_time = float(end_str)
            if start_time >= end_time:
                print("Error: Start time must be less than end time.")
                continue
            
            print(f"Generating plot for time range: {start_time}s - {end_time}s")
            plot_data(main_df, pps_df, gpx_df, start_time, end_time)
        except (ValueError, TypeError):
            print("Invalid input. Please enter valid numbers for the time range.")

if __name__ == "__main__":
    main()