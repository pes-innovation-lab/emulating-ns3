#ifndef SIM_COMMON_ENV_CONFIG_H
#define SIM_COMMON_ENV_CONFIG_H

#include <cstdint>
#include <cstdlib>
#include <string>

inline std::string
GetEnvStr(const char *name, const std::string &def)
{
  const char *v = std::getenv(name);
  return (v && *v) ? std::string(v) : def;
}

inline double
GetEnvDouble(const char *name, double def)
{
  const char *v = std::getenv(name);
  return (v && *v) ? std::atof(v) : def;
}

inline uint32_t
GetEnvUint(const char *name, uint32_t def)
{
  const char *v = std::getenv(name);
  return (v && *v) ? (uint32_t) std::atoi(v) : def;
}

#endif // SIM_COMMON_ENV_CONFIG_H
