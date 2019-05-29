
#include <string>
#include <iostream>
#include <sstream>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
using namespace std;
using namespace mxnavi::config;

#define PRINT_BINARY(buff, size, w)               \
    do                                            \
    {                                             \
        std::stringstream sm("");                 \
        for (uint32_t off = 0; off < size; ++off) \
        {                                         \
            sm.width(w);                          \
            sm.fill('0');                         \
            sm << std::hex << (int32_t)buff[off]; \
        }                                         \
        std::string banery_str;                   \
        sm >> banery_str;                         \
        return banery_str;                        \
    } while (0);

string print_binary(const uint8_t* buff, uint32_t size)
{
    uint32_t log_level = 0;
    Config::GetInstance().GetConfigOption(ConfigOpt::LOG_LogLevel, log_level);
    if (log_level > 1)
    {
        return "";
    }

    PRINT_BINARY(buff, size, 2);
}

string print_binary(const uint16_t* buff, uint32_t size)
{
    uint32_t log_level = 0;
    Config::GetInstance().GetConfigOption(ConfigOpt::LOG_LogLevel, log_level);
    if (log_level > 1)
    {
        return "";
    }

    PRINT_BINARY(buff, size, 4);
}


bool Base64Encode(const std::string& input, std::string& output)
{
    typedef boost::archive::iterators::base64_from_binary<
        boost::archive::iterators::transform_width<string::const_iterator, 6, 8>>
        iter;

    stringstream result;
    copy(iter(input.begin()), iter(input.end()), ostream_iterator<char>(result));

    size_t Num = (3 - input.length() % 3) % 3;

    for (size_t i = 0; i < Num; i++)
    {
        result.put('=');
    }

    output = result.str();

    return !output.empty();
}

bool Base64Decode(const std::string& input, std::string& output)
{
    typedef boost::archive::iterators::
        transform_width<boost::archive::iterators::binary_from_base64<string::const_iterator>, 8, 6>
            iter;

    stringstream result;

    string input_tmp = input.substr(0, input.find_last_not_of('=') + 1);

    try
    {
        copy(iter(input_tmp.begin()), iter(input_tmp.end()), ostream_iterator<char>(result));
    }
    catch (...)
    {
        return false;
    }

    output = result.str();

    return !output.empty();
}

bool HexStrToStr(const std::string& input, std::string& output)
{
    if (input.empty())
    {
        return false;
    }

    for (decltype(input.size()) offset = 0; offset < input.size(); ++++offset)
    {
        stringstream sm(input.substr(offset, 2));
        uint16_t     tmp_hex;
        sm >> std::hex >> tmp_hex;

        output.append(1, tmp_hex);
    }

    return !output.empty();
}

bool StrToHexStr(const std::string& input, std::string& output)
{
    if (input.empty())
    {
        return false;
    }

    stringstream sm;
    for (decltype(input.size()) offset = 0; offset < input.size(); ++offset)
    {
        uint16_t tmp_hex = input[offset];
        sm << std::hex << tmp_hex;
    }

    sm >> output;

    return !output.empty();
}

#include <map>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include "common/common.h"
#include "sqlite/sqlite3.h"
#include "config/config.hpp"

using namespace std;
using namespace mxnavi::config;

namespace mxnavi
{
namespace config
{
Config* Config::m_instance = NULL;

Config::Config()
{
}

Config::~Config()
{
}

bool Config::CreateInstance()
{
    if (NULL == m_instance)
    {
        m_instance = new Config();

        if (NULL == m_instance)
        {
            return false;
        }
    }

    return true;
}

void Config::DestroyInstance()
{
    if (m_instance != NULL)
    {
        delete m_instance;
        m_instance = NULL;
    }
}

Config& Config::GetInstance()
{
    return *m_instance;
}

Config* Config::GetInstancePtr()
{
    return m_instance;
}


int32_t Config::ConfigInit()
{
    auto ret = this->ConfigInitFile();
    if (ret != TBOX_SUCCESS)
    {
        exit(0);
    }

    ret = this->ConfigInitDb();
    if (ret != TBOX_SUCCESS)
    {
        exit(0);
    }

    return TBOX_SUCCESS;
}

int32_t Config::ConfigInitDb()
{
    sqlite3*      db_handle = nullptr;
    sqlite3_stmt* stmt      = nullptr;
    string        db_path   = this->source_path_;

    if (db_path.back() != '/')
    {
        db_path.append("/");
    }

    db_path.append(SYS_CONFIG_DB_NAME);

    auto is_create_db = false;
    if (access(db_path.c_str(), F_OK) == -1)
    {
        is_create_db = true;
    }

    auto ret = sqlite3_open(db_path.c_str(), &db_handle);
    if (ret != SQLITE_OK)
    {
        sqlite3_close(db_handle);

        return TBOX_ERROR_RESULT_CONFIG;
    }

    if (is_create_db)
    {
        string create_sql =
            "CREATE TABLE PARAM(LOCAL_CYCLE INTEGER, REPORT_CYCLE INTEGER, ALARM_CYCLE INTEGER, EP_URL "
            "TEXT, EP_PORT INT, PP_URL TEXT, PP_PORT INT, TBOX_TIMEOUT INT, PF_TIMEOUT INT, INTERVAL INT, HB_CYCLE "
            "INT, MODE_TYPE INT, MODE_BEGIN_TM INTEGER, MODE_TM_QUANTUM INT, LOG_LEVEL INT, SAMPLING INT)";
        ret = sqlite3_exec(db_handle, create_sql.c_str(), nullptr, 0, nullptr);
        if (ret != SQLITE_OK)
        {
            return TBOX_ERROR_RESULT_CONFIG;
        }

        string insert_sql =
            "INSERT INTO PARAM (LOCAL_CYCLE, REPORT_CYCLE, ALARM_CYCLE, EP_URL, EP_PORT, PP_URL, PP_PORT, "
            "TBOX_TIMEOUT, PF_TIMEOUT, INTERVAL, HB_CYCLE, MODE_TYPE, MODE_BEGIN_TM, MODE_TM_QUANTUM, "
            "LOG_LEVEL, SAMPLING) values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

        ret = sqlite3_prepare_v2(db_handle, insert_sql.c_str(), -1, &stmt, nullptr);
        if (ret != SQLITE_OK)
        {
            return TBOX_ERROR_RESULT_CONFIG;
        }

        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_uint64>(this->common_data_store_interval_));
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_uint64>(this->real_time_upload_));
        sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_uint64>(this->alarm_data_store_interval_));
        sqlite3_bind_text(stmt, 4, this->server_name_.c_str(), this->server_name_.size(), SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, static_cast<int>(this->server_port_));
        sqlite3_bind_text(
            stmt, 6, this->public_platform_url_.c_str(), this->public_platform_url_.size(), SQLITE_STATIC);
        sqlite3_bind_int(stmt, 7, static_cast<int>(this->public_platform_port_));
        sqlite3_bind_int(stmt, 8, static_cast<int>(this->tbox_rsp_interval_));
        sqlite3_bind_int(stmt, 9, static_cast<int>(this->platform_rsp_interval_));
        sqlite3_bind_int(stmt, 10, static_cast<int>(this->repeat_login_intervel_));
        sqlite3_bind_int64(stmt, 11, static_cast<sqlite3_uint64>(this->heartbeat_time_));
        sqlite3_bind_int(stmt, 12, static_cast<int>(this->mode_type_));
        sqlite3_bind_int64(stmt, 13, static_cast<sqlite3_uint64>(this->mode_begin_time_));
        sqlite3_bind_int(stmt, 14, static_cast<int>(this->mode_time_quantum_));
        sqlite3_bind_int(stmt, 15, static_cast<int>(this->log_level_));
        sqlite3_bind_int(stmt, 16, static_cast<int>(this->sampling_));

        ret = sqlite3_step(stmt);
        if (ret != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            return TBOX_ERROR_RESULT_CONFIG;
        }
    }
    else
    {
        string query_sql = "SELECT * FROM PARAM";

        ret = sqlite3_prepare_v2(db_handle, query_sql.c_str(), -1, &stmt, nullptr);
        if (ret != SQLITE_OK)
        {
            return TBOX_ERROR_RESULT_CONFIG;
        }

        auto col_num = sqlite3_column_count(stmt);
        if (col_num != 16)
        {
            sqlite3_finalize(stmt);
            return TBOX_ERROR_RESULT_CONFIG;
        }

        for (;;)
        {
            if (sqlite3_step(stmt) != SQLITE_ROW)
            {
                break;
            }

            for (auto col = 0; col < col_num; ++col)
            {
                auto type     = sqlite3_column_type(stmt, col);
                auto col_name = sqlite3_column_name(stmt, col);

                switch (type)
                {
                case SQLITE_TEXT:
                {
                    if (strcmp(col_name, "EP_URL") != 0 && strcmp(col_name, "PP_URL") != 0)
                    {
                        return TBOX_ERROR_RESULT_CONFIG;
                    }

                    if (strcmp(col_name, "EP_URL") == 0)
                    {
                        this->server_name_ = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
                    }
                    else if (strcmp(col_name, "PP_URL") == 0)
                    {
                        this->public_platform_url_ = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
                    }

                    break;
                }
                case SQLITE_BLOB:
                    break;
                case SQLITE_INTEGER:
                    TBOX_LOG_DEBUG(LOGGER_MODULE_ID_UPPER_SYSTEM_DB,
                                   "column name:%s, val:%u",
                                   col_name,
                                   sqlite3_column_int(stmt, col));
                    if (strcmp(col_name, "LOCAL_CYCLE") != 0 && strcmp(col_name, "REPORT_CYCLE") != 0 &&
                        strcmp(col_name, "ALARM_CYCLE") != 0 && strcmp(col_name, "EP_PORT") != 0 &&
                        strcmp(col_name, "PP_PORT") != 0 && strcmp(col_name, "TBOX_TIMEOUT") != 0 &&
                        strcmp(col_name, "PF_TIMEOUT") != 0 && strcmp(col_name, "INTERVAL") != 0 &&
                        strcmp(col_name, "HB_CYCLE") != 0 && strcmp(col_name, "MODE_TYPE") != 0 &&
                        strcmp(col_name, "MODE_BEGIN_TM") != 0 && strcmp(col_name, "MODE_TM_QUANTUM") != 0 &&
                        strcmp(col_name, "LOG_LEVEL") != 0 && strcmp(col_name, "SAMPLING") != 0)
                    {
                        return TBOX_ERROR_RESULT_CONFIG;
                    }

                    if (strcmp(col_name, "LOCAL_CYCLE") == 0)
                    {
                        this->common_data_store_interval_ = static_cast<uint64_t>(sqlite3_column_int64(stmt, col));
                    }
                    else if (strcmp(col_name, "REPORT_CYCLE") == 0)
                    {
                        this->real_time_upload_ = static_cast<uint64_t>(sqlite3_column_int64(stmt, col));
                    }
                    else if (strcmp(col_name, "ALARM_CYCLE") == 0)
                    {
                        this->alarm_data_store_interval_ = static_cast<uint64_t>(sqlite3_column_int64(stmt, col));
                    }
                    else if (strcmp(col_name, "EP_PORT") == 0)
                    {
                        this->server_port_ = static_cast<uint32_t>(sqlite3_column_int(stmt, col));
                    }
                    else if (strcmp(col_name, "PP_PORT") == 0)
                    {
                        this->public_platform_port_ = static_cast<uint32_t>(sqlite3_column_int(stmt, col));
                    }
                    else if (strcmp(col_name, "TBOX_TIMEOUT") == 0)
                    {
                        this->tbox_rsp_interval_ = static_cast<uint32_t>(sqlite3_column_int(stmt, col));
                    }
                    else if (strcmp(col_name, "PF_TIMEOUT") == 0)
                    {
                        this->platform_rsp_interval_ = static_cast<uint32_t>(sqlite3_column_int(stmt, col));
                    }
                    else if (strcmp(col_name, "INTERVAL") == 0)
                    {
                        this->repeat_login_intervel_ = static_cast<uint32_t>(sqlite3_column_int(stmt, col));
                    }
                    else if (strcmp(col_name, "HB_CYCLE") == 0)
                    {
                        this->heartbeat_time_ = static_cast<uint64_t>(sqlite3_column_int64(stmt, col));
                    }
                    else if (strcmp(col_name, "MODE_TYPE") == 0)
                    {
                        this->mode_type_ = static_cast<uint32_t>(sqlite3_column_int(stmt, col));
                    }
                    else if (strcmp(col_name, "MODE_BEGIN_TM") == 0)
                    {
                        this->mode_begin_time_ = static_cast<uint64_t>(sqlite3_column_int64(stmt, col));
                    }
                    else if (strcmp(col_name, "MODE_TM_QUANTUM") == 0)
                    {
                        this->mode_time_quantum_ = static_cast<uint32_t>(sqlite3_column_int(stmt, col));
                    }
                    else if (strcmp(col_name, "LOG_LEVEL") == 0)
                    {
                        this->log_level_ = static_cast<uint32_t>(sqlite3_column_int(stmt, col));
                    }
                    else if (strcmp(col_name, "SAMPLING") == 0)
                    {
                        this->sampling_ = static_cast<uint16_t>(sqlite3_column_int(stmt, col));
                    }

                    break;
                default:
                    return TBOX_ERROR_RESULT_CONFIG;
                }
            }
        }
    }

    sqlite3_finalize(stmt);

    return TBOX_SUCCESS;
}

int32_t Config::ConfigInitFile()
{
    map<ConfigOpt, string> map_option = {{ConfigOpt::ServerNameType, "server_name"},
                                         {ConfigOpt::ServerPortType, "server_port"},
                                         {ConfigOpt::CaCertNameType, "ca_cert_name"},
                                         {ConfigOpt::ClientCertNameType, "client_cert_name"},
                                         {ConfigOpt::ClientKeyNameType, "client_key_name"},
                                         {ConfigOpt::ClientKeyPasswd, "client_key_passwd"},
                                         {ConfigOpt::DbPathType, "db_path"},
                                         {ConfigOpt::SourcePathType, "source_path"},
                                         {ConfigOpt::RealTimeCollectType, "real_time_collect"},
                                         {ConfigOpt::RealTimeUploadType, "real_time_upload"},
                                         {ConfigOpt::CommonDataStorIntervalType, "common_data_store_interval"},
                                         {ConfigOpt::AlarmDataStoreIntervalType, "alarm_data_store_interval"},
                                         {ConfigOpt::DataStoreTimeType, "data_store_time"},
                                         {ConfigOpt::HeartbeatTimeType, "heartbeat_time"},
                                         {ConfigOpt::DeleteDataInterval, "delete_data_interval"},
                                         {ConfigOpt::LOG_LogModule, "log_module"},
                                         {ConfigOpt::LOG_LogLevel, "log_level"},
                                         {ConfigOpt::VDCRC_LowerSystemIP, "lower_system_ip"},
                                         {ConfigOpt::VDCRC_LowerSystemPort, "lower_system_port"},
                                         {ConfigOpt::VDCRC_GprmcBakInterval, "gprmc_bak_interval"},
                                         {ConfigOpt::VDCRC_GprmcBakDefaultLat, "gprmc_bak_default_lat"},
                                         {ConfigOpt::VDCRC_GprmcBakDefaultLon, "gprmc_bak_default_lon"},
                                         {ConfigOpt::UpgradePkgStorePath, "upgrade_pkg_store_path"},
                                         {ConfigOpt::UpgradePkgDecryptKey, "upgrade_pkg_decrypt_key"},
                                         {ConfigOpt::UpgradeCaCertName, "upgrade_ca_cert_name"},
                                         {ConfigOpt::PlatformRspInterval, "platform_rsp_interval"},
                                         {ConfigOpt::TboxRspInterval, "tbox_rsp_interval"},
                                         {ConfigOpt::RepeatLoginIntervel, "repeat_login_intervel"},
                                         {ConfigOpt::Sampling, "sampling"},
                                         {ConfigOpt::ModeType, "mode_type"},
                                         {ConfigOpt::PublicPlatformUrl, "public_platform_url"},
                                         {ConfigOpt::PublicPlatformPort, "public_platform_port"},
                                         {ConfigOpt::ModeBeginTime, "mode_begin_time"},
                                         {ConfigOpt::ModeTimeQuantum, "mode_time_quantum"},
                                         {ConfigOpt::TimingInterval, "timing_interval"},
                                         {ConfigOpt::Vehicle_Uid, "vehicle_uid"},
                                         {ConfigOpt::Iccid, "iccid"}};

    ifstream infile("/home/root/upload.conf");
    if (!infile.is_open())
    {
        infile.open("./upload.conf");
        if (!infile.is_open())
        {
            return TBOX_ERROR_RESULT_CONFIG;
        }
    }

    while (!infile.eof())
    {
        string line;
        getline(infile, line);

        line.erase(remove_if(line.begin(), line.end(), [](unsigned char elem) { return isspace(elem); }), line.end());

        if (line[0] == '#' || line[0] == '[')
        {
            continue;
        }

        auto it = std::find_if(map_option.begin(), map_option.end(), [&](map<ConfigOpt, string>::value_type& elem) {
            return (line.substr(0, line.find_first_of('=')) == elem.second);
        });

        if (it == map_option.end())
        {
            continue;
        }

        line.erase(0, line.find('=') + 1);

        switch (it->first)
        {
        case ConfigOpt::SourcePathType:
            if (line.back() != '/')
            {
                line.append("/");
            }
            this->source_path_ = line;
            break;
        case ConfigOpt::ServerNameType:
            this->server_name_ = line;
            break;
        case ConfigOpt::ServerPortType:
            this->StringToAnyType(line, this->server_port_);
            break;
        case ConfigOpt::CaCertNameType:
            this->ca_cert_name_ = line;
            break;
        case ConfigOpt::ClientCertNameType:
            this->client_cert_name_ = line;
            break;
        case ConfigOpt::ClientKeyNameType:
            this->client_key_name_ = line;
            break;
        case ConfigOpt::ClientKeyPasswd:
            this->client_key_passwd_ = line;
            break;
        case ConfigOpt::DataStoreTimeType:
            this->StringToAnyType(line, this->data_store_time_);
            break;
        case ConfigOpt::DbPathType:
            this->db_path_ = line;
            break;
        case ConfigOpt::RealTimeCollectType:
            this->StringToAnyType(line, this->real_time_collect_);
            break;
        case ConfigOpt::RealTimeUploadType:
            this->StringToAnyType(line, this->real_time_upload_);
            break;
        case ConfigOpt::CommonDataStorIntervalType:
            this->StringToAnyType(line, this->common_data_store_interval_);
            break;
        case ConfigOpt::AlarmDataStoreIntervalType:
            this->StringToAnyType(line, this->alarm_data_store_interval_);
            break;
        case ConfigOpt::HeartbeatTimeType:
            this->StringToAnyType(line, this->heartbeat_time_);
            break;
        case ConfigOpt::DeleteDataInterval:
            this->StringToAnyType(line, this->delete_data_interval_);
            break;
        case ConfigOpt::LOG_LogModule:
            this->StringToAnyType(line, this->log_module_);
            break;
        case ConfigOpt::LOG_LogLevel:
            this->StringToAnyType(line, this->log_level_);
            break;
        case ConfigOpt::VDCRC_LowerSystemIP:
            this->StringToAnyType(line, this->lower_system_ip);
            break;
        case ConfigOpt::VDCRC_LowerSystemPort:
            this->StringToAnyType(line, this->lower_system_port);
            break;
        case ConfigOpt::VDCRC_GprmcBakInterval:
            this->StringToAnyType(line, this->gprmc_bak_interval);
            break;
        case ConfigOpt::VDCRC_GprmcBakDefaultLat:
            this->StringToAnyType(line, this->gprmc_bak_default_lat);
            break;
        case ConfigOpt::VDCRC_GprmcBakDefaultLon:
            this->StringToAnyType(line, this->gprmc_bak_default_lon);
            break;
        case ConfigOpt::UpgradePkgStorePath:
            this->upgrade_pkg_store_path_ = line;
            break;
        case ConfigOpt::UpgradePkgDecryptKey:
            this->upgrade_pkg_decrypt_key_ = line;
            break;
        case ConfigOpt::UpgradeCaCertName:
            this->upgrade_ca_cert_name_ = line;
            break;
        case ConfigOpt::PlatformRspInterval:
            this->StringToAnyType(line, this->platform_rsp_interval_);
            break;
        case ConfigOpt::TboxRspInterval:
            this->StringToAnyType(line, this->tbox_rsp_interval_);
            break;
        case ConfigOpt::RepeatLoginIntervel:
            this->StringToAnyType(line, this->repeat_login_intervel_);
            break;
        case ConfigOpt::Sampling:
            this->StringToAnyType(line, this->sampling_);
            break;
        case ConfigOpt::ModeType:
            this->StringToAnyType(line, this->mode_type_);
            break;
        case ConfigOpt::PublicPlatformUrl:
            this->public_platform_url_ = line;
            break;
        case ConfigOpt::PublicPlatformPort:
            this->StringToAnyType(line, this->public_platform_port_);
            break;
        case ConfigOpt::ModeBeginTime:
            this->StringToAnyType(line, this->mode_begin_time_);
            break;
        case ConfigOpt::ModeTimeQuantum:
            this->StringToAnyType(line, this->mode_time_quantum_);
            break;
        case ConfigOpt::TimingInterval:
            this->StringToAnyType(line, this->timing_interval_);
            break;
        case ConfigOpt::Vehicle_Uid:
            this->vehicle_uid_ = line;
            break;
        case ConfigOpt::Iccid:
            this->iccid_ = line;
            break;
        default:
            break;
        }
    }

    return TBOX_SUCCESS;
}

int32_t Config::UpdateDbConfig()
{
    sqlite3*      db_handle = nullptr;
    sqlite3_stmt* stmt      = nullptr;
    string        db_path   = this->source_path_;

    if (db_path.back() != '/')
    {
        db_path.append("/");
    }

    db_path.append(SYS_CONFIG_DB_NAME);

    auto ret = sqlite3_open(db_path.c_str(), &db_handle);
    if (ret != SQLITE_OK)
    {
        sqlite3_close(db_handle);

        return TBOX_ERROR_RESULT_CONFIG;
    }

    string update_sql = "UPDATE PARAM set LOCAL_CYCLE = ?, REPORT_CYCLE = ?, ALARM_CYCLE = ?, EP_URL = ?, EP_PORT = ?, "
                        "PP_URL = ?, PP_PORT = ?, TBOX_TIMEOUT = ?, PF_TIMEOUT = ?, INTERVAL = ?, HB_CYCLE = ?, "
                        "MODE_TYPE = ?, MODE_BEGIN_TM = ?, MODE_TM_QUANTUM = ?, LOG_LEVEL = ?, SAMPLING = ?";

    ret = sqlite3_prepare_v2(db_handle, update_sql.c_str(), -1, &stmt, nullptr);
    if (ret != SQLITE_OK)
    {
        return TBOX_ERROR_RESULT_CONFIG;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_uint64>(this->common_data_store_interval_));
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_uint64>(this->real_time_upload_));
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_uint64>(this->alarm_data_store_interval_));
    sqlite3_bind_text(stmt, 4, this->server_name_.c_str(), this->server_name_.size(), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, static_cast<int>(this->server_port_));
    sqlite3_bind_text(stmt, 6, this->public_platform_url_.c_str(), this->public_platform_url_.size(), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, static_cast<int>(this->public_platform_port_));
    sqlite3_bind_int(stmt, 8, static_cast<int>(this->tbox_rsp_interval_));
    sqlite3_bind_int(stmt, 9, static_cast<int>(this->platform_rsp_interval_));
    sqlite3_bind_int(stmt, 10, static_cast<int>(this->repeat_login_intervel_));
    sqlite3_bind_int64(stmt, 11, static_cast<sqlite3_uint64>(this->heartbeat_time_));
    sqlite3_bind_int(stmt, 12, static_cast<int>(this->mode_type_));
    sqlite3_bind_int64(stmt, 13, static_cast<sqlite3_uint64>(this->mode_begin_time_));
    sqlite3_bind_int(stmt, 14, static_cast<int>(this->mode_time_quantum_));
    sqlite3_bind_int(stmt, 15, static_cast<int>(this->log_level_));
    sqlite3_bind_int(stmt, 16, static_cast<int>(this->sampling_));

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        return TBOX_ERROR_RESULT_CONFIG;
    }
    return TBOX_SUCCESS;
}

