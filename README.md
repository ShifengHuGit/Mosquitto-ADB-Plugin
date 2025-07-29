# Mosquitto-ADB-Plugin

This is a plugin for [Mosquitto](https://mosquitto.org/) that inserts MQTT topic data into **Oracle Autonomous Database (ADB)**.

---

## ✨ Features

- ✅ Parses incoming MQTT messages
- ✅ Extracts telemetry data
- ✅ Inserts structured data into Oracle ADB using ODPI-C
- ✅ Runs inside a minimal container (Debian-based with Instant Client)

---

## 🧰 Prerequisites

Before using the plugin, make sure you:

1. ✅ **Create an Oracle ADB instance**
2. ✅ **Download the Oracle Wallet** for that ADB
3. ✅ **Create the required table** in ADB, e.g.:

   ```sql
   CREATE SEQUENCE telemetry_seq;

   CREATE TABLE telemetry (
     id        NUMBER PRIMARY KEY,
     vin       VARCHAR2(64),
     TripID    VARCHAR2(64),
     date_event TIMESTAMP DEFAULT SYSTIMESTAMP,
     data      CLOB
   );
 4.	✅ Edit plugin-config.json with your config:
      ```
     {
       "topic": "my/topic",
       "dbUser": "xxxxx",
       "dbPassword":"xxxxxxxxx"
       "walletPath": "/opt/oracle/wallets/adb",
       "tableName": "telemetry",
       "logPath": "/tmp/mosq_plugin.log",
       "tnsName": "your_tns_name"
     }	

5.	✅ Place the Oracle Wallet inside the Docker image:
	•	Must be at: /opt/oracle/wallets/adb
	•	Confirm sqlnet.ora exists and has correct paths
 ---
 ## 🐳 Building the Image with Podman
 Use podman or docker to build the plugin container image:

  ``` podman build -f Dockerfile -t mosquitto-with-adb-plugin:0.1 . ```
 
 ## 🚀 Running the Plugin
  To run the Mosquitto broker with the plugin:

  ```  podman run --rm -p 1883:1883 mosquitto-with-adb-plugin:0.1```
 
