#include <mosquitto_broker.h>
#include <mosquitto_plugin.h>
#include <mosquitto.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cstring>

#include <dpi.h> // ODPI-C

#include "json.hpp" // nlohmann/json.hpp

using json = nlohmann::json;

#define UNUSED(A) (void)(A)

static mosquitto_plugin_id_t *mosq_pid = NULL;

struct TopicConfig
{
    std::string topic;
    std::string dbUser;
    std::string dbPassword; 
    std::string walletPath;
    std::string tableName;
    std::string logPath;
    std::string tnsName;
};

dpiConn *g_conn = nullptr;
dpiContext *g_context = nullptr;

std::vector<TopicConfig> g_configs;
std::ofstream log_file;

void log(const std::string &msg)
{
    if (log_file.is_open())
    {
        log_file << msg << std::endl;
    }
    else
    {
        std::cerr << "Log: " << msg << std::endl;
    }
}

bool load_config(const char *path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        std::cerr << "[CONFIG] Failed to open: " << path << std::endl;
        return false;
    }

    try
    {
        json j;
        f >> j;

        for (const auto &item : j)
        {
            TopicConfig cfg;
            cfg.topic = item.at("Topic").get<std::string>();
            cfg.dbUser = item["Config"].at("DBUser").get<std::string>();
            cfg.dbPassword = item["Config"].at("DBPassword").get<std::string>();
            cfg.walletPath = item["Config"].at("Wallet").get<std::string>();
            cfg.tableName = item["Config"].at("TableName").get<std::string>();
            cfg.logPath = item["Config"].at("LogPath").get<std::string>();
            cfg.tnsName = item["Config"].at("TNSName").get<std::string>();

            g_configs.push_back(cfg);
        }
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[CONFIG] Exception while parsing config: " << e.what() << std::endl;
        return false;
    }
}

bool connect_adb(const std::string &user,  const std::string &password, const std::string &wallet_path, const std::string &tns_name)
{
    std::cerr << "[ADB] === Starting IoT ADB connection ===" << std::endl;
    std::cerr << "[ADB] db user: " << user << std::endl;
    std::cerr << "[ADB] wallet path: " << wallet_path << std::endl;
    std::cerr << "[ADB] TNS name: " << tns_name << std::endl;

    // dpiContext *ctx = nullptr;
    dpiConnCreateParams connParams;
    dpiCommonCreateParams commonParams;

    // Create DPI context
    if (dpiContext_create(DPI_MAJOR_VERSION, DPI_MINOR_VERSION, &g_context, NULL) != DPI_SUCCESS || !g_context)
    {
        std::cerr << "[ADB] dpiContext_create FAILED" << std::endl;
        if (g_context)
        {
            dpiErrorInfo err;
            dpiContext_getError(g_context, &err);
            std::cerr << "[ADB] ERROR message: " << err.message << std::endl;
        }
        else
        {
            std::cerr << "[ADB] dpiContext is NULL — cannot get error info." << std::endl;
        }
        return false;
    }

    std::cerr << "[ADB] dpiContext_create OK" << std::endl;

    // Init common params
    if (dpiContext_initCommonCreateParams(g_context, &commonParams) != DPI_SUCCESS)
    {
        dpiErrorInfo err;
        dpiContext_getError(g_context, &err);
        std::cerr << "[ADB] dpiContext_initCommonCreateParams failed: " << err.message << std::endl;
        return false;
    }

    std::cerr << "[ADB] dpiContext_initCommonCreateParams OK" << std::endl;

    // Init conn params
    if (dpiContext_initConnCreateParams(g_context, &connParams) != DPI_SUCCESS)
    {
        dpiErrorInfo err;
        dpiContext_getError(g_context, &err);
        std::cerr << "[ADB] dpiContext_initConnCreateParams failed: " << err.message << std::endl;
        return false;
    }

    std::cerr << "[ADB] dpiContext_initConnCreateParams OK" << std::endl;

    // 设置 TNS_ADMIN 环境变量指向 Wallet 路径
    setenv("TNS_ADMIN", wallet_path.c_str(), 1);
    std::cerr << "[ADB] TNS_ADMIN set to: " << wallet_path << std::endl;

    // 不启用 externalAuth 模式（使用 Wallet）
    connParams.externalAuth = 0;
    
    // 建立连接（用户名保留，密码为空）
    if (dpiConn_create(g_context,
                       user.c_str(), user.length(),
                       password.c_str(), password.length(),
                       tns_name.c_str(), tns_name.length(),
                       &commonParams, &connParams,
                       &g_conn) != DPI_SUCCESS)
    {
        dpiErrorInfo err;
        dpiContext_getError(g_context, &err);
        std::cerr << "[ADB] dpiConn_create failed: " << err.message << std::endl;
        return false;
    }

    std::cerr << "[ADB] Connection to ADB successful!" << std::endl;
    return true;
}

