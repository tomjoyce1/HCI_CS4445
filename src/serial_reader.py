import serial
import serial.tools.list_ports
import time
import rtmidi
import pygame
import re
import numpy as np
import json
import os
import argparse
import threading
import tkinter as tk
from tkinter import ttk, messagebox
import math
from pathlib import Path
import sys

# Define color schemes for light and dark modes
class ColorScheme:
    def __init__(self):
        # Light mode (default)
        self.light = {
            "bg": "#f0f0f0",
            "fg": "#000000",
            "canvas_bg": "#ffffff",
            "frame_bg": "#f5f5f5",
            "button_bg": "#e0e0e0",
            "highlight_bg": "#d0d0d0",
            "text_color": "#000000",
            "axis_x": "#ff0000",
            "axis_y": "#00a000",
            "axis_z": "#0000ff",
            "status_ok": "#00a000",
            "status_error": "#ff0000",
            "cube_lines": "#000000"
        }
        
        # Dark mode
        self.dark = {
            "bg": "#2d2d2d",
            "fg": "#ffffff",
            "canvas_bg": "#1a1a1a",
            "frame_bg": "#333333",
            "button_bg": "#444444",
            "highlight_bg": "#555555",
            "text_color": "#ffffff",
            "axis_x": "#ff5050",
            "axis_y": "#50ff50",
            "axis_z": "#5050ff",
            "status_ok": "#50ff50",
            "status_error": "#ff5050",
            "cube_lines": "#ffffff"
        }
        
        # Current scheme (start with light)
        self.current = self.light

