from flask import Flask, request, jsonify
import json

app = Flask(__name__)

@app.route("/api/sensor_json", methods=["POST"])
def receive_co2_data():
    data = request.get_json()

    if not data:
        return jsonify({"error": "Missing JSON body"}), 400

    if "data" not in data or "ppm" not in data["data"]:
        return jsonify({"error": "Missing CO₂ data field"}), 400

    co2_ppm = data["data"]["ppm"]
    logger_id = data.get("loggerId", "unknown")
    cell_id = data.get("cellId", "unknown")
    timestamp = data.get("ts", "unknown")
    # debug
    print(f"Payload: {json.dumps(data)}")

    return jsonify({"status": "success"})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)