#!/bin/bash

# ESP32-C3 SNTP HTTP Project Build and Flash Script
# Make sure ESP-IDF is properly installed and sourced

set -e  # Exit on any error

echo "ESP32-C3 SNTP HTTP Project Build Script"
echo "========================================"

# Check if ESP-IDF is sourced
if [ -z "$IDF_PATH" ]; then
    echo "Error: ESP-IDF environment not found!"
    echo "Please run: . \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

echo "ESP-IDF Path: $IDF_PATH"

# Set target to ESP32-C3
echo "Setting target to ESP32-C3..."
idf.py set-target esp32c3

# Clean previous build (optional)
if [ "$1" == "clean" ]; then
    echo "Cleaning previous build..."
    idf.py clean
fi

# Build the project
echo "Building project..."
idf.py build

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build successful!"
    
    # Ask user if they want to flash
    read -p "Do you want to flash the device? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        # Try to detect port automatically
        PORT=""
        if [ -e "/dev/ttyUSB0" ]; then
            PORT="/dev/ttyUSB0"
        elif [ -e "/dev/ttyACM0" ]; then
            PORT="/dev/ttyACM0"
        elif [ -e "/dev/cu.usbserial-*" ]; then
            PORT=$(ls /dev/cu.usbserial-* | head -n1)
        fi
        
        if [ -n "$PORT" ]; then
            echo "Detected port: $PORT"
            echo "Flashing and monitoring..."
            idf.py -p $PORT flash monitor
        else
            echo "Could not detect port automatically."
            echo "Available ports:"
            ls /dev/tty* | grep -E "(USB|ACM|usbserial)" || echo "No USB/serial ports found"
            read -p "Enter port (e.g., /dev/ttyUSB0): " PORT
            if [ -n "$PORT" ]; then
                idf.py -p $PORT flash monitor
            else
                echo "No port specified. Skipping flash."
            fi
        fi
    else
        echo "Skipping flash. To flash manually, run:"
        echo "idf.py -p /dev/ttyUSB0 flash monitor"
    fi
else
    echo "Build failed!"
    exit 1
fi

echo "Done!"