#include <faabric/util/config.h>
#include <faabric/util/logging.h>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace faabric::util {

void initLogging()
{
    {
        auto new_logger =
          spdlog::stdout_color_mt("default", spdlog::color_mode::always);
        spdlog::set_default_logger(std::move(new_logger));
    }

    // Docs: https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
    spdlog::set_pattern("%^[%H:%M:%S:%e] [%t] [%l]%$ %v");

    // Set the current level for the level for the default logger (note that the
    // minimum log level is determined in the header).
    const SystemConfig& conf = getSystemConfig();
    if (conf.logLevel == "trace") {
        spdlog::set_level(spdlog::level::trace);

        // Check the minimum level permits trace logging
#if SPDLOG_ACTIVE_LEVEL > SPDLOG_LEVEL_TRACE
        SPDLOG_WARN(
          "Logging set to trace but minimum log level set too high ({})",
          SPDLOG_ACTIVE_LEVEL);
#endif
    } else if (conf.logLevel == "debug") {
        spdlog::set_level(spdlog::level::debug);

        // Check the minimum level permits debug logging
#if SPDLOG_ACTIVE_LEVEL > SPDLOG_LEVEL_DEBUG
        SPDLOG_WARN(
          "Logging set to debug but minimum log level set too high ({})",
          SPDLOG_ACTIVE_LEVEL);
#endif
    } else {
        spdlog::set_level(spdlog::level::info);
    }
}
}
