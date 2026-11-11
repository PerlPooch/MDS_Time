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
#include <cstdlib>
#include <ctime>
#include <string>

inline bool failStringAllocation = false;
inline bool failTimeZoneSetup = false;

// Simulate Arduino's empty String result when allocation fails.
class String : public std::string {
public:
	using std::string::string;
	using std::string::operator=;

	// Construct an empty string without allocating storage.
	String() = default;

	// Fail only the next text allocation requested by the test.
	String(const char* text) : std::string(failStringAllocation ? "" : text) {
		failStringAllocation = false;
	}
};

// Inject a timezone setup failure while retaining real host timezone conversion.
inline int fakeSetenv(const char* name, const char* value, int overwrite) {
	return failTimeZoneSetup ? -1 : ::setenv(name, value, overwrite);
}

// Supply a deterministic UTC clock from the test fixture.
time_t fakeTime(time_t* result);

#define setenv fakeSetenv
#define time fakeTime
