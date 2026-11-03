/*
 * ===========================================================================
 * MDS_Time 0.1.0 - https://home.marcspencer.com/mds-time
 *
 * Copyright (c) 2026 Marc D. Spencer.
 * All Rights Reserved under the applicable license.
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * ===========================================================================
 */

#pragma once

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <atomic>
#include <functional>

// One opt-in owner of the ESP32 system clock/SNTP service and process timezone.
class MDS_Time {
public:
	enum class TimeZoneMode {
		Posix,
		FixedOffset
	};

	static constexpr const char* DefaultTimeZone = "EST5EDT,M3.2.0/2,M11.1.0/2";
	static constexpr const char* DefaultServer1 = "pool.ntp.org";
	static constexpr const char* DefaultServer2 = "time.nist.gov";
	static constexpr uint32_t DefaultInterval = 3600;

	struct Options {
		const char* timeZone = DefaultTimeZone;  // Posix mode only, e.g. UTC0/EST5 or DST rules.
		const char* server1 = DefaultServer1;
		const char* server2 = DefaultServer2;
		uint32_t intervalSeconds = DefaultInterval;

		// Appended to preserve existing aggregate initializers.
		TimeZoneMode timeZoneMode = TimeZoneMode::Posix;
		// FixedOffset mode only: seconds east of UTC, from -86400 to +86400.
		// No automatic DST; the application must supply any changed offset.
		int32_t utcOffsetSeconds = 0;
	};

	using StatusCallback = std::function<void(const String&)>;

	MDS_Time() = default;
	~MDS_Time();
	MDS_Time(const MDS_Time&) = delete;
	MDS_Time& operator=(const MDS_Time&) = delete;

	// Copy options and configure local conversion; unchanged options are a no-op.
	bool begin(const Options& options);

	// Start/restart background SNTP when networkReady; never wait for a response.
	void tick(bool networkReady);

	// True only after a successful NTP notification with a usable epoch.
	bool valid() const { return synchronized_; }
	time_t utcNow() const;
	time_t lastSyncUtc() const { return lastSyncUtc_; }

	// Convert system UTC using the configured POSIX timezone, without blocking.
	bool localTime(struct tm& result) const;
	const char* timeZone() const { return timeZone_.c_str(); }
	uint32_t configurationVersion() const { return configurationVersion_; }
	const char* lastError() const { return error_.c_str(); }
	void setStatusCallback(StatusCallback callback) { statusCallback_ = callback; }

private:
	static void synchronized(struct timeval* time);
	void status(const char* message);
	bool fail(const char* message);

	static std::atomic<bool> syncPending_;
	String timeZone_;
	String server1_;
	String server2_;
	String error_;
	StatusCallback statusCallback_;
	uint32_t intervalSeconds_ = DefaultInterval;
	uint32_t configurationVersion_ = 0;
	time_t lastSyncUtc_ = 0;
	bool configured_ = false;
	bool started_ = false;
	bool networkWasReady_ = false;
	bool synchronized_ = false;
};
