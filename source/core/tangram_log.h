#pragma once
#include "tangram.h"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
std::shared_ptr<spdlog::logger>& getLogger();

#if TANGRAM_DEBUG
#define TLOG_TRACE(...)    getLogger()->trace(__VA_ARGS__)
#define TLOG_INFO(...)     getLogger()->info(__VA_ARGS__)
#define TLOG_WARN(...)     getLogger()->warn(__VA_ARGS__)
#define TLOG_ERROR(...)    getLogger()->error(__VA_ARGS__)
#define TLOG_CRITICAL(...) getLogger()->critical(__VA_ARGS__)
#define TLOG_UNIMPLEMENTED(...)     getLogger()->warn(__VA_ARGS__)
#define TLOG_IMPLEMENTED(...)     getLogger()->trace(__VA_ARGS__)
#define TLOG_RELEASE(...)     getLogger()->trace(__VA_ARGS__)
#else

#define TLOG_TRACE(...)    
#define TLOG_INFO(...)     
#define TLOG_WARN(...)     
#define TLOG_ERROR(...)    
#define TLOG_CRITICAL(...) 
#define TLOG_RELEASE(...)  getLogger()->trace(__VA_ARGS__)

#endif