class MIDIController:
    def __init__(self, force_select=False, gui_mode=False, parent=None):
        self.midi_out = rtmidi.MidiOut()
        self.config_file = Path.home() / '.6dof_config2.json'
        self.gui_mode = gui_mode
        self.parent = parent
        port_index = self._get_midi_port(force_select)
        
        if port_index is None:
            print("No MIDI ports available. Creating virtual port...")
            self.midi_out.open_virtual_port("6DOF Controller")
        else:
            self.midi_out.open_port(port_index)
        
        # MIDI CC numbers for pitch, roll, and yaw
        self.PITCH_CC = 16
        self.ROLL_CC = 17
        self.YAW_CC = 18
        
        # Mapping configuration
        self.MID_RANGE = 30  # degrees - how far from center before entering outer range
        self.MID_RANGE_PROPORTION = 0.8  # proportion of MIDI range allocated to middle section
    
    def _get_midi_port(self, force_select):
        available_ports = self.midi_out.get_ports()
        
        if not available_ports:
            return None
            
        # Load saved configuration
        saved_port = None
        if not force_select and self.config_file.exists():
            try:
                with open(self.config_file, 'r') as f:
                    config = json.load(f)
                    saved_port = config.get('midi_port')
                    if saved_port in available_ports:
                        print(f"Using saved MIDI port: {saved_port}")
                        return available_ports.index(saved_port)
            except (json.JSONDecodeError, IOError):
                pass
        
        # GUI Mode - Show dialog instead of console input
        if self.gui_mode:
            return self._gui_select_midi_port(available_ports)
        
        # Console Mode - Show selection menu
        print("\nAvailable MIDI ports:")
        for i, port in enumerate(available_ports):
            print(f"{i}: {port}")
        
        while True:
            try:
                choice = int(input("\nSelect MIDI port number: "))
                if 0 <= choice < len(available_ports):
                    # Save the selection
                    try:
                        with open(self.config_file, 'w') as f:
                            json.dump({'midi_port': available_ports[choice]}, f)
                        return choice
                    except IOError as e:
                        print(f"Warning: Could not save MIDI port selection: {e}")
                        return choice
                else:
                    print("Invalid selection. Please try again.")
            except ValueError:
                print("Please enter a number.")
    
    def _gui_select_midi_port(self, available_ports):
        """Display a GUI dialog to select MIDI port"""
        if not available_ports:
            return None
            
        if self.parent:
            # Create a popup dialog window
            dialog = tk.Toplevel(self.parent)
            dialog.title("Select MIDI Port")
            dialog.geometry("400x300")
            dialog.transient(self.parent)  # Make dialog modal
            dialog.grab_set()
            
            # Center the dialog
            dialog.update_idletasks()
            width = dialog.winfo_width()
            height = dialog.winfo_height()
            x = (dialog.winfo_screenwidth() // 2) - (width // 2)
            y = (dialog.winfo_screenheight() // 2) - (height // 2)
            dialog.geometry(f'+{x}+{y}')
            
            # Create a label
            ttk.Label(dialog, text="Select a MIDI port:", padding=10).pack()
            
            # Create a listbox to display the ports
            port_listbox = tk.Listbox(dialog, width=50, height=10)
            port_listbox.pack(padx=10, pady=10, fill=tk.BOTH, expand=True)
            
            # Add ports to the listbox
            for port in available_ports:
                port_listbox.insert(tk.END, port)
            
            # Select the first port by default
            port_listbox.selection_set(0)
            
            # Result variable
            result = [None]  # Use list to allow modification from nested function
            
            # Function to handle selection
            def on_select():
                selection = port_listbox.curselection()
                if selection:
                    index = selection[0]
                    # Save selection to config
                    try:
                        with open(self.config_file, 'w') as f:
                            json.dump({'midi_port': available_ports[index]}, f)
                    except IOError:
                        pass
                    result[0] = index
                    dialog.destroy()
            
            # Add a select button
            ttk.Button(dialog, text="Select", command=on_select).pack(pady=10)
            
            # Wait until the dialog is closed
            self.parent.wait_window(dialog)
            
            return result[0]
        else:
            # If no parent window is provided, default to first port
            return 0 if available_ports else None
    
    def send_controller_change(self, cc_number, value):
        # Piecewise linear mapping
        if abs(value) <= self.MID_RANGE:
            # Middle range: Map ±MID_RANGE° to ±MID_RANGE_PROPORTION of the MIDI range
            mapped = (value / self.MID_RANGE) * self.MID_RANGE_PROPORTION
        else:
            # Outer range: Map remaining angles to the outer (1-MID_RANGE_PROPORTION) of the MIDI range
            sign = 1 if value > 0 else -1
            remaining_angle = abs(value) - self.MID_RANGE
            mapped = sign * (self.MID_RANGE_PROPORTION + (remaining_angle / (180 - self.MID_RANGE)) * (1 - self.MID_RANGE_PROPORTION))
            
        # Scale to MIDI range (0-127) and ensure the midpoint (0 degrees) maps to 63/64
        midi_value = min(127, max(0, int(63.5 + (mapped * 63.5))))
        
        # Send MIDI CC message on channel 1
        self.midi_out.send_message([0xB0, cc_number, midi_value])
        
    def close(self):
        self.midi_out.close_port()

def smooth_value(values, new_value, window_size=3):
    values.append(new_value)
    if len(values) > window_size:
        values.pop(0)
    return np.mean(values)

class SensorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("6DOF MIDI Controller")
        self.root.geometry("800x600")
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        
        # Initialize color scheme
        self.colors = ColorScheme()
        self.is_dark_mode = False
        
        # Load theme preference from config
        self.config_file = Path.home() / '.6dof_config2.json'
        self._load_theme_preference()
        
        # Configure initial theme
        self._configure_theme()
        
        self.midi_controller = None
        self.serial_conn = None
        self.running = False
        self.data_thread = None
        
        # Data buffers
        self.roll_buffer = []
        self.yaw_buffer = []
        
        # Current values
        self.pitch = 0
        self.roll = 0
        self.yaw = 0
                
        self.playing = False
        self.playback_index = 0

        self._create_widgets()
        self._list_ports()
    
    def _load_theme_preference(self):
        """Load dark mode preference from config file"""
        if self.config_file.exists():
            try:
                with open(self.config_file, 'r') as f:
                    config = json.load(f)
                    self.is_dark_mode = config.get('dark_mode', False)
                    if self.is_dark_mode:
                        self.colors.current = self.colors.dark
                    else:
                        self.colors.current = self.colors.light
            except (json.JSONDecodeError, IOError):
                pass
    
    def _save_theme_preference(self):
        """Save dark mode preference to config file"""
        config = {}
        if self.config_file.exists():
            try:
                with open(self.config_file, 'r') as f:
                    config = json.load(f)
            except (json.JSONDecodeError, IOError):
                pass
                
        config['dark_mode'] = self.is_dark_mode
        
        try:
            with open(self.config_file, 'w') as f:
                json.dump(config, f)
        except IOError as e:
            print(f"Warning: Could not save theme preference: {e}")
    
    def _configure_theme(self):
        """Apply the current theme to the root window"""
        style = ttk.Style()
        
        if self.is_dark_mode:
            # Configure ttk styles for dark mode
            self.root.configure(bg=self.colors.current["bg"])
            style.configure('TFrame', background=self.colors.current["bg"])
            style.configure('TLabel', background=self.colors.current["bg"], foreground=self.colors.current["fg"])
            style.configure('TButton', background=self.colors.current["button_bg"])
            style.configure('TCheckbutton', background=self.colors.current["bg"], foreground=self.colors.current["fg"])
            style.configure('TLabelframe', background=self.colors.current["bg"], foreground=self.colors.current["fg"])
            style.configure('TLabelframe.Label', background=self.colors.current["bg"], foreground=self.colors.current["fg"])
            style.map('TButton', background=[('active', self.colors.current["highlight_bg"])])
        else:
            # Reset to default light theme
            self.root.configure(bg=self.colors.current["bg"])
            style.configure('TFrame', background=self.colors.current["bg"])
            style.configure('TLabel', background=self.colors.current["bg"], foreground=self.colors.current["fg"])
            style.configure('TButton', background=self.colors.current["button_bg"])
            style.configure('TCheckbutton', background=self.colors.current["bg"], foreground=self.colors.current["fg"])
            style.configure('TLabelframe', background=self.colors.current["bg"], foreground=self.colors.current["fg"])
            style.configure('TLabelframe.Label', background=self.colors.current["bg"], foreground=self.colors.current["fg"])
            style.map('TButton', background=[('active', self.colors.current["highlight_bg"])])
    
    def toggle_dark_mode(self):
        """Toggle between light and dark modes"""
        self.is_dark_mode = not self.is_dark_mode
        self.colors.current = self.colors.dark if self.is_dark_mode else self.colors.light
        
        # Update dark mode button text
        self.dark_mode_button.config(text="Light Mode" if self.is_dark_mode else "Dark Mode")
        
        # Apply theme
        self._configure_theme()
        
        # Update canvas background
        self.canvas.config(bg=self.colors.current["canvas_bg"])
        
        # If we have orientation data, redraw the visualization with the new colors
        if hasattr(self, 'pitch') and hasattr(self, 'roll') and hasattr(self, 'yaw'):
            self.draw_orientation(self.pitch, self.roll, self.yaw)
        
        # Save preference
        self._save_theme_preference()

    def toggle_playback(self):
        if not pygame.mixer.get_init():
            pygame.mixer.init()
        file_path = os.path.join(os.getcwd(), "recording.mp3")  

        if pygame.mixer.music.get_busy():
            pygame.mixer.music.stop()
        
        else:
            try:
                pygame.mixer.music.load(file_path)
                pygame.mixer.music.play()
            except pygame.error as e:
                print('Error loading noooo')


    def _create_widgets(self):
        # Create main frames
        control_frame = ttk.Frame(self.root, padding="10")
        control_frame.pack(fill=tk.X)
        
        top_buttons_frame = ttk.Frame(control_frame)
        top_buttons_frame.grid(row=0, column=0, columnspan=5, sticky=tk.W+tk.E)
        
        # Dark mode toggle button
        self.dark_mode_button = ttk.Button(
            top_buttons_frame, 
            text="Dark Mode" if not self.is_dark_mode else "Light Mode", 
            command=self.toggle_dark_mode
        )
        self.dark_mode_button.pack(side=tk.RIGHT, padx=5, pady=5)
        
        # Serial port selection
        ttk.Label(control_frame, text="Serial Port:").grid(row=1, column=0, padx=5, pady=5, sticky=tk.W)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(control_frame, textvariable=self.port_var, width=30)
        self.port_combo.grid(row=1, column=1, padx=5, pady=5, sticky=tk.W)
        
        ttk.Button(control_frame, text="Refresh Ports", command=self._list_ports).grid(
            row=1, column=2, padx=5, pady=5)
        
        # Connect/disconnect button
        self.connect_button = ttk.Button(control_frame, text="Connect", command=self.toggle_connection)
        self.connect_button.grid(row=1, column=3, padx=5, pady=5)
        
        # MIDI selection button
        ttk.Button(control_frame, text="Select MIDI Port", command=self.select_midi_port).grid(
            row=1, column=4, padx=5, pady=5)
        
        

        # Playback button        
        self.play_button = ttk.Button(control_frame, text="▶️ User Instructions", command=self.toggle_playback)
        self.play_button.grid(row=1, column=6, padx=5, pady=5)

        # Status indicators
        status_frame = ttk.LabelFrame(control_frame, text="Status", padding="10")
        status_frame.grid(row=2, column=0, columnspan=5, padx=5, pady=5, sticky=tk.W+tk.E)
        
        ttk.Label(status_frame, text="Serial:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.serial_status = ttk.Label(status_frame, text="Disconnected", foreground=self.colors.current["status_error"])
        self.serial_status.grid(row=0, column=1, padx=5, pady=5, sticky=tk.W)
        
        ttk.Label(status_frame, text="MIDI:").grid(row=0, column=2, padx=5, pady=5, sticky=tk.W)
        self.midi_status = ttk.Label(status_frame, text="Disconnected", foreground=self.colors.current["status_error"])
        self.midi_status.grid(row=0, column=3, padx=5, pady=5, sticky=tk.W)
        
        visual_frame = ttk.Frame(self.root, padding="10")
        visual_frame.pack(fill=tk.BOTH, expand=True)
        
        # Data display
        data_frame = ttk.LabelFrame(visual_frame, text="Sensor Data", padding="10")
        data_frame.pack(fill=tk.BOTH, expand=True)
        
        # Create canvas for 3D visualization with the current theme background
        self.canvas = tk.Canvas(data_frame, bg=self.colors.current["canvas_bg"], width=400, height=300)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Angle displays
        values_frame = ttk.Frame(data_frame)
        values_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=10, pady=10)
        
        ttk.Label(values_frame, text="Pitch:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.pitch_value = ttk.Label(values_frame, text="0.00°")
        self.pitch_value.grid(row=0, column=1, padx=5, pady=5, sticky=tk.W)
        
        ttk.Label(values_frame, text="Roll:").grid(row=1, column=0, padx=5, pady=5, sticky=tk.W)
        self.roll_value = ttk.Label(values_frame, text="0.00°")
        self.roll_value.grid(row=1, column=1, padx=5, pady=5, sticky=tk.W)
        
        ttk.Label(values_frame, text="Yaw:").grid(row=2, column=0, padx=5, pady=5, sticky=tk.W)
        self.yaw_value = ttk.Label(values_frame, text="0.00°")
        self.yaw_value.grid(row=2, column=1, padx=5, pady=5, sticky=tk.W)
        
        # MIDI CC value display
        ttk.Label(values_frame, text="MIDI CC Values:").grid(row=3, column=0, columnspan=2, padx=5, pady=(15,5), sticky=tk.W)
        
        ttk.Label(values_frame, text="Pitch CC:").grid(row=4, column=0, padx=5, pady=5, sticky=tk.W)
        self.pitch_cc = ttk.Label(values_frame, text="64")
        self.pitch_cc.grid(row=4, column=1, padx=5, pady=5, sticky=tk.W)
        
        ttk.Label(values_frame, text="Roll CC:").grid(row=5, column=0, padx=5, pady=5, sticky=tk.W)
        self.roll_cc = ttk.Label(values_frame, text="64")
        self.roll_cc.grid(row=5, column=1, padx=5, pady=5, sticky=tk.W)
        
        ttk.Label(values_frame, text="Yaw CC:").grid(row=6, column=0, padx=5, pady=5, sticky=tk.W)
        self.yaw_cc = ttk.Label(values_frame, text="64")
        self.yaw_cc.grid(row=6, column=1, padx=5, pady=5, sticky=tk.W)
    
    def _list_ports(self):
        """Update the list of available serial ports"""
        ports = []
        try:
            ports = [p.device for p in serial.tools.list_ports.comports()]
        except:
            messagebox.showerror("Error", "Failed to get available serial ports")
            
        self.port_combo['values'] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])
    
    def toggle_connection(self):
        """Connect to or disconnect from the serial port"""
        if self.running:
            self.stop_serial()
        else:
            self.start_serial()
    
    def start_serial(self):
        """Connect to serial port and start data acquisition"""
        port = self.port_var.get()
        if not port:
            messagebox.showerror("Error", "Please select a serial port")
            return
            
        try:
            self.serial_conn = serial.Serial(
                port=port,
                baudrate=115200,
                timeout=1
            )
            
            # Initialize MIDI controller if not already done
            if not self.midi_controller:
                self.midi_controller = MIDIController(gui_mode=True, parent=self.root)
                self.midi_status.config(text=f"Connected", foreground=self.colors.current["status_ok"])
            
            self.running = True
            self.connect_button.config(text="Disconnect")
            self.serial_status.config(text=f"Connected to {port}", foreground=self.colors.current["status_ok"])
            
            # Reset input buffer
            self.serial_conn.reset_input_buffer()
            
            # Start data acquisition thread
            self.data_thread = threading.Thread(target=self.read_data_loop)
            self.data_thread.daemon = True
            self.data_thread.start()
            
        except serial.SerialException as e:
            messagebox.showerror("Connection Error", f"Failed to connect to {port}: {str(e)}")
    
    def stop_serial(self):
        """Stop data acquisition and disconnect"""
        self.running = False
        if self.data_thread:
            self.data_thread.join(timeout=1.0)
            self.data_thread = None
            
        if self.serial_conn:
            self.serial_conn.close()
            self.serial_conn = None
            
        self.connect_button.config(text="Connect")
        self.serial_status.config(text="Disconnected", foreground=self.colors.current["status_error"])
    
    def select_midi_port(self):
        """Force reselection of MIDI port"""
        if self.midi_controller:
            self.midi_controller.close()
        
        self.midi_controller = MIDIController(force_select=True, gui_mode=True, parent=self.root)
        self.midi_status.config(text=f"Connected", foreground=self.colors.current["status_ok"])
    
    def read_data_loop(self):
        """Background thread to read data from serial port"""
        # Clear the buffers at start
        self.roll_buffer = []
        self.yaw_buffer = []
        
        # Track timing to maintain consistent rate
        last_process_time = time.time()
        target_interval = 0.02  # 50Hz processing rate
        
        while self.running and self.serial_conn:
            try:
                if self.serial_conn.in_waiting > 0:
                    line = self.serial_conn.readline().decode('utf-8').strip()
                    
                    # Only process at the target rate
                    current_time = time.time()
                    if current_time - last_process_time < target_interval:
                        continue
                    
                    last_process_time = current_time
                    
                    try:
                        # Parse the comma-separated angle values
                        yaw, pitch, roll = map(float, line.split(','))
                        
                        # Apply smoothing to roll and yaw
                        smoothed_roll = smooth_value(self.roll_buffer, roll)
                        smoothed_yaw = smooth_value(self.yaw_buffer, yaw)
                        
                        # Update current values
                        self.pitch = pitch
                        self.roll = smoothed_roll
                        self.yaw = smoothed_yaw
                        
                        # Send MIDI CC messages
                        midi_cc_pitch = self.calculate_midi_cc(self.midi_controller.PITCH_CC, pitch)
                        midi_cc_roll = self.calculate_midi_cc(self.midi_controller.ROLL_CC, smoothed_roll)
                        midi_cc_yaw = self.calculate_midi_cc(self.midi_controller.YAW_CC, smoothed_yaw)
                        
                        # Update the GUI (thread-safe)
                        self.root.after(0, self.update_display, pitch, smoothed_roll, smoothed_yaw, 
                                        midi_cc_pitch, midi_cc_roll, midi_cc_yaw)
                        
                    except ValueError:
                        pass  # Ignore parsing errors
                        
                # Reduced sleep time for more responsive reads
                time.sleep(0.001)  # Just enough to prevent CPU hogging
                
            except Exception as e:
                print(f"Error in read loop: {e}")
                self.root.after(0, self.handle_error, str(e))
                break
    
    def calculate_midi_cc(self, cc_number, value):
        """Calculate MIDI CC value from angle"""
        # Replicate the MIDI mapping logic from MIDIController class
        if abs(value) <= self.midi_controller.MID_RANGE:
            mapped = (value / self.midi_controller.MID_RANGE) * self.midi_controller.MID_RANGE_PROPORTION
        else:
            sign = 1 if value > 0 else -1
            remaining_angle = abs(value) - self.midi_controller.MID_RANGE
            mapped = sign * (self.midi_controller.MID_RANGE_PROPORTION + 
                            (remaining_angle / (180 - self.midi_controller.MID_RANGE)) * 
                            (1 - self.midi_controller.MID_RANGE_PROPORTION))
            
        midi_value = min(127, max(0, int(63.5 + (mapped * 63.5))))
        
        # Send MIDI CC message
        self.midi_controller.send_controller_change(cc_number, value)
        
        return midi_value
    
    def update_display(self, pitch, roll, yaw, midi_pitch, midi_roll, midi_yaw):
        """Update the GUI with new sensor values"""
        # Update text displays
        self.pitch_value.config(text=f"{pitch:.2f}°")
        self.roll_value.config(text=f"{roll:.2f}°")
        self.yaw_value.config(text=f"{yaw:.2f}°")
        
        # Update MIDI CC displays
        self.pitch_cc.config(text=f"{midi_pitch}")
        self.roll_cc.config(text=f"{midi_roll}")
        self.yaw_cc.config(text=f"{midi_yaw}")
        
        # Store current values for potential redraw on theme change
        self.pitch = pitch
        self.roll = roll
        self.yaw = yaw
        
        # Update the 3D visualization
        self.draw_orientation(pitch, roll, yaw)
    
    def draw_orientation(self, pitch, roll, yaw):
        """Draw a simple 3D representation of the sensor orientation"""
        self.canvas.delete("all")
        
        # Canvas dimensions
        width = self.canvas.winfo_width()
        height = self.canvas.winfo_height()
        center_x = width / 2
        center_y = height / 2
        
        # Size of the box
        size = min(width, height) * 0.4
        
        # Convert angles to radians
        pitch_rad = math.radians(pitch)
        roll_rad = math.radians(roll)
        yaw_rad = math.radians(yaw)
        
        # Draw a simple cube representation
        self._draw_cube(center_x, center_y, size, pitch_rad, roll_rad, yaw_rad)
        
        # Draw axes labels with the current theme colors
        self.canvas.create_text(width - 50, center_y, text="Roll", fill=self.colors.current["axis_x"])
        self.canvas.create_text(center_x, height - 20, text="Pitch", fill=self.colors.current["axis_y"])
        self.canvas.create_text(20, center_y, text="Yaw", fill=self.colors.current["axis_z"])
    
    def _draw_cube(self, cx, cy, size, pitch, roll, yaw):
        """Draw a cube rotated according to the given angles"""
        # Define the cube vertices in 3D space
        vertices = [
            [-1, -1, -1], [1, -1, -1], [1, 1, -1], [-1, 1, -1],
            [-1, -1, 1], [1, -1, 1], [1, 1, 1], [-1, 1, 1]
        ]
        
        # Apply rotations
        rotated = []
        for v in vertices:
            # Apply yaw, pitch, roll rotations (in that order)
            x, y, z = self._rotate_point(v[0], v[1], v[2], pitch, roll, yaw)
            
            # Project to 2D and scale
            screen_x = cx + x * size
            screen_y = cy - z * size  # Invert y for screen coordinates
            
            rotated.append((screen_x, screen_y))
        
        # Draw the cube edges
        edges = [
            (0, 1), (1, 2), (2, 3), (3, 0),  # Back face
            (4, 5), (5, 6), (6, 7), (7, 4),  # Front face
            (0, 4), (1, 5), (2, 6), (3, 7)   # Connecting edges
        ]
        
        # Draw each edge
        for edge in edges:
            self.canvas.create_line(
                rotated[edge[0]][0], rotated[edge[0]][1],
                rotated[edge[1]][0], rotated[edge[1]][1],
                width=2,
                fill=self.colors.current["cube_lines"]
            )
        
        # Draw colored axes
        origin = self._rotate_point(0, 0, 0, pitch, roll, yaw)
        origin_x = cx + origin[0] * size
        origin_y = cy - origin[2] * size
        
        # X axis (red)
        x_end = self._rotate_point(1.5, 0, 0, pitch, roll, yaw)
        self.canvas.create_line(
            origin_x, origin_y,
            cx + x_end[0] * size, cy - x_end[2] * size,
            width=3, fill=self.colors.current["axis_x"], arrow=tk.LAST
        )
        
        # Y axis (green)
        y_end = self._rotate_point(0, 1.5, 0, pitch, roll, yaw)
        self.canvas.create_line(
            origin_x, origin_y,
            cx + y_end[0] * size, cy - y_end[2] * size,
            width=3, fill=self.colors.current["axis_y"], arrow=tk.LAST
        )
        
        # Z axis (blue)
        z_end = self._rotate_point(0, 0, 1.5, pitch, roll, yaw)
        self.canvas.create_line(
            origin_x, origin_y,
            cx + z_end[0] * size, cy - z_end[2] * size,
            width=3, fill=self.colors.current["axis_z"], arrow=tk.LAST
        )
    
    def _rotate_point(self, x, y, z, pitch, roll, yaw):
        """Apply 3D rotation to a point"""
        # Yaw rotation (around Z axis)
        x_yaw = x * math.cos(yaw) - y * math.sin(yaw)
        y_yaw = x * math.sin(yaw) + y * math.cos(yaw)
        
        # Pitch rotation (around Y axis)
        x_pitch = x_yaw * math.cos(pitch) + z * math.sin(pitch)
        z_pitch = -x_yaw * math.sin(pitch) + z * math.cos(pitch)
        
        # Roll rotation (around X axis)
        y_roll = y_yaw * math.cos(roll) - z_pitch * math.sin(roll)
        z_roll = y_yaw * math.sin(roll) + z_pitch * math.cos(roll)
        
        return x_pitch, y_roll, z_roll
    
    def handle_error(self, error_msg):
        """Handle errors from the data thread"""
        self.stop_serial()
        messagebox.showerror("Error", f"Serial communication error: {error_msg}")
    
    def on_closing(self):
        """Clean up before closing the application"""
        self.running = False
        if self.data_thread:
            self.data_thread.join(timeout=1.0)
            
        if self.serial_conn:
            self.serial_conn.close()
            
        if self.midi_controller:
            self.midi_controller.close()
            
        self.root.destroy()

def read_serial_data(force_select_midi=False):
    # Configure the serial port
    while True:
        try:
            ser = serial.Serial(
                port='COM6',
                baudrate=115200,
                timeout=1
            )
            break
        except serial.SerialException as e:
            print(f"Failed to open port com7: {e}")
            print("\nAvailable ports:")
            try:
                ports = list(serial.tools.list_ports.comports())
                for i, port in enumerate(ports):
                    print(f"{i}: {port.device} - {port.description}")
                
                while True:
                    try:
                        choice = int(input("\nSelect port number (or -1 to exit): "))
                        if choice == -1:
                            return
                        if 0 <= choice < len(ports):
                            port = ports[choice].device
                            try:
                                ser = serial.Serial(
                                    port=port,
                                    baudrate=115200,
                                    timeout=1
                                )
                                print(f"Successfully opened port {port}")
                                break
                            except serial.SerialException as e:
                                print(f"Failed to open port {port}: {e}")
                        else:
                            print("Invalid selection. Please try again.")
                    except ValueError:
                        print("Please enter a number.")
            except ImportError:
                print("Could not list available ports. Please check your connection and try again.")
                return
    
    # Initialize MIDI controller
    midi_controller = MIDIController(force_select=force_select_midi)
    
    # Buffers for smoothing
    roll_buffer = []
    yaw_buffer = []
    
    try:
        ser.reset_input_buffer()
        
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8').strip()
                try:
                    # Parse the comma-separated angle values
                    yaw, pitch, roll = map(float, line.split(','))
                    
                    # Apply smoothing to roll and yaw
                    smoothed_roll = smooth_value(roll_buffer, roll)
                    smoothed_yaw = smooth_value(yaw_buffer, yaw)
                    
                    print(f"Pitch: {pitch:.2f}°, Roll: {smoothed_roll:.2f}°, Yaw: {smoothed_yaw:.2f}°")
                    
                    # Send MIDI CC messages
                    midi_controller.send_controller_change(midi_controller.PITCH_CC, pitch)
                    midi_controller.send_controller_change(midi_controller.ROLL_CC, smoothed_roll)
                    midi_controller.send_controller_change(midi_controller.YAW_CC, smoothed_yaw)
                    
                except ValueError as e:
                    print(f"Error parsing data: {line}")

    except KeyboardInterrupt:
        print("\nStopping serial reader...")
    finally:
        midi_controller.close()
        ser.close()

def main():
    parser = argparse.ArgumentParser(description='6DOF MIDI Controller')
    parser.add_argument('--select-midi', action='store_true', 
                      help='Force MIDI port selection menu')
    parser.add_argument('--no-gui', action='store_true',
                      help='Run in console mode without GUI')
    args = parser.parse_args()
    
    if args.no_gui:
        # Run in console mode
        read_serial_data(force_select_midi=args.select_midi)
    else:
        # Run GUI mode
        root = tk.Tk()
        app = SensorGUI(root)
        if args.select_midi:
            app.select_midi_port()
        root.mainloop()

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--no-gui":
        print("Starting serial reader with MIDI output... Press Ctrl+C to stop.")
    main()
