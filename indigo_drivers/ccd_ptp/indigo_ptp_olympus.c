// Copyright (c) 2026 CloudMakers, s. r. o.
// All rights reserved.
//
// You can use this software under the terms of 'INDIGO Astronomy
// open-source license' (see LICENSE.md).
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS 'AS IS' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/** INDIGO PTP Olympus/OM System implementation
 \file indigo_ptp_olympus.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>

#include <indigo/indigo_ccd_driver.h>
#include <indigo/indigo_usb_utils.h>

#include "indigo_ptp.h"
#include "indigo_ptp_olympus.h"

#define OLYMPUS_PRIVATE_DATA	((olympus_private_data *)(PRIVATE_DATA->vendor_private_data))
#define OLYMPUS_CAPTURE_PRESS 0x03
#define OLYMPUS_CAPTURE_RELEASE 0x06
#define OLYMPUS_CAMERA_CONTROL_MODE_PC 0x0001

char *ptp_operation_olympus_code_label(uint16_t code) {
	switch (code) {
		case ptp_operation_olympus_Capture: return "ptp_operation_olympus_Capture";
		case ptp_operation_olympus_GetDateTime: return "ptp_operation_olympus_GetDateTime";
		case ptp_operation_olympus_GetLiveViewImage: return "ptp_operation_olympus_GetLiveViewImage";
		case ptp_operation_olympus_GetImage: return "ptp_operation_olympus_GetImage";
		case ptp_operation_olympus_ChangedProperties: return "ptp_operation_olympus_ChangedProperties";
		case ptp_operation_olympus_MFDrive: return "ptp_operation_olympus_MFDrive";
		case ptp_operation_olympus_SetProperties: return "ptp_operation_olympus_SetProperties";
	}
	return ptp_operation_code_label(code);
}

char *ptp_event_olympus_code_label(uint16_t code) {
	switch (code) {
		case ptp_event_olympus_ObjectAddedLegacy: return "ptp_event_olympus_ObjectAddedLegacy";
		case ptp_event_olympus_DevicePropChangedLegacy: return "ptp_event_olympus_DevicePropChangedLegacy";
		case ptp_event_olympus_CreateRecView: return "ptp_event_olympus_CreateRecView";
		case ptp_event_olympus_ObjectAdded: return "ptp_event_olympus_ObjectAdded";
		case ptp_event_olympus_CaptureComplete: return "ptp_event_olympus_CaptureComplete";
		case ptp_event_olympus_DevicePropChanged: return "ptp_event_olympus_DevicePropChanged";
	}
	return ptp_event_code_label(code);
}

char *ptp_property_olympus_code_name(uint16_t code) {
	switch (code) {
		case ptp_property_ExposureTime: return DSLR_SHUTTER_PROPERTY_NAME;
		case ptp_property_FNumber: return DSLR_APERTURE_PROPERTY_NAME;
		case ptp_property_ExposureProgramMode: return DSLR_PROGRAM_PROPERTY_NAME;
		case ptp_property_ExposureIndex: return DSLR_ISO_PROPERTY_NAME;
		case ptp_property_WhiteBalance: return DSLR_WHITE_BALANCE_PROPERTY_NAME;
		case ptp_property_CompressionSetting: return DSLR_COMPRESSION_PROPERTY_NAME;
		case ptp_property_FocusMode: return DSLR_FOCUS_MODE_PROPERTY_NAME;
		case ptp_property_FocusMeteringMode: return DSLR_FOCUS_METERING_PROPERTY_NAME;
		case ptp_property_ExposureMeteringMode: return DSLR_EXPOSURE_METERING_PROPERTY_NAME;
		case ptp_property_ExposureBiasCompensation: return DSLR_EXPOSURE_COMPENSATION_PROPERTY_NAME;
		case ptp_property_BatteryLevel: return DSLR_BATTERY_LEVEL_PROPERTY_NAME;
		case ptp_property_StillCaptureMode: return DSLR_CAPTURE_MODE_PROPERTY_NAME;
		// vendor codes confirmed on a real OM-1: the body exposes NO standard
		// exposure properties, all controls live in the 0xD0xx range
		case ptp_property_olympus_Aperture: return DSLR_APERTURE_PROPERTY_NAME;
		case ptp_property_olympus_FocusMode: return DSLR_FOCUS_MODE_PROPERTY_NAME;
		case ptp_property_olympus_ExposureMeteringMode: return DSLR_EXPOSURE_METERING_PROPERTY_NAME;
		case ptp_property_olympus_ISO: return DSLR_ISO_PROPERTY_NAME;
		case ptp_property_olympus_ISOSensitivity: return DSLR_ISO_PROPERTY_NAME;
		// d00c is a coarse still/movie state register, not a mode selector (the OM-1
		// does not export the P/A/S/M dial position at all), keep it out of the UI
		case ptp_property_olympus_ExposureProgram: return "ADV_CameraState";
		case ptp_property_olympus_ExposureBias: return DSLR_EXPOSURE_COMPENSATION_PROPERTY_NAME;
		case ptp_property_olympus_DriveMode: return DSLR_CAPTURE_MODE_PROPERTY_NAME;
		case ptp_property_olympus_ImageFormat: return DSLR_COMPRESSION_PROPERTY_NAME;
		case ptp_property_olympus_Shutterspeed: return DSLR_SHUTTER_PROPERTY_NAME;
		case ptp_property_olympus_WhiteBalance: return DSLR_WHITE_BALANCE_PROPERTY_NAME;
		// still unidentified or not user-facing, kept in the advanced group
		case ptp_property_olympus_ExposureCompensation: return "ADV_ExposureCompensationLegacy";
		case ptp_property_olympus_ColorTemperature: return "ADV_ColorTemperature";
		case ptp_property_olympus_FaceDetection: return "ADV_FaceDetection";
		case ptp_property_olympus_AspectRatio: return "ADV_AspectRatio";
		case ptp_property_olympus_AFArea: return "ADV_AFArea";
		case ptp_property_olympus_CameraControlMode: return "ADV_CameraControlMode";
		case ptp_property_olympus_LiveViewModeOM: return "ADV_LiveViewModeOM";
		case ptp_property_olympus_CaptureTarget: return "ADV_CaptureTarget";
	}
	return ptp_property_code_name(code);
}

char *ptp_property_olympus_code_label(uint16_t code) {
	switch (code) {
		case ptp_property_olympus_Aperture: return "Aperture";
		case ptp_property_olympus_FocusMode: return "Focus mode";
		case ptp_property_olympus_ExposureMeteringMode: return "Exposure metering";
		case ptp_property_olympus_ISO: return "ISO";
		case ptp_property_olympus_ISOSensitivity: return "ISO";
		case ptp_property_olympus_ExposureProgram: return "Camera state";
		case ptp_property_olympus_ExposureBias: return "Exposure compensation";
		case ptp_property_olympus_ExposureCompensation: return "Exposure compensation (legacy)";
		case ptp_property_olympus_DriveMode: return "Drive mode";
		case ptp_property_olympus_ImageFormat: return "Image format";
		case ptp_property_olympus_ColorTemperature: return "Color temperature";
		case ptp_property_olympus_FaceDetection: return "Face detection";
		case ptp_property_olympus_AspectRatio: return "Aspect ratio";
		case ptp_property_olympus_Shutterspeed: return "Shutter speed";
		case ptp_property_olympus_WhiteBalance: return "White balance";
		case ptp_property_olympus_AFArea: return "AF area";
		case ptp_property_olympus_CameraControlMode: return "Camera control mode";
		case ptp_property_olympus_LiveViewModeOM: return "Live view mode";
		case ptp_property_olympus_CaptureTarget: return "Capture target";
	}
	return ptp_property_code_label(code);
}

char *ptp_property_olympus_value_code_label(indigo_device *device, uint16_t property, uint64_t code) {
	static char label[PTP_MAX_CHARS];
	switch (property) {
		case ptp_property_ExposureProgramMode:
			// hidden standard property, values confirmed on a real OM-1 (2 = dial P)
			switch (code) {
				case 1: return "M";
				case 2: return "P";
				case 3: return "A";
				case 4: return "S";
			}
			break;
		case ptp_property_olympus_Aperture: {
			// confirmed on OM-1: f-number * 10 (0x0a = f/1.0, 0x28 = f/4.0)
			sprintf(label, "f/%.1f", (int)code / 10.0);
			return label;
		}
		case ptp_property_olympus_Shutterspeed: {
			// confirmed on OM-1: the B dial position reports one of these sentinels
			// depending on the selected sub-mode
			switch ((uint32_t)code) {
				case 0xFFFFFFFC: return "Bulb";
				case 0xFFFFFFFD: return "Live Time";
				case 0xFFFFFFFA: return "Live Comp";
			}
			// confirmed on OM-1: numerator << 16 | denominator, in seconds
			int numerator = (int)(code >> 16);
			int denominator = (int)(code & 0xFFFF);
			if (denominator == 0) {
				break;
			}
			if (numerator == 1) {
				sprintf(label, "1/%d", denominator);
			} else if (numerator % denominator == 0) {
				sprintf(label, "%d\"", numerator / denominator);
			} else {
				sprintf(label, "%g\"", (double)numerator / denominator);
			}
			return label;
		}
		case ptp_property_olympus_ISOSensitivity: {
			if (code == 0xFFFFFFFF) {
				return "Auto";
			}
			sprintf(label, "%d", (int)code);
			return label;
		}
		case ptp_property_olympus_ExposureBias: {
			// confirmed on OM-1: signed EV * 1000 (300 = +0.3 EV, 62536 = -3.0 EV)
			sprintf(label, "%+.1f", (int16_t)code / 1000.0);
			return label;
		}
		case ptp_property_olympus_ExposureProgram:
			// confirmed by a full dial sweep on a real OM-1: this is NOT the P/A/S/M
			// selector (all native stills modes report 0x8802), it encodes the
			// capture family only
			switch (code) {
				case 0x8100: return "Still (electronic shutter)";
				case 0x8801: return "Still (custom mode)";
				case 0x8802: return "Still";
				case 0x8804: return "Movie";
			}
			break;
		case ptp_property_olympus_DriveMode:
			// confirmed on OM-1 by stepping through the drive menu: 0x20 flag =
			// silent (electronic shutter), 0x40 flag = Pro Capture
			switch (code) {
				case 0x01: return "Single";
				case 0x21: return "Silent Single";
				case 0x07: return "Sequential";
				case 0x27: return "Silent Sequential";
				case 0x28: return "SH1";
				case 0x29: return "SH2";
				case 0x43: return "Pro Capture";
				case 0x48: return "Pro Capture SH1";
				case 0x49: return "Pro Capture SH2";
				case 0x04: return "Self-timer 12s";
				case 0x05: return "Self-timer 2s";
				case 0x24: return "Silent Self-timer 2s";
				case 0x06: return "Custom Self-timer";
			}
			break;
		case ptp_property_olympus_ImageFormat:
			switch (code) {
				case 0x020: return "RAW";
				case 0x101: return "JPEG SF";
				case 0x102: return "JPEG F";
				case 0x103: return "JPEG N";
				case 0x104: return "JPEG B";
				case 0x121: return "RAW + JPEG SF";
				case 0x122: return "RAW + JPEG F";
				case 0x123: return "RAW + JPEG N";
				case 0x124: return "RAW + JPEG B";
			}
			break;
		case ptp_property_olympus_FocusMode:
			switch (code) {
				case 1: return "MF";
				case 2: return "S-AF";
				case 0x8002: return "C-AF";
				case 0x8004: return "S-AF + MF";
				case 0x8007: return "Starry Sky AF";
			}
			break;
		case ptp_property_olympus_ExposureMeteringMode:
			switch (code) {
				case 2: return "Center weighted";
				case 4: return "Spot";
				case 0x8001: return "ESP";
				case 0x8011: return "Spot highlight";
				case 0x8012: return "Spot shadow";
			}
			break;
		case ptp_property_olympus_WhiteBalance:
			switch (code) {
				case 1: return "Auto";
				case 2: return "Sunny";
				case 3: return "Shade";
				case 4: return "Cloudy";
				case 5: return "Incandescent";
				case 6: return "Fluorescent";
				case 7: return "Underwater";
				case 8: return "Flash";
				case 9: return "Custom 1";
				case 10: return "Custom 2";
				case 11: return "Custom 3";
				case 12: return "Custom 4";
				case 13: return "Color temperature";
			}
			break;
		case ptp_property_olympus_CaptureTarget:
			switch (code) {
				case 1: return "RAM";
				case 2: return "Card";
				case 3: return "RAM + Card";
			}
			break;
		case ptp_property_olympus_CameraControlMode:
			switch (code) {
				case 1: return "PC control";
				case 2: return "Camera";
			}
			break;
	}
	return ptp_property_value_code_label(device, property, code);
}

bool ptp_olympus_fix_property(indigo_device *device, ptp_property *property) {
	switch (property->code) {
		case ptp_property_olympus_ImageFormat: {
			OLYMPUS_PRIVATE_DATA->is_dual_compression = property->value.sw.value >= 0x121 && property->value.sw.value <= 0x124;
			return true;
		}
		case ptp_property_ExposureProgramMode: {
			// the descriptor claims the property is settable but the camera answers
			// DevicePropNotSupported to writes, the physical dial is the only authority
			property->writable = false;
			return true;
		}
	}
	return false;
}

bool ptp_olympus_handle_event(indigo_device *device, ptp_event_code code, uint32_t *params) {
	switch ((int)code) {
		case ptp_event_olympus_ObjectAddedLegacy:
		case ptp_event_olympus_ObjectAdded: {
			// OM-1: param1 is the handle of the newly stored image, the camera sends
			// one event per storage slot the image was saved to - download the first
			// copy only and skip (optionally delete) the second one
			INDIGO_DRIVER_LOG(DRIVER_NAME, "%s: param1 = %08x", ptp_event_olympus_code_label(code), params[0]);
			if (params[0] != 0) {
				void *buffer = NULL;
				if (ptp_transaction_1_0_i(device, ptp_operation_GetObjectInfo, params[0], &buffer, NULL)) {
					uint32_t size;
					char filename[PTP_MAX_CHARS];
					uint8_t *source = buffer;
					source = ptp_decode_uint32(source + 8, &size);
					ptp_decode_string(source + 40, filename);
					free(buffer);
					if (size == OLYMPUS_PRIVATE_DATA->last_object_size && !strcmp(filename, OLYMPUS_PRIVATE_DATA->last_object_name)) {
						INDIGO_DRIVER_LOG(DRIVER_NAME, "duplicate copy of '%s' from second storage slot skipped", filename);
						if (DSLR_DELETE_IMAGE_ON_ITEM->sw.value) {
							ptp_transaction_1_0(device, ptp_operation_DeleteObject, params[0]);
						}
						return true;
					}
					strncpy(OLYMPUS_PRIVATE_DATA->last_object_name, filename, sizeof(OLYMPUS_PRIVATE_DATA->last_object_name));
					OLYMPUS_PRIVATE_DATA->last_object_name[sizeof(OLYMPUS_PRIVATE_DATA->last_object_name) - 1] = '\0';
					OLYMPUS_PRIVATE_DATA->last_object_size = size;
				}
				return ptp_handle_event(device, ptp_event_ObjectAdded, params);
			}
			return true;
		}
		case ptp_event_olympus_CaptureComplete:
			// OM-1: param1 is NOT an object handle, the image arrives via 0xC102,
			// downloading here would fetch (and possibly delete) an unrelated object
			INDIGO_DRIVER_LOG(DRIVER_NAME, "%s: param1 = %08x", ptp_event_olympus_code_label(code), params[0]);
			return true;
		case ptp_event_olympus_CreateRecView:
			return true;
		case ptp_event_olympus_DevicePropChangedLegacy:
		case ptp_event_olympus_DevicePropChanged:
			// param1 is the changed vendor property code
			return ptp_handle_event(device, ptp_event_DevicePropChanged, params);
	}
	return ptp_handle_event(device, code, params);
}

static void ptp_olympus_check_event(indigo_device *device) {
#ifdef USE_ICA_TRANSPORT
	ptp_get_event(device);
#else
	ptp_container event;
	int length = 0;
	memset(&event, 0, sizeof(event));
	int rc = libusb_bulk_transfer(PRIVATE_DATA->handle, PRIVATE_DATA->ep_int, (unsigned char *)&event, sizeof(event), &length, 1000);
	if (rc >= 0) {
		INDIGO_DRIVER_DEBUG(DRIVER_NAME, "libusb_bulk_transfer() -> %s, %d", rc < 0 ? libusb_error_name(rc) : "OK", length);
		PTP_DUMP_CONTAINER(&event);
		ptp_olympus_handle_event(device, event.code, event.payload.params);
	}
#endif
	if (ptp_operation_supported(device, ptp_operation_olympus_ChangedProperties)) {
		void *buffer = NULL;
		uint32_t size = 0;
		if (ptp_transaction_0_0_i(device, ptp_operation_olympus_ChangedProperties, &buffer, &size) && buffer && size >= sizeof(uint32_t)) {
			uint32_t count = 0;
			uint8_t *source = ptp_decode_uint32(buffer, &count);
			// the payload is a count-prefixed dump of all vendor property descriptors
			// (~11KB on the OM-1), the OM-1 does not send c108 for every camera-side
			// change (e.g. dial moves between P/A/S/M) and value changes may keep the
			// size, so detect changes by checksum and refresh the mapped controls
			uint32_t checksum = 2166136261u;
			for (uint32_t i = 0; i < size; i++) {
				checksum = (checksum ^ ((uint8_t *)buffer)[i]) * 16777619u;
			}
			if (count > 0 && checksum != OLYMPUS_PRIVATE_DATA->last_changed_checksum) {
				OLYMPUS_PRIVATE_DATA->last_changed_checksum = checksum;
				static uint16_t core_properties[] = {
					ptp_property_ExposureProgramMode,
					ptp_property_olympus_Aperture,
					ptp_property_olympus_FocusMode,
					ptp_property_olympus_ExposureMeteringMode,
					ptp_property_olympus_DriveMode,
					ptp_property_olympus_ImageFormat,
					ptp_property_olympus_ExposureBias,
					ptp_property_olympus_Shutterspeed,
					ptp_property_olympus_WhiteBalance,
					ptp_property_olympus_ISOSensitivity,
					0
				};
				for (int i = 0; core_properties[i]; i++) {
					ptp_property *property = ptp_property_supported(device, core_properties[i]);
					if (property && ptp_refresh_property(device, property)) {
						ptp_olympus_fix_property(device, property);
						ptp_update_property(device, property);
					}
				}
				if (size == sizeof(uint32_t) + count * sizeof(uint16_t) && count <= PTP_MAX_ELEMENTS) {
					for (uint32_t i = 0; i < count; i++) {
						uint16_t property_code = 0;
						source = ptp_decode_uint16(source, &property_code);
						ptp_property *property = ptp_property_supported(device, property_code);
						if (property && ptp_refresh_property(device, property)) {
							ptp_update_property(device, property);
						}
					}
				}
			}
		}
		if (buffer) {
			free(buffer);
		}
	}
	if (IS_CONNECTED) {
		indigo_reschedule_timer(device, 1, &PRIVATE_DATA->event_checker);
	}
}

bool ptp_olympus_initialise(indigo_device *device) {
	DSLR_MIRROR_LOCKUP_PROPERTY->hidden = true;
	PRIVATE_DATA->vendor_private_data = indigo_safe_malloc(sizeof(olympus_private_data));
	// the OM Capture application reads the storage ids before switching to PC control
	// mode, some bodies seem to depend on that order (libgphoto2 camera_init)
	void *buffer = NULL;
	uint32_t size = 0;
	if (ptp_transaction_0_0_i(device, ptp_operation_GetStorageIDs, &buffer, &size)) {
		uint32_t count = 0;
		ptp_decode_uint32(buffer, &count);
		INDIGO_DRIVER_LOG(DRIVER_NAME, "ptp_operation_GetStorageIDs: %d storage(s)", count);
	}
	if (buffer) {
		free(buffer);
		buffer = NULL;
	}
	// switch to PC control mode (libgphoto2 ptp_olympus_init_pc_mode), non-fatal so
	// that a failed run still produces the device info dump needed for bring-up
	uint16_t value = OLYMPUS_CAMERA_CONTROL_MODE_PC;
	if (ptp_transaction_0_1_o(device, ptp_operation_SetDevicePropValue, ptp_property_olympus_CameraControlMode, &value, sizeof(uint16_t))) {
		INDIGO_DRIVER_LOG(DRIVER_NAME, "CameraControlMode set to PC control");
	} else {
		INDIGO_DRIVER_LOG(DRIVER_NAME, "CameraControlMode set failed (%04x)", PRIVATE_DATA->last_error);
	}
	indigo_usleep(100000);
	ptp_get_event(device);
	if (!ptp_initialise(device)) {
		return false;
	}
	// the OM-1 keeps the real P/A/S/M exposure mode in the standard but
	// unadvertised ExposureProgramMode property, inject it Fuji-style
	if (ptp_transaction_1_0_i(device, ptp_operation_GetDevicePropDesc, ptp_property_ExposureProgramMode, &buffer, &size)) {
		int last = 0;
		for (last = 0; PRIVATE_DATA->info_properties_supported[last]; last++) {
		}
		PRIVATE_DATA->info_properties_supported[last] = ptp_property_ExposureProgramMode;
		ptp_decode_property(buffer, size, device, PRIVATE_DATA->properties + last);
		// the descriptor claims the property is settable but the camera answers
		// DevicePropNotSupported to writes, the physical dial is the only authority
		PRIVATE_DATA->properties[last].writable = false;
	}
	if (buffer) {
		free(buffer);
		buffer = NULL;
	}
	indigo_set_timer(device, 0.5, ptp_olympus_check_event, &PRIVATE_DATA->event_checker);
	return true;
}

bool ptp_olympus_exposure(indigo_device *device) {
	// in the B dial position the shutter property reports one of the
	// bulb/live-time/live-comp sentinels (0xFFFFFFFx) instead of a real fraction:
	// hold the shutter with 0x03, time the exposure on the host, release with 0x06
	ptp_property *shutter = ptp_property_supported(device, ptp_property_olympus_Shutterspeed);
	bool is_bulb = shutter && (shutter->value.sw.value & 0xFFFFFF00) == 0xFFFFFF00;
	PRIVATE_DATA->image_added = false;
	bool result = ptp_transaction_1_0(device, ptp_operation_olympus_Capture, OLYMPUS_CAPTURE_PRESS);
	if (result) {
		if (is_bulb) {
			ptp_blob_exposure_timer(device);
		}
		// the release must be sent even after an abort, it ends the exposure
		result = ptp_transaction_1_0(device, ptp_operation_olympus_Capture, OLYMPUS_CAPTURE_RELEASE) && result;
	} else {
		INDIGO_DRIVER_LOG(DRIVER_NAME, "ptp_operation_olympus_Capture failed (%04x)", PRIVATE_DATA->last_error);
	}
	if (result) {
		if (CCD_IMAGE_PROPERTY->state == INDIGO_BUSY_STATE && CCD_PREVIEW_ENABLED_ITEM->sw.value && ptp_olympus_check_dual_compression(device)) {
			CCD_PREVIEW_IMAGE_PROPERTY->state = INDIGO_BUSY_STATE;
			indigo_update_property(device, CCD_PREVIEW_IMAGE_PROPERTY, NULL);
		}
		// the image arrives asynchronously via the event pipe, wait for the exposure
		// plus a 60s margin for processing and download
		int timeout = 600 + 10 * (int)CCD_EXPOSURE_ITEM->number.target;
		for (int i = 0; i < timeout && !PRIVATE_DATA->abort_capture && !PRIVATE_DATA->image_added; i++) {
			indigo_usleep(100000);
		}
		result = PRIVATE_DATA->image_added;
	}
	if (!result || PRIVATE_DATA->abort_capture) {
		if (CCD_IMAGE_PROPERTY->state != INDIGO_OK_STATE) {
			CCD_IMAGE_PROPERTY->state = INDIGO_ALERT_STATE;
			indigo_update_property(device, CCD_IMAGE_PROPERTY, NULL);
		}
		if (CCD_PREVIEW_IMAGE_PROPERTY->state != INDIGO_OK_STATE) {
			CCD_PREVIEW_IMAGE_PROPERTY->state = INDIGO_ALERT_STATE;
			indigo_update_property(device, CCD_PREVIEW_IMAGE_PROPERTY, NULL);
		}
		if (CCD_IMAGE_FILE_PROPERTY->state != INDIGO_OK_STATE) {
			CCD_IMAGE_FILE_PROPERTY->state = INDIGO_ALERT_STATE;
			indigo_update_property(device, CCD_IMAGE_FILE_PROPERTY, NULL);
		}
	}
	return result && !PRIVATE_DATA->abort_capture;
}

bool ptp_olympus_liveview(indigo_device *device) {
	void *buffer = NULL;
	uint32_t size = 0;
	int retry_count = 0;
	uint32_t mode = 0x04000300;
	// enable the live view stream (libgphoto2 uses the same LiveViewModeOM value)
	if (!ptp_transaction_0_1_o(device, ptp_operation_SetDevicePropValue, ptp_property_olympus_LiveViewModeOM, &mode, sizeof(uint32_t))) {
		INDIGO_DRIVER_LOG(DRIVER_NAME, "failed to enable live view (%04x)", PRIVATE_DATA->last_error);
		return false;
	}
	while (!PRIVATE_DATA->abort_capture && CCD_STREAMING_COUNT_ITEM->number.value != 0) {
		if (ptp_transaction_1_0_i(device, ptp_operation_olympus_GetLiveViewImage, 1, &buffer, &size) && size > 1024) {
			if (CCD_UPLOAD_MODE_LOCAL_ITEM->sw.value || CCD_UPLOAD_MODE_BOTH_ITEM->sw.value) {
				CCD_IMAGE_FILE_PROPERTY->state = INDIGO_BUSY_STATE;
				indigo_update_property(device, CCD_IMAGE_FILE_PROPERTY, NULL);
			}
			if (CCD_UPLOAD_MODE_CLIENT_ITEM->sw.value || CCD_UPLOAD_MODE_BOTH_ITEM->sw.value) {
				CCD_IMAGE_PROPERTY->state = INDIGO_BUSY_STATE;
				indigo_update_property(device, CCD_IMAGE_PROPERTY, NULL);
			}
			indigo_process_dslr_image(device, buffer, size, ".jpeg", true);
			if (PRIVATE_DATA->image_buffer) {
				free(PRIVATE_DATA->image_buffer);
			}
			PRIVATE_DATA->image_buffer = buffer;
			buffer = NULL;
			CCD_STREAMING_COUNT_ITEM->number.value--;
			if (CCD_STREAMING_COUNT_ITEM->number.value < 0) {
				CCD_STREAMING_COUNT_ITEM->number.value = -1;
			}
			indigo_update_property(device, CCD_STREAMING_PROPERTY, NULL);
			retry_count = 0;
		} else {
			// DeviceBusy or an undersized placeholder frame while live view spins up
			if (buffer) {
				free(buffer);
				buffer = NULL;
			}
			if (retry_count++ > 100) {
				INDIGO_DRIVER_LOG(DRIVER_NAME, "live view failed to start (%04x)", PRIVATE_DATA->last_error);
				mode = 0;
				ptp_transaction_0_1_o(device, ptp_operation_SetDevicePropValue, ptp_property_olympus_LiveViewModeOM, &mode, sizeof(uint32_t));
				indigo_finalize_dslr_video_stream(device);
				return false;
			}
		}
		indigo_usleep(50000);
	}
	mode = 0;
	ptp_transaction_0_1_o(device, ptp_operation_SetDevicePropValue, ptp_property_olympus_LiveViewModeOM, &mode, sizeof(uint32_t));
	indigo_finalize_dslr_video_stream(device);
	return !PRIVATE_DATA->abort_capture;
}

bool ptp_olympus_focus(indigo_device *device, int steps) {
	if (steps == 0) {
		return true;
	}
	// MFDrive: param1 = direction (0x01 near, 0x02 far), param2 = step size in
	// lens-specific units (libgphoto2 presets: 0x03 small, 0x0e medium, 0x3c large)
	uint32_t direction = steps > 0 ? 0x02 : 0x01;
	uint32_t step_size = steps > 0 ? steps : -steps;
	return ptp_transaction_2_0(device, ptp_operation_olympus_MFDrive, direction, step_size);
}

bool ptp_olympus_set_property(indigo_device *device, ptp_property *property) {
	if (!ptp_set_property(device, property)) {
		return false;
	}
	// the OM-1 acknowledges PC-initiated changes but reports them via events only
	// much later (with the next camera-side interaction), re-read immediately so
	// clients see the actual camera state
	if (ptp_refresh_property(device, property)) {
		ptp_update_property(device, property);
	}
	return true;
}

bool ptp_olympus_check_dual_compression(indigo_device *device) {
	return OLYMPUS_PRIVATE_DATA->is_dual_compression;
}