void Config::GetConfigOption(ConfigOpt opt, string& val)
{
    switch (opt)
    {
    case ConfigOpt::SourcePathType:
        val = this->source_path_;
        break;
    case ConfigOpt::ServerNameType:
        val = this->server_name_;
        break;
    case ConfigOpt::CaCertNameType:
        val = this->ca_cert_name_;
        break;
    case ConfigOpt::ClientCertNameType:
        val = this->client_cert_name_;
        break;
    case ConfigOpt::ClientKeyNameType:
        val = this->client_key_name_;
        break;
    case ConfigOpt::ClientKeyPasswd:
        val = this->client_key_passwd_;
        break;
    case ConfigOpt::DbPathType:
        val = this->db_path_;
        break;
    case ConfigOpt::VDCRC_LowerSystemIP:
        val = this->lower_system_ip;
        break;
    case ConfigOpt::UpgradePkgStorePath:
        val = this->upgrade_pkg_store_path_;
        break;
    case ConfigOpt::UpgradePkgDecryptKey:
        val = this->upgrade_pkg_decrypt_key_;
        break;
    case ConfigOpt::UpgradeCaCertName:
        val = this->upgrade_ca_cert_name_;
        break;
    case ConfigOpt::PublicPlatformUrl:
        val = this->public_platform_url_;
        break;
    case ConfigOpt::Vehicle_Uid:
        val = this->vehicle_uid_;
        break;
    case ConfigOpt::Iccid:
        val = this->iccid_;
        break;
    default:
        break;
    }

    return;
}

void Config::GetConfigOption(ConfigOpt opt, uint16_t& val)
{
    switch (opt)
    {
    case ConfigOpt::VDCRC_LowerSystemPort:
        val = this->lower_system_port;
        break;
    case ConfigOpt::RepeatLoginIntervel:
        val = this->repeat_login_intervel_;
        break;
    case ConfigOpt::Sampling:
        val = this->sampling_;
        break;
    case ConfigOpt::ModeType:
        val = this->mode_type_;
        break;
    default:
        break;
    }

    return;
}

void Config::GetConfigOption(ConfigOpt opt, uint32_t& val)
{
    switch (opt)
    {
    case ConfigOpt::ServerPortType:
        val = this->server_port_;
        break;
    case ConfigOpt::LOG_LogModule:
        val = this->log_module_;
        break;
    case ConfigOpt::LOG_LogLevel:
        val = this->log_level_;
        break;
    case ConfigOpt::VDCRC_GprmcBakInterval:
        val = this->gprmc_bak_interval;
        break;
    case ConfigOpt::VDCRC_GprmcBakDefaultLat:
        val = this->gprmc_bak_default_lat;
        break;
    case ConfigOpt::VDCRC_GprmcBakDefaultLon:
        val = this->gprmc_bak_default_lon;
        break;
    case ConfigOpt::PlatformRspInterval:
        val = this->platform_rsp_interval_;
        break;
    case ConfigOpt::TboxRspInterval:
        val = this->tbox_rsp_interval_;
        break;
    case ConfigOpt::PublicPlatformPort:
        val = this->public_platform_port_;
        break;
    case ConfigOpt::ModeTimeQuantum:
        val = this->mode_time_quantum_;
        break;
    case ConfigOpt::TimingInterval:
        val = this->timing_interval_;
        break;
    default:
        break;
    }

    return;
}

void Config::GetConfigOption(ConfigOpt opt, uint64_t& val)
{
    switch (opt)
    {
    case ConfigOpt::RealTimeCollectType:
        val = this->real_time_collect_;
        break;
    case ConfigOpt::RealTimeUploadType:
        val = this->real_time_upload_;
        break;
    case ConfigOpt::CommonDataStorIntervalType:
        val = this->common_data_store_interval_;
        break;
    case ConfigOpt::AlarmDataStoreIntervalType:
        val = this->alarm_data_store_interval_;
        break;
    case ConfigOpt::HeartbeatTimeType:
        val = this->heartbeat_time_;
        break;
    case ConfigOpt::DeleteDataInterval:
        val = this->delete_data_interval_;
        break;
    case ConfigOpt::DataStoreTimeType:
        val = this->data_store_time_;
        break;
    case ConfigOpt::ModeBeginTime:
        val = this->mode_begin_time_;
        break;
    default:
        break;
    }

    return;
}

void Config::SetConfigOption(ConfigOpt opt, const string& val)
{
    switch (opt)
    {
    case ConfigOpt::SourcePathType:
        this->source_path_ = val;
        break;
    case ConfigOpt::ServerNameType:
        this->server_name_ = val;
        break;
    case ConfigOpt::CaCertNameType:
        this->ca_cert_name_ = val;
        break;
    case ConfigOpt::ClientCertNameType:
        this->client_cert_name_ = val;
        break;
    case ConfigOpt::ClientKeyNameType:
        this->client_key_name_ = val;
        break;
    case ConfigOpt::ClientKeyPasswd:
        this->client_key_passwd_ = val;
        break;
    case ConfigOpt::DbPathType:
        this->db_path_ = val;
        break;
    case ConfigOpt::VDCRC_LowerSystemIP:
        this->lower_system_ip = val;
        break;
    case ConfigOpt::UpgradePkgStorePath:
        this->upgrade_pkg_store_path_ = val;
        break;
    case ConfigOpt::UpgradePkgDecryptKey:
        this->upgrade_pkg_decrypt_key_ = val;
        break;
    case ConfigOpt::UpgradeCaCertName:
        this->upgrade_ca_cert_name_ = val;
        break;
    case ConfigOpt::PublicPlatformUrl:
        this->public_platform_url_ = val;
        break;
    default:
        break;
    }

    return;
}

void Config::SetConfigOption(ConfigOpt opt, uint16_t val)
{
    switch (opt)
    {
    case ConfigOpt::VDCRC_LowerSystemPort:
        this->lower_system_port = val;
        break;
    case ConfigOpt::Sampling:
        this->sampling_ = val;
        break;
    case ConfigOpt::ModeType:
        this->mode_type_ = val;
        break;
    default:
        break;
    }

    return;
}

void Config::SetConfigOption(ConfigOpt opt, uint32_t val)
{
    switch (opt)
    {
    case ConfigOpt::ServerPortType:
        this->server_port_ = val;
        break;
    case ConfigOpt::LOG_LogModule:
        this->log_module_ = val;
        break;
    case ConfigOpt::LOG_LogLevel:
        this->log_level_ = val;
        break;
    case ConfigOpt::VDCRC_GprmcBakInterval:
        this->gprmc_bak_interval = val;
        break;
    case ConfigOpt::VDCRC_GprmcBakDefaultLat:
        this->gprmc_bak_default_lat = val;
        break;
    case ConfigOpt::VDCRC_GprmcBakDefaultLon:
        this->gprmc_bak_default_lon = val;
        break;
    case ConfigOpt::PlatformRspInterval:
        this->platform_rsp_interval_ = val;
        break;
    case ConfigOpt::TboxRspInterval:
        this->tbox_rsp_interval_ = val;
        break;
    case ConfigOpt::PublicPlatformPort:
        this->public_platform_port_ = val;
        break;
    case ConfigOpt::ModeTimeQuantum:
        this->mode_time_quantum_ = val;
        break;
    case ConfigOpt::RepeatLoginIntervel:
        this->repeat_login_intervel_ = val;
        break;
    default:
        break;
    }

    return;
}

void Config::SetConfigOption(ConfigOpt opt, uint64_t val)
{
    switch (opt)
    {
    case ConfigOpt::RealTimeCollectType:
        this->real_time_collect_ = val;
        break;
    case ConfigOpt::RealTimeUploadType:
        this->real_time_upload_ = val;
        break;
    case ConfigOpt::CommonDataStorIntervalType:
        this->common_data_store_interval_ = val;
        break;
    case ConfigOpt::AlarmDataStoreIntervalType:
        this->alarm_data_store_interval_ = val;
        break;
    case ConfigOpt::HeartbeatTimeType:
        this->heartbeat_time_ = val;
        break;
    case ConfigOpt::DeleteDataInterval:
        this->delete_data_interval_ = val;
        break;
    case ConfigOpt::DataStoreTimeType:
        this->data_store_time_ = val;
        break;
    case ConfigOpt::ModeBeginTime:
        this->mode_begin_time_ = val;
        break;
    default:
        break;
    }

    return;
}

void Config::ConfigShow()
{
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "Config Show:");
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "server_name = [%s]", this->server_name_.c_str());
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "server_port = [%u]", this->server_port_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "db_path     = [%s]", this->db_path_.c_str());
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "source_path = [%s]", this->source_path_.c_str());
    TBOX_LOG_INFO(
        LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "upgrade_pkg_store_path = [%s]", this->upgrade_pkg_store_path_.c_str());
    TBOX_LOG_INFO(
        LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "upgrade_pkg_decrypt_key = [%s]", this->upgrade_pkg_decrypt_key_.c_str());
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "ca_cert_name = [%s]", this->ca_cert_name_.c_str());
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "client_cert_name = [%s]", this->client_cert_name_.c_str());
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "client_key_name = [%s]", this->client_key_name_.c_str());
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "client_key_passwd = [%s]", this->client_key_passwd_.c_str());
    TBOX_LOG_INFO(
        LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "upgrade_ca_cert_name = [%s]", this->upgrade_ca_cert_name_.c_str());
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "real_time_collect = [%u]", this->real_time_collect_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "real_time_upload = [%u]", this->real_time_upload_);
    TBOX_LOG_INFO(
        LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "common_data_store_interval = [%u]", this->common_data_store_interval_);
    TBOX_LOG_INFO(
        LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "alarm_data_store_interval = [%u]", this->alarm_data_store_interval_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "data_store_time = [%u]", this->data_store_time_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "heartbeat_time = [%u]", this->heartbeat_time_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "delete_data_interval = [%u]", this->delete_data_interval_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "log_module = [%x]", this->log_module_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "log_level = [%u]", this->log_level_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "lower_system_ip = [%s]", this->lower_system_ip.c_str());
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "lower_system_port = [%u]", this->lower_system_port);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "platform_rsp_interval = [%u]", this->platform_rsp_interval_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "tbox_rsp_interval = [%u]", this->tbox_rsp_interval_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "repeat_login_intervel = [%u]", this->repeat_login_intervel_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "sampling = [%u]", this->sampling_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "mode_type = [%u]", this->mode_type_);
    TBOX_LOG_INFO(
        LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "public_platform_url = [%s]", this->public_platform_url_.c_str());
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "public_platform_port = [%u]", this->public_platform_port_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "mode_begin_time = [%u]", this->mode_begin_time_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "mode_time_quantum = [%u]", this->mode_time_quantum_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "timing_interval = [%u]", this->timing_interval_);
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "vehicle_uid = [%s]", this->vehicle_uid_.c_str());
    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_CONFIG, "iccid = [%s]", this->iccid_.c_str());

    return;
}

}  // namespace config
}  // namespace mxnavi
#include <cstdio>
#include "common/common.h"
#include "http/http_client.hpp"

namespace mxnavi
{
namespace http
{
HttpClient::HttpClient()
{
}

HttpClient::~HttpClient()
{
}

int32_t HttpClient::HttpInit()
{
    auto ret = curl_global_init(CURL_GLOBAL_ALL);
    if (ret != CURLE_OK)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "curl_global_init failed:%s", curl_easy_strerror(ret));
        return TBOX_ERROR_RESULT_HTTP;
    }

    return TBOX_SUCCESS;
}

int32_t HttpClient::HttpRequest(shared_ptr<HttpSetOpt> opt)
{
    CURL *handle = curl_easy_init();
    if (handle == nullptr)
    {
        return TBOX_ERROR_RESULT_HTTP;
    }

    auto ret = this->HttpSetting(opt, handle);
    if (ret != TBOX_SUCCESS)
    {
        return ret;
    }

    auto curl_ret = curl_easy_perform(handle);
    if (curl_ret != CURLE_OK)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "curl_easy_perform failed:%s", curl_easy_strerror(curl_ret));
        return TBOX_ERROR_RESULT_HTTP;
    }

    curl_easy_cleanup(handle);

    return TBOX_SUCCESS;
}

void HttpClient::HttpDestory()
{
    curl_global_cleanup();
}

size_t HttpClient::RwCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    HttpSetOpt *opt = static_cast<HttpSetOpt *>(userdata);

    return opt->get_callback()->Callback(ptr, size, nmemb, static_cast<size_t>(opt->get_file_size()), static_cast<size_t>(opt->get_resume_offset()));
}

size_t HttpClient::HeaderCallback(char *buff, size_t size, size_t nitems, void *userdata)
{
    HttpSetOpt *opt = static_cast<HttpSetOpt *>(userdata);
    long len = 0;
    auto num = std::sscanf(buff, "Content-Length: %ld\n", &len);
    if (num)
    {
        opt->set_file_size(static_cast<uint64_t>(len));
    }

    return (size * nitems);
}

int HttpClient::ProgressCallback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
    HttpSetOpt *opt = static_cast<HttpSetOpt *>(clientp);

    if (0 == opt->get_opt_type())
    {
        opt->set_file_size(static_cast<uint64_t>(dltotal));
        opt->set_resume_offset(static_cast<uint64_t>(dlnow));
    }
    else
    {
        opt->set_file_size(static_cast<uint64_t>(ultotal));
        opt->set_resume_offset(static_cast<uint64_t>(ulnow));
    }

    return 0;
}

int32_t HttpClient::HttpSetting(const shared_ptr<HttpSetOpt> opt, CURL *handle)
{
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_URL, opt->get_url().c_str());
    curl_easy_setopt(handle, CURLOPT_PROGRESSDATA, static_cast<void *>(opt.get()));
    curl_easy_setopt(handle, CURLOPT_PROGRESSFUNCTION, HttpClient::ProgressCallback);
    curl_easy_setopt(handle, CURLOPT_UPLOAD, static_cast<long>(opt->get_opt_type()));
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, static_cast<long>(opt->get_conn_timeout()));
    curl_easy_setopt(handle, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(opt->get_resume_offset()));

    if (0 == opt->get_opt_type())
    {
        curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, HttpClient::HeaderCallback);
        curl_easy_setopt(handle, CURLOPT_HEADERDATA, static_cast<void *>(opt.get()));
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, HttpClient::RwCallback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, static_cast<void *>(opt.get()));
    }
    else
    {
        curl_easy_setopt(handle, CURLOPT_READFUNCTION, HttpClient::RwCallback);
        curl_easy_setopt(handle, CURLOPT_READDATA, static_cast<void *>(opt.get()));
        curl_easy_setopt(handle, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(opt->get_file_size()));
    }

    return TBOX_SUCCESS;
}

}  // namespace http
}  // namespace mxnavi
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "common/common.h"
#include "ssl/ssl_client.hpp"

namespace mxnavi
{
namespace ssl
{
SslClient::SslClient()
{
}

SslClient::~SslClient()
{
}

int32_t SslClient::SslInit(const string& server_name, uint32_t server_port)
{
    int32_t ret = TBOX_SUCCESS;

    SSLeay_add_ssl_algorithms();

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    const SSL_METHOD *ssl_meth = nullptr;
    ssl_meth                   = TLSv1_2_client_method();
    if (nullptr == ssl_meth)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "TLSv1_2_client_method failed!");
        return TBOX_ERROR_RESULT_SYSTEM;
    }

    this->ssl_ctx_ = SSL_CTX_new(ssl_meth);
    if (nullptr == this->ssl_ctx_)
    {
        ERR_print_errors_fp(stdout);
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "SSL_CTX_new failed!");

        return TBOX_ERROR_RESULT_SYSTEM;
    }

    SSL_CTX_set_verify(this->ssl_ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, 0);

    string path;
    Config::GetInstance().GetConfigOption(ConfigOpt::SourcePathType, path);
    this->ca_cert_.append(path);

    string optionname;
    Config::GetInstance().GetConfigOption(ConfigOpt::CaCertNameType, optionname);
    this->ca_cert_.append(optionname);
    if (SSL_CTX_load_verify_locations(this->ssl_ctx_, this->ca_cert_.c_str(), 0) != 1)
    {
        SSL_CTX_free(this->ssl_ctx_);
        this->ssl_ctx_ = nullptr;
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "Failed to load CA file:%s", this->ca_cert_.c_str());

        return TBOX_ERROR_RESULT_SYSTEM;
    }

    Config::GetInstance().GetConfigOption(ConfigOpt::ClientKeyPasswd, this->client_key_passwd_);
    SSL_CTX_set_default_passwd_cb_userdata(this->ssl_ctx_, (void *)this->client_key_passwd_.c_str());

    Config::GetInstance().GetConfigOption(ConfigOpt::ClientCertNameType, optionname);
    this->client_cert_.append(path);
    this->client_cert_.append(optionname);
    if (SSL_CTX_use_certificate_file(this->ssl_ctx_, this->client_cert_.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        SSL_CTX_free(this->ssl_ctx_);
        this->ssl_ctx_ = nullptr;
        ERR_print_errors_fp(stdout);
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "SSL_CTX_use_certificate_file failed");

        return TBOX_ERROR_RESULT_SYSTEM;
    }

    Config::GetInstance().GetConfigOption(ConfigOpt::ClientKeyNameType, optionname);
    this->client_key_.append(path);
    this->client_key_.append(optionname);
    if (SSL_CTX_use_PrivateKey_file(this->ssl_ctx_, this->client_key_.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        SSL_CTX_free(this->ssl_ctx_);
        this->ssl_ctx_ = nullptr;
        ERR_print_errors_fp(stdout);
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "SSL_CTX_use_PrivateKey_file failed");

        return TBOX_ERROR_RESULT_SYSTEM;
    }

    if (!SSL_CTX_check_private_key(this->ssl_ctx_))
    {
        SSL_CTX_free(this->ssl_ctx_);
        this->ssl_ctx_ = nullptr;
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "Private key does not match the certificate public key");

        return TBOX_ERROR_RESULT_SYSTEM;
    }

    SSL_CTX_set_mode(this->ssl_ctx_, SSL_MODE_AUTO_RETRY);

    struct hostent *hostent_info = gethostbyname(server_name.c_str());
    if (hostent_info == nullptr)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "gethostbyname error");
        return TBOX_ERROR_RESULT_SYSTEM;
    }

    char dst_ip[32] = {};
    if (inet_ntop(hostent_info->h_addrtype, hostent_info->h_addr, dst_ip, sizeof(dst_ip)) == nullptr)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "inet_ntop error");
        return TBOX_ERROR_RESULT_SYSTEM;
    }

    this->tcp_handle_ = make_shared<TcpClient>(string(dst_ip), server_port);
    if (!this->tcp_handle_)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "create tcp client error");

        return TBOX_ERROR_RESULT_MEMORY;
    }

    this->native_handle_ = this->tcp_handle_->Init();
    if (this->native_handle_ < 0)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "tcp init failed, ret:%d", this->native_handle_);

        return TBOX_ERROR_RESULT_SYSTEM;
    }

    if (nullptr != this->tcp_handle_)
    {
        ret = this->tcp_handle_->Connect(this->native_handle_);
        if (TBOX_SUCCESS != ret)
        {
            TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "tcp connect failed, ret:%d", ret);

            return TBOX_ERROR_RESULT_SYSTEM;
        }
    }

    if (nullptr != this->ssl_ctx_)
    {
        this->ssl_handle_ = SSL_new(this->ssl_ctx_);
        if (nullptr == this->ssl_handle_)
        {
            ERR_print_errors_fp(stdout);
            TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "SSL_new failed");
            return TBOX_ERROR_RESULT_SYSTEM;
        }
    }

    ret = SSL_set_fd(this->ssl_handle_, this->native_handle_);
    if (1 != ret)
    {
        ERR_print_errors_fp(stdout);
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "SSL_set_fd failed, ret=%d", ret);
        return TBOX_ERROR_RESULT_SYSTEM;
    }

    return TBOX_SUCCESS;
}

void SslClient::SslDestory()
{
    if (nullptr != this->ssl_handle_)
    {
        SSL_shutdown(this->ssl_handle_);
    }

    if (nullptr != this->tcp_handle_)
    {
        this->tcp_handle_->Destory(this->native_handle_);
        this->native_handle_ = -1;

        this->tcp_handle_ = nullptr;
    }

    if (nullptr != this->ssl_handle_)
    {
        SSL_free(this->ssl_handle_);
        this->ssl_handle_ = nullptr;
    }

    if (nullptr != this->ssl_ctx_)
    {
        SSL_CTX_free(this->ssl_ctx_);
        this->ssl_ctx_ = nullptr;
    }

    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "SslDestory");

    return;
}

int32_t SslClient::SslConnect()
{
    int32_t ret = TBOX_SUCCESS;

    if (nullptr != this->ssl_handle_)
    {
        ret = SSL_connect(this->ssl_handle_);
        if (ret < 0)
        {
            int32_t ret1 = SSL_get_error(this->ssl_handle_, ret);
            if ((SSL_ERROR_WANT_READ == ret1) || (SSL_ERROR_WANT_WRITE == ret1))
            {
                return TBOX_ERROR_RESULT_AGAIN;
            }

            return TBOX_ERROR_RESULT_CONNECT;
        }
        else if (1 == ret)
        {
            return TBOX_SUCCESS;
        }

        return TBOX_ERROR_RESULT_CONNECT;
    }

    return TBOX_ERROR_RESULT_CONNECT;
}

int32_t SslClient::SslGetNativeHandle()
{
    return this->native_handle_;
}

int32_t SslClient::SslRead(uint8_t *buff, size_t len)
{
    if ((nullptr == buff) || (0 >= len))
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "invaled args, len:%u", len);

        return TBOX_ERROR_RESULT_ARGS;
    }

    int32_t ret = SSL_read(this->ssl_handle_, buff, len);
    if (ret <= 0)
    {
        int32_t ret1 = SSL_get_error(this->ssl_handle_, ret);
        if ((SSL_ERROR_WANT_READ == ret1) || (SSL_ERROR_WANT_WRITE == ret1))
        {
            return TBOX_ERROR_RESULT_AGAIN;
        }

        return TBOX_ERROR_RESULT_CONNECT;
    }

    return ret;
}

int32_t SslClient::SslWrite(const uint8_t *buff, size_t len)
{
    if ((nullptr == buff) || (0 >= len))
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "invaled args, len:%u", len);

        return TBOX_ERROR_RESULT_ARGS;
    }

    int32_t ret = SSL_write(this->ssl_handle_, buff, len);
    if (ret <= 0)
    {
        int32_t ret1 = SSL_get_error(this->ssl_handle_, ret);
        if ((SSL_ERROR_WANT_READ == ret1) || (SSL_ERROR_WANT_WRITE == ret1))
        {
            return TBOX_ERROR_RESULT_AGAIN;
        }

        return TBOX_ERROR_RESULT_CONNECT;
    }

    return ret;
}

}  // namespace ssl{
}  // namespace mxnavi{
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "common/common.h"
#include "ssl/tcp_client.hpp"

namespace mxnavi
{
namespace tcp
{
TcpClient::TcpClient(const string &ip, uint16_t port) : port_(port), ip_(ip)
{
    // do nothing
}

TcpClient::~TcpClient()
{
    // do nothing
}

int32_t TcpClient::Init()
{
    int32_t ret = -1;
    int32_t sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        TBOX_LOG_ERROR(
            LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "create socket error:%s(errno:%d)", strerror(errno), errno);

        return sockfd;
    }

