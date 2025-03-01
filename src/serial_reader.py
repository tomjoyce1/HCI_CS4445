import serial
import time
import rtmidi
import re
import numpy as np
import json
import os
import argparse
from pathlib import Path

class MIDIController:
    def __init__(self, force_select=False):
        self.midi_out = rtmidi.MidiOut()
        self.config_file = Path.home() / '.6dof_config.json'
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
        
        # Show selection menu
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

def smooth_value(values, new_value, window_size=5):
    values.append(new_value)
    if len(values) > window_size:
        values.pop(0)
    return np.mean(values)

def read_serial_data(force_select_midi=False):
    # Configure the serial port
    ser = serial.Serial(
        port='com7',
        baudrate=115200,
        timeout=1
    )
    
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
    args = parser.parse_args()
    
    read_serial_data(force_select_midi=args.select_midi)

if __name__ == "__main__":
    print("Starting serial reader with MIDI output... Press Ctrl+C to stop.")
    main()
