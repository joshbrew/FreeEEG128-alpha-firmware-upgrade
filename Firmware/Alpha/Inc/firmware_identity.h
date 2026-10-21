#ifndef FREEEEG128_FIRMWARE_IDENTITY_H
#define FREEEEG128_FIRMWARE_IDENTITY_H
#define FREEEEG128_VERSION "qos-5"
/* build.py sets a fingerprint of the prepared sources, compiler and build flags. */
#ifndef FREEEEG128_BUILD_ID
#define FREEEEG128_BUILD_ID "development"
#endif
#define FREEEEG128_BUILD_TAG "FreeEEG128-build:" FREEEEG128_BUILD_ID
#endif
