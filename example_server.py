#!/usr/bin/env python3
"""
Example HTTP server to receive data from ESP32-C3 SNTP HTTP IoT device.
This server demonstrates how to handle the JSON data sent by the device.

Usage:
    python3 example_server.py

The server will listen on http://localhost:8080/api/data
"""

from http.server import HTTPServer, BaseHTTPRequestHandler
import json
import time
from datetime import datetime
import binascii

class DataHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != '/api/data':
            self.send_error(404, "Not Found")
            return
        
        # Get content length
        content_length = int(self.headers.get('Content-Length', 0))
        if content_length == 0:
            self.send_error(400, "No data received")
            return
        
        # Read POST data
        post_data = self.rfile.read(content_length)
        
        try:
            # Parse JSON data
            data = json.loads(post_data.decode('utf-8'))
            
            print(f"\n{'='*60}")
            print(f"Received data at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
            print(f"{'='*60}")
            
            # Print metadata
            packet_count = data.get('packet_count', 0)
            timestamp = data.get('timestamp', 0)
            print(f"Packet count: {packet_count}")
            print(f"Transmission timestamp: {timestamp} ({datetime.fromtimestamp(timestamp)})")
            
            # Process each packet
            packets = data.get('packets', [])
            for i, packet in enumerate(packets):
                print(f"\nPacket {i+1}:")
                print(f"  Epoch time: {packet['epoch_time']} ({datetime.fromtimestamp(packet['epoch_time'])})")
                print(f"  ESP timer (μs): {packet['esp_timer_us']}")
                print(f"  Has epoch: {packet['has_epoch']}")
                print(f"  Data length: {packet['data_len']} bytes")
                
                # Decode hex data to ASCII if possible
                try:
                    hex_data = packet['data']
                    binary_data = binascii.unhexlify(hex_data)
                    ascii_data = binary_data.decode('ascii', errors='ignore')
                    print(f"  Data (hex): {hex_data}")
                    print(f"  Data (ASCII): '{ascii_data}'")
                except Exception as e:
                    print(f"  Data (hex): {packet['data']}")
                    print(f"  Data decode error: {e}")
            
            # Send success response
            response = {
                "status": "success",
                "received_packets": packet_count,
                "server_time": int(time.time()),
                "message": "Data received successfully"
            }
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(response).encode('utf-8'))
            
            print(f"\nResponse sent: {response}")
            
        except json.JSONDecodeError as e:
            print(f"JSON decode error: {e}")
            self.send_error(400, f"Invalid JSON: {e}")
        except Exception as e:
            print(f"Error processing data: {e}")
            self.send_error(500, f"Server error: {e}")
    
    def do_GET(self):
        if self.path == '/':
            # Serve a simple status page
            html = """
            <!DOCTYPE html>
            <html>
            <head>
                <title>ESP32-C3 Data Receiver</title>
                <style>
                    body { font-family: Arial, sans-serif; margin: 40px; }
                    .status { background: #e8f5e8; padding: 20px; border-radius: 5px; }
                </style>
            </head>
            <body>
                <h1>ESP32-C3 SNTP HTTP Data Receiver</h1>
                <div class="status">
                    <h2>Server Status: Running</h2>
                    <p>Listening for POST requests on <code>/api/data</code></p>
                    <p>Current time: {}</p>
                </div>
                <h2>Configuration for ESP32-C3:</h2>
                <pre>
#define HTTP_POST_URL "http://YOUR_SERVER_IP:8080/api/data"
#define HTTP_CONTENT_TYPE "application/json"
#define HTTP_AUTH_HEADER ""  // No auth required for this example
#define USE_HTTPS false
                </pre>
            </body>
            </html>
            """.format(datetime.now().strftime('%Y-%m-%d %H:%M:%S'))
            
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            self.wfile.write(html.encode('utf-8'))
        else:
            self.send_error(404, "Not Found")
    
    def log_message(self, format, *args):
        # Suppress default HTTP logging to keep output clean
        pass

def run_server(port=8080):
    server_address = ('', port)
    httpd = HTTPServer(server_address, DataHandler)
    
    print(f"ESP32-C3 Data Receiver Server")
    print(f"{'='*40}")
    print(f"Listening on port {port}")
    print(f"Endpoint: http://localhost:{port}/api/data")
    print(f"Status page: http://localhost:{port}/")
    print(f"{'='*40}")
    print(f"Waiting for data from ESP32-C3...")
    
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server...")
        httpd.shutdown()

if __name__ == '__main__':
    run_server()