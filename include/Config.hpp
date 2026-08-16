//
// Created by tgian on 2026-08-16.
//

#ifndef KURUMI_CONFIG_HPP
#define KURUMI_CONFIG_HPP
#include <filesystem>
#include <cinttypes>

namespace kurumi {

    struct Config {

        struct Discord {
            std::string bot_token;
            std::string guild_id;
            std::string channel_id;
        };

        Config() = default;

        std::string toss_api_key;
        std::string naver_api_key;
        Discord discord;

        static std::optional<Config> load(std::filesystem::path config_path);
        static bool save(std::filesystem::path config_path, const Config& config);
    };
}
#endif //KURUMI_CONFIG_HPP