    int32_t ip_reuse = 1;
    ret              = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &ip_reuse, sizeof(int32_t));
    if (ret < 0)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "setsockopt error:%s(errno:%d)", strerror(errno), errno);
        return ret;
    }

    int32_t flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0)
    {
        TBOX_LOG_ERROR(
            LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "fcntl(F_GETFL) error:%s(errno:%d)", strerror(errno), errno);
        return -1;
    }

    ret = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    if (ret < 0)
    {
        TBOX_LOG_ERROR(
            LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "fcntl(F_SETFL) error:%s(errno:%d)", strerror(errno), errno);
        return ret;
    }

    return sockfd;
}

int32_t TcpClient::Connect(int32_t sockfd)
{
    int32_t ret = -1;

    if (sockfd < 0)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "invalid sockfd:%d", sockfd);
        return ret;
    }

    struct sockaddr_in socket_addr;
    memset(&socket_addr, 0, sizeof(socket_addr));

    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port   = htons(this->port_);
    ret                    = inet_pton(AF_INET, this->ip_.c_str(), &socket_addr.sin_addr);
    if (ret < 0)
    {
        TBOX_LOG_ERROR(
            LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "change ip to net error:%s(errno:%d)", strerror(errno), errno);
        return -1;
    }
    else if (0 == ret)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK,
                       "src ip format is not match for AF_INET, src_ip:%s",
                       this->ip_.c_str());
        return -1;
    }

    ret = ::connect(sockfd, (struct sockaddr *)&socket_addr, sizeof(socket_addr));
    if (0 != ret)
    {
        if (EINPROGRESS == errno)
        {
            return 0;
        }

        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK,
                       "connect error:%s(errno:%d), ip[%s], port[%u])",
                       strerror(errno),
                       errno,
                       this->ip_.c_str(),
                       this->port_);

        return errno;
    }

    return 0;
}

void TcpClient::Destory(int32_t sockfd)
{
    close(sockfd);

    TBOX_LOG_INFO(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "Destory");

    return;
}

int32_t TcpClient::Send(int32_t sockfd, const uint8_t *buff, size_t len)
{
    if ((nullptr == buff) || 0 >= len)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "send() error args");
        return 0;
    }

    return ::send(sockfd, buff, len, 0);
}

int32_t TcpClient::Recv(int32_t sockfd, uint8_t *buff, size_t len)
{
    if ((nullptr == buff) || 0 >= len)
    {
        TBOX_LOG_ERROR(LOGGER_MODULE_ID_UPPER_SYSTEM_NETWORK, "recv() error args");
        return -1;
    }

    return ::recv(sockfd, buff, len, 0);
}

}  // namespace mxnavi{
}  // namespace tcp{

#include "include/common/common.h"
#include "source/vdcrc/vdcrc.hpp"
#include "source/vdcrc/logger/vdcrc_logger.h"

VDCRC::VDCRC ( const std::shared_ptr<VDCRC_Protocol> &_protocol_lower_system,
               const std::shared_ptr<VDCRC_Protocol> &_protocol_gnss )
    : protocol_lower_system_ ( _protocol_lower_system ), protocol_gnss_ ( _protocol_gnss )
{
}

VDCRC::~VDCRC()
{
}

int32_t VDCRC::Start()
{
    if ( !protocol_lower_system_ || !protocol_gnss_ )
    {
        VDCRC_Common_Err ( "VDCRC::Start() failed, protocol_lower_system_=%p, protocol_gnss_=%p",
                           protocol_lower_system_.get(), protocol_gnss_.get() );
        return TBOX_ERROR_BASE;
    }

    protocol_gnss_->setListener ( this );
    protocol_lower_system_->setListener ( this );

    int32_t result = protocol_gnss_->Start();
    if ( TBOX_SUCCESS != result )
    {
        VDCRC_Common_Err ( "protocol_gnss_->Start() failed, result = %d", result );
        return result;
    }

    result = protocol_lower_system_->Start();
    if ( TBOX_SUCCESS != result )
    {
        VDCRC_Common_Err ( "protocol_lower_system_->Start() failed, result = %d", result );
        return result;
    }

    return precondition_();
}

int32_t VDCRC::Stop()
{
    if ( protocol_lower_system_ )
    {
        protocol_lower_system_->Stop();
        protocol_lower_system_->setListener ( nullptr );
    }

    if ( protocol_gnss_ )
    {
        protocol_gnss_->Stop();
        protocol_gnss_->setListener ( nullptr );
    }

    return TBOX_SUCCESS;
}

int32_t VDCRC::ChangeWorkingMode ( VDCRC_WorkingMode _mode )
{
    int32_t result = TBOX_ERROR_BASE;

    if ( protocol_lower_system_ )
    {
        result = protocol_lower_system_->ChangeWorkingMode ( _mode );
    }

    return result;
}

