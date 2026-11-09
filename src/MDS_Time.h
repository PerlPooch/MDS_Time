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
	enum class Error {
		None,
		InvalidOptions,
		AllocationFailed,
		TimeZoneSetupFailed,
		InvalidTimestamp
	};

	enum class TimeZoneMode {
		Posix,
		FixedOffset
	};

	static constexpr const char* DefaultTimeZone = "EST5EDT,M3.2.0/2,M11.1.0/2";
	static constexpr const char* DefaultServer1 = "pool.ntp.org";
	static constexpr const char* DefaultServer2 = "time.nist.gov";
	static constexpr uint32_t DefaultInterval = 3600;

	struct Options {
		const char* timeZone = DefaultTimeZone; // Posix mode only, e.g. UTC0/EST5 or DST rules.
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

	// Create an unconfigured time service; begin() supplies its options.
	MDS_Time() = default;

	// Stop owned SNTP work and release its callback and borrowed server names.
	~MDS_Time();

	// Prevent copying ownership of process-global SNTP state.
	MDS_Time(const MDS_Time&) = delete;

	// Prevent assignment of an object that owns process-global time settings.
	MDS_Time& operator=(const MDS_Time&) = delete;

	// Copy options and configure local conversion; unchanged options are a no-op.
	bool begin(const Options& options);

	// Start/restart background SNTP when networkReady; never wait for a response.
	void tick(bool networkReady);

	// True only after a successful NTP notification with a usable epoch.
	bool valid() const {
		return synchronized_;
	}

	// Read the system UTC clock; valid() reports whether NTP has synchronized it.
	time_t utcNow() const;

	// Return the latest successful synchronization time, or zero before the first sync.
	time_t lastSyncUtc() const {
		return lastSyncUtc_;
	}

	// Convert system UTC using the configured POSIX timezone, without blocking.
	bool localTime(struct tm& result) const;

	// Return the configured POSIX timezone expression used for local conversion.
	const char* timeZone() const {
		return timeZone_.c_str();
	}

	// Return the counter incremented after each successful options change.
	uint32_t configurationVersion() const {
		return configurationVersion_;
	}

	// Return the latest failure code; ordinary waits and unchanged options preserve timestamp errors.
	Error error() const {
		return error_;
	}

	// Return details of the latest failure, or an empty string when error() is None.
	const char* lastError() const {
		return errorText_;
	}

	// Register the time service status callback; nullptr clears it.
	void setStatusCallback(StatusCallback callback) {
		statusCallback_ = callback;
	}

private:
	// Record an SNTP notification for handling in the main tick() call.
	static void synchronized(struct timeval* time);

	// Deliver a time service message when an application callback is registered.
	void status(const char* message);

	// Record a failure without emitting a status callback; return false to the caller.
	bool fail(Error error, const char* message);

	// Clear the failure code and its diagnostic text after a successful operation.
	void clearError();

	static std::atomic<bool> syncPending_;
	String timeZone_;
	String server1_;
	String server2_;
	Error error_ = Error::None;
	const char* errorText_ = "";
	StatusCallback statusCallback_;
	uint32_t intervalSeconds_ = DefaultInterval;
	uint32_t configurationVersion_ = 0;
	time_t lastSyncUtc_ = 0;
	bool configured_ = false;
	bool started_ = false;
	bool networkWasReady_ = false;
	bool synchronized_ = false;
};
