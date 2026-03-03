#include "logging/logger.hpp"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace anolis_provider_sim {
namespace logging {
namespace {

int get_fd(FILE *stream) {
#ifdef _WIN32
  return _fileno(stream);
#else
  return fileno(stream);
#endif
}

int dup_fd(int fd) {
#ifdef _WIN32
  return _dup(fd);
#else
  return dup(fd);
#endif
}

int dup2_fd(int from_fd, int to_fd) {
#ifdef _WIN32
  return _dup2(from_fd, to_fd);
#else
  return dup2(from_fd, to_fd);
#endif
}

void close_fd(int fd) {
#ifdef _WIN32
  (void)_close(fd);
#else
  (void)close(fd);
#endif
}

void set_env_var(const char *name, const char *value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void clear_env_var(const char *name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

class ScopedEnvVar {
public:
  ScopedEnvVar(const char *name, const char *value) : name_(name) {
    const char *original = std::getenv(name_.c_str());
    if (original != nullptr) {
      had_original_ = true;
      original_value_ = original;
    }

    if (value == nullptr) {
      clear_env_var(name_.c_str());
    } else {
      set_env_var(name_.c_str(), value);
    }
  }

  ~ScopedEnvVar() {
    if (had_original_) {
      set_env_var(name_.c_str(), original_value_.c_str());
    } else {
      clear_env_var(name_.c_str());
    }
  }

private:
  std::string name_;
  bool had_original_{false};
  std::string original_value_;
};

std::string capture_stderr(const std::function<void()> &fn) {
  std::fflush(stderr);

  FILE *tmp = std::tmpfile();
  if (tmp == nullptr) {
    throw std::runtime_error("tmpfile() failed");
  }

  const int stderr_fd = get_fd(stderr);
  const int tmp_fd = get_fd(tmp);
  if (stderr_fd < 0 || tmp_fd < 0) {
    std::fclose(tmp);
    throw std::runtime_error("failed to acquire file descriptors");
  }

  const int saved_stderr_fd = dup_fd(stderr_fd);
  if (saved_stderr_fd < 0) {
    std::fclose(tmp);
    throw std::runtime_error("dup(stderr) failed");
  }

  if (dup2_fd(tmp_fd, stderr_fd) < 0) {
    close_fd(saved_stderr_fd);
    std::fclose(tmp);
    throw std::runtime_error("dup2(tmp, stderr) failed");
  }

  try {
    fn();
  } catch (...) {
    std::fflush(stderr);
    (void)dup2_fd(saved_stderr_fd, stderr_fd);
    close_fd(saved_stderr_fd);
    std::fclose(tmp);
    throw;
  }

  std::fflush(stderr);
  if (dup2_fd(saved_stderr_fd, stderr_fd) < 0) {
    close_fd(saved_stderr_fd);
    std::fclose(tmp);
    throw std::runtime_error("dup2(saved, stderr) failed");
  }
  close_fd(saved_stderr_fd);

  std::rewind(tmp);
  std::string output;
  char buffer[512];
  while (true) {
    const size_t bytes_read = std::fread(buffer, 1, sizeof(buffer), tmp);
    if (bytes_read == 0) {
      break;
    }
    output.append(buffer, bytes_read);
  }

  std::fclose(tmp);
  return output;
}

class LoggerTest : public ::testing::Test {
protected:
  void SetUp() override { Logger::init(LogLevel::Info); }
  void TearDown() override { Logger::init(LogLevel::Info); }
};

TEST_F(LoggerTest, ParseLevelSupportsCaseInsensitiveValuesAndAliases) {
  bool ok = false;

  EXPECT_EQ(Logger::parse_level("debug", &ok), LogLevel::Debug);
  EXPECT_TRUE(ok);

  EXPECT_EQ(Logger::parse_level("Info", &ok), LogLevel::Info);
  EXPECT_TRUE(ok);

  EXPECT_EQ(Logger::parse_level("WARNING", &ok), LogLevel::Warn);
  EXPECT_TRUE(ok);

  EXPECT_EQ(Logger::parse_level("error", &ok), LogLevel::Error);
  EXPECT_TRUE(ok);

  EXPECT_EQ(Logger::parse_level("off", &ok), LogLevel::None);
  EXPECT_TRUE(ok);

  EXPECT_EQ(Logger::parse_level("not-a-level", &ok), LogLevel::Info);
  EXPECT_FALSE(ok);
}

TEST_F(LoggerTest, InitFromEnvWarnsOnInvalidValueAndFallsBackToInfo) {
  ScopedEnvVar invalid_level("ANOLIS_PROVIDER_SIM_LOG_LEVEL", "not-valid");

  const std::string stderr_out =
      capture_stderr([]() { Logger::init_from_env(); });

  EXPECT_EQ(Logger::level(), LogLevel::Info);
  EXPECT_NE(stderr_out.find("[WARN]"), std::string::npos);
  EXPECT_NE(stderr_out.find("[Logger]"), std::string::npos);
  EXPECT_NE(
      stderr_out.find("Invalid ANOLIS_PROVIDER_SIM_LOG_LEVEL='not-valid'"),
      std::string::npos);
}

TEST_F(LoggerTest, ThresholdFilteringSuppressesLowerPriorityMessages) {
  Logger::init(LogLevel::Error);

  const std::string stderr_out = capture_stderr([]() {
    Logger::log(LogLevel::Info, "UnitTest", __FILE__, __LINE__,
                "this should not be emitted");
    Logger::log(LogLevel::Error, "UnitTest", __FILE__, __LINE__,
                "this should be emitted");
  });

  EXPECT_EQ(stderr_out.find("this should not be emitted"), std::string::npos);
  EXPECT_NE(stderr_out.find("this should be emitted"), std::string::npos);
  EXPECT_NE(stderr_out.find("[ERROR]"), std::string::npos);
}

TEST_F(LoggerTest, MacrosSupportStreamedMessageExpressions) {
  Logger::init(LogLevel::Debug);

  const std::string stderr_out = capture_stderr([]() {
    const int value = 42;
    const std::string suffix = "ok";
    PSIM_LOG_INFO("MacroTest", "value=" << value << " status=" << suffix);
  });

  EXPECT_NE(stderr_out.find("[INFO]"), std::string::npos);
  EXPECT_NE(stderr_out.find("[MacroTest]"), std::string::npos);
  EXPECT_NE(stderr_out.find("value=42 status=ok"), std::string::npos);
}

} // namespace
} // namespace logging
} // namespace anolis_provider_sim