int callback_message(int event, void *event_data, void *userdata)
{
    UNUSED(event);
    UNUSED(userdata);

    std::cerr << "[CallBack]  callback triggered..." << std::endl;
    auto *ed = (mosquitto_evt_message *)event_data;
    std::string topic = ed->topic;

    for (auto &cfg : g_configs)
    {
        if (topic == cfg.topic)
        {
            log("Received matching message on topic: " + topic);
            std::cerr << "[CallBack]  Found matching config for topic: " << cfg.topic << std::endl;
            std::string payload((char *)ed->payload, ed->payloadlen);
            std::cerr << "[CallBack]  Payload: " << payload << std::endl;
            json j;
            try
            {
                j = json::parse(payload);
            }
            catch (const std::exception &e)
            {
                log(std::string("[ERROR] JSON parse failed: ") + e.what());
                return MOSQ_ERR_SUCCESS;
            }

            // 提取 VIN、TripID 和 telemetry 数据
            std::string vin = j.value("VIN", "");
            std::string tripId = j.value("TripID", "");
            json telemetryData = j["telemetry"];
            std::string telemetryJson = telemetryData.dump();
            if (vin.empty() || tripId.empty())
            {
                log("[ERROR] VIN or TripID missing in message");
                return MOSQ_ERR_SUCCESS;
            }

            // 构造 SQL 语句
            std::string sql = "INSERT INTO telemetry (id, vin, TripID, date_event, data) "
                              "VALUES (telemetry_seq.NEXTVAL, :1, :2, SYSTIMESTAMP, :3)";

            dpiStmt *stmt;
            if (dpiConn_prepareStmt(g_conn, 0, sql.c_str(), sql.length(), NULL, 0, &stmt) != DPI_SUCCESS)
            {
                log("[ERROR] Connection Prepare failed");
                return MOSQ_ERR_SUCCESS;
            }

            dpiData vinData, tripData, jsonData;

            vinData.isNull = 0;
            vinData.value.asBytes.ptr = (char *)vin.c_str();
            vinData.value.asBytes.length = vin.length();

            tripData.isNull = 0;
            tripData.value.asBytes.ptr = (char *)tripId.c_str();
            tripData.value.asBytes.length = tripId.length();

            jsonData.isNull = 0;
            jsonData.value.asBytes.ptr = (char *)telemetryJson.c_str();
            jsonData.value.asBytes.length = telemetryJson.length();

            if (dpiStmt_bindValueByPos(stmt, 1, DPI_NATIVE_TYPE_BYTES, &vinData) != DPI_SUCCESS ||
                dpiStmt_bindValueByPos(stmt, 2, DPI_NATIVE_TYPE_BYTES, &tripData) != DPI_SUCCESS ||
                dpiStmt_bindValueByPos(stmt, 3, DPI_NATIVE_TYPE_BYTES, &jsonData) != DPI_SUCCESS)
            {

                std::cerr << "[ADBERROR] Bind failed " << std::endl;
                dpiStmt_release(stmt);
                return MOSQ_ERR_SUCCESS;
            }

            if (dpiStmt_execute(stmt, DPI_MODE_EXEC_DEFAULT, NULL) != DPI_SUCCESS)
            {
                dpiErrorInfo errorInfo;
                dpiContext_getError(g_context, &errorInfo);
                std::cerr << "[ADB] Execute failed: "
                          << "ORA-" << errorInfo.code << ": " << errorInfo.message << std::endl;

                dpiStmt_release(stmt);
                return MOSQ_ERR_SUCCESS;
            }

            // 提交事务 
            if (dpiConn_commit(g_conn) != DPI_SUCCESS)
            {
                dpiErrorInfo err;
                dpiContext_getError(g_context, &err);
                std::cerr << "[ADB] Commit failed: ORA-" << err.code << ": " << err.message << std::endl;
                dpiStmt_release(stmt);
                return MOSQ_ERR_SUCCESS;
            }

            dpiStmt_release(stmt);
            log("[SUCCESS ADB] Inserted telemetry data for VIN " + vin);
            std::cerr << "[SUCCESS Callback]  Inserted telemetry data for VIN: " << vin << std::endl;
        }
    }
    std::cerr << "[CallBack]  Callback processing complete." << std::endl;
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_plugin_version(int supported_version_count, const int *supported_versions)
{
    for (int i = 0; i < supported_version_count; ++i)
    {
        if (supported_versions[i] == 5)
            return 5;
    }
    return -1;
}

int mosquitto_plugin_init(mosquitto_plugin_id_t *identifier, void **user_data,
                          struct mosquitto_opt *opts, int opt_count)
{
    UNUSED(user_data);
    mosq_pid = identifier;

    std::cerr << "[PLUGIN INIT] Called mosquitto_plugin_init()" << std::endl;

    const char *config_file = nullptr;
    for (int i = 0; i < opt_count; ++i)
    {
        std::cerr << "[PLUGIN INIT] Opt: " << opts[i].key << " = " << opts[i].value << std::endl;
        if (strcmp(opts[i].key, "config") == 0)
        {
            config_file = opts[i].value;
        }
    }

    if (!config_file)
    {
        std::cerr << "[PLUGIN INIT] config_file is NULL!" << std::endl;
        return MOSQ_ERR_UNKNOWN;
    }

    std::cerr << "[PLUGIN INIT] config_file = " << config_file << std::endl;

    if (!load_config(config_file))
    {
        std::cerr << "[PLUGIN INIT] load_config() failed" << std::endl;
        return MOSQ_ERR_UNKNOWN;
    }

    std::cerr << "[PLUGIN INIT] log path = " << g_configs[0].logPath << std::endl;
    log_file.open(g_configs[0].logPath, std::ios::app);

    if (g_configs.empty())
    {
        std::cerr << "[PLUGIN INIT] g_configs is EMPTY!" << std::endl;
        return MOSQ_ERR_UNKNOWN;
    }

    std::cerr << "[PLUGIN INIT] TNS NAME = " << g_configs[0].tnsName << std::endl;
    std::cerr << "[PLUGIN INIT] db user = " << g_configs[0].dbUser << std::endl;
    std::cerr << "[PLUGIN INIT] wallet = " << g_configs[0].walletPath << std::endl;

    if (!log_file.is_open())
    {
        std::cerr << "[PLUGIN INIT] Failed to open log file!" << std::endl;
    }

    if (!connect_adb(g_configs[0].dbUser, g_configs[0].dbPassword, g_configs[0].walletPath, g_configs[0].tnsName))
    {
        std::cerr << "[PLUGIN INIT] ADB connection failed!" << std::endl;
        return MOSQ_ERR_UNKNOWN;
    }

    std::cerr << "[PLUGIN INIT] Plugin initialized, registering callback..." << std::endl;
    return mosquitto_callback_register(mosq_pid, MOSQ_EVT_MESSAGE, callback_message, NULL, NULL);
}

int mosquitto_plugin_cleanup(void *user_data, struct mosquitto_opt *opts, int opt_count)
{
    UNUSED(user_data);
    UNUSED(opts);
    UNUSED(opt_count);

    if (g_conn)
        dpiConn_release(g_conn);
    log("Plugin cleanup complete.");
    if (log_file.is_open())
        log_file.close();

    return mosquitto_callback_unregister(mosq_pid, MOSQ_EVT_MESSAGE, callback_message, NULL);
}