int32_t VDCRC::RegisteNotificationListener ( VDCRC_NotificationListener *_listener )
{
    if ( nullptr == _listener )
    {
        VDCRC_Common_Err ( "VDCRC::RegisteNotificationListener() failed, nullptr == _listener" );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Common_Debug ( "VDCRC::RegisteNotificationListener( %p )", _listener );

    std::lock_guard<std::mutex> guard ( notification_listener_set_mutex_ );

    notification_listener_set_.insert ( _listener );

    return TBOX_SUCCESS;
}

int32_t VDCRC::UnRegisteNotificationListener ( VDCRC_NotificationListener *_listener )
{
    VDCRC_Common_Debug ( "VDCRC::UnRegisteNotificationListener( %p )", _listener );

    std::lock_guard<std::mutex> guard ( notification_listener_set_mutex_ );

    notification_listener_set_.erase ( _listener );

    return TBOX_SUCCESS;
}

int32_t VDCRC::RegisteResponseListener ( VDCRC_ResponseListener *_listener )
{
    if ( nullptr == _listener )
    {
        VDCRC_Common_Err ( "VDCRC::RegisteResponseListener() failed, nullptr == _listener" );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Common_Debug ( "VDCRC::RegisteResponseListener( %p )", _listener );

    std::lock_guard<std::mutex> guard ( response_listener_set_mutex_ );

    response_listener_set_.insert ( _listener );

    return TBOX_SUCCESS;
}

int32_t VDCRC::UnRegisteResponseListener ( VDCRC_ResponseListener *_listener )
{
    VDCRC_Common_Debug ( "VDCRC::UnRegisteResponseListener( %p )", _listener );

    std::lock_guard<std::mutex> guard ( response_listener_set_mutex_ );

    response_listener_set_.erase ( _listener );

    return TBOX_SUCCESS;
}

std::shared_ptr<const VDCRC_StateMessage> VDCRC::GetStateMessage ( VDCRC_StateMessageType _msg_type )
{
    std::shared_ptr<const VDCRC_StateMessage> state_msg;

    std::lock_guard<std::mutex> guard ( state_message_mutex_ );

    StateMessageContainer::iterator iter = state_message_set_.find ( _msg_type );
    if ( iter != state_message_set_.end() )
    {
        state_msg = iter->second;
    }

    return state_msg;
}

std::shared_ptr<const VDCRC_Response_AskForVersion> VDCRC::AskForVersion ( VDCRC_Response_AskForVersionType _type )
{
    std::shared_ptr<const VDCRC_Response_AskForVersion> ask_for_version;

    std::lock_guard<std::mutex> guard ( ask_for_version_mutex_ );

    AskForVersionContainer::iterator iter = ask_for_version_set_.find ( _type );
    if ( iter != ask_for_version_set_.end() )
    {
        ask_for_version = iter->second;
    }

    return ask_for_version;
}

int32_t VDCRC::SendRequest ( const VDCRC_Request *_request )
{
    int32_t result = TBOX_ERROR_BASE;

    if ( protocol_lower_system_ )
    {
        result = protocol_lower_system_->SendRequest ( _request );
    }

    return result;
}

void VDCRC::onNotificationReceived ( const std::vector<std::shared_ptr<VDCRC_Notification>> &_notification_set )
{
    for ( const auto &notification : _notification_set )
    {
        if ( !notification )
        {
            VDCRC_Common_Warn ( "onNotificationReceived(), !notification." );
            continue;
        }

        VDCRC_NotificationType notification_type = notification->notification_type();

        switch ( notification_type )
        {
        case VDCRC_NotificationType::StateMessage:
            onNotificationReceived_StateMessage ( std::dynamic_pointer_cast<VDCRC_StateMessage> ( notification ) );
            break;

        case VDCRC_NotificationType::AlarmMessage:
            onNotificationReceived_AlarmMessage ( std::dynamic_pointer_cast<VDCRC_AlarmMessage> ( notification ) );
            break;

        default:
            onNotificationReceived ( notification );
            break;
        }
    }
}

void VDCRC::onNotificationReceived_StateMessage ( const std::shared_ptr<VDCRC_StateMessage> &_state_message )
{
    if ( !_state_message )
    {
        VDCRC_Common_Err ( "onNotificationReceived_StateMessage(), !_state_message." );
        return;
    }

    // store
    {
        std::lock_guard<std::mutex> guard ( state_message_mutex_ );
        state_message_set_[_state_message->state_message_type()] = _state_message;
    }

    // notify
    onNotificationReceived ( _state_message );
}

void VDCRC::onNotificationReceived_AlarmMessage ( const std::shared_ptr<VDCRC_AlarmMessage> &_alarm_message )
{
    if ( !_alarm_message )
    {
        VDCRC_Common_Err ( "onNotificationReceived_AlarmMessage(), !_alarm_message." );
        return;
    }

    std::shared_ptr<VDCRC_AlarmMessage> old_alarm_message;

    // store
    {
        VDCRC_AlarmMessageType alarm_message_type = _alarm_message->alarm_message_type();

        std::lock_guard<std::mutex> guard ( alarm_message_mutex_ );

        old_alarm_message = alarm_message_set_[alarm_message_type];

        alarm_message_set_[alarm_message_type] = _alarm_message;
    }

    // notify
    if ( _alarm_message->is_need_notify ( old_alarm_message.get() ) )
    {
        onNotificationReceived ( _alarm_message );
    }
}

void VDCRC::onNotificationReceived ( const std::shared_ptr<VDCRC_Notification> &_notification )
{
    if ( !_notification )
    {
        VDCRC_Common_Err ( "onNotificationReceived(), !_notification." );
        return;
    }

    std::lock_guard<std::mutex> guard ( notification_listener_set_mutex_ );

    for ( auto listener : notification_listener_set_ )
    {
        if ( ( listener != nullptr ) && listener->blend_with ( _notification->notification_type() ) )
        {
            listener->onNotificationReceived ( _notification );
        }
    }
}

void VDCRC::onResponseReceived ( const std::vector<std::shared_ptr<VDCRC_Response>> &_response_set )
{
    for ( const auto &response : _response_set )
    {
        if ( !response )
        {
            VDCRC_Common_Warn ( "VDCRC::onResponseReceived(), !response." );
            continue;
        }

        VDCRC_ResponseType response_type = response->response_type();

        switch ( response_type )
        {
        case VDCRC_ResponseType::AskForVersion:
            onResponseReceived_AskForVersion ( std::dynamic_pointer_cast<VDCRC_Response_AskForVersion> ( response ) );
            break;

        default:
            onResponseReceived ( response );
            break;
        }
    }
}

void VDCRC::onResponseReceived_AskForVersion ( const std::shared_ptr<VDCRC_Response_AskForVersion> &_ask_for_version )
{
    if ( !_ask_for_version )
    {
        VDCRC_Common_Err ( "onResponseReceived_AskForVersion(), !_ask_for_version." );
        return;
    }

    // store
    {
        std::lock_guard<std::mutex> guard ( ask_for_version_mutex_ );
        ask_for_version_set_[_ask_for_version->ask_for_version_type()] = _ask_for_version;
    }

    // notify
    onResponseReceived ( _ask_for_version );
}

void VDCRC::onResponseReceived ( const std::shared_ptr<VDCRC_Response> &_response )
{
    if ( !_response )
    {
        VDCRC_Common_Err ( "onResponseReceived(), !_response." );
        return;
    }

    std::lock_guard<std::mutex> guard ( response_listener_set_mutex_ );

    for ( auto listener : response_listener_set_ )
    {
        if ( ( listener != nullptr ) && listener->blend_with ( _response->response_type() ) )
        {
            listener->onResponseReceived ( _response );
        }
    }
}

// TODO - duanjj - 下面的代码写的很不舒服，后续需要考虑如何修改
#include "include/vdcrc/message/request/ask_for_version/ask_for_version_lower_sys.hpp"
#include "include/vdcrc/message/response/ask_for_version/response_ask_for_version_upper_sys_software.hpp"
#include "include/vdcrc/message/response/ask_for_version/response_ask_for_version_upper_sys_firmware.hpp"
#include "include/vdcrc/message/response/ask_for_version/response_ask_for_version_tbox_hardware.hpp"
#include "version.h"
#include <unistd.h>

int32_t VDCRC::precondition_ ( void )
{
    int32_t retval = TBOX_ERROR_BASE;

    for ( ;; )
    {
        // 请求下位机固件版本
        {
            if ( !protocol_lower_system_ )
            {
                VDCRC_Common_Err ( "VDCRC::precondition_() failed: !protocol_lower_system_." );
                retval = TBOX_ERROR_BASE;
                break;
            }

            // 当为PC版时，使用的是异步的TCP连接，这里等待1秒，保证TCP建立连接成功后再发送请求
#if MODULE_LINUX_PC
            sleep ( 1 );
#endif
            VDCRC_Request_AskForVersion_LowerSys ask_for_version_lower_sys;
            retval = protocol_lower_system_->SendRequest ( &ask_for_version_lower_sys );
            if ( retval != TBOX_SUCCESS )
            {
                VDCRC_Common_Err ( "protocol_lower_system_->SendRequest() failed: return %d.", retval );
                break;
            }
        }

        // 上位机软件版本
        {
            char version[32] = {0};
            snprintf ( version, sizeof ( version ), "%s%06d%02d%02d%02d%02d%d",
                       PRODUCT_NAME, PRODUCT_ID,
                       UPPERSYS_SOFTWARE_VERSION_MAJOR, UPPERSYS_SOFTWARE_VERSION_MIDDLE,
                       UPPERSYS_SOFTWARE_VERSION_MINOR, UPPERSYS_SOFTWARE_VERSION_MODIFY,
                       SUB_APP_UPPERSYS_SOFTWARE_ID );

            std::shared_ptr<VDCRC_Response_AskForVersion_UpperSysSoftware> ask_for_version;
            ask_for_version = std::make_shared<VDCRC_Response_AskForVersion_UpperSysSoftware> ( version );
            if ( !ask_for_version )
            {
                VDCRC_Common_Err ( "VDCRC::precondition_() failed: !ask_for_version." );
                retval = TBOX_ERROR_BASE;
                break;
            }

            std::lock_guard<std::mutex> guard ( ask_for_version_mutex_ );
            ask_for_version_set_[ask_for_version->ask_for_version_type()] = ask_for_version;
        }

        // 上位机固件版本
        // TODO - duanjj - 和庆鹏确认一下上位机固件版本如何获取
        {
            const char *version = "T501001010101002";

            std::shared_ptr<VDCRC_Response_AskForVersion_UpperSysFirmware> ask_for_version;
            ask_for_version = std::make_shared<VDCRC_Response_AskForVersion_UpperSysFirmware> ( version );
            if ( !ask_for_version )
            {
                VDCRC_Common_Err ( "VDCRC::precondition_() failed: !ask_for_version." );
                retval = TBOX_ERROR_BASE;
                break;
            }

            std::lock_guard<std::mutex> guard ( ask_for_version_mutex_ );
            ask_for_version_set_[ask_for_version->ask_for_version_type()] = ask_for_version;
        }

        // TBox硬件版本
        // TODO - duanjj - 和庆鹏确认一下硬件版本如何获取，同时下位机也需要使用硬件版本
        {
            const char *version = "T501001010101001";

            std::shared_ptr<VDCRC_Response_AskForVersion_TBoxHardware> ask_for_version;
            ask_for_version = std::make_shared<VDCRC_Response_AskForVersion_TBoxHardware> ( version );
            if ( !ask_for_version )
            {
                VDCRC_Common_Err ( "VDCRC::precondition_() failed: !ask_for_version." );
                retval = TBOX_ERROR_BASE;
                break;
            }

            std::lock_guard<std::mutex> guard ( ask_for_version_mutex_ );
            ask_for_version_set_[ask_for_version->ask_for_version_type()] = ask_for_version;
        }

        retval = TBOX_SUCCESS;
        break;
    }

    return retval;
}

#include "source/vdcrc/protocol/source/gnss/gprmc_parser/include/gprmc_format.hpp"
#include "source/vdcrc/logger/vdcrc_logger.h"

VDCRC_GprmcFormat::VDCRC_GprmcFormat()
{
    raw_time_ = -1;
    status_ = 'V';
    lat_ = 0;
    lat_direct_ = 'N';
    lon_ = 0;
    lon_direct_ = 'E';
    speed_ = 0;
    cog_ = 0;
}

VDCRC_GprmcFormat::VDCRC_GprmcFormat ( const VDCRC_GprmcFormat &_other )
{
    raw_time_ = _other.raw_time_;
    status_ = _other.status_;
    lat_ = _other.lat_;
    lat_direct_ = _other.lat_direct_;
    lon_ = _other.lon_;
    lon_direct_ = _other.lon_direct_;
    speed_ = _other.speed_;
    cog_ = _other.cog_;

    field_set_ = _other.field_set_;
}

VDCRC_GprmcFormat::~VDCRC_GprmcFormat()
{
}

VDCRC_GprmcFormat &VDCRC_GprmcFormat::operator= ( const VDCRC_GprmcFormat &_other )
{
    if ( this == &_other )
    {
        return *this;
    }

    raw_time_ = _other.raw_time_;
    status_ = _other.status_;
    lat_ = _other.lat_;
    lat_direct_ = _other.lat_direct_;
    lon_ = _other.lon_;
    lon_direct_ = _other.lon_direct_;
    speed_ = _other.speed_;
    cog_ = _other.cog_;

    field_set_ = _other.field_set_;

    return *this;
}

bool VDCRC_GprmcFormat::operator== ( const VDCRC_GprmcFormat &_other ) const
{
    if ( field_set_ != _other.field_set_ )
    {
        return false;
    }

    if ( field_set_.test ( VDCRC_GprmcFormat_FieldType::RawTime ) )
    {
        if ( raw_time_ != _other.raw_time_ )
        {
            return false;
        }
    }

    if ( field_set_.test ( VDCRC_GprmcFormat_FieldType::Status ) )
    {
        if ( status_ != _other.status_ )
        {
            return false;
        }
    }

    if ( field_set_.test ( VDCRC_GprmcFormat_FieldType::Lat ) )
    {
        if ( lat_ != _other.lat_ )
        {
            return false;
        }
    }

    if ( field_set_.test ( VDCRC_GprmcFormat_FieldType::LatDirect ) )
    {
        if ( lat_direct_ != _other.lat_direct_ )
        {
            return false;
        }
    }

    if ( field_set_.test ( VDCRC_GprmcFormat_FieldType::Lon ) )
    {
        if ( lon_ != _other.lon_ )
        {
            return false;
        }
    }

    if ( field_set_.test ( VDCRC_GprmcFormat_FieldType::LonDirect ) )
    {
        if ( lon_direct_ != _other.lon_direct_ )
        {
            return false;
        }
    }

    if ( field_set_.test ( VDCRC_GprmcFormat_FieldType::Speed ) )
    {
        if ( speed_ != _other.speed_ )
        {
            return false;
        }
    }

    if ( field_set_.test ( VDCRC_GprmcFormat_FieldType::COG ) )
    {
        if ( cog_ != _other.cog_ )
        {
            return false;
        }
    }

    return true;
}

void VDCRC_GprmcFormat::reset ( void )
{
    field_set_.reset();
}

// 时间
bool VDCRC_GprmcFormat::test_raw_time ( void ) const
{
    return field_set_.test ( VDCRC_GprmcFormat_FieldType::RawTime );
}

time_t VDCRC_GprmcFormat::raw_time ( void ) const
{
    return raw_time_;
}

int32_t VDCRC_GprmcFormat::set_raw_time ( time_t _raw_time )
{
    raw_time_ = _raw_time;
    field_set_.set ( VDCRC_GprmcFormat_FieldType::RawTime );

    return TBOX_SUCCESS;
}

// 状态
bool VDCRC_GprmcFormat::test_status ( void ) const
{
    return field_set_.test ( VDCRC_GprmcFormat_FieldType::Status );
}

char VDCRC_GprmcFormat::status ( void ) const
{
    return status_;
}

int32_t VDCRC_GprmcFormat::set_status ( char _status )
{
    if ( ( _status != 'A' ) && ( _status != 'V' ) )
    {
        VDCRC_Protocol_GNSS_Err ( "VDCRC_GprmcFormat::set_status() failed: INVALID _status(%c).", _status );
        return TBOX_ERROR_BASE;
    }

    status_ = _status;
    field_set_.set ( VDCRC_GprmcFormat_FieldType::Status );

    return TBOX_SUCCESS;
}

// 纬度
bool VDCRC_GprmcFormat::test_lat ( void ) const
{
    return field_set_.test ( VDCRC_GprmcFormat_FieldType::Lat );
}

uint32_t VDCRC_GprmcFormat::lat ( void ) const
{
    return lat_;
}

int32_t VDCRC_GprmcFormat::set_lat ( uint32_t _lat )
{
    lat_ = _lat;
    field_set_.set ( VDCRC_GprmcFormat_FieldType::Lat );

    return TBOX_SUCCESS;
}

// 纬度半球
bool VDCRC_GprmcFormat::test_lat_direct ( void ) const
{
    return field_set_.test ( VDCRC_GprmcFormat_FieldType::LatDirect );
}

char VDCRC_GprmcFormat::lat_direct ( void ) const
{
    return lat_direct_;
}

int32_t VDCRC_GprmcFormat::set_lat_direct ( char _lat_direct )
{
    if ( ( _lat_direct != 'N' ) && ( _lat_direct != 'S' ) )
    {
        VDCRC_Protocol_GNSS_Err ( "VDCRC_GprmcFormat::set_lat_direct() failed: INVALID _lat_direct(%c).", _lat_direct );
        return TBOX_ERROR_BASE;
    }

    lat_direct_ = _lat_direct;
    field_set_.set ( VDCRC_GprmcFormat_FieldType::LatDirect );

    return TBOX_SUCCESS;
}

// 经度
bool VDCRC_GprmcFormat::test_lon ( void ) const
{
    return field_set_.test ( VDCRC_GprmcFormat_FieldType::Lon );
}

uint32_t VDCRC_GprmcFormat::lon ( void ) const
{
    return lon_;
}

int32_t VDCRC_GprmcFormat::set_lon ( uint32_t _lon )
{
    lon_ = _lon;
    field_set_.set ( VDCRC_GprmcFormat_FieldType::Lon );

    return TBOX_SUCCESS;
}

// 经度半球
bool VDCRC_GprmcFormat::test_lon_direct ( void ) const
{
    return field_set_.test ( VDCRC_GprmcFormat_FieldType::LonDirect );
}

char VDCRC_GprmcFormat::lon_direct ( void ) const
{
    return lon_direct_;
}

int32_t VDCRC_GprmcFormat::set_lon_direct ( char _lon_direct )
{
    if ( ( _lon_direct != 'E' ) && ( _lon_direct != 'W' ) )
    {
        VDCRC_Protocol_GNSS_Err ( "VDCRC_GprmcFormat::set_lon_direct() failed: INVALID _lon_direct(%c).", _lon_direct );
        return TBOX_ERROR_BASE;
    }

    lon_direct_ = _lon_direct;
    field_set_.set ( VDCRC_GprmcFormat_FieldType::LonDirect );

    return TBOX_SUCCESS;
}

// 速度
bool VDCRC_GprmcFormat::test_speed ( void ) const
{
    return field_set_.test ( VDCRC_GprmcFormat_FieldType::Speed );
}

uint16_t VDCRC_GprmcFormat::speed ( void ) const
{
    return speed_;
}

int32_t VDCRC_GprmcFormat::set_speed ( uint16_t _speed )
{
    speed_ = _speed;
    field_set_.set ( VDCRC_GprmcFormat_FieldType::Speed );

    return TBOX_SUCCESS;
}

// 方位角
bool VDCRC_GprmcFormat::test_cog ( void ) const
{
    return field_set_.test ( VDCRC_GprmcFormat_FieldType::COG );
}

uint16_t VDCRC_GprmcFormat::cog ( void ) const
{
    return cog_;
}

int32_t VDCRC_GprmcFormat::set_cog ( uint16_t _cog )
{
    if ( ( _cog < 0 ) || ( 3599 < _cog ) )
    {
        VDCRC_Protocol_GNSS_Err ( "VDCRC_GprmcFormat::set_cog() failed: INVALID _cog(%u).", _cog );
        return TBOX_ERROR_BASE;
    }

    cog_ = _cog;
    field_set_.set ( VDCRC_GprmcFormat_FieldType::COG );

    return TBOX_SUCCESS;
}

#include <string.h>

#include "source/vdcrc/protocol/source/lower_system/down_stream_frame_package.hpp"
#include "source/vdcrc/logger/vdcrc_logger.h"
#include "source/vdcrc/protocol/source/lower_system/down_stream_data_format.h"

#include "include/vdcrc/message/request/ask_for_version/ask_for_version_lower_sys.hpp"

#include "include/vdcrc/message/request/ctrl_message/ctrl_message_air_temperature.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_air.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_chair.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_charge.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_doors.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_ekey.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_electromotor.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_engine.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_light_horn.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_private_information.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_sunroof.hpp"
#include "include/vdcrc/message/request/ctrl_message/ctrl_message_trunk.hpp"

#include "include/vdcrc/message/request/lower_system_upgrade/lower_system_upgrade_notify.hpp"
#include "include/vdcrc/message/request/lower_system_upgrade/lower_system_upgrade_send_line.hpp"

static bool read_1_Bit ( void *_dest, size_t _byte_offset, uint8_t _bit_offset )
{
    return ( ( ( ( uint8_t * ) _dest ) [_byte_offset] & ( 1 << _bit_offset ) ) != 0 );
}

static void write_1_Bit ( void *_dest, size_t _byte_offset, uint8_t _bit_offset, bool _value )
{
    if ( _value )
    {
        ( ( uint8_t * ) _dest ) [_byte_offset] |= ( 1 << _bit_offset );
    }
    else
    {
        ( ( uint8_t * ) _dest ) [_byte_offset] &= ~ ( 1 << _bit_offset );
    }
}

static void *write_bytes ( void *_dest, size_t _byte_offset, const void *_src, size_t _count )
{
    return memcpy ( ( uint8_t * ) _dest + _byte_offset, _src, _count );
}

static void *write_1_Byte ( void *_dest, size_t _byte_offset, uint8_t _value )
{
    return write_bytes ( _dest, _byte_offset, &_value, sizeof ( uint8_t ) );
}

static void *write_2_Byte ( void *_dest, size_t _byte_offset, uint16_t _value )
{
    return write_bytes ( _dest, _byte_offset, &_value, sizeof ( uint16_t ) );
}

static void VDCRC_DownStream_FramePackage_Header ( void *_header, uint16_t _body_length )
{
    // flag1
    write_1_Byte ( _header, DOWN_STREAM_HEADER_FLAG1_BYTE_OFFSET, DOWN_STREAM_HEADER_FLAG1_VALUE );
    // flag2
    write_1_Byte ( _header, DOWN_STREAM_HEADER_FLAG2_BYTE_OFFSET, DOWN_STREAM_HEADER_FLAG2_VALUE );
    // body length
    write_2_Byte ( _header, DOWN_STREAM_HEADER_BODY_LENGTH_BYTE_OFFSET, _body_length );
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_LightHorn ( const VDCRC_CtrlMessage_LightHorn *_light_horn, size_t &_frame_length )
{
    if ( nullptr == _light_horn )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_LightHorn() failed, nullptr == _light_horn" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + REMOTE_VEHICLE_TRACKING_CTRL_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_LightHorn() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, REMOTE_VEHICLE_TRACKING_CTRL_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;

    // command id
    write_2_Byte ( body, REMOTE_VEHICLE_TRACKING_CTRL_CMD_ID_BYTE_OFFSET, REMOTE_VEHICLE_TRACKING_CTRL_CMD_ID_VALUE );

    // Remote Vehicle Tracking
    if ( _light_horn->ctrl_entry_valid ( VDCRC_CtrlMessage_LightHorn_CtrlEntry::RemoteVehicleTracking ) && _light_horn->unchecked_switch_state ( VDCRC_CtrlMessage_LightHorn_CtrlEntry::RemoteVehicleTracking ) )
    {
        write_1_Bit ( body, REMOTE_VEHICLE_TRACKING_CTRL_TRACKING_ENTRY_BYTE_OFFSET, REMOTE_VEHICLE_TRACKING_CTRL_TRACKING_ENTRY_BIT_OFFSET, true );
        write_1_Bit ( body, REMOTE_VEHICLE_TRACKING_CTRL_TRACKING_SWITCH_BYTE_OFFSET, REMOTE_VEHICLE_TRACKING_CTRL_TRACKING_SWITCH_BIT_OFFSET, true );
    }
    else
    {
        write_1_Bit ( body, REMOTE_VEHICLE_TRACKING_CTRL_TRACKING_ENTRY_BYTE_OFFSET, REMOTE_VEHICLE_TRACKING_CTRL_TRACKING_ENTRY_BIT_OFFSET, false );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_Engine ( const VDCRC_CtrlMessage_Engine *_engine, size_t &_frame_length )
{
    if ( nullptr == _engine )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Engine() failed, nullptr == _engine" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + ENGINE_CTRL_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Engine() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, ENGINE_CTRL_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;

    // command id
    write_2_Byte ( body, ENGINE_CTRL_CMD_ID_BYTE_OFFSET, ENGINE_CTRL_CMD_ID_VALUE );

    // 远程启停发动机控制
    if ( _engine->ctrl_entry_valid ( VDCRC_CtrlMessage_Engine_CtrlEntry::EngineControlSS ) )
    {
        write_1_Bit ( body, ENGINE_CTRL_ENGINE_CTRL_SS_ENTRY_BYTE_OFFSET, ENGINE_CTRL_ENGINE_CTRL_SS_ENTRY_BIT_OFFSET, true );

        if ( _engine->unchecked_switch_state ( VDCRC_CtrlMessage_Engine_CtrlEntry::EngineControlSS ) )
        {
            write_1_Bit ( body, ENGINE_CTRL_ENGINE_CTRL_SS_SWITCH_BYTE_OFFSET, ENGINE_CTRL_ENGINE_CTRL_SS_SWITCH_BIT_OFFSET, true );
        }
        else
        {
            write_1_Bit ( body, ENGINE_CTRL_ENGINE_CTRL_SS_SWITCH_BYTE_OFFSET, ENGINE_CTRL_ENGINE_CTRL_SS_SWITCH_BIT_OFFSET, false );
        }
    }
    else
    {
        write_1_Bit ( body, ENGINE_CTRL_ENGINE_CTRL_SS_ENTRY_BYTE_OFFSET, ENGINE_CTRL_ENGINE_CTRL_SS_ENTRY_BIT_OFFSET, false );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_Charge ( const VDCRC_CtrlMessage_Charge *_charge, size_t &_frame_length )
{
    if ( nullptr == _charge )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Charge() failed, nullptr == _charge" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + CHARGE_CTRL_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Charge() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, CHARGE_CTRL_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;

    // command id
    write_2_Byte ( body, CHARGE_CTRL_CMD_ID_BYTE_OFFSET, CHARGE_CTRL_CMD_ID_VALUE );

    // 远程充电命令
    if ( _charge->ctrl_entry_valid ( VDCRC_CtrlMessage_Charge_CtrlEntry::RemoteCharge ) )
    {
        write_1_Bit ( body, CHARGE_CTRL_REMOTE_CHARGE_ENTRY_BYTE_OFFSET, CHARGE_CTRL_REMOTE_CHARGE_ENTRY_BIT_OFFSET, true );

        if ( _charge->unchecked_switch_state ( VDCRC_CtrlMessage_Charge_CtrlEntry::RemoteCharge ) )
        {
            write_1_Bit ( body, CHARGE_CTRL_REMOTE_CHARGE_SWITCH_BYTE_OFFSET, CHARGE_CTRL_REMOTE_CHARGE_SWITCH_BIT_OFFSET, true );
        }
        else
        {
            write_1_Bit ( body, CHARGE_CTRL_REMOTE_CHARGE_SWITCH_BYTE_OFFSET, CHARGE_CTRL_REMOTE_CHARGE_SWITCH_BIT_OFFSET, false );
        }
    }
    else
    {
        write_1_Bit ( body, CHARGE_CTRL_REMOTE_CHARGE_ENTRY_BYTE_OFFSET, CHARGE_CTRL_REMOTE_CHARGE_ENTRY_BIT_OFFSET, false );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_Ekey ( const VDCRC_CtrlMessage_Ekey *_ekey, size_t &_frame_length )
{
    if ( nullptr == _ekey )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Ekey() failed, nullptr == _ekey" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + EKEY_CTRL_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Ekey() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, EKEY_CTRL_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;

    // command id
    write_2_Byte ( body, EKEY_CTRL_CMD_ID_BYTE_OFFSET, EKEY_CTRL_CMD_ID_VALUE );

    // 远程使能功能允许/禁止（解除防盗）
    if ( _ekey->ctrl_entry_valid ( VDCRC_CtrlMessage_Ekey_CtrlEntry::EngineCtrlEDReq ) )
    {
        write_1_Bit ( body, EKEY_CTRL_ENGINE_CTRL_ED_REQ_ENTRY_BYTE_OFFSET, EKEY_CTRL_ENGINE_CTRL_ED_REQ_ENTRY_BIT_OFFSET, true );

        if ( _ekey->unchecked_switch_state ( VDCRC_CtrlMessage_Ekey_CtrlEntry::EngineCtrlEDReq ) )
        {
            write_1_Bit ( body, EKEY_CTRL_ENGINE_CTRL_ED_REQ_SWITCH_BYTE_OFFSET, EKEY_CTRL_ENGINE_CTRL_ED_REQ_SWITCH_BIT_OFFSET, true );
        }
        else
        {
            write_1_Bit ( body, EKEY_CTRL_ENGINE_CTRL_ED_REQ_SWITCH_BYTE_OFFSET, EKEY_CTRL_ENGINE_CTRL_ED_REQ_SWITCH_BIT_OFFSET, false );
        }
    }
    else
    {
        write_1_Bit ( body, EKEY_CTRL_ENGINE_CTRL_ED_REQ_ENTRY_BYTE_OFFSET, EKEY_CTRL_ENGINE_CTRL_ED_REQ_ENTRY_BIT_OFFSET, false );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_Air ( const VDCRC_CtrlMessage_Air *_air, size_t &_frame_length )
{
    if ( nullptr == _air )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Air() failed, nullptr == _air" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + AIR_CTRL_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Air() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, AIR_CTRL_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;

    // command id
    write_2_Byte ( body, AIR_CTRL_CMD_ID_BYTE_OFFSET, AIR_CTRL_CMD_ID_VALUE );
    // 空调控制类型赋默认值
    write_1_Byte ( body, AIR_CTRL_CTRL_TYPE_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_VALUE_NO_CONTROL );
    // 一键除霜, 0x1-ON;
    if ( !read_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET ) && _air->ctrl_entry_valid ( VDCRC_CtrlMessage_Air_CtrlEntry::Defrost ) && _air->unchecked_switch_state ( VDCRC_CtrlMessage_Air_CtrlEntry::Defrost ) )
    {
        write_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET, true );
        write_1_Byte ( body, AIR_CTRL_CTRL_TYPE_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_VALUE_DEFROST );
    }
    // 最大制冷, 0x1-ON;
    if ( !read_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET ) && _air->ctrl_entry_valid ( VDCRC_CtrlMessage_Air_CtrlEntry::ACMax ) && _air->unchecked_switch_state ( VDCRC_CtrlMessage_Air_CtrlEntry::ACMax ) )
    {
        write_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET, true );
        write_1_Byte ( body, AIR_CTRL_CTRL_TYPE_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_VALUE_AC_MAX );
    }
    // 最大制热, 0x1-ON;
    if ( !read_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET ) && _air->ctrl_entry_valid ( VDCRC_CtrlMessage_Air_CtrlEntry::HotMax ) && _air->unchecked_switch_state ( VDCRC_CtrlMessage_Air_CtrlEntry::HotMax ) )
    {
        write_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET, true );
        write_1_Byte ( body, AIR_CTRL_CTRL_TYPE_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_VALUE_HOT_MAX );
    }
    // 负离子, 0x1-ON;
    if ( !read_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET ) && _air->ctrl_entry_valid ( VDCRC_CtrlMessage_Air_CtrlEntry::CleanAir ) && _air->unchecked_switch_state ( VDCRC_CtrlMessage_Air_CtrlEntry::CleanAir ) )
    {
        write_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET, true );
        write_1_Byte ( body, AIR_CTRL_CTRL_TYPE_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_VALUE_CLEAN_AIR );
    }
    // 座舱清洁, 0x1-ON;
    if ( !read_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET ) && _air->ctrl_entry_valid ( VDCRC_CtrlMessage_Air_CtrlEntry::CleanCar ) && _air->unchecked_switch_state ( VDCRC_CtrlMessage_Air_CtrlEntry::CleanCar ) )
    {
        write_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET, true );
        write_1_Byte ( body, AIR_CTRL_CTRL_TYPE_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_VALUE_CLEAN_CAR );
    }
    // 全自动, 0x1-ON;
    if ( !read_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET ) && _air->ctrl_entry_valid ( VDCRC_CtrlMessage_Air_CtrlEntry::Auto ) && _air->unchecked_switch_state ( VDCRC_CtrlMessage_Air_CtrlEntry::Auto ) )
    {
        write_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET, true );
        write_1_Byte ( body, AIR_CTRL_CTRL_TYPE_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_VALUE_AUTO );
    }
    // 空气净化, 0x0-OFF; 0x1-ON;
    if ( !read_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET ) && _air->ctrl_entry_valid ( VDCRC_CtrlMessage_Air_CtrlEntry::RemotePurify ) )
    {
        write_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET, true );

        if ( _air->unchecked_switch_state ( VDCRC_CtrlMessage_Air_CtrlEntry::RemotePurify ) )
        {
            write_1_Byte ( body, AIR_CTRL_CTRL_TYPE_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_VALUE_REMOTE_PURIFY_ON );
        }
        else
        {
            write_1_Byte ( body, AIR_CTRL_CTRL_TYPE_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_VALUE_REMOTE_PURIFY_OFF );
        }
    }
    // AC开关, 0x0-OFF;
    if ( !read_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET ) && _air->ctrl_entry_valid ( VDCRC_CtrlMessage_Air_CtrlEntry::ACSwitch ) && !_air->unchecked_switch_state ( VDCRC_CtrlMessage_Air_CtrlEntry::ACSwitch ) )
    {
        write_1_Bit ( body, AIR_CTRL_CTRL_TYPE_ENTRY_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_ENTRY_BIT_OFFSET, true );
        write_1_Byte ( body, AIR_CTRL_CTRL_TYPE_BYTE_OFFSET, AIR_CTRL_CTRL_TYPE_VALUE_AC_OFF );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_Electromotor ( const VDCRC_CtrlMessage_Electromotor *_electromotor, size_t &_frame_length )
{
    if ( nullptr == _electromotor )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Electromotor() failed, nullptr == _electromotor" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + ELECTROMOTOR_CTRL_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Electromotor() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, ELECTROMOTOR_CTRL_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;

    // command id
    write_2_Byte ( body, ELECTROMOTOR_CTRL_CMD_ID_BYTE_OFFSET, ELECTROMOTOR_CTRL_CMD_ID_VALUE );

    // 远程上/下电
    if ( _electromotor->ctrl_entry_valid ( VDCRC_CtrlMessage_Electromotor_CtrlEntry::PowerControlReq ) )
    {
        write_1_Bit ( body, ELECTROMOTOR_CTRL_POWER_CTRL_REQ_ENTRY_BYTE_OFFSET, ELECTROMOTOR_CTRL_POWER_CTRL_REQ_ENTRY_BIT_OFFSET, true );

        if ( _electromotor->unchecked_switch_state ( VDCRC_CtrlMessage_Electromotor_CtrlEntry::PowerControlReq ) )
        {
            write_1_Bit ( body, ELECTROMOTOR_CTRL_POWER_CTRL_REQ_SWITCH_BYTE_OFFSET, ELECTROMOTOR_CTRL_POWER_CTRL_REQ_SWITCH_BIT_OFFSET, true );
        }
        else
        {
            write_1_Bit ( body, ELECTROMOTOR_CTRL_POWER_CTRL_REQ_SWITCH_BYTE_OFFSET, ELECTROMOTOR_CTRL_POWER_CTRL_REQ_SWITCH_BIT_OFFSET, false );
        }
    }
    else
    {
        write_1_Bit ( body, ELECTROMOTOR_CTRL_POWER_CTRL_REQ_ENTRY_BYTE_OFFSET, ELECTROMOTOR_CTRL_POWER_CTRL_REQ_ENTRY_BIT_OFFSET, false );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_Doors ( const VDCRC_CtrlMessage_Doors *_doors, size_t &_frame_length )
{
    if ( nullptr == _doors )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Doors() failed, nullptr == _doors" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + REMOTE_CTRL_LOCK_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Doors() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, REMOTE_CTRL_LOCK_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;
    // command id
    write_2_Byte ( body, REMOTE_CTRL_LOCK_CMD_ID_BYTE_OFFSET, REMOTE_CTRL_LOCK_CMD_ID_VALUE );
    // remote lock
    if ( ( _doors->ctrl_entry_valid ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_LF ) ) || ( _doors->ctrl_entry_valid ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_LR ) ) || ( _doors->ctrl_entry_valid ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_RF ) ) || ( _doors->ctrl_entry_valid ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_RR ) ) )
    {
        write_1_Bit ( body, REMOTE_CTRL_LOCK_CTRL_TYPE_ENTRY_BYTE_OFFSET, REMOTE_CTRL_LOCK_CTRL_TYPE_ENTRY_BIT_OFFSET, true );

        if ( ( _doors->ctrl_entry_valid ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_LF ) && _doors->unchecked_switch_state ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_LF ) ) || ( _doors->ctrl_entry_valid ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_LR ) && _doors->unchecked_switch_state ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_LR ) ) || ( _doors->ctrl_entry_valid ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_RF ) && _doors->unchecked_switch_state ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_RF ) ) || ( _doors->ctrl_entry_valid ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_RR ) && _doors->unchecked_switch_state ( VDCRC_CtrlMessage_Doors_CtrlEntry::RemoteDoorLock_RR ) ) )
        {
            write_1_Byte ( body, REMOTE_CTRL_LOCK_CTRL_TYPE_BYTE_OFFSET, REMOTE_CTRL_LOCK_CTRL_TYPE_VALUE_LOCK );
        }
        else
        {
            write_1_Byte ( body, REMOTE_CTRL_LOCK_CTRL_TYPE_BYTE_OFFSET, REMOTE_CTRL_LOCK_CTRL_TYPE_VALUE_UNLOCK );
        }
    }
    else
    {
        write_1_Bit ( body, REMOTE_CTRL_LOCK_CTRL_TYPE_ENTRY_BYTE_OFFSET, REMOTE_CTRL_LOCK_CTRL_TYPE_ENTRY_BIT_OFFSET, false );
        write_1_Byte ( body, REMOTE_CTRL_LOCK_CTRL_TYPE_BYTE_OFFSET, REMOTE_CTRL_LOCK_CTRL_TYPE_VALUE_NO_CONTROL );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_Chair ( const VDCRC_CtrlMessage_Chair *_chair, size_t &_frame_length )
{
    if ( nullptr == _chair )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Chair() failed, nullptr == _chair" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + CHAIR_CTRL_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Chair() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, CHAIR_CTRL_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;
    // command id
    write_2_Byte ( body, CHAIR_CTRL_CMD_ID_BYTE_OFFSET, CHAIR_CTRL_CMD_ID_VALUE );
    // 座椅加热
    if ( _chair->ctrl_entry_valid ( VDCRC_CtrlMessage_Chair_CtrlEntry::SeatHot ) )
    {
        write_1_Bit ( body, CHAIR_CTRL_SEAT_HOT_ENTRY_BYTE_OFFSET, CHAIR_CTRL_SEAT_HOT_ENTRY_BIT_OFFSET, true );

        if ( _chair->unchecked_switch_state ( VDCRC_CtrlMessage_Chair_CtrlEntry::SeatHot ) )
        {
            write_1_Bit ( body, CHAIR_CTRL_SEAT_HOT_SWITCH_BYTE_OFFSET, CHAIR_CTRL_SEAT_HOT_SWITCH_BIT_OFFSET, true );
        }
        else
        {
            write_1_Bit ( body, CHAIR_CTRL_SEAT_HOT_SWITCH_BYTE_OFFSET, CHAIR_CTRL_SEAT_HOT_SWITCH_BIT_OFFSET, false );
        }
    }
    else
    {
        write_1_Bit ( body, CHAIR_CTRL_SEAT_HOT_ENTRY_BYTE_OFFSET, CHAIR_CTRL_SEAT_HOT_ENTRY_BIT_OFFSET, false );
    }
    // 座椅通风
    if ( _chair->ctrl_entry_valid ( VDCRC_CtrlMessage_Chair_CtrlEntry::SeatVentilation ) )
    {
        write_1_Bit ( body, CHAIR_CTRL_SEAT_VENTILATION_ENTRY_BYTE_OFFSET, CHAIR_CTRL_SEAT_VENTILATION_ENTRY_BIT_OFFSET, true );

        if ( _chair->unchecked_switch_state ( VDCRC_CtrlMessage_Chair_CtrlEntry::SeatVentilation ) )
        {
            write_1_Bit ( body, CHAIR_CTRL_SEAT_VENTILATION_SWITCH_BYTE_OFFSET, CHAIR_CTRL_SEAT_VENTILATION_SWITCH_BIT_OFFSET, true );
        }
        else
        {
            write_1_Bit ( body, CHAIR_CTRL_SEAT_VENTILATION_SWITCH_BYTE_OFFSET, CHAIR_CTRL_SEAT_VENTILATION_SWITCH_BIT_OFFSET, false );
        }
    }
    else
    {
        write_1_Bit ( body, CHAIR_CTRL_SEAT_VENTILATION_ENTRY_BYTE_OFFSET, CHAIR_CTRL_SEAT_VENTILATION_ENTRY_BIT_OFFSET, false );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_PrivateInformation ( const VDCRC_CtrlMessage_PrivateInformation *_private_info, size_t &_frame_length )
{
    if ( nullptr == _private_info )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_PrivateInformation() failed, nullptr == _private_info" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + PRIVATE_INFO_CTRL_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_PrivateInformation() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, PRIVATE_INFO_CTRL_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;
    // command id
    write_2_Byte ( body, PRIVATE_INFO_CTRL_CMD_ID_BYTE_OFFSET, PRIVATE_INFO_CTRL_CMD_ID_VALUE );

    // 远程中控屏保护
    if ( _private_info->ctrl_entry_valid ( VDCRC_CtrlMessage_PrivateInformation_CtrlEntry::DVDPrivateInfoLockReq ) )
    {
        write_1_Bit ( body, PRIVATE_INFO_CTRL_DVD_PRIVATE_INFO_LOCK_REQ_ENTRY_BYTE_OFFSET, PRIVATE_INFO_CTRL_DVD_PRIVATE_INFO_LOCK_REQ_ENTRY_BIT_OFFSET, true );

        if ( _private_info->unchecked_switch_state ( VDCRC_CtrlMessage_PrivateInformation_CtrlEntry::DVDPrivateInfoLockReq ) )
        {
            write_1_Bit ( body, PRIVATE_INFO_CTRL_DVD_PRIVATE_INFO_LOCK_REQ_SWITCH_BYTE_OFFSET, PRIVATE_INFO_CTRL_DVD_PRIVATE_INFO_LOCK_REQ_SWITCH_BIT_OFFSET, true );
        }
        else
        {
            write_1_Bit ( body, PRIVATE_INFO_CTRL_DVD_PRIVATE_INFO_LOCK_REQ_SWITCH_BYTE_OFFSET, PRIVATE_INFO_CTRL_DVD_PRIVATE_INFO_LOCK_REQ_SWITCH_BIT_OFFSET, false );
        }
    }
    else
    {
        write_1_Bit ( body, PRIVATE_INFO_CTRL_DVD_PRIVATE_INFO_LOCK_REQ_ENTRY_BYTE_OFFSET, PRIVATE_INFO_CTRL_DVD_PRIVATE_INFO_LOCK_REQ_ENTRY_BIT_OFFSET, false );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_Sunroof ( const VDCRC_CtrlMessage_Sunroof *_sunroof, size_t &_frame_length )
{
    if ( nullptr == _sunroof )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Sunroof() failed, nullptr == _sunroof" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + SUNROOF_CTRL_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Sunroof() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, SUNROOF_CTRL_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;
    // command id
    write_2_Byte ( body, SUNROOF_CTRL_CMD_ID_BYTE_OFFSET, SUNROOF_CTRL_CMD_ID_VALUE );

    // 远程天窗开关
    if ( _sunroof->ctrl_entry_valid ( VDCRC_CtrlMessage_Sunroof_CtrlEntry::OpenClose ) )
    {
        write_1_Bit ( body, SUNROOF_CTRL_SUNROOF_OPEN_CLOSE_ENTRY_BYTE_OFFSET, SUNROOF_CTRL_SUNROOF_OPEN_CLOSE_ENTRY_BIT_OFFSET, true );

        if ( _sunroof->unchecked_switch_state ( VDCRC_CtrlMessage_Sunroof_CtrlEntry::OpenClose ) )
        {
            write_1_Bit ( body, SUNROOF_CTRL_SUNROOF_OPEN_CLOSE_SWITCH_BYTE_OFFSET, SUNROOF_CTRL_SUNROOF_OPEN_CLOSE_SWITCH_BIT_OFFSET, true );
        }
        else
        {
            write_1_Bit ( body, SUNROOF_CTRL_SUNROOF_OPEN_CLOSE_SWITCH_BYTE_OFFSET, SUNROOF_CTRL_SUNROOF_OPEN_CLOSE_SWITCH_BIT_OFFSET, false );
        }
    }
    else
    {
        write_1_Bit ( body, SUNROOF_CTRL_SUNROOF_OPEN_CLOSE_ENTRY_BYTE_OFFSET, SUNROOF_CTRL_SUNROOF_OPEN_CLOSE_ENTRY_BIT_OFFSET, false );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_AirTemperature ( const VDCRC_CtrlMessage_AirTemperature *_air_temperature, size_t &_frame_length )
{
    if ( nullptr == _air_temperature )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_AirTemperature() failed, nullptr == _air_temperature" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + AIR_TEMPERATURE_CTRL_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_AirTemperature() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, AIR_TEMPERATURE_CTRL_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;
    // command id
    write_2_Byte ( body, AIR_TEMPERATURE_CTRL_CMD_ID_BYTE_OFFSET, AIR_TEMPERATURE_CTRL_CMD_ID_VALUE );
    // 空调温度
    uint16_t temperature = _air_temperature->temperature();
    if ( 0xFFFF == temperature )
    {
        write_1_Byte ( body, AIR_TEMPERATURE_CTRL_TEMPERATURE_BYTE_OFFSET, 0xFF );
    }
    else
    {
        write_1_Byte ( body, AIR_TEMPERATURE_CTRL_TEMPERATURE_BYTE_OFFSET, ( uint8_t ) ( temperature / 5 ) ); // 单位转换：0.1度 -> 0.5度
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage_Trunk ( const VDCRC_CtrlMessage_Trunk *_trunk, size_t &_frame_length )
{
    if ( nullptr == _trunk )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Trunk() failed, nullptr == _trunk" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + REMOTE_CTRL_LOCK_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage_Trunk() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, REMOTE_CTRL_LOCK_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;
    // command id
    write_2_Byte ( body, REMOTE_CTRL_LOCK_CMD_ID_BYTE_OFFSET, REMOTE_CTRL_LOCK_CMD_ID_VALUE );

    // 后备箱开启
    if ( _trunk->ctrl_entry_valid ( VDCRC_CtrlMessage_Trunk_CtrlEntry::RemoteOpenTrunk ) && _trunk->unchecked_switch_state ( VDCRC_CtrlMessage_Trunk_CtrlEntry::RemoteOpenTrunk ) )
    {
        write_1_Bit ( body, REMOTE_CTRL_LOCK_CTRL_TYPE_ENTRY_BYTE_OFFSET, REMOTE_CTRL_LOCK_CTRL_TYPE_ENTRY_BIT_OFFSET, true );
        write_1_Byte ( body, REMOTE_CTRL_LOCK_CTRL_TYPE_BYTE_OFFSET, REMOTE_CTRL_LOCK_CTRL_TYPE_VALUE_OPEN_TRUNK );
    }
    else
    {
        write_1_Bit ( body, REMOTE_CTRL_LOCK_CTRL_TYPE_ENTRY_BYTE_OFFSET, REMOTE_CTRL_LOCK_CTRL_TYPE_ENTRY_BIT_OFFSET, false );
        write_1_Byte ( body, REMOTE_CTRL_LOCK_CTRL_TYPE_BYTE_OFFSET, REMOTE_CTRL_LOCK_CTRL_TYPE_VALUE_NO_CONTROL );
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_CtrlMessage ( const VDCRC_CtrlMessage *_ctrl_message, size_t &_frame_length )
{
    if ( nullptr == _ctrl_message )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage() failed, nullptr == _ctrl_message" );
        return nullptr;
    }

    if ( !_ctrl_message->is_valid() )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage() failed, !_ctrl_message->is_valid()" );
        return nullptr;
    }

    const void *frame_buffer = nullptr;

    VDCRC_CtrlMessageType ctrl_message_type = _ctrl_message->ctrl_message_type();
    VDCRC_Protocol_LowerSystem_Debug ( "VDCRC_DownStream_FramePackage_CtrlMessage(), ctrl_message_type = %d", ctrl_message_type );

    switch ( ctrl_message_type )
    {
    // 寻车控制
    case VDCRC_CtrlMessageType::LightHorn:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_LightHorn ( dynamic_cast<const VDCRC_CtrlMessage_LightHorn *> ( _ctrl_message ), _frame_length );
        break;

    // 发动机控制
    case VDCRC_CtrlMessageType::Engine:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_Engine ( dynamic_cast<const VDCRC_CtrlMessage_Engine *> ( _ctrl_message ), _frame_length );
        break;

    // 充电启动/停止
    case VDCRC_CtrlMessageType::Charge:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_Charge ( dynamic_cast<const VDCRC_CtrlMessage_Charge *> ( _ctrl_message ), _frame_length );
        break;

    // 电子钥匙控制
    case VDCRC_CtrlMessageType::Ekey:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_Ekey ( dynamic_cast<const VDCRC_CtrlMessage_Ekey *> ( _ctrl_message ), _frame_length );
        break;

    // 空调控制
    case VDCRC_CtrlMessageType::Air:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_Air ( dynamic_cast<const VDCRC_CtrlMessage_Air *> ( _ctrl_message ), _frame_length );
        break;

    // 电动机控制
    case VDCRC_CtrlMessageType::Electromotor:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_Electromotor ( dynamic_cast<const VDCRC_CtrlMessage_Electromotor *> ( _ctrl_message ), _frame_length );
        break;

    // 车门控制
    case VDCRC_CtrlMessageType::Doors:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_Doors ( dynamic_cast<const VDCRC_CtrlMessage_Doors *> ( _ctrl_message ), _frame_length );
        break;

    // 座椅控制
    case VDCRC_CtrlMessageType::Chair:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_Chair ( dynamic_cast<const VDCRC_CtrlMessage_Chair *> ( _ctrl_message ), _frame_length );
        break;

    // 中控屏保护
    case VDCRC_CtrlMessageType::PrivateInformation:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_PrivateInformation ( dynamic_cast<const VDCRC_CtrlMessage_PrivateInformation *> ( _ctrl_message ), _frame_length );
        break;

    // 天窗控制
    case VDCRC_CtrlMessageType::Sunroof:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_Sunroof ( dynamic_cast<const VDCRC_CtrlMessage_Sunroof *> ( _ctrl_message ), _frame_length );
        break;

    // 空调温度设定
    case VDCRC_CtrlMessageType::AirTemperature:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_AirTemperature ( dynamic_cast<const VDCRC_CtrlMessage_AirTemperature *> ( _ctrl_message ), _frame_length );
        break;

    // 后备箱控制
    case VDCRC_CtrlMessageType::Trunk:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage_Trunk ( dynamic_cast<const VDCRC_CtrlMessage_Trunk *> ( _ctrl_message ), _frame_length );
        break;

    default:
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_CtrlMessage() failed, unknown ctrl_message_type(%d).", ctrl_message_type );
        break;
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_LowerSystemUpgrade_Notify ( const VDCRC_Request_LowerSystemUpgrade_Nofity *_notify, size_t &_frame_length )
{
    if ( nullptr == _notify )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_LowerSystemUpgrade_Notify() failed, nullptr == _notify" );
        return nullptr;
    }

    const char *firmware_version = _notify->firmware_version();
    if ( nullptr == firmware_version )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_LowerSystemUpgrade_Notify() failed, nullptr == firmware_version" );
        return nullptr;
    }

    size_t firmware_version_byte_count = LOWER_SYSTEM_UPGRADE_NOTIFY_FIRMWARE_VERSION_BYTE_COUNT ( firmware_version );

    uint16_t body_length = LOWER_SYSTEM_UPGRADE_NOTIFY_BYTE_COUNT ( firmware_version_byte_count );

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + body_length;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_LowerSystemUpgrade_Notify() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, body_length );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;
    // command id
    write_2_Byte ( body, LOWER_SYSTEM_UPGRADE_NOTIFY_CMD_ID_BYTE_OFFSET, LOWER_SYSTEM_UPGRADE_NOTIFY_CMD_ID_VALUE );
    // upgrade flag
    write_1_Byte ( body, LOWER_SYSTEM_UPGRADE_NOTIFY_UPGRADE_FLAG_BYTE_OFFSET, LOWER_SYSTEM_UPGRADE_NOTIFY_UPGRADE_FLAG_VALUE );
    // firmware version
    write_bytes ( body, LOWER_SYSTEM_UPGRADE_NOTIFY_FIRMWARE_VERSION_BYTE_OFFSET, firmware_version, firmware_version_byte_count );

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_LowerSystemUpgrade_SendLine ( const VDCRC_Request_LowerSystemUpgrade_SendLine *_send_line, size_t &_frame_length )
{
    if ( nullptr == _send_line )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_LowerSystemUpgrade_SendLine() failed, nullptr == _send_line" );
        return nullptr;
    }

    const char *s19_line = _send_line->s19_line();
    if ( nullptr == s19_line )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_LowerSystemUpgrade_SendLine() failed, nullptr == s19_line" );
        return nullptr;
    }

    size_t s19_line_byte_count = LOWER_SYSTEM_UPGRADE_SEND_LINE_S19_LINE_BYTE_COUNT ( s19_line );
    if ( s19_line_byte_count <= 0 )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_LowerSystemUpgrade_SendLine() failed, s19_line_byte_count <= 0" );
        return nullptr;
    }

    uint16_t body_length = LOWER_SYSTEM_UPGRADE_SEND_LINE_BYTE_COUNT ( s19_line_byte_count );

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + body_length;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_LowerSystemUpgrade_SendLine() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, body_length );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;
    // command id
    write_2_Byte ( body, LOWER_SYSTEM_UPGRADE_SEND_LINE_CMD_ID_BYTE_OFFSET, LOWER_SYSTEM_UPGRADE_SEND_LINE_CMD_ID_VALUE );
    // s19 line
    write_bytes ( body, LOWER_SYSTEM_UPGRADE_SEND_LINE_S19_LINE_BYTE_OFFSET, s19_line, s19_line_byte_count );

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_AskForVersion_LowerSys ( const VDCRC_Request_AskForVersion_LowerSys *_lower_sys, size_t &_frame_length )
{
    if ( nullptr == _lower_sys )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_AskForVersion_LowerSys() failed, nullptr == _lower_sys" );
        return nullptr;
    }

    _frame_length = DOWN_STREAM_HEADER_BYTE_COUNT + ASK_FOR_VERSION_LOWER_SYS_BYTE_COUNT;

    void *frame_buffer = malloc ( _frame_length );
    if ( nullptr == frame_buffer )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_AskForVersion_LowerSys() failed, nullptr == frame_buffer" );
        return nullptr;
    }

    memset ( frame_buffer, 0x00, _frame_length );

    void *header = frame_buffer;
    VDCRC_DownStream_FramePackage_Header ( header, ASK_FOR_VERSION_LOWER_SYS_BYTE_COUNT );

    void *body = ( uint8_t * ) frame_buffer + DOWN_STREAM_HEADER_BYTE_COUNT;

    // command id
    write_2_Byte ( body, ASK_FOR_VERSION_LOWER_SYS_CMD_ID_BYTE_OFFSET, ASK_FOR_VERSION_LOWER_SYS_CMD_ID_VALUE );

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_LowerSystemUpgrade ( const VDCRC_Request_LowerSystemUpgrade *_lower_system_upgrade, size_t &_frame_length )
{
    if ( nullptr == _lower_system_upgrade )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_LowerSystemUpgrade() failed, nullptr == _lower_system_upgrade." );
        return nullptr;
    }

    const void *frame_buffer = nullptr;

    VDCRC_Request_LowerSystemUpgradeType lower_system_upgrade_type = _lower_system_upgrade->lower_system_upgrade_type();
    VDCRC_Protocol_LowerSystem_Debug ( "VDCRC_DownStream_FramePackage_LowerSystemUpgrade(), lower_system_upgrade_type = %d", lower_system_upgrade_type );

    switch ( lower_system_upgrade_type )
    {
    // 通知下位机有新的固件版本
    case VDCRC_Request_LowerSystemUpgradeType::Notify:
        frame_buffer = VDCRC_DownStream_FramePackage_LowerSystemUpgrade_Notify ( dynamic_cast<const VDCRC_Request_LowerSystemUpgrade_Nofity *> ( _lower_system_upgrade ), _frame_length );
        break;

    // 发送S19文件的一行给下位机，不包括'\r'、\n'和'\0'
    case VDCRC_Request_LowerSystemUpgradeType::SendLine:
        frame_buffer = VDCRC_DownStream_FramePackage_LowerSystemUpgrade_SendLine ( dynamic_cast<const VDCRC_Request_LowerSystemUpgrade_SendLine *> ( _lower_system_upgrade ), _frame_length );
        break;

    default:
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_LowerSystemUpgrade() failed, unknown lower_system_upgrade_type(%d).", lower_system_upgrade_type );
        break;
    }

    return frame_buffer;
}

static const void *VDCRC_DownStream_FramePackage_AskForVersion ( const VDCRC_Request_AskForVersion *_ask_for_version, size_t &_frame_length )
{
    if ( nullptr == _ask_for_version )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_AskForVersion() failed, nullptr == _ask_for_version." );
        return nullptr;
    }

    const void *frame_buffer = nullptr;

    VDCRC_Request_AskForVersionType ask_for_version_type = _ask_for_version->ask_for_version_type();
    VDCRC_Protocol_LowerSystem_Debug ( "VDCRC_DownStream_FramePackage_AskForVersion(), ask_for_version_type = %d", ask_for_version_type );

    switch ( ask_for_version_type )
    {
    // 请求下位机版本
    case VDCRC_Request_AskForVersionType::LowerSys:
        frame_buffer = VDCRC_DownStream_FramePackage_AskForVersion_LowerSys ( dynamic_cast<const VDCRC_Request_AskForVersion_LowerSys *> ( _ask_for_version ), _frame_length );
        break;

    default:
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage_AskForVersion() failed, unknown ask_for_version_type(%d).", ask_for_version_type );
        break;
    }

    return frame_buffer;
}

const void *VDCRC_DownStream_FramePackage ( const VDCRC_Request *_request, size_t &_frame_length )
{
    if ( nullptr == _request )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage() failed, nullptr == _request" );
        return nullptr;
    }

    const void *frame_buffer = nullptr;

    VDCRC_RequestType request_type = _request->request_type();
    VDCRC_Protocol_LowerSystem_Debug ( "VDCRC_DownStream_FramePackage(), request_type = %d", request_type );

    switch ( request_type )
    {
    case VDCRC_RequestType::CtrlMessage:
        frame_buffer = VDCRC_DownStream_FramePackage_CtrlMessage ( dynamic_cast<const VDCRC_CtrlMessage *> ( _request ), _frame_length );
        break;

    case VDCRC_RequestType::LowerSystemUpgrade:
        frame_buffer = VDCRC_DownStream_FramePackage_LowerSystemUpgrade ( dynamic_cast<const VDCRC_Request_LowerSystemUpgrade *> ( _request ), _frame_length );
        break;

    case VDCRC_RequestType::AskForVersion:
        frame_buffer = VDCRC_DownStream_FramePackage_AskForVersion ( dynamic_cast<const VDCRC_Request_AskForVersion *> ( _request ), _frame_length );
        break;

    default:
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_DownStream_FramePackage() failed, unknown request_type(%d).", request_type );
        break;
    }

    return frame_buffer;
}

void VDCRC_DownStream_FrameDestory ( const void *_buf )
{
    free ( ( void * ) _buf );
}

#include <string.h>

#include "source/vdcrc/protocol/source/lower_system/up_stream_body_parser.hpp"
#include "source/vdcrc/logger/vdcrc_logger.h"
#include "source/vdcrc/protocol/source/lower_system/up_stream_data_format.h"

#include "include/vdcrc/message/notification/state_message/state_message_air_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_alarm_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_chargeable_dev_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_doors_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_ekey_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_engine_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_extremum_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_fuel_bat_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_lighting_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_motor_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_private_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_sunroof_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_vehicle_info.hpp"
#include "include/vdcrc/message/notification/state_message/state_message_windows_info.hpp"

#include "include/vdcrc/message/notification/alarm_message/alarm_message_crash.hpp"

#include "include/vdcrc/message/notification/lower_system_upgrade/lower_system_upgrade_request.hpp"

#include "include/vdcrc/message/notification/power_management/notification_power_management_sleep.hpp"

#include "include/vdcrc/message/response/ask_for_version/response_ask_for_version_lower_sys.hpp"

#include "include/vdcrc/message/response/lower_system_upgrade/response_lower_system_upgrade_ack.hpp"
#include "include/vdcrc/message/response/lower_system_upgrade/response_lower_system_upgrade_retry.hpp"

static void *read_bytes ( void *_dest, const void *_src, size_t _offset, size_t _count )
{
    return memcpy ( _dest, ( const uint8_t * ) _src + _offset, _count );
}

static std::shared_ptr<VDCRC_VehicleInfoMessage> VDCRC_UpStream_BodyParse_StateMessage_VehicleInfo ( const void *_data )
{
    VDCRC_VehicleInfo_t vehicle_info;
    memset ( &vehicle_info, 0x00, sizeof ( vehicle_info ) );

    // 车辆状态
    read_bytes ( &vehicle_info.car_status, _data, CAR_STATUS, CAR_STATUS_LENGTH );
    // 充电状态
    read_bytes ( &vehicle_info.charging_status, _data, CHARGING_STATUS, CHARGING_STATUS_LENGTH );
    // 运行模式
    read_bytes ( &vehicle_info.run_mode, _data, RUNNING_MODE, RUNNING_MODE_LENGTH );
    // 车速
    read_bytes ( &vehicle_info.speed, _data, SPEED, SPEED_LENGTH );
    // 累计里程
    read_bytes ( &vehicle_info.odo, _data, ODO, ODO_LENGTH );
    // 续航里程
    read_bytes ( &vehicle_info.recharge, _data, RECHARGE, RECHARGE_LENGTH );
    // 总电压
    read_bytes ( &vehicle_info.total_vol, _data, TOTAL_VOL, TOTAL_VOL_LENGTH );
    // 总电流
    read_bytes ( &vehicle_info.total_cur, _data, TOTAL_CUR, TOTAL_CUR_LENGTH );
    // SOC
    read_bytes ( &vehicle_info.soc, _data, SOC, SOC_LENGTH );
    // 平均电耗
    read_bytes ( &vehicle_info.avg_ele_consume, _data, AVG_ELE_CONSUME, AVG_ELE_CONSUME_LENGTH );
    // DC/DC状态
    read_bytes ( &vehicle_info.dc_status, _data, DCDC_STATUS, DCDC_STATUS_LENGTH );
    // 挡位
    read_bytes ( &vehicle_info.shift, _data, SHIFT, SHIFT_LENGTH );
    // 绝缘电阻
    read_bytes ( &vehicle_info.insulation_resistance, _data, INSULATION_RESISTANCE, INSULATION_RESISTANCE_LENGTH );
    // 加速踏板位置有效值
    read_bytes ( &vehicle_info.acceleratror_pedal_pos_valid, _data, ACCELERATROR_PEDAL_POS_VALID, ACCELERATROR_PEDAL_POS_VALID_LENGTH );
    // 加速踏板位置
    read_bytes ( &vehicle_info.acceleratror_pedal_pos, _data, ACCELERATROR_PEDAL_POS, ACCELERATROR_PEDAL_POS_LENGTH );
    // 制动踏板位置有效性
    read_bytes ( &vehicle_info.brake_pedal_pos_valid, _data, BRAKE_PEDAL_POS_VALID, BRAKE_PEDAL_POS_VALID_LENGTH );
    // 制动踏板位置
    read_bytes ( &vehicle_info.brake_pedal_pos, _data, BRAKE_PEDAL_POS, BRAKE_PEDAL_POS_LENGTH );
    // 制动踏板状态有效值
    read_bytes ( &vehicle_info.brake_pedal_status_valid, _data, BRAKE_PEDAL_STATUS_VALID, BRAKE_PEDAL_STATUS_VALID_LENGTH );
    // 制动踏板状态
    read_bytes ( &vehicle_info.brake_pedal_status, _data, BRAKE_PEDAL_STATUS, BRAKE_PEDAL_STATUS_LENGTH );

    return std::make_shared<VDCRC_VehicleInfoMessage> ( vehicle_info );
}

static std::shared_ptr<VDCRC_StateMessage_MotorInfo> VDCRC_UpStream_BodyParse_StateMessage_MotorInfo ( const void *_data )
{
    VDCRC_StateMessage_MotorInfo_t motor_info;
    memset ( &motor_info, 0x00, sizeof ( motor_info ) );

    // 驱动电机个数，当前个数为1
    read_bytes ( &motor_info.num, _data, MOTOR_NUM, MOTOR_NUM_LENGTH );
    motor_info.num = std::min ( ( size_t ) motor_info.num, ARRAY_MAX ( motor_info.motors ) );
    // 驱动电机状态
    read_bytes ( &motor_info.motors[0].status, _data, MOTOR_STATUS, MOTOR_STATUS_LENGTH );
    // 驱动电机控制器温度
    read_bytes ( &motor_info.motors[0].mcu_tem, _data, MOTOR_MCU_TEM, MOTOR_MCU_TEM_LENGTH );
    // 驱动电机转速
    read_bytes ( &motor_info.motors[0].rotate_speed, _data, MOTOR_SPEED, MOTOR_SPEED_LENGTH );
    // 驱动电机转矩
    read_bytes ( &motor_info.motors[0].torque, _data, MOTOR_TORQUE, MOTOR_TORQUE_LENGTH );
    // 驱动电机温度
    read_bytes ( &motor_info.motors[0].tem, _data, MOTOR_TEM, MOTOR_TEM_LENGTH );
    // 驱动电机控制器电压
    read_bytes ( &motor_info.motors[0].mcu_vol, _data, MOTOR_MCU_VOL, MOTOR_MCU_VOL_LENGTH );
    // 驱动电机控制器母线电流
    read_bytes ( &motor_info.motors[0].mcu_cur, _data, MOTOR_MCU_CUR, MOTOR_MCU_CUR_LENGTH );

    return std::make_shared<VDCRC_StateMessage_MotorInfo> ( motor_info );
}

static std::shared_ptr<VDCRC_StateMessage_FuelBatInfo> VDCRC_UpStream_BodyParse_StateMessage_FuelBatInfo ( const void *_data )
{
    VDCRC_StateMessage_FuelBatInfo_t fuel_bat_info;
    memset ( &fuel_bat_info, 0x00, sizeof ( fuel_bat_info ) );

    // 燃料电池电压
    read_bytes ( &fuel_bat_info.vol, _data, FUEL_BAT_VOL, FUEL_BAT_VOL_LENGTH );
    // 燃料电池电流
    read_bytes ( &fuel_bat_info.cur, _data, FUEL_BAT_CUR, FUEL_BAT_CUR_LENGTH );
    // 燃料消耗率
    read_bytes ( &fuel_bat_info.consume_rate, _data, FUEL_BAT_CONSUME_RATE, FUEL_BAT_CONSUME_RATE_LENGTH );
    // 燃料电池温度探针个数，当前个数为0
    read_bytes ( &fuel_bat_info.tem_prob_num, _data, FUEL_BAT_TEM_PROB_NUM, FUEL_BAT_TEM_PROB_NUM_LENGTH );
    fuel_bat_info.tem_prob_num = std::min ( ( size_t ) fuel_bat_info.tem_prob_num, ARRAY_MAX ( fuel_bat_info.prob_tem ) );
    // 燃料电池探针温度值，当前个数为0
    for ( uint16_t index = 0; index < fuel_bat_info.tem_prob_num; ++index )
    {
        // fuel_bat_info.prob_tem[index] = read_1_Byte( _data, xxxxxx + index );
    }
    // 氢系统中最高温度
    read_bytes ( &fuel_bat_info.argon_sys_max_tem, _data, ARGON_SYS_MAX_TEM, ARGON_SYS_MAX_TEM_LENGTH );
    // 氢系统中最高温度探针序号
    read_bytes ( &fuel_bat_info.argon_sys_max_tem_prob_no, _data, ARGON_SYS_MAX_TEM_PROB_NO, ARGON_SYS_MAX_TEM_PROB_NO_LENGTH );
    // 氢气最高浓度
    read_bytes ( &fuel_bat_info.hydrogen_max_density, _data, HYDROGEN_MAX_DENSITY, HYDROGEN_MAX_DENSITY_LENGTH );
    // 氢气最高浓度传感器代号
    read_bytes ( &fuel_bat_info.hydrogen_max_density_sensor_no, _data, HYDROGEN_MAX_DENSITY_SENSOR_NO, HYDROGEN_MAX_DENSITY_SENSOR_NO_LENGTH );
    // 氢气最高压力
    read_bytes ( &fuel_bat_info.hydrogen_max_pressure, _data, HYDROGEN_MAX_PRESSURE, HYDROGEN_MAX_PRESSURE_LENGTH );
    // 氢气最高压力传感器代号
    read_bytes ( &fuel_bat_info.hydrogen_max_pressure_sensor_no, _data, HYDROGEN_MAX_PRESSURE_SENSOR_NO, HYDROGEN_MAX_PRESSURE_SENSOR_NO_LENGTH );
    // 高压DC/DC状态
    read_bytes ( &fuel_bat_info.high_pressure_dc_status, _data, HIGH_PRESSURE_DCDC_STATUS, HIGH_PRESSURE_DCDC_STATUS_LENGTH );

    return std::make_shared<VDCRC_StateMessage_FuelBatInfo> ( fuel_bat_info );
}

static std::shared_ptr<VDCRC_StateMessage_EngineInfo> VDCRC_UpStream_BodyParse_StateMessage_EngineInfo ( const void *_data )
{
    VDCRC_StateMessage_EngineInfo_t engine_info;
    memset ( &engine_info, 0x00, sizeof ( engine_info ) );

    // 发动机状态
    read_bytes ( &engine_info.status, _data, ENGINE_STATUS, ENGINE_STATUS_LENGTH );
    // 曲轴转速
    read_bytes ( &engine_info.crankshaft_speed, _data, CRANKSHAFT_SPEED, CRANKSHAFT_SPEED_LENGTH );
    // 燃料消耗率
    read_bytes ( &engine_info.fuel_consume_rate, _data, FUEL_CONSUME_RATE, FUEL_CONSUME_RATE_LENGTH );

    return std::make_shared<VDCRC_StateMessage_EngineInfo> ( engine_info );
}

static std::shared_ptr<VDCRC_StateMessage_ExtremumInfo> VDCRC_UpStream_BodyParse_StateMessage_ExtremumInfo ( const void *_data )
{
    VDCRC_StateMessage_ExtremumInfo_t extremum_info;
    memset ( &extremum_info, 0x00, sizeof ( extremum_info ) );

    // 最高电压电池单体序号
    read_bytes ( &extremum_info.highest_vol_cell_no, _data, HIGHEST_VOL_CELL_NO, HIGHEST_VOL_CELL_NO_LENGTH );
    // 电池单体电压最高值
    read_bytes ( &extremum_info.highest_cell_vol, _data, HIGHEST_CELL_VOL, HIGHEST_CELL_VOL_LENGTH );
    // 最低电压电池单体序号
    read_bytes ( &extremum_info.lowest_vol_cell_no, _data, LOWEST_VOL_CELL_NO, LOWEST_VOL_CELL_NO_LENGTH );
    // 电池单体电压最低值
    read_bytes ( &extremum_info.lowest_cell_vol, _data, LOWEST_CELL_VOL, LOWEST_CELL_VOL_LENGTH );
    // 最高温度探针序号
    read_bytes ( &extremum_info.highest_tem_probe_no, _data, HIGHEST_TEM_PROBE_NO, HIGHEST_TEM_PROBE_NO_LENGTH );
    // 最高温度值
    read_bytes ( &extremum_info.highest_probe_tem, _data, HIGHEST_PROBE_TEM, HIGHEST_PROBE_TEM_LENGTH );
    // 最低温度探针序号
    read_bytes ( &extremum_info.lowest_tem_probe_no, _data, LOWEST_TEM_PROBE_NO, LOWEST_TEM_PROBE_NO_LENGTH );
    // 最低温度值
    read_bytes ( &extremum_info.lowest_probe_tem, _data, LOWEST_PROBE_TEM, LOWEST_PROBE_TEM_LENGTH );

    return std::make_shared<VDCRC_StateMessage_ExtremumInfo> ( extremum_info );
}

static std::shared_ptr<VDCRC_StateMessage_AlarmInfo> VDCRC_UpStream_BodyParse_StateMessage_AlarmInfo ( const void *_data )
{
    VDCRC_StateMessage_AlarmInfo_t alarm_info;
    memset ( &alarm_info, 0x00, sizeof ( alarm_info ) );

    // vcu故障等级
    read_bytes ( &alarm_info.error_level_vcu, _data, ERROR_LEVEL_VCU, ERROR_LEVEL_VCU_LENGTH );
    // 电机故障等级
    read_bytes ( &alarm_info.error_level_motor, _data, ERROR_LEVEL_MOTOR, ERROR_LEVEL_MOTOR_LENGTH );
    // 电池故障等级
    read_bytes ( &alarm_info.error_level_bat, _data, ERROR_LEVEL_BAT, ERROR_LEVEL_BAT_LENGTH );
    // egsm故障等级
    read_bytes ( &alarm_info.error_level_egsm, _data, ERROR_LEVEL_EGSM, ERROR_LEVEL_EGSM_LENGTH );
    // dcdc故障等级
    read_bytes ( &alarm_info.error_level_dcdc, _data, ERROR_LEVEL_DCDC, ERROR_LEVEL_DCDC_LENGTH );
    // obc故障等级
    read_bytes ( &alarm_info.error_level_obc, _data, ERROR_LEVEL_OBC, ERROR_LEVEL_OBC_LENGTH );
    // 温度差异报警
    read_bytes ( &alarm_info.alarm_tem_diff, _data, ALARM_TEM_DIFF, ALARM_TEM_DIFF_LENGTH );
    // 电池高温报警
    read_bytes ( &alarm_info.alarm_bat_over_tem, _data, ALARM_BAT_OVER_TEM, ALARM_BAT_OVER_TEM_LENGTH );
    // 车载储能装置类型过压报警
    read_bytes ( &alarm_info.alarm_bat_over_vol, _data, ALARM_BAT_OVER_VOL, ALARM_BAT_OVER_VOL_LENGTH );
    // 车载储能装置类型欠压报警
    read_bytes ( &alarm_info.alarm_bat_under_vol, _data, ALARM_BAT_UNDER_VOL, ALARM_BAT_UNDER_VOL_LENGTH );
    // soc低报警
    read_bytes ( &alarm_info.alarm_soc_low, _data, ALARM_SOC_LOW, ALARM_SOC_LOW_LENGTH );
    // 单体电池过压报警
    read_bytes ( &alarm_info.alarm_cell_over_vol, _data, ALARM_CELL_OVER_VOL, ALARM_CELL_OVER_VOL_LENGTH );
    // 单体电池欠压报警
    read_bytes ( &alarm_info.alarm_cell_under_vol, _data, ALARM_CELL_UNDER_VOL, ALARM_CELL_UNDER_VOL_LENGTH );
    // soc过高报警
    read_bytes ( &alarm_info.alarm_soc_high, _data, ALARM_SOC_HIGH, ALARM_SOC_HIGH_LENGTH );
    // soc跳变报警
    read_bytes ( &alarm_info.alarm_soc_jump, _data, ALARM_SOC_JUMP, ALARM_SOC_JUMP_LENGTH );
    // 可充电储能系统不匹配报警
    read_bytes ( &alarm_info.alarm_bat_not_match, _data, ALARM_BAT_NOT_MATCH, ALARM_BAT_NOT_MATCH_LENGTH );
    // 单体电池一致性差报警
    read_bytes ( &alarm_info.alarm_cell_cons_diff, _data, ALARM_CELL_CONS_DIFF, ALARM_CELL_CONS_DIFF_LENGTH );
    // 绝缘报警
    read_bytes ( &alarm_info.alarm_insulation, _data, ALARM_INSULATION, ALARM_INSULATION_LENGTH );
    // dcdc温度报警
    read_bytes ( &alarm_info.alarm_dcdc_tem, _data, ALARM_DCDC_TEM, ALARM_DCDC_TEM_LENGTH );
    // 制动系统报警
    read_bytes ( &alarm_info.alarm_brake, _data, ALARM_BRAKE, ALARM_BRAKE_LENGTH );
    // dcdc状态报警
    read_bytes ( &alarm_info.alarm_dcdc_status, _data, ALARM_DCDC_STATUS, ALARM_DCDC_STATUS_LENGTH );
    // 驱动电机控制器温度报警
    read_bytes ( &alarm_info.alarm_motor_mcu_tem, _data, ALARM_MOTOR_MCU_TEM, ALARM_MOTOR_MCU_TEM_LENGTH );
    // 高压互锁状态报警
    read_bytes ( &alarm_info.alarm_high_vol_lock_loop, _data, ALARM_HIGH_VOL_LOCK_LOOP, ALARM_HIGH_VOL_LOCK_LOOP_LENGTH );
    // 驱动电机温度报警
    read_bytes ( &alarm_info.alarm_motor_tem, _data, ALARM_MOTOR_TEM, ALARM_MOTOR_TEM_LENGTH );
    // 车载储能装置类型过充
    read_bytes ( &alarm_info.alarm_bat_over_charge, _data, ALARM_BAT_OVER_CHARGE, ALARM_BAT_OVER_CHARGE_LENGTH );

#if 0
    VDCRC_Protocol_LowerSystem_Debug ( "VDCRC_StateMessage_AlarmInfo_t:" );
    VDCRC_Protocol_LowerSystem_Debug ( "error_level_vcu = %02X", alarm_info.error_level_vcu );
    VDCRC_Protocol_LowerSystem_Debug ( "error_level_motor = %02X", alarm_info.error_level_motor );
    VDCRC_Protocol_LowerSystem_Debug ( "error_level_bat = %02X", alarm_info.error_level_bat );
    VDCRC_Protocol_LowerSystem_Debug ( "error_level_egsm = %02X", alarm_info.error_level_egsm );
    VDCRC_Protocol_LowerSystem_Debug ( "error_level_dcdc = %02X", alarm_info.error_level_dcdc );
    VDCRC_Protocol_LowerSystem_Debug ( "error_level_obc = %02X", alarm_info.error_level_obc );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_tem_diff = %02X", alarm_info.alarm_tem_diff );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_bat_over_tem = %02X", alarm_info.alarm_bat_over_tem );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_bat_over_vol = %02X", alarm_info.alarm_bat_over_vol );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_bat_under_vol = %02X", alarm_info.alarm_bat_under_vol );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_soc_low = %02X", alarm_info.alarm_soc_low );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_cell_over_vol = %02X", alarm_info.alarm_cell_over_vol );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_cell_under_vol = %02X", alarm_info.alarm_cell_under_vol );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_soc_high = %02X", alarm_info.alarm_soc_high );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_soc_jump = %02X", alarm_info.alarm_soc_jump );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_bat_not_match = %02X", alarm_info.alarm_bat_not_match );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_cell_cons_diff = %02X", alarm_info.alarm_cell_cons_diff );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_insulation = %02X", alarm_info.alarm_insulation );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_dcdc_tem = %02X", alarm_info.alarm_dcdc_tem );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_brake = %02X", alarm_info.alarm_brake );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_dcdc_status = %02X", alarm_info.alarm_dcdc_status );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_motor_mcu_tem = %02X", alarm_info.alarm_motor_mcu_tem );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_high_vol_lock_loop = %02X", alarm_info.alarm_high_vol_lock_loop );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_motor_tem = %02X", alarm_info.alarm_motor_tem );
    VDCRC_Protocol_LowerSystem_Debug ( "alarm_bat_over_charge = %02X", alarm_info.alarm_bat_over_charge );
