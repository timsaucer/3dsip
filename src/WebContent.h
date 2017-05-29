/*
 ****************************************************************************
 *  Copyright (c) 2014 Uriah Liggett <freelaserscanner@gmail.com>           *
 *	This file is part of FreeLSS.                                           *
 *                                                                          *
 *  FreeLSS is free software: you can redistribute it and/or modify         *
 *  it under the terms of the GNU General Public License as published by    *
 *  the Free Software Foundation, either version 3 of the License, or       *
 *  (at your option) any later version.                                     *
 *                                                                          *
 *  FreeLSS is distributed in the hope that it will be useful,              *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *  GNU General Public License for more details.                            *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with FreeLSS.  If not, see <http://www.gnu.org/licenses/>.       *
 ****************************************************************************
*/

#pragma once


namespace freelss
{

class Progress;

class WebContent
{
public:

	/** The temporary data that is built up as the calibration is performed */
	struct CalibrationData
	{
		real camZ;        // World Z camera location
		real rightLaserX; // In scaled image pixels
		real rightLaserY; // In scaled image pixelss
	};

	static std::string scan(const std::vector<ScanResult>& pastScans);
	static std::string scanRunning(Progress& progress, real remainingTime);
	static std::string cal1(const std::string& message);
	static std::string settings(const std::string& message);
	static std::string setup(const std::string& message);
	static std::string viewScan(const std::string& plyFilename);
	static std::string showUpdate(SoftwareUpdate * update, const std::string& message);
	static std::string updateApplied(SoftwareUpdate * update, const std::string& message, bool success);
	static std::string network(const std::string& message, bool hiddenEssidInput);
	static std::string security(const std::string& message);

	static const std::string PROFILE_NAME;
	static const std::string SERIAL_NUMBER;
	static const std::string CAMERA_X;
	static const std::string CAMERA_Y;
	static const std::string CAMERA_Z;
	static const std::string CAMERA_MODE;
    static const std::string SCANNING_MODE_SELECTION;
	static const std::string RIGHT_LASER_X;
	static const std::string RIGHT_LASER_Y;
	static const std::string RIGHT_LASER_Z;
	static const std::string RIGHT_LASER_PIN;
	static const std::string LEFT_LASER_X;
	static const std::string LEFT_LASER_Y;
	static const std::string LEFT_LASER_Z;
	static const std::string LEFT_LASER_PIN;
	static const std::string LASER_MAGNITUDE_THRESHOLD;
	static const std::string LASER_ON_VALUE;
	static const std::string LASER_SELECTION;
	static const std::string STABILITY_DELAY;
	static const std::string MAX_LASER_WIDTH;
	static const std::string MIN_LASER_WIDTH;
	static const std::string STEPS_PER_REVOLUTION;
	static const std::string ENABLE_PIN;
	static const std::string STEP_PIN;
	static const std::string DIRECTION_PIN;
	static const std::string DIRECTION_VALUE;
	static const std::string RESPONSE_DELAY;
	static const std::string STEP_DELAY;
	static const std::string FRAMES_PER_REVOLUTION;
    static const std::string MICROSTEP_M0_PIN;
    static const std::string MICROSTEP_M1_PIN;
    static const std::string MICROSTEP_M2_PIN;
    static const std::string MOTOR_LIMIT_SWITCH_PIN;
    static const std::string MOTOR_LIMIT_SWITCH_VALUE;
    static const std::string MICROSTEPPING_FACTOR;
	static const std::string GENERATE_XYZ;
	static const std::string GENERATE_STL;
	static const std::string GENERATE_PLY;
	static const std::string SEPARATE_LASERS_BY_COLOR;
	static const std::string UNIT_OF_LENGTH;
	static const std::string VERSION_NAME;
	static const std::string GROUND_PLANE_HEIGHT;
	static const std::string PLY_DATA_FORMAT;
	static const std::string FREE_DISK_SPACE;
	static const std::string ENABLE_BURST_MODE;
	static const std::string ENABLE_LIGHTING;
	static const std::string LIGHTING_PIN;
	static const std::string CREATE_BASE_FOR_OBJECT;
	static const std::string WIFI_ESSID;
	static const std::string WIFI_ESSID_HIDDEN;
	static const std::string WIFI_PASSWORD;
	static const std::string KERNEL_VERSION;
	static const std::string MENU2;
	static const std::string ENABLE_AUTHENTICATION;
	static const std::string AUTH_USERNAME;
	static const std::string AUTH_PASSWORD1;
	static const std::string AUTH_PASSWORD2;

private:
	static std::string setting(const std::string& name, const std::string& label,
			const std::string& value, const std::string& description, const std::string& units = "", bool readOnly = false, bool password = false);

