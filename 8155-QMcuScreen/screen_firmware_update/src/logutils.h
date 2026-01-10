#ifndef LOG_UTILS_H
#define LOG_UTILS_H
#include <sys/slog.h>
#include <sys/slogcodes.h>
#define dbg_out(fmt, args...)	slogf(_SLOG_SETCODE(_SLOG_SYSLOG, 0), _SLOG_DEBUG1, fmt, ##args)
#define info_out(fmt, args...)	slogf(_SLOG_SETCODE(_SLOG_SYSLOG, 0), _SLOG_INFO, fmt, ##args)
#define warn_out(fmt, args...)	slogf(_SLOG_SETCODE(_SLOG_SYSLOG, 0), _SLOG_WARNING, fmt, ##args)
#define err_out(fmt, args...)	slogf(_SLOG_SETCODE(_SLOG_SYSLOG, 0), _SLOG_ERROR, fmt, ##args)
#endif