#endif

    return std::make_shared<VDCRC_StateMessage_AlarmInfo> ( alarm_info );
}

static std::shared_ptr<VDCRC_StateMessage_ChargeableDevInfo> VDCRC_UpStream_BodyParse_StateMessage_ChargeableDevInfo ( const void *_data )
{
    VDCRC_StateMessage_ChargeableDevInfo_t chargeable_dev_info;
    memset ( &chargeable_dev_info, 0x00, sizeof ( chargeable_dev_info ) );

    // 可充电储能装置电流
    read_bytes ( &chargeable_dev_info.current, _data, TOTAL_CUR, TOTAL_CUR_LENGTH );
    // 单体电池总数
    read_bytes ( &chargeable_dev_info.cell_num, _data, CELL_NUM, CELL_NUM_LENGTH );
    chargeable_dev_info.cell_num = std::min ( ( size_t ) chargeable_dev_info.cell_num, ARRAY_MAX ( chargeable_dev_info.cell_vol ) );
    // 单体电池电压
    for ( uint16_t index = 0; index < chargeable_dev_info.cell_num; ++index )
    {
        read_bytes ( &chargeable_dev_info.cell_vol[index], _data, CELL_VOL_001 + ( index * CELL_VOL_001_LENGTH ), CELL_VOL_001_LENGTH );
    }
    // 温度探针个数
    read_bytes ( &chargeable_dev_info.tem_prob_num, _data, TEM_PROB_NUM, TEM_PROB_NUM_LENGTH );
    chargeable_dev_info.tem_prob_num = std::min ( ( size_t ) chargeable_dev_info.tem_prob_num, ARRAY_MAX ( chargeable_dev_info.prob_tem ) );
    // 探针温度值
    for ( uint16_t index = 0; index < chargeable_dev_info.tem_prob_num; ++index )
    {
        read_bytes ( &chargeable_dev_info.prob_tem[index], _data, PROB_TEM_001 + ( index * PROB_TEM_001_LENGTH ), PROB_TEM_001_LENGTH );
    }

    return std::make_shared<VDCRC_StateMessage_ChargeableDevInfo> ( chargeable_dev_info );
}