	static std::string setting(const std::string& name, const std::string& label,
			int value, const std::string& description, const std::string& units = "", bool readOnly = false);

	static std::string setting(const std::string& name, const std::string& label,
			real value, const std::string& description, const std::string& units = "", bool readOnly = false);

	static std::string checkbox(const std::string& name, const std::string& label, bool checked, const std::string& description);

	static std::string scanResult(size_t index, const ScanResult& result);

	static const int LOW_DISK_SPACE_MB;

	static const std::string CSS;
	static const std::string JAVASCRIPT;
	static const std::string SERIAL_NUMBER_DESCR;
	static const std::string CAMERA_X_DESCR;
	static const std::string CAMERA_Y_DESCR;
	static const std::string CAMERA_Z_DESCR;
	static const std::string RIGHT_LASER_X_DESCR;
	static const std::string RIGHT_LASER_Y_DESCR;
	static const std::string RIGHT_LASER_Z_DESCR;
	static const std::string LEFT_LASER_X_DESCR;
	static const std::string LEFT_LASER_Y_DESCR;
	static const std::string LEFT_LASER_Z_DESCR;
	static const std::string LASER_MAGNITUDE_THRESHOLD_DESCR;
	static const std::string RIGHT_LASER_PIN_DESCR;
	static const std::string LEFT_LASER_PIN_DESCR;
	static const std::string LASER_ON_VALUE_DESCR;
	static const std::string STABILITY_DELAY_DESCR;
	static const std::string MAX_LASER_WIDTH_DESCR;
	static const std::string MIN_LASER_WIDTH_DESCR;
	static const std::string STEPS_PER_REVOLUTION_DESCR;
	static const std::string ENABLE_PIN_DESCR;
	static const std::string STEP_PIN_DESCR;
	static const std::string STEP_DELAY_DESCR;
	static const std::string DIRECTION_PIN_DESCR;
	static const std::string RESPONSE_DELAY_DESCR;
	static const std::string FRAMES_PER_REVOLUTION_DESCR;
    static const std::string MICROSTEP_M0_PIN_DESCR;
    static const std::string MICROSTEP_M1_PIN_DESCR;
    static const std::string MICROSTEP_M2_PIN_DESCR;
    static const std::string MOTOR_LIMIT_SWITCH_PIN_DESCR;
    static const std::string MOTOR_LIMIT_SWITCH_VALUE_DESCR;
	static const std::string GENERATE_XYZ_DESCR;
	static const std::string GENERATE_STL_DESCR;
	static const std::string GENERATE_PLY_DESCR;
	static const std::string SEPARATE_LASERS_BY_COLOR_DESCR;
	static const std::string GROUND_PLANE_HEIGHT_DESCR;
	static const std::string PLY_DATA_FORMAT_DESCR;
	static const std::string ENABLE_BURST_MODE_DESCR;
	static const std::string ENABLE_LIGHTING_DESCR;
	static const std::string LIGHTING_PIN_DESCR;
	static const std::string CREATE_BASE_FOR_OBJECT_DESCR;
	static const std::string WIFI_ESSID_DESCR;
	static const std::string WIFI_PASSWORD_DESCR;
	static const std::string ENABLE_AUTHENTICATION_DESCR;
	static const std::string AUTH_USERNAME_DESCR;
	static const std::string AUTH_PASSWORD1_DESCR;
	static const std::string AUTH_PASSWORD2_DESCR;
};

}
