#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Logger
{
	struct LogEntry
	{
		std::string m_strMessage;
		std::string m_strTimestamp;
	};

	inline std::vector<LogEntry> m_vecLogs;
	inline std::mutex m_mtxLogs;
	inline constexpr size_t MAX_LOGS = 500; // Maximum number of logs to keep

	/**
	 * Add a log message
	 * @param strMessage The log message
	 */
	inline void Log(const std::string& strMessage)
	{
		std::lock_guard<std::mutex> lock(m_mtxLogs);

		// Get current time
		auto tNow = std::chrono::system_clock::now();
		auto tTime = std::chrono::system_clock::to_time_t(tNow);
		auto tMs = std::chrono::duration_cast<std::chrono::milliseconds>(
			tNow.time_since_epoch()) % 1000;

		std::stringstream ss;
		ss << std::put_time(std::localtime(&tTime), "%H:%M:%S");
		ss << "." << std::setfill('0') << std::setw(3) << tMs.count();

		LogEntry entry;
		entry.m_strMessage = strMessage;
		entry.m_strTimestamp = ss.str();

		m_vecLogs.push_back(entry);

		// Keep only the last MAX_LOGS entries
		if (m_vecLogs.size() > MAX_LOGS)
		{
			m_vecLogs.erase(m_vecLogs.begin());
		}
	}

	/**
	 * Clear all logs
	 */
	inline void Clear()
	{
		std::lock_guard<std::mutex> lock(m_mtxLogs);
		m_vecLogs.clear();
	}

	/**
	 * Get all logs (thread-safe copy)
	 * @return Copy of log entries
	 */
	inline std::vector<LogEntry> GetLogs()
	{
		std::lock_guard<std::mutex> lock(m_mtxLogs);
		return m_vecLogs;
	}
}