static std::shared_ptr<VDCRC_StateMessage_EkeyInfo> VDCRC_UpStream_BodyParse_StateMessage_EkeyInfo ( const void *_data )
{
    VDCRC_StateMessage_EkeyInfo_t ekey_info;
    memset ( &ekey_info, 0x00, sizeof ( ekey_info ) );

    // 车身防盗状态
    read_bytes ( &ekey_info.body_guard_status, _data, BODY_GUARD_STATUS, BODY_GUARD_STATUS_LENGTH );

    return std::make_shared<VDCRC_StateMessage_EkeyInfo> ( ekey_info );
}

static std::shared_ptr<VDCRC_StateMessage_WindowsInfo> VDCRC_UpStream_BodyParse_StateMessage_WindowsInfo ( const void *_data )
{
    VDCRC_StateMessage_WindowsInfo_t windows_info;
    memset ( &windows_info, 0x00, sizeof ( windows_info ) );

    // 左前窗开度值
    read_bytes ( &windows_info.LF_opened_val, _data, LF_WIN_OPENED_VALUE, LF_WIN_OPENED_VALUE_LENGTH );
    // 左后窗开度值
    read_bytes ( &windows_info.LR_opened_val, _data, LR_WIN_OPENED_VALUE, LR_WIN_OPENED_VALUE_LENGTH );
    // 右前窗开度值
    read_bytes ( &windows_info.RF_opened_val, _data, RF_WIN_OPENED_VALUE, RF_WIN_OPENED_VALUE_LENGTH );
    // 右后窗开度值
    read_bytes ( &windows_info.RR_opened_val, _data, RR_WIN_OPENED_VALUE, RR_WIN_OPENED_VALUE_LENGTH );

    // 左前窗状态
    read_bytes ( &windows_info.LF_window_status, _data, LF_WIN_STATUS, LF_WIN_STATUS_LENGTH );
    // 左后窗状态
    read_bytes ( &windows_info.LR_window_status, _data, LR_WIN_STATUS, LR_WIN_STATUS_LENGTH );
    // 右前窗状态
    read_bytes ( &windows_info.RF_window_status, _data, RF_WIN_STATUS, RF_WIN_STATUS_LENGTH );
    // 右后窗状态
    read_bytes ( &windows_info.RR_window_status, _data, RR_WIN_STATUS, RR_WIN_STATUS_LENGTH );

    return std::make_shared<VDCRC_StateMessage_WindowsInfo> ( windows_info );
}

static std::shared_ptr<VDCRC_StateMessage_AirInfo> VDCRC_UpStream_BodyParse_StateMessage_AirInfo ( const void *_data )
{
    VDCRC_StateMessage_AirInfo_t air_info;
    memset ( &air_info, 0x00, sizeof ( air_info ) );

    // 鼓风机状态
    read_bytes ( &air_info.BLW_work_status, _data, BLW_STATUS, BLW_STATUS_LENGTH );
    // AC开关
    read_bytes ( &air_info.AC, _data, AC_KEY, AC_KEY_LENGTH );
    // 后除霜
    read_bytes ( &air_info.rear_defrost, _data, REAR_DE_FROSE, REAR_DE_FROSE_LENGTH );
    // 前除霜
    read_bytes ( &air_info.front_defrost, _data, FRONT_DE_FROSE, FRONT_DE_FROSE_LENGTH );
    // 循环模式
    read_bytes ( &air_info.cycle_mode, _data, CYCLE_MODE, CYCLE_MODE_LENGTH );
    // 吹风模式
    read_bytes ( &air_info.blower_mode, _data, DRYER_MODE, DRYER_MODE_LENGTH );
    // 风速等级1
    read_bytes ( &air_info.wind_level_1, _data, AIR_VOLUME_1, AIR_VOLUME_1_LENGTH );
    // 风速等级2
    read_bytes ( &air_info.wind_level_2, _data, AIR_VOLUME_2, AIR_VOLUME_2_LENGTH );

    // 车外温度
    read_bytes ( &air_info.temperature_external, _data, OUT_TEM, OUT_TEM_LENGTH );
    // 车内温度1
    read_bytes ( &air_info.temparature_inside_1, _data, IN_TEM_1, IN_TEM_1_LENGTH );
    // 车内温度2
    read_bytes ( &air_info.temparature_inside_2, _data, IN_TEM_2, IN_TEM_2_LENGTH );

    return std::make_shared<VDCRC_StateMessage_AirInfo> ( air_info );
}

static std::shared_ptr<VDCRC_StateMessage_DoorsInfo> VDCRC_UpStream_BodyParse_StateMessage_DoorsInfo ( const void *_data )
{
    VDCRC_StateMessage_DoorsInfo_t doors_info;
    memset ( &doors_info, 0x00, sizeof ( doors_info ) );

    // 左前门锁状态
    read_bytes ( &doors_info.LF_door_lock, _data, LF_DOOR_LOCK_STATUS, LF_DOOR_LOCK_STATUS_LENGTH );
    // 左后门锁状态
    read_bytes ( &doors_info.LR_door_lock, _data, LR_DOOR_LOCK_STATUS, LR_DOOR_LOCK_STATUS_LENGTH );
    // 右前门锁状态
    read_bytes ( &doors_info.RF_door_lock, _data, RF_DOOR_LOCK_STATUS, RF_DOOR_LOCK_STATUS_LENGTH );
    // 右后门锁状态
    read_bytes ( &doors_info.RR_door_lock, _data, RR_DOOR_LOCK_STATUS, RR_DOOR_LOCK_STATUS_LENGTH );
    // 背门锁状态
    read_bytes ( &doors_info.Back_door_lock, _data, BACK_DOOR_LOCK_STATUS, BACK_DOOR_LOCK_STATUS_LENGTH );

    // 左前门状态
    read_bytes ( &doors_info.LF_door_sts, _data, LF_DOOR_STATUS, LF_DOOR_STATUS_LENGTH );
    // 左后门状态
    read_bytes ( &doors_info.LR_door_sts, _data, LR_DOOR_STATUS, LR_DOOR_STATUS_LENGTH );
    // 右前门状态
    read_bytes ( &doors_info.RF_door_sts, _data, RF_DOOR_STATUS, RF_DOOR_STATUS_LENGTH );
    // 右后门状态
    read_bytes ( &doors_info.RR_door_sts, _data, RR_DOOR_STATUS, RR_DOOR_STATUS_LENGTH );
    // 背门状态
    read_bytes ( &doors_info.Back_door_sts, _data, BACK_DOOR_STATUS, BACK_DOOR_STATUS_LENGTH );

    return std::make_shared<VDCRC_StateMessage_DoorsInfo> ( doors_info );
}

static std::shared_ptr<VDCRC_StateMessage_PrivateInfo> VDCRC_UpStream_BodyParse_StateMessage_PrivateInfo ( const void *_data )
{
    VDCRC_StateMessage_PrivateInfo_t private_info;
    memset ( &private_info, 0x00, sizeof ( private_info ) );

    // 中控屏保护
    read_bytes ( &private_info.private_info_lock, _data, PRIVATE_INFO_LOCK, PRIVATE_INFO_LOCK_LENGTH );

    return std::make_shared<VDCRC_StateMessage_PrivateInfo> ( private_info );
}

static std::shared_ptr<VDCRC_StateMessage_SunroofInfo> VDCRC_UpStream_BodyParse_StateMessage_SunroofInfo ( const void *_data )
{
    VDCRC_StateMessage_SunroofInfo_t sunroof_info;
    memset ( &sunroof_info, 0x00, sizeof ( sunroof_info ) );

    // 天窗窗帘状态
    read_bytes ( &sunroof_info.curtain_status, _data, SUN_ROOF_CURTAIN_STATUS, SUN_ROOF_CURTAIN_STATUS_LENGTH );
    // 天窗玻璃状态
    read_bytes ( &sunroof_info.glass_status, _data, SUN_ROOF_GLASS_STATUS, SUN_ROOF_GLASS_STATUS_LENGTH );
    // 天窗玻璃位置
    read_bytes ( &sunroof_info.glass_pos, _data, SUN_ROOF_GLASS_POS, SUN_ROOF_GLASS_POS_LENGTH );

    return std::make_shared<VDCRC_StateMessage_SunroofInfo> ( sunroof_info );
}

