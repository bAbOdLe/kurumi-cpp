#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include "Config.hpp"

namespace kurumi {

    void to_json(nlohmann::json& json, const Config& config);
    void from_json(const nlohmann::json& json, Config& config);


    std::optional<Config> Config::load(std::filesystem::path config_path) {

        try {
            std::ifstream _file{config_path};

            if (!_file.is_open()) {
                return std::nullopt;
            }

            nlohmann::json _json = nlohmann::json::parse(_file);
            return _json.get<Config>();
        }
        catch (...) {
            return std::nullopt;
        }
    }

    bool Config::save(std::filesystem::path config_path, const Config& config) {

        std::ofstream _file{config_path};

        if (!_file.is_open()) {
            return false;
        }

        std::print(_file,"{}", nlohmann::json{config}.dump(4));
        return true;
    }

    void to_json(nlohmann::json& json, const Config& config) {

        json = nlohmann::json {
            {"toss_api_key", config.toss_api_key},
            {"naver_api_key", config.naver_api_key},
            { "discord", nlohmann::json {
                {"bot_token", config.discord.bot_token },
                {"guild_id", config.discord.guild_id },
                {"channel_id", config.discord.channel_id }
            }}
        };
    }

    void from_json(const nlohmann::json& json, Config& config) {

        json.at("toss_api_key").get_to(config.toss_api_key);
        json.at("naver_api_key").get_to(config.naver_api_key);
        json.at("discord").at("bot_token").get_to(config.discord.bot_token);
        json.at("discord").at("guild_id").get_to(config.discord.guild_id);
        json.at("discord").at("channel_id").get_to(config.discord.channel_id);
    }
}
