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

#include <MDS_WiFi.h>
#include <MDS_Time.h>
#include <time.h>

MDS_WiFi wifi;
MDS_Time clockService;

void setup() {
    Serial.begin(115200);

    MDS_WiFi::Options wifiOptions;

    wifiOptions.accessPointPrefix = "Clock";
    wifiOptions.connectionPolicy =
        MDS_WiFi::ConnectionPolicy::RetrySavedNetworkAsync;

    wifi.begin(wifiOptions);

    MDS_Time::Options timeOptions;

    // US Eastern with daylight saving. Use "UTC0" for UTC instead.
    // Alternatively, select FixedOffset mode and supply MDS_Weather's UTC offset.
    // Refresh that offset to follow daylight saving changes.
    timeOptions.timeZone = "EST5EDT,M3.2.0/2,M11.1.0/2";
    timeOptions.server1 = "pool.ntp.org";

    if (!clockService.begin(timeOptions)) {
        Serial.println(clockService.lastError());
    }
}

void loop() {
    wifi.tick();
    clockService.tick(wifi.ready());

    static uint32_t lastReport = 0;
    const uint32_t now = millis();

    if (now - lastReport >= 1000) {
        lastReport = now;

        struct tm local;

        if (clockService.localTime(local)) {
            char text[32];

            strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local);
            Serial.println(text);
        } else {
            Serial.println("Waiting for time synchronization...");
        }
    }
}