static std::shared_ptr<VDCRC_StateMessage_LightingInfo> VDCRC_UpStream_BodyParse_StateMessage_LightingInfo ( const void *_data )
{
    VDCRC_StateMessage_LightingInfo_t lighting_info;
    memset ( &lighting_info, 0x00, sizeof ( lighting_info ) );

    // 近光灯状态
    read_bytes ( &lighting_info.low_beam, _data, LOW_BEAM, LOW_BEAM_LENGTH );
    // 远光灯状态
    read_bytes ( &lighting_info.high_beam, _data, HIGH_BEAM, HIGH_BEAM_LENGTH );
    // 左转灯状态
    read_bytes ( &lighting_info.left_cornering_lamp, _data, LT_LAMP, LT_LAMP_LENGTH );
    // 右转灯状态
    read_bytes ( &lighting_info.right_cornering_lamp, _data, RT_LAMP, RT_LAMP_LENGTH );
    // 位置灯有效状态
    read_bytes ( &lighting_info.pos_lamp_valid, _data, POS_LAMP_VALID, POS_LAMP_VALID_LENGTH );
    // 位置灯状态
    read_bytes ( &lighting_info.pos_lamp, _data, POS_LAMP, POS_LAMP_LENGTH );
    // 前雾灯有效状态
    read_bytes ( &lighting_info.front_fog_lamp_valid, _data, FRONT_FOG_LAMP_VALID, FRONT_FOG_LAMP_VALID_LENGTH );
    // 前雾灯状态
    read_bytes ( &lighting_info.front_fog_lamp, _data, FRONT_FOG_LAMP, FRONT_FOG_LAMP_LENGTH );
    // 后雾灯有效状态
    read_bytes ( &lighting_info.rear_fog_lamp_valid, _data, REAR_FOG_LAMP_VALID, REAR_FOG_LAMP_VALID_LENGTH );
    // 后雾灯状态
    read_bytes ( &lighting_info.rear_fog_lamp, _data, REAR_FOG_LAMP, REAR_FOG_LAMP_LENGTH );

    return std::make_shared<VDCRC_StateMessage_LightingInfo> ( lighting_info );
}

static std::shared_ptr<VDCRC_AlarmMessage_Crash> VDCRC_UpStream_BodyParse_AlarmMessage_Crash ( const void *_data )
{
    std::shared_ptr<VDCRC_AlarmMessage_Crash> sp_crash = std::make_shared<VDCRC_AlarmMessage_Crash>();

    if ( sp_crash )
    {
        uint8_t crash_status = 0x0;
        read_bytes ( &crash_status, _data, CRASH_STATUS, CRASH_STATUS_LENGTH );

        sp_crash->set_crash_status ( crash_status );
    }

    return sp_crash;
}

static int32_t VDCRC_UpStream_BodyParse_LowerSystemUpgrade (
    const void *_data,
    size_t _count,
    std::vector<std::shared_ptr<VDCRC_Notification>> &_notification_set,
    std::vector<std::shared_ptr<VDCRC_Response>> &_response_set )
{
    int32_t retval = TBOX_ERROR_BASE;

    for ( ;; )
    {
        if ( nullptr == _data )
        {
            VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse_LowerSystemUpgrade() failed, nullptr == _data." );
            break;
        }

        if ( _count != UP_STREAM_LOWER_SYSTEM_UPGRADE_DATA_BYTE_COUNT )
        {
            VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse_LowerSystemUpgrade() failed, _count(%u) != UP_STREAM_LOWER_SYSTEM_UPGRADE_DATA_BYTE_COUNT(%u).",
                                             _count, UP_STREAM_LOWER_SYSTEM_UPGRADE_DATA_BYTE_COUNT );
            break;
        }

        // identity
        uint8_t identity = 0;
        read_bytes ( &identity, _data, UP_STREAM_LOWER_SYSTEM_UPGRADE_IDENTITY_BYTE_OFFSET, UP_STREAM_LOWER_SYSTEM_UPGRADE_IDENTITY_BYTE_COUNT );

        // REQUEST
        if ( UP_STREAM_LOWER_SYSTEM_UPGRADE_IDENTITY_VALUE_REQUEST == identity )
        {
            VDCRC_Protocol_LowerSystem_Debug ( "VDCRC_Notification_LowerSystemUpgrade_Request." );

            std::shared_ptr<VDCRC_Notification> notification = std::make_shared<VDCRC_Notification_LowerSystemUpgrade_Request>();
            if ( !notification )
            {
                break;
            }

            _notification_set.push_back ( notification );
        }

        // RETRY
        else if ( UP_STREAM_LOWER_SYSTEM_UPGRADE_IDENTITY_VALUE_RETRY == identity )
        {
            VDCRC_Protocol_LowerSystem_Debug ( "VDCRC_Response_LowerSystemUpgrade_Retry." );

            std::shared_ptr<VDCRC_Response> response = std::make_shared<VDCRC_Response_LowerSystemUpgrade_Retry>();
            if ( !response )
            {
                break;
            }

            _response_set.push_back ( response );
        }

        // ACK
        else if ( UP_STREAM_LOWER_SYSTEM_UPGRADE_IDENTITY_VALUE_ACK == identity )
        {
            VDCRC_Protocol_LowerSystem_Debug ( "VDCRC_Response_LowerSystemUpgrade_ACK." );

            std::shared_ptr<VDCRC_Response> response = std::make_shared<VDCRC_Response_LowerSystemUpgrade_ACK>();
            if ( !response )
            {
                break;
            }

            _response_set.push_back ( response );
        }

        // default
        else
        {
            VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse_Notification_LowerSystemUpgrade() failed, UNKNOWN identity(0x%02X).", identity );
            break;
        }

        retval = TBOX_SUCCESS;
        break;
    }

    return retval;
}

static std::shared_ptr<VDCRC_Response_AskForVersion_LowerSys> VDCRC_UpStream_BodyParse_Response_AskForVersion_LowerSys ( const void *_data, size_t _count )
{
    return std::make_shared<VDCRC_Response_AskForVersion_LowerSys> ( ( const char * ) _data, _count );
}

static int32_t VDCRC_UpStream_BodyParse_PowerManagement (
    const void *_data,
    size_t _count,
    std::vector<std::shared_ptr<VDCRC_Notification>> &_notification_set,
    std::vector<std::shared_ptr<VDCRC_Response>> &_response_set )
{
    int32_t retval = TBOX_ERROR_BASE;

    for ( ;; )
    {
        if ( nullptr == _data )
        {
            VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse_PowerManagement() failed, nullptr == _data." );
            break;
        }

        if ( _count != UP_STREAM_POWER_MANAGEMENT_DATA_BYTE_COUNT )
        {
            VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse_PowerManagement() failed, _count(%u) != UP_STREAM_POWER_MANAGEMENT_DATA_BYTE_COUNT(%u).",
                                             _count, UP_STREAM_POWER_MANAGEMENT_DATA_BYTE_COUNT );
            break;
        }

        // identity
        uint8_t identity = 0;
        read_bytes ( &identity, _data, UP_STREAM_POWER_MANAGEMENT_IDENTITY_BYTE_OFFSET, UP_STREAM_POWER_MANAGEMENT_IDENTITY_BYTE_COUNT );

        // SLEEP
        if ( UP_STREAM_POWER_MANAGEMENT_IDENTITY_VALUE_SLEEP == identity )
        {
            VDCRC_Protocol_LowerSystem_Debug ( "VDCRC_Notification_PowerManagement_Sleep." );

            std::shared_ptr<VDCRC_Notification> notification = std::make_shared<VDCRC_Notification_PowerManagement_Sleep>();
            if ( !notification )
            {
                break;
            }

            _notification_set.push_back ( notification );
        }

        // default
        else
        {
            VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse_PowerManagement() failed, UNKNOWN identity(0x%02X).", identity );
            break;
        }

        retval = TBOX_SUCCESS;
        break;
    }

    return retval;
}

int32_t VDCRC_UpStream_BodyParse ( const void *_body, size_t _count,
                                   std::vector<std::shared_ptr<VDCRC_Notification>> &_notification_set,
                                   std::vector<std::shared_ptr<VDCRC_Response>> &_response_set )
{
    if ( nullptr == _body )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse() failed, nullptr == _body." );
        return TBOX_ERROR_BASE;
    }

    if ( _count < UP_STREAM_CMD_ID_BYTE_COUNT )
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse() failed, _count(%u) < UP_STREAM_CMD_ID_BYTE_COUNT.", _count );
        return TBOX_ERROR_BASE;
    }

    // CMD_ID
    uint16_t cmd_id = 0;
    read_bytes ( &cmd_id, _body, UP_STREAM_CMD_ID_BYTE_OFFSET, UP_STREAM_CMD_ID_BYTE_COUNT );

    // DATA
    const void *data = ( uint8_t * ) _body + UP_STREAM_CMD_ID_BYTE_COUNT;

    // GB32960
    if ( ( GB32960_ID == cmd_id ) && ( UP_STREAM_GB32960_BODY_BYTE_COUNT == _count ) )
    {
        // 整车数据
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_VehicleInfo ( data ) );

        // 驱动电机数据
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_MotorInfo ( data ) );

        // 燃料电池数据
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_FuelBatInfo ( data ) );

        // 燃料电池数据
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_EngineInfo ( data ) );

        // 极值数据
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_ExtremumInfo ( data ) );

        // 报警数据
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_AlarmInfo ( data ) );

        // 可充电储能装置数据(电压/温度)
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_ChargeableDevInfo ( data ) );

        // 电子钥匙信息
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_EkeyInfo ( data ) );

        // 车窗状态信息
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_WindowsInfo ( data ) );

        // 空调信息
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_AirInfo ( data ) );

        // 车门状态信息
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_DoorsInfo ( data ) );

        // 远程屏幕锁
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_PrivateInfo ( data ) );

        // 天窗状态信息
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_SunroofInfo ( data ) );

        // 车灯信息
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_StateMessage_LightingInfo ( data ) );

        // 车辆碰撞状态
        _notification_set.push_back ( VDCRC_UpStream_BodyParse_AlarmMessage_Crash ( data ) );
    }

    // 下位机固件更新
    else if ( ( UP_STREAM_LOWER_SYSTEM_UPGRADE_CMD_ID == cmd_id ) && ( UP_STREAM_LOWER_SYSTEM_UPGRADE_BODY_BYTE_COUNT == _count ) )
    {
        int32_t retval = VDCRC_UpStream_BodyParse_LowerSystemUpgrade ( data, UP_STREAM_LOWER_SYSTEM_UPGRADE_DATA_BYTE_COUNT, _notification_set, _response_set );
        if ( retval != TBOX_SUCCESS )
        {
            VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse_LowerSystemUpgrade() failed: retval = %d.", retval );
            return retval;
        }
    }

    // 下位机应答固件版本号
    else if ( UP_STREAM_LOWER_SYSTEM_VERSION_CMD_ID == cmd_id )
    {
        std::shared_ptr<VDCRC_Response_AskForVersion_LowerSys> lowerSysVersion;
        lowerSysVersion = VDCRC_UpStream_BodyParse_Response_AskForVersion_LowerSys ( data, UP_STREAM_LOWER_SYSTEM_VERSION_BYTE_COUNT ( _count ) );
        if ( !lowerSysVersion )
        {
            VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse_Response_AskForVersion_LowerSys() failed." );
            return TBOX_ERROR_BASE;
        }

        _response_set.push_back ( lowerSysVersion );
    }

    // 下位机通知上位机电源管理(休眠)
    else if ( ( UP_STREAM_POWER_MANAGEMENT_CMD_ID == cmd_id ) && ( UP_STREAM_POWER_MANAGEMENT_BODY_BYTE_COUNT == _count ) )
    {
        int32_t retval = VDCRC_UpStream_BodyParse_PowerManagement ( data, UP_STREAM_POWER_MANAGEMENT_DATA_BYTE_COUNT, _notification_set, _response_set );
        if ( retval != TBOX_SUCCESS )
        {
            VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse_LowerSystemUpgrade() failed: retval = %d.", retval );
            return retval;
        }
    }

    // UNKNOWN
    else
    {
        VDCRC_Protocol_LowerSystem_Err ( "VDCRC_UpStream_BodyParse() failed, cmd_id = 0x%04X, _count = %u.", cmd_id, _count );
        return TBOX_ERROR_BASE;
    }

    return TBOX_SUCCESS;
}

#include "source/vdcrc/transport/include/transport_config_uart.hpp"
#include "include/common/common.h"
#include "source/vdcrc/logger/vdcrc_logger.h"

VDCRC_TransportConfig_UART::VDCRC_TransportConfig_UART()
    : port_ ( "/dev/ttyHS0" )
    , baud_rate_ ( 115200 )
    , data_bit_ ( 8 )
    , stop_bit_ ( 1 )
    , parity_ ( 'N' )
    , flow_ctrl_ ( 0 )
{

}

VDCRC_TransportConfig_UART::VDCRC_TransportConfig_UART ( const VDCRC_TransportConfig_UART &_other )
{
    port_       =   _other.port_;
    baud_rate_  =   _other.baud_rate_;
    data_bit_   =   _other.data_bit_;
    stop_bit_   =   _other.stop_bit_;
    parity_     =   _other.parity_;
    flow_ctrl_  =   _other.flow_ctrl_;
}

VDCRC_TransportConfig_UART::~VDCRC_TransportConfig_UART()
{

}

VDCRC_TransportConfig_UART &VDCRC_TransportConfig_UART::operator= ( const VDCRC_TransportConfig_UART &_other )
{
    if ( this == &_other )
    {
        return  *this;
    }

    port_       =   _other.port_;
    baud_rate_  =   _other.baud_rate_;
    data_bit_   =   _other.data_bit_;
    stop_bit_   =   _other.stop_bit_;
    parity_     =   _other.parity_;
    flow_ctrl_  =   _other.flow_ctrl_;

    return  *this;
}

VDCRC_TransportType VDCRC_TransportConfig_UART::type() const
{
    return  VDCRC_TransportType::UART;
}

const char *VDCRC_TransportConfig_UART::port() const
{
    return  port_.c_str ();
}

int32_t VDCRC_TransportConfig_UART::set_port ( const char *_port )
{
    if ( nullptr == _port )
    {
        VDCRC_Transport_UART_Err ( "VDCRC_TransportConfig_UART::set_port() failed, nullptr == _port." );
        return  TBOX_ERROR_RESULT_ARGS;
    }

    port_ = _port;

    return  TBOX_SUCCESS;
}

uint32_t VDCRC_TransportConfig_UART::baud_rate() const
{
    return  baud_rate_;
}

void VDCRC_TransportConfig_UART::set_baud_rate ( uint32_t _baud_rate )
{
    baud_rate_ = _baud_rate;
}

uint8_t VDCRC_TransportConfig_UART::data_bit() const
{
    return  data_bit_;
}

void VDCRC_TransportConfig_UART::set_data_bit ( uint8_t _data_bit )
{
    data_bit_ = _data_bit;
}

uint8_t VDCRC_TransportConfig_UART::stop_bit() const
{
    return  stop_bit_;
}

void VDCRC_TransportConfig_UART::set_stop_bit ( uint8_t _stop_bit )
{
    stop_bit_ = _stop_bit;
}

char VDCRC_TransportConfig_UART::parity() const
{
    return  parity_;
}

void VDCRC_TransportConfig_UART::set_parity ( char _parity )
{
    parity_ = _parity;
}

uint8_t VDCRC_TransportConfig_UART::flow_ctrl() const
{
    return  flow_ctrl_;
}

void VDCRC_TransportConfig_UART::set_flow_ctrl ( uint8_t _flow_ctrl )
{
    flow_ctrl_ = _flow_ctrl;
}

#include "include/common/common.h"
#include "source/vdcrc/transport/source/transport_gnss.hpp"
#include <string.h>
#include "source/vdcrc/logger/vdcrc_logger.h"

#ifndef MODULE_QUECTEL_OPENLINUX
int QL_LOC_Client_Init ( loc_client_handle_type *ph_loc )
{
    if ( ph_loc != nullptr )
    {
        *ph_loc = 1;
    }

    return 0;
}

int QL_LOC_AddRxIndMsgHandler ( QL_LOC_RxIndMsgHandlerFunc_t handlerPtr, void *contextPtr )
{
    return 0;
}

int QL_LOC_Set_Indications ( loc_client_handle_type h_loc, int bit_mask )
{
    return 0;
}

int QL_LOC_Set_Position_Mode ( loc_client_handle_type h_loc, QL_LOC_POS_MODE_INFO_T *pt_mode )
{
    return 0;
}

int QL_LOC_Start_Navigation ( loc_client_handle_type h_loc )
{
    return 0;
}

int QL_LOC_Stop_Navigation ( loc_client_handle_type h_loc )
{
    return 0;
}

int QL_LOC_Client_Deinit ( loc_client_handle_type h_loc )
{
    return 0;
}
#endif

VDCRC_Transport_GNSS::VDCRC_Transport_GNSS ( const VDCRC_TransportConfig_GNSS *_config )
    : VDCRC_Transport ( _config ), listener_ ( nullptr ), h_loc_ ( 0 )
{
    if ( _config != nullptr )
    {
        config_ = *_config;
    }
    else
    {
        VDCRC_Transport_GNSS_Err ( "VDCRC_Transport_GNSS::VDCRC_Transport_GNSS(), nullptr == _config." );
    }
}

VDCRC_Transport_GNSS::~VDCRC_Transport_GNSS()
{
}

VDCRC_TransportType VDCRC_Transport_GNSS::type() const
{
    return VDCRC_TransportType::GNSS;
}

void VDCRC_Transport_GNSS::setListener ( VDCRC_TransportListener *_listener )
{
    VDCRC_Transport_GNSS_Debug ( "VDCRC_Transport_GNSS::setListener( %p )", _listener );

    listener_ = _listener;
}

