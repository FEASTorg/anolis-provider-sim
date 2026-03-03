#pragma once

#include <sstream>
#include <string>

namespace anolis_provider_sim {
namespace logging {

enum class LogLevel {
  Debug,
  Info,
  Warn,
  Error,
  None,
};

class Logger {
public:
  static void init(LogLevel threshold = LogLevel::Info);
  static void init_from_env(
      const char *env_var = "ANOLIS_PROVIDER_SIM_LOG_LEVEL");
  static void set_level(LogLevel threshold);
  static LogLevel level();

  static LogLevel parse_level(const std::string &value, bool *ok = nullptr);
  static const char *to_string(LogLevel level);

  static void log(LogLevel level, const std::string &component,
                  const char *file, int line, const std::string &message);
};

} // namespace logging
} // namespace anolis_provider_sim

#define PSIM_LOG_INTERNAL(level, component, msg)                              \
  do {                                                                        \
    std::ostringstream _psim_log_component_ss;                               \
    _psim_log_component_ss << component;                                      \
    std::ostringstream _psim_log_message_ss;                                 \
    _psim_log_message_ss << msg;                                              \
    ::anolis_provider_sim::logging::Logger::log(                              \
        (level), _psim_log_component_ss.str(), __FILE__, __LINE__,            \
        _psim_log_message_ss.str());                                          \
  } while (0)

#define PSIM_LOG_DEBUG(component, msg)                                        \
  PSIM_LOG_INTERNAL(::anolis_provider_sim::logging::LogLevel::Debug,          \
                    (component), (msg))
#define PSIM_LOG_INFO(component, msg)                                         \
  PSIM_LOG_INTERNAL(::anolis_provider_sim::logging::LogLevel::Info,           \
                    (component), (msg))
#define PSIM_LOG_WARN(component, msg)                                         \
  PSIM_LOG_INTERNAL(::anolis_provider_sim::logging::LogLevel::Warn,           \
                    (component), (msg))
#define PSIM_LOG_ERROR(component, msg)                                        \
  PSIM_LOG_INTERNAL(::anolis_provider_sim::logging::LogLevel::Error,          \
                    (component), (msg))
