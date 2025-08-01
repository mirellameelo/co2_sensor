from flask import Flask, request, jsonify
import json
import os

app = Flask(__name__)

DATA_FILE = "real_soil.jsonl"

@app.route("/api/sensor_json", methods=["POST"], strict_slashes=False)
def receive_sensor_data():
    data = request.get_json()

    if not data:
        return jsonify({"error": "Missing JSON body"}), 400

    if "data" not in data or "CO2" not in data["data"]:
        return jsonify({"error": "Missing CO₂ data field"}), 400

    co2_ppm = data["data"]["CO2"]
    light_res = data["data"].get("Photoresistivity", "N/A")
    switch_state = data["data"].get("state", "N/A")
    logger_id = data.get("loggerId", "unknown")
    cell_id = data.get("cellId", "unknown")
    timestamp = data.get("ts", "unknown")

    print(f"Payload: {json.dumps(data)}")
    print(f"[{timestamp}] Logger {logger_id}, Cell {cell_id}, CO₂: {co2_ppm} ppm, Light Resistivity: {light_res} V, Switch State: {switch_state}")

    # Save to local file
    try:
        with open(DATA_FILE, "a") as f:
            f.write(json.dumps(data) + "\n")
    except Exception as e:
        print(f"[ERROR] Failed to write data to file: {e}")
        return jsonify({"error": "Failed to save data"}), 500

    return jsonify({"status": "success"})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
