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

#include <MDS_Time.h>
#include <esp_sntp.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using Error = MDS_Time::Error;
using TimeZoneMode = MDS_Time::TimeZoneMode;
static time_t currentUtc = 0;

// Return a deterministic system clock for timestamp validation.
time_t fakeTime(time_t* result) {
	if (result) {
		*result = currentUtc;
	}

	return currentUtc;
}

// Deliver an NTP notification; the service must handle it on the next tick.
static void notify(time_t utc) {
	currentUtc = utc;
	assert(syncCallback);
	timeval event{};
	event.tv_sec = utc;
	syncCallback(&event);
}

// Exercise each failure code, recovery path and non-clearing operation.
int main() {
	MDS_Time clock;
	MDS_Time::Options options;
	std::vector<String> messages;

	assert(clock.error() == Error::None);
	assert(std::strlen(clock.lastError()) == 0);
	assert(!clock.valid());
	clock.tick(false);
	assert(clock.error() == Error::None);

	clock.setStatusCallback([&](const String& message) {
		// Capture deferred diagnostics without writing to Serial.
		messages.push_back(message);
	});

	options.timeZone = nullptr;
	assert(!clock.begin(options));
	assert(clock.error() == Error::InvalidOptions);
	assert(std::strcmp(clock.lastError(), "Invalid POSIX timezone") == 0);
	assert(messages.empty());
	options.timeZone = MDS_Time::DefaultTimeZone;
	options.timeZoneMode = TimeZoneMode::FixedOffset;
	options.utcOffsetSeconds = 86401;
	assert(!clock.begin(options));
	assert(clock.error() == Error::InvalidOptions);
	options.timeZoneMode = static_cast<TimeZoneMode>(99);
	assert(!clock.begin(options));
	assert(clock.error() == Error::InvalidOptions);
	options = MDS_Time::Options{};
	options.intervalSeconds = 1;
	assert(!clock.begin(options));
	assert(clock.error() == Error::InvalidOptions);
	options = MDS_Time::Options{};

	failStringAllocation = true;
	assert(!clock.begin(options));
	assert(clock.error() == Error::AllocationFailed);
	assert(!clock.valid());
	failTimeZoneSetup = true;
	assert(!clock.begin(options));
	assert(clock.error() == Error::TimeZoneSetupFailed);
	assert(std::strlen(clock.lastError()) != 0);
	failTimeZoneSetup = false;
	assert(clock.begin(options));
	assert(clock.error() == Error::None);
	assert(std::strlen(clock.lastError()) == 0);

	clock.tick(true);
	notify(0);
	const auto beforeNotification = messages.size();
	assert(clock.error() == Error::None);
	clock.tick(true);
	assert(messages.size() == beforeNotification + 1);
	assert(messages.back() == "Time: invalid NTP timestamp");
	assert(clock.error() == Error::InvalidTimestamp);
	assert(!clock.valid());

	const auto version = clock.configurationVersion();
	assert(clock.begin(options));
	assert(clock.configurationVersion() == version);
	assert(clock.error() == Error::InvalidTimestamp);
	clock.setStatusCallback(nullptr);
	clock.tick(false);
	clock.tick(true);

	struct tm local {};

	assert(!clock.localTime(local));
	(void)clock.utcNow();
	(void)clock.lastSyncUtc();
	(void)clock.timeZone();
	assert(clock.error() == Error::InvalidTimestamp);

	notify(1700000000);
	clock.tick(true);
	assert(clock.valid());
	assert(clock.error() == Error::None);
	assert(std::strlen(clock.lastError()) == 0);

	// Successful NTP cannot repair a rejected configuration change.
	options.intervalSeconds = 1;
	assert(!clock.begin(options));
	notify(1700000100);
	clock.tick(true);
	assert(clock.valid());
	assert(clock.error() == Error::InvalidOptions);
	options.intervalSeconds = MDS_Time::DefaultInterval;
	assert(clock.begin(options));
	assert(clock.error() == Error::None);

	// An explicit options change clears an earlier timestamp failure.
	notify(0);
	clock.tick(true);
	assert(clock.error() == Error::InvalidTimestamp);
	options.timeZone = "UTC0";
	assert(clock.begin(options));
	assert(clock.error() == Error::None);
	assert(!clock.valid());
	assert(clock.configurationVersion() == version + 1);
	puts("PASS: Time error codes, failure injection, deferred timestamps and recovery.");
}
