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

#include <MDS_Time.h>
#include <esp_sntp.h>
#include <cassert>
#include <cstdio>
#include <vector>

static time_t currentUtc = 1700000000;

// Return a deterministic system clock for deferred synchronization processing.
time_t fakeTime(time_t* result) {
    if (result) {
        *result = currentUtc;
    }

    return currentUtc;
}

// Verify safe callback defaults without interrupting an established clock.
int main() {
    bool networkReady = false;
    int networkQueries = 0;
    std::vector<String> messages;
    MDS_Time clock;
    MDS_Time::Options options;

    clock.setStatusCallback([&](const String& message) {
        // Detect repeated waiting diagnostics and deferred notifications.
        messages.push_back(message);
    });
    assert(clock.begin(options));
    clock.tick();
    clock.tick();
    assert(sntpStarts == 0 && messages.empty());
    assert(clock.error() == MDS_Time::Error::None);

    clock.setNetworkReadyCallback([&]() {
        // Reflect connectivity changes and count calls per tick.
        ++networkQueries;
        return networkReady;
    });
    clock.tick();
    assert(networkQueries == 1 && sntpStarts == 0);

    networkReady = true;
    clock.tick();
    assert(networkQueries == 2 && sntpStarts == 1);
    clock.tick();
    assert(networkQueries == 3 && sntpStarts == 1);

    // A queued NTP notification must still be consumed after clearing connectivity.
    timeval notification{};
    notification.tv_sec = currentUtc;
    syncCallback(&notification);
    clock.setNetworkReadyCallback(nullptr);
    clock.tick();
    assert(clock.valid() && clock.lastSyncUtc() == currentUtc);
    assert(networkQueries == 3 && sntpStarts == 1);

    const auto messageCount = messages.size();
    ++currentUtc;
    clock.tick();
    assert(clock.valid() && clock.utcNow() == currentUtc);
    assert(messages.size() == messageCount && sntpStarts == 1);
    assert(clock.begin(options));

    clock.setNetworkReadyCallback([&]() {
        // Reconnecting should restart synchronization exactly once.
        return networkReady;
    });
    clock.tick();
    assert(sntpStarts == 2);
    networkReady = false;
    clock.tick();
    clock.tick();
    assert(clock.valid() && sntpStarts == 2);
    networkReady = true;
    clock.tick();
    assert(sntpStarts == 3);

    currentUtc = 0;
    syncCallback(&notification);
    clock.setNetworkReadyCallback(nullptr);
    clock.tick();
    assert(!clock.valid() && clock.error() == MDS_Time::Error::InvalidTimestamp);
    clock.tick();
    assert(clock.error() == MDS_Time::Error::InvalidTimestamp && sntpStarts == 3);

    puts("PASS: Time unset, changing and cleared readiness callbacks; deferred synchronization and errors.");
}
