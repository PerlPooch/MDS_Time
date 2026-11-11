/*
 * ===========================================================================
 * MDS_Time - https://home.marcspencer.com/mds-time
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

#include <cstdint>
#include <sys/time.h>

using SyncCallback = void (*)(struct timeval*);
inline SyncCallback syncCallback = nullptr;
inline int sntpStarts = 0;
inline int sntpStops = 0;
constexpr int ESP_SNTP_OPMODE_POLL = 0;

// Retain the notification callback so tests can simulate NTP responses.
inline void esp_sntp_set_time_sync_notification_cb(SyncCallback callback) {
	syncCallback = callback;
}

// Count shutdowns without starting a network service.
inline void esp_sntp_stop() {
	++sntpStops;
}

// Accept the requested operating mode without network activity.
inline void esp_sntp_setoperatingmode(int) {
}

// Accept borrowed server names; lifecycle coverage lives in the client suite.
inline void esp_sntp_setservername(int, const char*) {
}

// Accept the interval without scheduling background work.
inline void esp_sntp_set_sync_interval(uint32_t) {
}

// Count attempts to start background synchronization.
inline void esp_sntp_init() {
	++sntpStarts;
}
