/*
 * ===========================================================================
 * MDS_Time 0.2.0 - https://home.marcspencer.com/mds-time
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

#include "MDS_Time.h"
#include <esp_sntp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

std::atomic<bool> MDS_Time::syncPending_{false};

// Stop owned SNTP work and release its callback and borrowed server names.
MDS_Time::~MDS_Time() {
	if (configured_) {
		if (started_) {
			esp_sntp_stop();
		}

		esp_sntp_set_time_sync_notification_cb(nullptr);
		esp_sntp_setservername(0, nullptr);
		esp_sntp_setservername(1, nullptr);
		esp_sntp_setservername(2, nullptr);
	}
}

// Record an SNTP notification for handling in the main tick() call.
void MDS_Time::synchronized(struct timeval*) {
	// SNTP calls from another task; user callbacks run later in tick().
	syncPending_.store(true);
}

// Deliver a time service message when an application callback is registered.
void MDS_Time::status(const char* message) {
	if (statusCallback_) {
		statusCallback_(String(message));
	}
}

// Record a failure without emitting a status callback; return false to the caller.
bool MDS_Time::fail(Error error, const char* message) {
	error_ = error;
	errorText_ = message;
	return false;
}

// Clear the failure code and its diagnostic text after a successful operation.
void MDS_Time::clearError() {
	error_ = Error::None;
	errorText_ = "";
}

// Validate and copy time options; unchanged options leave the service undisturbed.
bool MDS_Time::begin(const Options& options) {
	const char* zoneText = options.timeZone;
	char fixedZone[20];

	switch (options.timeZoneMode) {
		case TimeZoneMode::Posix:
			if (!zoneText || !*zoneText || strlen(zoneText) > 128) {
				return fail(Error::InvalidOptions, "Invalid POSIX timezone");
			}
			break;

		case TimeZoneMode::FixedOffset: {
			const int32_t offset = options.utcOffsetSeconds;

			if (offset < -86400 || offset > 86400) {
				return fail(Error::InvalidOptions, "Invalid UTC offset");
			}

			const unsigned seconds = static_cast<unsigned>(offset < 0 ? -offset : offset);

			// POSIX signs are the reverse of seconds east of UTC (e.g. OpenWeather).
			snprintf(fixedZone, sizeof(fixedZone), "UTC%c%02u:%02u:%02u", offset < 0 ? '+' : '-',
				seconds / 3600, (seconds / 60) % 60, seconds % 60);
			zoneText = fixedZone;
			break;
		}

		default:
			return fail(Error::InvalidOptions, "Invalid timezone mode");
	}

	if (!options.server1 || !*options.server1 || strlen(options.server1) > 64 || !options.server2
		|| strlen(options.server2) > 64 || options.intervalSeconds < 60 || options.intervalSeconds > 86400) {
		return fail(Error::InvalidOptions, "Invalid time options");
	}

	if (configured_ && timeZone_ == zoneText && server1_ == options.server1 && server2_ == options.server2
		&& intervalSeconds_ == options.intervalSeconds) {
		// Reapplying options does not establish whether an invalid clock has recovered.
		if (error_ != Error::InvalidTimestamp) {
			clearError();
		}

		return true;
	}

	String zone(zoneText);
	String first(options.server1);
	String second(options.server2);

	if (zone.length() != strlen(zoneText) || first.length() != strlen(options.server1)
		|| second.length() != strlen(options.server2)) {
		return fail(Error::AllocationFailed, "Cannot allocate time options");
	}

	// Stop the service before replacing strings borrowed by the SNTP implementation.
	if (started_) {
		esp_sntp_stop();
		started_ = false;
	}

	if (setenv("TZ", zone.c_str(), 1) != 0) {
		return fail(Error::TimeZoneSetupFailed, "Cannot configure timezone");
	}

	tzset();
	timeZone_ = std::move(zone);
	server1_ = std::move(first);
	server2_ = std::move(second);
	intervalSeconds_ = options.intervalSeconds;
	esp_sntp_set_time_sync_notification_cb(synchronized);
	configured_ = true;
	++configurationVersion_;
	clearError();
	return true;
}

// Service synchronization notices and start SNTP when network readiness allows.
void MDS_Time::tick() {
	if (!configured_) {
		return;
	}

	const bool networkReady = networkReadyCallback_ && networkReadyCallback_();

	if (networkReady && (!started_ || !networkWasReady_)) {
		if (started_) {
			esp_sntp_stop();
		}

		esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
		esp_sntp_setservername(0, server1_.c_str());
		esp_sntp_setservername(1, server2_.length() ? server2_.c_str() : nullptr);
		esp_sntp_setservername(2, nullptr);
		esp_sntp_set_sync_interval(intervalSeconds_ * 1000UL);
		esp_sntp_init();
		started_ = true;
		status("Time: waiting for NTP");
	}

	if (!networkReady && networkWasReady_) {
		status(synchronized_ ? "Time: offline; clock continues" : "Time: WiFi unavailable; waiting");
	}

	networkWasReady_ = networkReady;

	if (syncPending_.exchange(false)) {
		const time_t now = utcNow();

		if (now >= 1577836800) { // Reject pre-2020 startup/invalid timestamps.
			synchronized_ = true;
			lastSyncUtc_ = now;

			// A valid clock resolves timestamp failures, not rejected configuration changes.
			if (error_ == Error::InvalidTimestamp) {
				clearError();
			}

			status("Time: synchronized");
		} else {
			synchronized_ = false;
			fail(Error::InvalidTimestamp, "Time: invalid NTP timestamp");
			status(errorText_);
		}
	}
}

// Read the system UTC clock; valid() reports whether NTP has synchronized it.
time_t MDS_Time::utcNow() const {
	return time(nullptr);
}

// Convert synchronized UTC into local calendar fields; return false before synchronization.
bool MDS_Time::localTime(struct tm& result) const {
	if (!synchronized_) {
		return false;
	}

	const time_t now = utcNow();
	return localtime_r(&now, &result) != nullptr;
}
