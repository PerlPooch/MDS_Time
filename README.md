# MDS_Time

Background NTP synchronization with UTC time and POSIX time zone conversion.

## Requirements

- **Platforms:** ESP32.
- **Dependencies:** Time and SNTP services supplied by the ESP32 board package.

## Installation

Place the `MDS_Time` folder in your Arduino sketchbook's `libraries` directory. Install the required board package and dependencies using the Arduino IDE.

## Example

Print local date and time once a second, with automatic daylight saving changes. This example uses **MDS_WiFi** and **WiFiManager 2.0.17 or later** for connectivity.

With no saved Wi-Fi credentials, join the `Clock <MAC>` access point (password `12345678`) and enter your network details in its portal. NTP synchronization runs in the background; after synchronization, the clock keeps advancing even if Wi-Fi disconnects, until the device restarts.

```cpp
#include <MDS_WiFi.h>
#include <MDS_Time.h>
#include <time.h>

MDS_WiFi wifi;
MDS_Time clockService;

// Configure connectivity, readiness sources, and service options.
void setup() {
    Serial.begin(115200);

    MDS_WiFi::Options wifiOptions;

    wifiOptions.accessPointPrefix = "Clock";
    wifiOptions.connectionPolicy =
        MDS_WiFi::ConnectionPolicy::RetrySavedNetworkAsync;

    wifi.begin(wifiOptions);

    clockService.setNetworkReadyCallback([]() {
        // Use the connection manager's station and IP readiness.
        return wifi.ready();
    });

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

// Service background work and report newly available data.
void loop() {
    wifi.tick();
    clockService.tick();

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
```

## Documentation

Detailed design and API documentation: [MDS_Time](https://home.marcspencer.com/mds-time).

## License

Firmware and software: **MPL-2.0**. Documentation and website: **CC BY-SA 4.0**. Any hardware design files and Gerbers: **CERN-OHL-W-2.0**.

## Contributions

Contributions require a Developer Certificate of Origin (DCO) sign-off. Use `git commit -s` to add a `Signed-off-by` line to each contribution commit.

### Error reporting

`error()` returns `Error::None`, `InvalidOptions`, `AllocationFailed`,
`TimeZoneSetupFailed`, or `InvalidTimestamp`. `lastError()` supplies diagnostic
text; `valid()` independently reports whether NTP has established a usable clock.

Successful configuration clears errors. Reapplying unchanged options preserves
an invalid-timestamp error until a valid NTP notification arrives or the options
change. Valid NTP notifications clear timestamp failures, while rejected
configuration changes remain visible until a successful configuration call.
Queries, callback registration, network changes and ordinary retry ticks do not
clear errors. Configuration failures are returned to the caller; asynchronous
timestamp failures also use the status callback.

### Readiness callbacks

Register `setNetworkReadyCallback()` before expecting network work to start.
Callbacks return `bool` and run synchronously from `tick()`; keep them quick and
nonblocking. An unset callback returns false in effect. Pass `nullptr` to clear
one. Registration is independent of `begin(options)`: missing callbacks do not
invalidate options or overwrite errors. Call `tick()` without arguments.

Time still processes synchronization notifications while waiting; a synchronized
clock remains valid when network readiness is lost or its callback is cleared.