int32_t VDCRC_Transport_GNSS::openConnection()
{
    VDCRC_Transport_GNSS_Debug ( "VDCRC_Transport_GNSS::openConnection() start." );

    VDCRC_Transport_GNSS_Info ( "mode=%d", config_.mode() );
    VDCRC_Transport_GNSS_Info ( "recurrence=%d", config_.recurrence() );
    VDCRC_Transport_GNSS_Info ( "min_interval=%d", config_.min_interval() );
    VDCRC_Transport_GNSS_Info ( "preferred_accuracy=%d", config_.preferred_accuracy() );
    VDCRC_Transport_GNSS_Info ( "preferred_time=%d", config_.preferred_time() );

    /* Init LOC module and return the h_loc, this should be called before any other QL_LOC_xxx api. */
    VDCRC_Transport_GNSS_Info ( "QL_LOC_Client_Init() start." );
    int result = QL_LOC_Client_Init ( &h_loc_ );
    if ( result != 0 )
    {
        VDCRC_Transport_GNSS_Err ( "QL_LOC_Client_Init() error, result = %d", result );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Transport_GNSS_Info ( "QL_LOC_Client_Init() end. return %d, h_loc_ = %d", result, h_loc_ );

    /* Add callback function if anything changed specified by the mask in QL_LOC_Set_Indications */
    void *contextPtr = this;
    result = QL_LOC_AddRxIndMsgHandler ( &VDCRC_Transport_GNSS::ql_loc_rx_ind_msg_cb, contextPtr );
    if ( result != 0 )
    {
        VDCRC_Transport_GNSS_Err ( "QL_LOC_AddRxIndMsgHandler() error, result = %d", result );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Transport_GNSS_Info ( "QL_LOC_AddRxIndMsgHandler() return %d", result );

    /* Set what we want callbacks for, for the detail mean of bit_mask, please refer to the macro of LOC_IND_xxxxx_INFO_ON */
    int bit_mask = LOC_IND_NMEA_INFO_ON;
    result = QL_LOC_Set_Indications ( h_loc_, bit_mask );
    if ( result != 0 )
    {
        VDCRC_Transport_GNSS_Err ( "QL_LOC_Set_Indications() error, result = %d", result );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Transport_GNSS_Info ( "QL_LOC_Set_Indications( h_loc_=%d, bit_mask=%d ) return %d", h_loc_, bit_mask, result );

    /* Set GPS position mode, detail info please refer to QL_LOC_POS_MODE_INFO_T */
    QL_LOC_POS_MODE_INFO_T loc_pos_mode;
    memset ( &loc_pos_mode, 0x00, sizeof ( loc_pos_mode ) );
    loc_pos_mode.mode = config_.mode();
    loc_pos_mode.recurrence = config_.recurrence();
    loc_pos_mode.min_interval = config_.min_interval();
    loc_pos_mode.preferred_accuracy = config_.preferred_accuracy();
    loc_pos_mode.preferred_time = config_.preferred_time();

    result = QL_LOC_Set_Position_Mode ( h_loc_, &loc_pos_mode );
    if ( result != 0 )
    {
        VDCRC_Transport_GNSS_Err ( "QL_LOC_Set_Position_Mode() error, result = %d", result );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Transport_GNSS_Info ( "QL_LOC_Set_Position_Mode( h_loc_=%d ) return %d", h_loc_, result );

    /* Start navigation, same as AT+QGPS=1, NMEA port start outputing nmea data */
    result = QL_LOC_Start_Navigation ( h_loc_ );
    if ( result != 0 )
    {
        VDCRC_Transport_GNSS_Err ( "QL_LOC_Start_Navigation() error, result = %d", result );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Transport_GNSS_Info ( "QL_LOC_Start_Navigation( h_loc_=%d ) return %d", h_loc_, result );

    VDCRC_Transport_GNSS_Info ( "VDCRC_Transport_GNSS::openConnection() success." );

    return TBOX_SUCCESS;
}

void VDCRC_Transport_GNSS::disconnect()
{
    VDCRC_Transport_GNSS_Debug ( "VDCRC_Transport_GNSS::disconnect() start." );

    /* Stop navigation, same as AT+QGPSEND,  NMEA port stop outputing nmea data */
    int result = QL_LOC_Stop_Navigation ( h_loc_ );
    VDCRC_Transport_GNSS_Info ( "QL_LOC_Stop_Navigation( h_loc_=%d ) return %d", h_loc_, result );

    /* DeInit LOC module and release resource, this should be called at last. */
    result = QL_LOC_Client_Deinit ( h_loc_ );
    VDCRC_Transport_GNSS_Info ( "QL_LOC_Client_Deinit( h_loc_=%d ) return %d", h_loc_, result );

    // reset handle
    h_loc_ = 0;

    VDCRC_Transport_GNSS_Info ( "VDCRC_Transport_GNSS::disconnect() success." );
}

bool VDCRC_Transport_GNSS::getIsConnected() const
{
    return ( h_loc_ > 0 );
}

int32_t VDCRC_Transport_GNSS::sendBytes ( const void *_buf, size_t _count )
{
    VDCRC_Transport_GNSS_Err ( "VDCRC_Transport_GNSS do not support sendBytes." );
    return TBOX_ERROR_BASE;
}

void VDCRC_Transport_GNSS::ql_loc_rx_ind_msg_cb ( loc_client_handle_type h_loc,
        E_QL_LOC_NFY_MSG_ID_T e_msg_id,
        void *pv_data,
        void *contextPtr )
{
    VDCRC_Transport_GNSS *gnss_obj = static_cast<VDCRC_Transport_GNSS *> ( contextPtr );
    if ( nullptr == gnss_obj )
    {
        VDCRC_Transport_GNSS_Err ( "ql_loc_rx_ind_msg_cb() error, nullptr == gnss_obj." );
        return;
    }

    gnss_obj->ql_loc_rx_ind_msg_cb ( h_loc, e_msg_id, pv_data );
}

void VDCRC_Transport_GNSS::ql_loc_rx_ind_msg_cb ( loc_client_handle_type h_loc,
        E_QL_LOC_NFY_MSG_ID_T e_msg_id,
        void *pv_data )
{
    if ( nullptr == listener_ )
    {
        VDCRC_Transport_GNSS_Err ( "ql_loc_rx_ind_msg_cb() error, nullptr == listener_." );
        return;
    }

    if ( h_loc_ != h_loc )
    {
        VDCRC_Transport_GNSS_Err ( "ql_loc_rx_ind_msg_cb() error, h_loc_(%d) != h_loc(%d).", h_loc_, h_loc );
        return;
    }

    if ( e_msg_id != E_QL_LOC_NFY_MSG_ID_NMEA_INFO )
    {
        VDCRC_Transport_GNSS_Err ( "ql_loc_rx_ind_msg_cb() error, e_msg_id(%d) != E_QL_LOC_NFY_MSG_ID_NMEA_INFO.", e_msg_id );
        return;
    }

    QL_LOC_NMEA_INFO_T *nmea_info = ( QL_LOC_NMEA_INFO_T * ) pv_data;
    if ( nullptr == nmea_info )
    {
        VDCRC_Transport_GNSS_Err ( "ql_loc_rx_ind_msg_cb() error, nullptr == nmea_info." );
        return;
    }

    // only deal with '$GPRMC' data
    if ( 'C' == nmea_info->nmea[5] )
    {
        VDCRC_Transport_GNSS_Info ( "nmea_info: timestamp=%lld, length=%d, nmea=%s", nmea_info->timestamp, nmea_info->length, nmea_info->nmea );
        listener_->onTransportBytesReceived ( nmea_info->nmea, nmea_info->length );
    }
}

#include "source/vdcrc/transport/source/transport_tcp.hpp"

#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "include/common/common.h"
#include "source/vdcrc/logger/vdcrc_logger.h"

static void log_out_bytes_ ( const char *_prefix, const uint8_t *_bytes, size_t _count )
{
    if ( ( nullptr == _prefix ) || ( nullptr == _bytes ) || ( _count <= 0 ) )
    {
        return;
    }

    std::string output;
    for ( size_t index = 0; index < _count; ++index )
    {
        if ( index != 0 )
        {
            output += " ";
        }
        char buffer[8] = {0};
        sprintf ( buffer, "%02X", _bytes[index] );
        output += buffer;
    }

    VDCRC_Transport_TCP_Info ( "%s %lu bytes = [%s]", _prefix, ( unsigned long ) _count, output.c_str() );
}

VDCRC_Transport_TCP::VDCRC_Transport_TCP ( const VDCRC_TransportConfig_TCP *_config )
    : VDCRC_Transport ( _config ), listener_ ( nullptr ), native_handle_ ( -1 ), tcp_handle_ ( nullptr ), thread_ ( nullptr )
{
    exit_pipefd_[0] = -1;
    exit_pipefd_[1] = -1;

    if ( _config != nullptr )
    {
        config_ = *_config;
    }
    else
    {
        VDCRC_Transport_TCP_Info ( "VDCRC_Transport_TCP::VDCRC_Transport_TCP(), nullptr == _config." );
    }
}

VDCRC_Transport_TCP::~VDCRC_Transport_TCP()
{
}

VDCRC_TransportType VDCRC_Transport_TCP::type() const
{
    return VDCRC_TransportType::TCP;
}

void VDCRC_Transport_TCP::setListener ( VDCRC_TransportListener *_listener )
{
    VDCRC_Transport_TCP_Debug ( "VDCRC_Transport_TCP::setListener ( %p )", _listener );
    listener_ = _listener;
}

int32_t VDCRC_Transport_TCP::openConnection()
{
    VDCRC_Transport_TCP_Debug ( "VDCRC_Transport_TCP::openConnection() start" );

    VDCRC_Transport_TCP_Debug ( "ip=%s", config_.ip() );
    VDCRC_Transport_TCP_Debug ( "port=%d", config_.port() );

    tcp_handle_ = new TcpClient ( config_.ip(), config_.port() );
    if ( nullptr == tcp_handle_ )
    {
        VDCRC_Transport_TCP_Err ( "Can not create tcp_client object." );
        return TBOX_ERROR_BASE;
    }

    native_handle_ = tcp_handle_->Init();
    if ( native_handle_ < 0 )
    {
        VDCRC_Transport_TCP_Err ( "tcp init failed, native_handle_ = %d", native_handle_ );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Transport_TCP_Info ( "native_handle_ = %d", native_handle_ );

    int32_t result = tcp_handle_->Connect ( native_handle_ );
    if ( result != 0 )
    {
        VDCRC_Transport_TCP_Err ( "tcp connect failed, result = %d", result );
        return TBOX_ERROR_BASE;
    }

    if ( pipe ( exit_pipefd_ ) != 0 )
    {
        VDCRC_Transport_TCP_Err ( "pipe() failed: errno(%d)-%s.", errno, strerror ( errno ) );
        return TBOX_ERROR_BASE;
    }

    // Create a thread to handle tcp rx data
    thread_ = new std::thread ( &VDCRC_Transport_TCP::Recv_Proc, this );
    if ( nullptr == thread_ )
    {
        VDCRC_Transport_TCP_Err ( "Create thread failed, nullptr == thread_" );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Transport_TCP_Debug ( "VDCRC_Transport_TCP::openConnection()" );

    return TBOX_SUCCESS;
}

void VDCRC_Transport_TCP::disconnect()
{
    VDCRC_Transport_TCP_Debug ( "VDCRC_Transport_TCP::disconnect(), start." );

    if ( thread_ != nullptr )
    {
        write ( exit_pipefd_[1], "exit", 4 );
        thread_->join();

        delete thread_;
        thread_ = nullptr;
    }

    if ( exit_pipefd_[0] >= 0 )
    {
        if ( close ( exit_pipefd_[0] ) != 0 )
        {
            VDCRC_Transport_TCP_Err ( "close ( %d ) failed: errno(%d)-%s.", exit_pipefd_[0], errno, strerror ( errno ) );
        }

        exit_pipefd_[0] = -1;
    }

    if ( exit_pipefd_[1] >= 0 )
    {
        if ( close ( exit_pipefd_[1] ) != 0 )
        {
            VDCRC_Transport_TCP_Err ( "close ( %d ) failed: errno(%d)-%s.", exit_pipefd_[1], errno, strerror ( errno ) );
        }

        exit_pipefd_[1] = -1;
    }

    if ( tcp_handle_ != nullptr )
    {
        if ( native_handle_ >= 0 )
        {
            tcp_handle_->Destory ( native_handle_ );
            native_handle_ = -1;
        }

        delete tcp_handle_;
        tcp_handle_ = nullptr;
    }

    VDCRC_Transport_TCP_Debug ( "VDCRC_Transport_TCP::disconnect()." );
}

bool VDCRC_Transport_TCP::getIsConnected() const
{
    return ( ( tcp_handle_ != nullptr ) && ( native_handle_ >= 0 ) );
}

int32_t VDCRC_Transport_TCP::sendBytes ( const void *_buf, size_t _count )
{
    if ( ( nullptr == _buf ) || ( _count <= 0 ) )
    {
        VDCRC_Transport_TCP_Err ( "VDCRC_Transport_TCP::sendBytes() failed, _buf=%p, _count=%d", _buf, _count );
        return TBOX_ERROR_BASE;
    }

    const uint8_t *bytes = ( const uint8_t * ) _buf;

    log_out_bytes_ ( "send", bytes, _count );

    while ( _count > 0 )
    {
        ssize_t written = tcp_handle_->Send ( native_handle_, bytes, _count );

        if ( written <= 0 )
        {
            if ( ( written < 0 ) && ( EINTR == errno ) )
            {
                written = 0;
            }
            else
            {
                VDCRC_Transport_TCP_Err ( "VDCRC_Transport_TCP::sendBytes() failed, written = %d, errno(%d):%s", written, errno, strerror ( errno ) );
                return TBOX_ERROR_BASE;
            }
        }

        _count -= written;
        bytes += written;
    }

    return TBOX_SUCCESS;
}

void VDCRC_Transport_TCP::Recv_Proc()
{
    const char *thread_name = "VDCRC_TCP_Read"; // length is restricted to 16 characters, including '\0'
    pthread_setname_np ( pthread_self(), thread_name );

    VDCRC_Transport_TCP_Info ( "VDCRC_Transport_TCP::Recv_Proc() start." );

    uint8_t buffer[512] = {0};

    while ( true )
    {
        fd_set fdset;
        FD_ZERO ( &fdset );
        FD_SET ( native_handle_, &fdset );
        FD_SET ( exit_pipefd_[0], &fdset );

        int nfds = std::max ( native_handle_, exit_pipefd_[0] ) + 1;

        int fs_sel = select ( nfds, &fdset, NULL, NULL, NULL );
        if ( -1 == fs_sel )
        {
            if ( EINTR == errno )
            {
                VDCRC_Transport_TCP_Debug ( "VDCRC_Transport_TCP::Recv_Proc(), select EINTR" );
                continue;
            }

            VDCRC_Transport_TCP_Err ( "VDCRC_Transport_TCP::Recv_Proc() failed to select, errno(%d)-%s", errno, strerror ( errno ) );
            exit ( -1 );
        }
        else if ( 0 == fs_sel )
        {
            // no data in Rx buffer
            VDCRC_Transport_TCP_Trace ( "VDCRC_Transport_TCP::Recv_Proc(), no data in Rx buffer." );
        }
        else
        {
            if ( FD_ISSET ( exit_pipefd_[0], &fdset ) )
            {
                VDCRC_Transport_TCP_Debug ( "VDCRC_Transport_TCP::Recv_Proc(), receive EXIT." );
                break;
            }

            if ( FD_ISSET ( native_handle_, &fdset ) )
            {
                ssize_t retval = 0;

                while ( ( retval = tcp_handle_->Recv ( native_handle_, buffer, sizeof ( buffer ) ) ) != 0 )
                {
                    if ( -1 == retval )
                    {
                        if ( EINTR == errno )
                        {
                            continue;
                        }
                        else if ( EAGAIN == errno )
                        {
                            break;
                        }
                        else
                        {
                            VDCRC_Transport_TCP_Critical ( "VDCRC_Transport_TCP::Recv_Proc(), recv failed, errno(%d)-%s, exit(-1).", errno, strerror ( errno ) );
                            exit ( -1 );
                        }
                    }

                    log_out_bytes_ ( "recv", buffer, retval );

                    // notify
                    if ( listener_ != nullptr )
                    {
                        listener_->onTransportBytesReceived ( buffer, retval );
                    }
                }

                if ( 0 == retval )
                {
                    VDCRC_Transport_TCP_Critical ( "VDCRC_Transport_TCP::Recv_Proc(), recv failed, 0 == retval" );
                    break;
                }
            }
        }
    }

    VDCRC_Transport_TCP_Info ( "VDCRC_Transport_TCP::Recv_Proc() end." );
}

#include "source/vdcrc/transport/source/transport_uart.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "include/common/common.h"
#include "source/vdcrc/logger/vdcrc_logger.h"

static speed_t convert_baud_rate ( uint32_t _baud_rate )
{
    switch ( _baud_rate )
    {
    case 300:
        return B300;
    case 1200:
        return B1200;
    case 2400:
        return B2400;
    case 4800:
        return B4800;
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 115200:
        return B115200;
    default:
        VDCRC_Transport_UART_Warn ( "convert_baud_rate(), Unsupported baud rate(%d), deal it with B115200", _baud_rate );
        return B115200;
    }
}

static int set_terminal_attributes ( int fdcom, const VDCRC_TransportConfig_UART &_config )
{
    struct termios options;
    bzero ( &options, sizeof ( options ) );
    cfmakeraw ( &options );

    // 波特率
    speed_t baud_rate = convert_baud_rate ( _config.baud_rate() );
    cfsetispeed ( &options, baud_rate );
    cfsetospeed ( &options, baud_rate );

    // 保证程序不会成为端口的占有者
    options.c_cflag |= CLOCAL;
    // 使能端口读取输入的数据
    options.c_cflag |= CREAD;

    // 流控
    switch ( _config.flow_ctrl() )
    {
    case 0:
        options.c_cflag &= ~CRTSCTS; // 不使用流控制
        break;

    case 1:
        options.c_cflag |= CRTSCTS; // 使用硬件流控制
        break;

    case 2:
        options.c_iflag |= IXON | IXOFF | IXANY; // 使用软件流控制
        break;

    default:
        options.c_cflag &= ~CRTSCTS; // 不使用流控制
        break;
    }

    // 数据位
    options.c_cflag &= ~CSIZE;
    switch ( _config.data_bit() )
    {
    case 5:
        options.c_cflag |= CS5;
        break;

    case 6:
        options.c_cflag |= CS6;
        break;

    case 7:
        options.c_cflag |= CS7;
        break;

    case 8:
    default:
        options.c_cflag |= CS8;
        break;
    }

    // 校验位
    switch ( _config.parity() )
    {
    case 'o':
    case 'O':
        options.c_cflag |= PARENB; // 奇校验
        options.c_cflag &= ~PARODD;
        break;

    case 'e':
    case 'E':
        options.c_cflag |= PARENB; //偶校验
        options.c_cflag |= PARODD;
        break;

    case 'n':
    case 'N':
    default:
        options.c_cflag &= ~PARENB; // 无奇偶校验位
        break;
    }

    // 停止位
    if ( 2 == _config.stop_bit() )
    {
        options.c_cflag |= CSTOPB;
    }
    else
    {
        options.c_cflag &= ~CSTOPB;
    }

    // 输出模式，原始数据输出
    options.c_oflag &= ~OPOST;
    // 控制字符, 所要读取字符的最小数量
    options.c_cc[VMIN] = 1;
    // 控制字符, 读取第一个字符的等待时间    unit: (1/10)second
    options.c_cc[VTIME] = 1;

    // 溢出的数据可以接收，但不读
    tcflush ( fdcom, TCIFLUSH );

    // 设置新属性，TCSANOW：所有改变立即生效
    int retval = tcsetattr ( fdcom, TCSANOW, &options );
    if ( retval != 0 )
    {
        VDCRC_Transport_UART_Err ( "tcsetattr(fdcom=%d) failed, errno(%d):%s", fdcom, errno, strerror ( errno ) );
    }

    return retval;
}

static void log_out_bytes_ ( const char *_prefix, const uint8_t *_bytes, size_t _count )
{
    if ( ( nullptr == _prefix ) || ( nullptr == _bytes ) || ( _count <= 0 ) )
    {
        return;
    }

    std::string output;
    for ( size_t index = 0; index < _count; ++index )
    {
        if ( index != 0 )
        {
            output += " ";
        }
        char buffer[8] = {0};
        sprintf ( buffer, "%02X", _bytes[index] );
        output += buffer;
    }

    VDCRC_Transport_UART_Info ( "%s %lu bytes = [%s]", _prefix, ( unsigned long ) _count, output.c_str() );
}

VDCRC_Transport_UART::VDCRC_Transport_UART ( const VDCRC_TransportConfig_UART *_config )
    : VDCRC_Transport ( _config ), listener_ ( nullptr ), fd_uart_ ( -1 ), thread_ ( nullptr )
{
    exit_pipefd_[0] = -1;
    exit_pipefd_[1] = -1;

    if ( _config != nullptr )
    {
        config_ = *_config;
    }
    else
    {
        VDCRC_Transport_UART_Info ( "VDCRC_Transport_UART::VDCRC_Transport_UART(), nullptr == _config." );
    }
}

VDCRC_Transport_UART::~VDCRC_Transport_UART()
{
}

VDCRC_TransportType VDCRC_Transport_UART::type() const
{
    return VDCRC_TransportType::UART;
}

void VDCRC_Transport_UART::setListener ( VDCRC_TransportListener *_listener )
{
    VDCRC_Transport_UART_Debug ( "VDCRC_Transport_UART::setListener ( %p )", _listener );
    listener_ = _listener;
}

int32_t VDCRC_Transport_UART::openConnection()
{
    VDCRC_Transport_UART_Debug ( "VDCRC_Transport_UART::openConnection() start." );

    VDCRC_Transport_UART_Info ( "port=%s", config_.port() );
    VDCRC_Transport_UART_Info ( "baud_rate=%d", config_.baud_rate() );
    VDCRC_Transport_UART_Info ( "data_bit=%d", config_.data_bit() );
    VDCRC_Transport_UART_Info ( "stop_bit=%d", config_.stop_bit() );
    VDCRC_Transport_UART_Info ( "parity=%c", config_.parity() );
    VDCRC_Transport_UART_Info ( "flow_ctrl=%d", config_.flow_ctrl() );

    // 打开串口
    fd_uart_ = open ( config_.port(), O_RDWR | O_NOCTTY | O_NONBLOCK );
    if ( -1 == fd_uart_ )
    {
        VDCRC_Transport_UART_Err ( "open ( %s ) failed, fd_uart_ = %d, errno(%d):%s", config_.port(), fd_uart_, errno, strerror ( errno ) );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Transport_UART_Info ( "open ( %s ), fd_uart_ = %d", config_.port(), fd_uart_ );

    // 恢复串口为非阻塞状态
    int32_t flags = fcntl ( fd_uart_, F_GETFL, 0 );
    if ( flags < 0 )
    {
        VDCRC_Transport_UART_Err ( "fcntl ( F_GETFL ) failed, flags = %d, errno(%d):%s", flags, errno, strerror ( errno ) );
        return TBOX_ERROR_BASE;
    }

    int retval = fcntl ( fd_uart_, F_SETFL, flags | O_NONBLOCK );
    if ( retval < 0 )
    {
        VDCRC_Transport_UART_Err ( "fcntl ( F_SETFL ) failed, retval = %d, errno(%d):%s", retval, errno, strerror ( errno ) );
        return TBOX_ERROR_BASE;
    }

    // 设置串口
    retval = set_terminal_attributes ( fd_uart_, config_ );
    if ( retval != 0 )
    {
        VDCRC_Transport_UART_Err ( "set_terminal_attributes(fd_uart_ = %d, config_) failed, retval = %d", fd_uart_, retval );
        return TBOX_ERROR_BASE;
    }

    if ( pipe ( exit_pipefd_ ) != 0 )
    {
        VDCRC_Transport_UART_Err ( "pipe() failed: errno(%d)-%s.", errno, strerror ( errno ) );
        return TBOX_ERROR_BASE;
    }

    // Create a thread to handle uart rx data
    thread_ = new std::thread ( &VDCRC_Transport_UART::UartRecv_Proc, this );
    if ( nullptr == thread_ )
    {
        VDCRC_Transport_UART_Err ( "Create thread failed, nullptr == thread_" );
        return TBOX_ERROR_BASE;
    }

    VDCRC_Transport_UART_Debug ( "VDCRC_Transport_UART::openConnection() success." );

    return TBOX_SUCCESS;
}

void VDCRC_Transport_UART::disconnect()
{
    VDCRC_Transport_UART_Debug ( "VDCRC_Transport_UART::disconnect() start." );

    if ( thread_ != nullptr )
    {
        write ( exit_pipefd_[1], "exit", 4 );
        thread_->join();

        delete thread_;
        thread_ = nullptr;
    }

    if ( exit_pipefd_[0] >= 0 )
    {
        if ( close ( exit_pipefd_[0] ) != 0 )
        {
            VDCRC_Transport_UART_Err ( "close ( %d ) failed: errno(%d)-%s.", exit_pipefd_[0], errno, strerror ( errno ) );
        }

        exit_pipefd_[0] = -1;
    }

    if ( exit_pipefd_[1] >= 0 )
    {
        if ( close ( exit_pipefd_[1] ) != 0 )
        {
            VDCRC_Transport_UART_Err ( "close ( %d ) failed: errno(%d)-%s.", exit_pipefd_[1], errno, strerror ( errno ) );
        }

        exit_pipefd_[1] = -1;
    }

    if ( fd_uart_ >= 0 )
    {
        if ( close ( fd_uart_ ) != 0 )
        {
            VDCRC_Transport_UART_Err ( "close ( %d ) failed: errno(%d)-%s.", fd_uart_, errno, strerror ( errno ) );
        }

        // reset handle
        fd_uart_ = -1;
    }

    VDCRC_Transport_UART_Debug ( "VDCRC_Transport_UART::disconnect()" );
}

bool VDCRC_Transport_UART::getIsConnected() const
{
    return ( fd_uart_ >= 0 );
}

int32_t VDCRC_Transport_UART::sendBytes ( const void *_buf, size_t _count )
{
    if ( ( nullptr == _buf ) || ( _count <= 0 ) )
    {
        VDCRC_Transport_UART_Err ( "VDCRC_Transport_UART::sendBytes() failed, _buf=%p, _count=%d", _buf, _count );
        return TBOX_ERROR_BASE;
    }

    const uint8_t *bytes = ( const uint8_t * ) _buf;

    log_out_bytes_ ( "write", bytes, _count );

    while ( _count > 0 )
    {
        ssize_t written = write ( fd_uart_, bytes, _count );

        if ( written <= 0 )
        {
            if ( ( written < 0 ) && ( EINTR == errno ) )
            {
                written = 0;
            }
            else
            {
                VDCRC_Transport_UART_Err ( "VDCRC_Transport_UART::sendBytes() failed, written = %d, errno(%d):%s", written, errno, strerror ( errno ) );
                return TBOX_ERROR_BASE;
            }
        }

        _count -= written;
        bytes += written;
    }

    return TBOX_SUCCESS;
}

void VDCRC_Transport_UART::UartRecv_Proc()
{
    const char *thread_name = "VDCRC_UART_Read";    // length is restricted to 16 characters, including '\0'
    pthread_setname_np ( pthread_self(), thread_name );

    VDCRC_Transport_UART_Info ( "VDCRC_Transport_UART::UartRecv_Proc() start." );

    uint8_t buffer[512] = {0};

    while ( true )
    {
        fd_set fdset;
        FD_ZERO ( &fdset );
        FD_SET ( fd_uart_, &fdset );
        FD_SET ( exit_pipefd_[0], &fdset );

        int nfds = std::max ( fd_uart_, exit_pipefd_[0] ) + 1;

        int fs_sel = select ( nfds, &fdset, NULL, NULL, NULL );
        if ( -1 == fs_sel )
        {
            if ( EINTR == errno )
            {
                VDCRC_Transport_UART_Debug ( "VDCRC_Transport_UART::UartRecv_Proc(), select EINTR" );
                continue;
            }

            VDCRC_Transport_UART_Err ( "VDCRC_Transport_UART::UartRecv_Proc() failed to select, errno(%d)-%s", errno, strerror ( errno ) );
            exit ( -1 );
        }
        else if ( 0 == fs_sel )
        {
            // no data in Rx buffer
            VDCRC_Transport_UART_Trace ( "VDCRC_Transport_UART::UartRecv_Proc(), no data in Rx buffer." );
        }
        else
        {
            if ( FD_ISSET ( exit_pipefd_[0], &fdset ) )
            {
                VDCRC_Transport_TCP_Debug ( "VDCRC_Transport_TCP::Recv_Proc(), receive EXIT." );
                break;
            }

            if ( FD_ISSET ( fd_uart_, &fdset ) )
            {
                ssize_t retval = 0;

                while ( ( retval = read ( fd_uart_, buffer, sizeof ( buffer ) ) ) != 0 )
                {
                    if ( -1 == retval )
                    {
                        if ( EINTR == errno )
                        {
                            continue;
                        }
                        else if ( EAGAIN == errno )
                        {
                            break;
                        }
                        else
                        {
                            VDCRC_Transport_UART_Critical ( "VDCRC_Transport_UART::UartRecv_Proc(), read failed, errno(%d)-%s", errno, strerror ( errno ) );
                            exit ( -1 );
                        }
                    }

                    log_out_bytes_ ( "read", buffer, retval );

                    // notify
                    if ( listener_ != nullptr )
                    {
                        listener_->onTransportBytesReceived ( buffer, retval );
                    }
                }
            }
        }
    }

    VDCRC_Transport_UART_Info ( "VDCRC_Transport_UART::UartRecv_Proc() end." );
}
