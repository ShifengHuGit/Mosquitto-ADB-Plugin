# Mosquitto-ADB-Plugin
This is a Plugin for Mosquitto to insert the Topic Data to Oracle ADB

use podman to build the image:

 e.g. : podman build -f dockerfile  -t mosquitto-with-adb-plugin:0.1

start the docker image

 e.g. : podman run --rm -p 1883:1883 mosquitto-with-adb-plugin:

  outputs: 
      1753772390: mosquitto version 2.0.11 starting
      1753772390: Config loaded from /mosquitto/config/mosquitto.conf.
      1753772390: Loading plugin: /mosquitto/plugins/mosq_adb_plugin.so
      [PLUGIN INIT] Called mosquitto_plugin_init()
      [PLUGIN INIT] Opt: config = /mosquitto/plugin-config.json
      [PLUGIN INIT] config_file = /mosquitto/plugin-config.json
      [PLUGIN INIT] log path = /tmp/mosq_plugin.log
      [PLUGIN INIT] TNS NAME = iotdatabase_medium
      [PLUGIN INIT] db user = admin
      [PLUGIN INIT] wallet = /opt/oracle/wallets/adb
      [ADB] === Starting IoT ADB connection ===
      [ADB] db user: admin
      [ADB] wallet path: /opt/oracle/wallets/adb
      [ADB] TNS name: iotdatabase_medium
      [ADB] dpiContext_create OK
      [ADB] dpiContext_initCommonCreateParams OK
      [ADB] dpiContext_initConnCreateParams OK
      [ADB] TNS_ADMIN set to: /opt/oracle/wallets/adb
      [ADB] Connection to ADB successful!
      [PLUGIN INIT] Plugin initialized, registering callback...
