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

#include "MDS_Time.h"
#include <esp_sntp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

std::atomic<bool> MDS_Time::syncPending_{false};

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

void MDS_Time::synchronized(struct timeval*) {
	// SNTP calls from another task; user callbacks run later in tick().
	syncPending_.store(true);
}

void MDS_Time::status(const char* message) {
	if (statusCallback_) {
		statusCallback_(String(message));
	}
}

bool MDS_Time::fail(const char* message) {
	error_ = message;
	return false;
}

bool MDS_Time::begin(const Options& options) {
	const char* zoneText = options.timeZone;
	char fixedZone[20];

	switch (options.timeZoneMode) {
	case TimeZoneMode::Posix:
		if (!zoneText || !*zoneText || strlen(zoneText) > 128) {
			return fail("Invalid POSIX timezone");
		}
		break;

	case TimeZoneMode::FixedOffset: {
		const int32_t offset = options.utcOffsetSeconds;

		if (offset < -86400 || offset > 86400) {
			return fail("Invalid UTC offset");
		}

		const unsigned seconds = static_cast<unsigned>(offset < 0 ? -offset : offset);

		// POSIX signs are the reverse of seconds east of UTC (e.g. OpenWeather).
		snprintf(fixedZone, sizeof(fixedZone), "UTC%c%02u:%02u:%02u",
			offset < 0 ? '+' : '-', seconds / 3600, (seconds / 60) % 60, seconds % 60);
		zoneText = fixedZone;
		break;
	}

	default:
		return fail("Invalid timezone mode");
	}

	if (!options.server1 || !*options.server1 || strlen(options.server1) > 64
		|| !options.server2 || strlen(options.server2) > 64
		|| options.intervalSeconds < 60 || options.intervalSeconds > 86400) {
		return fail("Invalid time options");
	}

	if (configured_ && timeZone_ == zoneText && server1_ == options.server1
		&& server2_ == options.server2 && intervalSeconds_ == options.intervalSeconds) {
		error_ = "";
		return true;
	}

	String zone(zoneText);
	String first(options.server1);
	String second(options.server2);

	if (zone.length() != strlen(zoneText) || first.length() != strlen(options.server1)
		|| second.length() != strlen(options.server2)) {
		return fail("Cannot allocate time options");
	}

	// Stop the service before replacing strings borrowed by the SNTP implementation.
	if (started_) {
		esp_sntp_stop();
		started_ = false;
	}

	if (setenv("TZ", zone.c_str(), 1) != 0) {
		return fail("Cannot configure timezone");
	}

	tzset();
	timeZone_ = std::move(zone);
	server1_ = std::move(first);
	server2_ = std::move(second);
	intervalSeconds_ = options.intervalSeconds;
	esp_sntp_set_time_sync_notification_cb(synchronized);
	configured_ = true;
	++configurationVersion_;
	error_ = "";
	return true;
}

void MDS_Time::tick(bool networkReady) {
	if (!configured_) {
		return;
	}

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

		if (now >= 1577836800) {  // Reject pre-2020 startup/invalid timestamps.
			synchronized_ = true;
			lastSyncUtc_ = now;
			status("Time: synchronized");
		} else {
			synchronized_ = false;
			status("Time: invalid NTP timestamp");
		}
	}
}

time_t MDS_Time::utcNow() const {
	return time(nullptr);
}

bool MDS_Time::localTime(struct tm& result) const {
	if (!synchronized_) {
		return false;
	}

	const time_t now = utcNow();
	return localtime_r(&now, &result) != nullptr;
}
