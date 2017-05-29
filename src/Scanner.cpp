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

#include "Main.h"
#include "Scanner.h"
#include "PresetManager.h"
#include "Setup.h"
#include "MotorDriver.h"
#include "LocationMapper.h"
#include "Laser.h"
#include "Camera.h"
#include "PixelLocationWriter.h"
#include "StlWriter.h"
#include "XyzWriter.h"
#include "LaserResultsMerger.h"
#include "FileWriter.h"
#include "Facetizer.h"
#include "PropertyReaderWriter.h"
#include "ObjectBaseCreator.h"

namespace freelss
{

static bool CompareScanResults(const ScanResult& a, const ScanResult& b)
{
	return a.getScanDate() > b.getScanDate();
}
static bool CompareScanResultFiles(const ScanResultFile& a, const ScanResultFile& b)
{
	return a.extension > b.extension;
}

static bool ComparePseudoSteps(const DataPoint& a, const DataPoint& b)
{
	if (a.pseudoFrame != b.pseudoFrame)
	{
		return a.pseudoFrame < b.pseudoFrame;
	}

	return a.pixel.y < b.pixel.y;
}

Scanner::Scanner() :
	m_laser(NULL),
	m_camera(NULL),
	m_turnTable(NULL),
	m_laserLocations(NULL),
	m_running(false),
	m_range(360),
	m_filename(""),
	m_progress(),
	m_status(),
	m_maxNumFrameRetries(5),                    // TODO: Place this in Database
	m_maxNumFailedRows(10),                      // TODO: Place this in Database
	m_numObjectBaseSubdivisions(3),
	m_columnPoints(NULL),
	m_remainingTime(0),
	m_firstRowRightLaserCol(0),
	m_firstRowLeftLaserCol(0),
	m_maxNumLocations(0),
	m_maxFramesPerRevolution(0),
	m_radiansBetweenLaserPlanes(0),
	m_radiansPerFrame(0),
	m_rightLaserLoc(),
	m_leftLaserLoc(),
	m_cameraLoc(),
	m_writeRangeCsvEnabled(false),
	m_rangeFout(),
	m_numFramesBetweenLaserPlanes(0),
	m_laserSelection(Laser::ALL_LASERS),
	m_task(GENERATE_SCAN),
	m_results(),
	m_laserDelaySec(0)
{
	// Do nothing
}

Scanner::~Scanner()
{
	delete [] m_laserLocations;
	delete [] m_columnPoints;
}

void Scanner::setTask(Scanner::Task task)
{
	m_task = task;
}

bool Scanner::isRunning()
{
	bool running;

	m_status.enter();
	running = m_running;
	m_status.leave();

	return running;
}

Progress& Scanner::getProgress()
{
	return m_progress;
}

real Scanner::getRemainingTime()
{
	real remainingTime;

	m_status.enter();
	remainingTime = m_remainingTime;
	m_status.leave();

	return remainingTime;
}

void Scanner::setRange(real range)
{
	if (range > 360)
	{
		m_range = 360;
	}
	else if (range < 0.01)
	{
		m_range = 0.01;
	}
	else
	{
		m_range = range;
	}
}

void Scanner::prepareScan()
{
	m_status.enter();
	m_running = true;
	m_progress.setPercent(0);

	m_remainingTime = 0;

	if (m_task == Scanner::GENERATE_SCAN)
	{
		m_progress.setLabel("Scanning");
	}
	else if (m_task == Scanner::GENERATE_DEBUG)
	{
		m_progress.setLabel("Generating debug info");
	}
	else
	{
		m_progress.setLabel("Unknown");
	}

	// Get handles to our hardware devices
	m_camera = Camera::getInstance();
	m_laser = Laser::getInstance();
	m_turnTable = MotorDriver::getInstance();

	// Initialize data structures
	m_maxNumLocations = m_camera->getImageHeight();

	delete [] m_laserLocations;
	m_laserLocations = new PixelLocation[m_maxNumLocations];

	delete [] m_columnPoints;
	m_columnPoints = new ColoredPoint[m_camera->getImageWidth()];

	// Set the base output file
	std::stringstream sstr;
	sstr << SCAN_OUTPUT_DIR << std::string("/") << time(NULL);

	m_filename = sstr.str();

	m_firstRowRightLaserCol = m_camera->getImageWidth() * 0.5;
	m_firstRowLeftLaserCol = m_camera->getImageWidth() * 0.5;

	Setup * setup = Setup::get();
	Preset& preset = PresetManager::get()->getActivePreset();

	// Set the laser delay
	switch (preset.cameraMode)
	{
	case Camera::CM_STILL_5MP:
		m_laserDelaySec = 0.36;
		break;

	case Camera::CM_VIDEO_5MP:
		m_laserDelaySec = 0.36;
		break;

	case Camera::CM_VIDEO_HD:
		m_laserDelaySec = 0.18;
		break;

	case Camera::CM_VIDEO_1P2MP:
		//m_laserDelaySec = 0.09;
		m_laserDelaySec = 0.18;
		break;

	case Camera::CM_VIDEO_VGA:
		m_laserDelaySec = 0.09;
		break;

    case Camera::CM_STILL_8MP:
        m_laserDelaySec = 0.36;
        break;
            
    case Camera::CM_VIDEO_8MP:
        m_laserDelaySec = 0.36;
        break;
            
	default:
		m_laserDelaySec = 0.09;
		break;
	}

	// Read the laser selection
	m_laserSelection = preset.laserSide;

	// Read the location of the lasers and camera
	m_rightLaserLoc = setup->rightLaserLocation;
	m_leftLaserLoc = setup->leftLaserLocation;
	m_cameraLoc = setup->cameraLocation;

	m_status.leave();
}

Scanner::LiveData Scanner::getLiveDataLock()
{
	m_results.enter();

	Scanner::LiveData data;
	data.leftLaserResults = &m_leftLaserResults;
	data.rightLaserResults = &m_rightLaserResults;

	return data;
}

void Scanner::releaseLiveDataLock()
{
	m_results.leave();
}

void Scanner::run()
{
	try
	{
		runScan();
	}
	catch (Exception& ex)
	{
		std::cerr << "!! Exception: " << ex << std::endl;
	}
	catch (std::exception& ex)
	{
		std::cerr << "!! Exception: " << ex.what() << std::endl;
	}
	catch (...)
	{
		std::cerr << "Unknown Exception Occurred" << std::endl;
	}
}

void Scanner::runScan()
{
	// Prepare to scan
	prepareScan();

	Setup * setup = Setup::get();
	Preset preset = PresetManager::get()->getActivePreset();

	Scanner::TimingStats timingStats;
	memset(&timingStats, 0, sizeof(Scanner::TimingStats));
	timingStats.startTime = GetTimeInSeconds();
	double time1 = 0;

	LocationMapper leftLocMapper(m_leftLaserLoc, m_cameraLoc);
	LocationMapper rightLocMapper(m_rightLaserLoc, m_cameraLoc);

	if (setup->haveLaserPlaneNormals)
	{
		std::cout << "Using auto-corrected laser plane normals" << std::endl;

		leftLocMapper.setLaserPlaneNormal(setup->leftLaserPlaneNormal);
		rightLocMapper.setLaserPlaneNormal(setup->rightLaserPlaneNormal);
	}

	// Compute the angle between the two laser planes
	real leftLaserX = ABS(m_leftLaserLoc.x);
	real rightLaserX = ABS(m_rightLaserLoc.x);
	real camZ = ABS(m_cameraLoc.z);

	// Sanity check to prevent divide by 0.  In reality the laser should never be this close to the camera
	if (leftLaserX < 0.001)
	{
		leftLaserX = 0.001;
	}

	if (rightLaserX < 0.001)
	{
		rightLaserX = 0.001;
	}

	//
	// tan(theta) = ABS(laserX) / camZ
	// theta = atan(ABS(laserX) / camZ)
	//
	real leftLaserAngle = atan(leftLaserX / camZ);
	real rightLaserAngle = atan(rightLaserX / camZ);

	m_radiansBetweenLaserPlanes = leftLaserAngle + rightLaserAngle;

	// Write the range CSV
	if (m_writeRangeCsvEnabled)
	{
		m_rangeFout.open((m_filename + ".csv").c_str());
		if (!m_rangeFout.is_open())
		{
			throw Exception("Error opening range CSV file");
		}
	}

	// Init the results vectors
	m_results.enter();
	m_leftLaserResults.clear();
	m_rightLaserResults.clear();
	m_results.leave();

	int numFrames = 0;
    int microstepRatio = pow(2,setup->motorMicrostepFactor);
	m_maxFramesPerRevolution = preset.framesPerRevolution;
	if (m_maxFramesPerRevolution < 1)
	{
		m_maxFramesPerRevolution = 1;
	}

	int stepsPerRevolution = Setup::get()->stepsPerRevolution * pow(2,setup->motorMicrostepFactor);
	if (m_maxFramesPerRevolution > stepsPerRevolution)
	{
		m_maxFramesPerRevolution = stepsPerRevolution;
	}

	try
	{
		// Make sure the lasers are off
		m_laser->turnOff(Laser::ALL_LASERS);
		delayAcquisitionForLaser();

		// Enable the turn table motor
		m_turnTable->setMotorEnabled(true);
        
        if (preset.scanMode == SCAN_IN_PLACE)
            m_turnTable -> moveToHomePosition ();

        // Wait a second in case the object shakes
		Thread::usleep(1000000);

		std::cout << "Enabled motor" << std::endl;

		if (m_laser == NULL)
		{
			throw Exception("Laser object is NULL");
		}

		if (m_turnTable == NULL)
		{
			throw Exception("Motor object is NULL");
		}

		float rotation = 0;

		// Read the number of motor steps per revolution
		int stepsPerRevolution = setup->stepsPerRevolution * pow(2,setup->motorMicrostepFactor);
        stepsPerRevolution *= microstepRatio;

		float rangeRadians =  (m_range / 360) * (2 * PI);

		// The number of steps for a single frame
		int stepsPerFrame = ceil(stepsPerRevolution / (float)m_maxFramesPerRevolution);
		if (stepsPerFrame < 1)
		{
			stepsPerFrame = 1;
		}

		// The number of radians a single step takes you
		m_radiansPerFrame = ((2 * PI) / (float) stepsPerRevolution);

		// The radians to move for a single frame
		float frameRadians = stepsPerFrame * m_radiansPerFrame;

		numFrames = ceil(rangeRadians / frameRadians);

		m_numFramesBetweenLaserPlanes = m_radiansBetweenLaserPlanes / frameRadians;

		std::cout << "Angle between laser planes: " << RADIANS_TO_DEGREES(m_radiansBetweenLaserPlanes)
				  << " degrees, radiansPerFrame=" << m_radiansPerFrame
				  << ", numFramesBetweenLaserPlanes=" << m_numFramesBetweenLaserPlanes
				  << ", numFrames=" << numFrames << std::endl;

        Image * image1 = NULL;
        if (preset.scanMode == SCAN_IN_PLACE)
        {
            try
            {
                // Take a picture with the laser off
                time1 = GetTimeInSeconds();
                image1 = acquireImage();

                m_laser->turnOn(Laser::RIGHT_LASER);
                delayAcquisitionForLaser();
            
            }
            catch (...)
            {
                releaseImage(image1);
                throw;
            }
        
        }
        
        for (int iFrame = 0; iFrame < numFrames; iFrame++)
		{
			timingStats.numFrames++;

			// Stop if the user asked us to
			if (m_stopRequested)
			{
				break;
			}

            if (preset.scanMode == SCAN_IN_PLACE)
                singleScanInPlace (iFrame, rotation, frameRadians, rightLocMapper, &timingStats, image1);
            else
                singleScanTurnTable(iFrame, rotation, frameRadians, leftLocMapper, rightLocMapper, &timingStats);

			rotation += frameRadians;

			// Update the progress
			double progress = (iFrame + 1.0) / numFrames;
			double timeElapsed = GetTimeInSeconds() - timingStats.startTime;
			double percentComplete = 100.0 * progress;
			double fullTimeSec = timeElapsed / progress;
			double remainingSec = fullTimeSec - timeElapsed;

			m_progress.setPercent(progress * 100);

			m_status.enter();
			m_remainingTime = remainingSec;
			m_status.leave();

			logTimingStats(std::cout, timingStats);
			std::cout << percentComplete << "% Complete, " << (remainingSec / 60) << " minutes remaining." << std::endl;
		}
        
        if (preset.scanMode == SCAN_IN_PLACE)
        {
            releaseImage(image1);
            m_laser->turnOff(Laser::RIGHT_LASER);
            delayAcquisitionForLaser();
        }
	}
	catch (...)
	{	
		m_turnTable->setMotorEnabled(false);

		m_status.enter();
		m_running = false;
		m_status.leave();

		if (m_writeRangeCsvEnabled)
		{
			m_rangeFout.close();
		}

		throw;
	}
	
	m_rangeFout.close();

	m_turnTable->setMotorEnabled(false);

	std::cout << "Merging laser results..." << std::endl;

	time1 = GetTimeInSeconds();

	// Merge the left and right lasers and sort the results
	std::vector<DataPoint> results;
	LaserResultsMerger merger;

	m_results.enter();
	merger.merge(results, m_leftLaserResults, m_rightLaserResults, m_maxFramesPerRevolution,
			     m_numFramesBetweenLaserPlanes, Camera::getInstance()->getImageHeight(), preset.laserMergeAction, m_progress);
	m_results.leave();

	// Sort by pseudo-step and row
	std::cout << "Sort 2... " << std::endl;
	std::sort(results.begin(), results.end(), ComparePseudoSteps);
	std::cout << "End Sort 2... " << std::endl;

	m_results.enter();

	std::cout << "Merged " << m_leftLaserResults.size() << " left laser and " << m_rightLaserResults.size() << " right laser results into " << results.size() << " results." << std::endl;

	m_leftLaserResults.clear();
	m_rightLaserResults.clear();

	m_results.leave();

	timingStats.laserMergeTime += GetTimeInSeconds() - time1;

#if 0
	time1 = GetTimeInSeconds();
	m_progress.setLabel("Balancing brightness");
	BrightnessBalancer balancer;
	balancer.balanceBrightness(results, m_progress);

	timingStats.pointProcessingTime += GetTimeInSeconds() - time1;
#endif

	std::cout << "Constructing mesh..." << std::endl;

	// Mesh the point cloud
	FaceMap faces;
	if (preset.generatePly || preset.generateStl)
	{
		Facetizer facetizer;

		time1 = GetTimeInSeconds();
		m_progress.setLabel("Facetizing Point Cloud");
		facetizer.facetize(faces, results, m_range > 359, m_progress, true);

		// Add the object base
		if (preset.createBaseForObject)
		{
			ObjectBaseCreator objectBaseCreator;
			objectBaseCreator.createBase(faces, results, preset.groundPlaneHeight, m_numObjectBaseSubdivisions, m_progress);
		}
		timingStats.facetizationTime += GetTimeInSeconds() - time1;
	}

	// Write the PLY file
	if (preset.generatePly)
	{
		m_progress.setLabel("Generating PLY file");
		m_progress.setPercent(0);

		std::string plyFilename = m_filename + ".ply";

		std::cout << "Writing PLY file... " << plyFilename <<  std::endl;
		time1 = GetTimeInSeconds();

		FileWriter plyOut(plyFilename.c_str());
		if (!plyOut.is_open())
		{
			throw Exception("Error opening file for writing: " + plyFilename);
		}

		/** Writes the results to a PLY file */
		PlyWriter plyWriter;
		plyWriter.setDataFormat(preset.plyDataFormat);
		plyWriter.setTotalNumPoints((int)results.size());
		plyWriter.setTotalNumFacesFromFaceMap(faces);
		plyWriter.begin(&plyOut);

		real percent = 0;
		for (size_t iRec = 0; iRec < results.size(); iRec++)
		{
			real newPct = 100.0f * iRec / results.size();
			if (newPct - percent > 0.1)
			{
				m_progress.setPercent(newPct);
				percent = newPct;
			}

			plyWriter.writePoints(&results[iRec].point, 1);
		}

		plyWriter.writeFaces(faces);
		plyWriter.end();
		plyOut.close();
		timingStats.plyWritingTime += GetTimeInSeconds() - time1;
	}

	// Generate the XYZ file
	if (preset.generateXyz)
	{
		time1 = GetTimeInSeconds();
		XyzWriter xyzWriter;
		xyzWriter.write(m_filename, results, m_progress);
		timingStats.xyzWritingTime += GetTimeInSeconds() - time1;
	}

	// Generate the STL file
	if (preset.generateStl)
	{
		time1 = GetTimeInSeconds();
		StlWriter stlWriter;
		stlWriter.write(m_filename, results, faces, m_progress);
		timingStats.stlWritingTime = GetTimeInSeconds() - time1;
	}

	logTimingStats(std::cout, timingStats);

	// Generate the log file
	std::string txtFilename = m_filename + ".log";
	std::ofstream fout (txtFilename.c_str());
	if (fout.is_open())
	{
		// Log the software version
		fout << "FreeLSS Version: " << FREELSS_VERSION_MAJOR << "." << FREELSS_VERSION_MINOR << std::endl;
		fout << "Preset: " << preset.name << std::endl;
		fout << "Range: " << m_range << " degrees" << std::endl;

		// Log the timing stats
		logTimingStats(fout, timingStats);

		// Get the settings that were used to generate this scan
		std::vector<Property> properties;
		PresetManager::get()->encodeProperties(properties);
		Setup::get()->encodeProperties(properties);

		// Write the settings and preset properties
		PropertyReaderWriter propWriter;
		propWriter.writeProperties(fout, properties);

		fout << std::endl;
		fout.close();
	}
	else
	{
		std::cerr << "Error opening file for writing: " << txtFilename << std::endl;
	}


	m_progress.setPercent(100);

	m_status.enter();
	m_running = false;
	m_status.leave();

	std::cout << "Done." << std::endl;
}

void Scanner::generateDebugInfo(Laser::LaserSide laserSide)
{
	Setup * setup = Setup::get();

	// Prepare for scanning
	prepareScan();

	std::string debuggingCsv = std::string(DEBUG_OUTPUT_DIR) + "/0.csv";

	// Make sure the lasers are off
	if (m_laser->isOn(Laser::LEFT_LASER) || m_laser->isOn(Laser::RIGHT_LASER))
	{
		m_laser->turnOff(Laser::ALL_LASERS);
		delayAcquisitionForLaser();
	}

	bool useLeftLaser = m_laserSelection == Laser::LEFT_LASER || m_laserSelection == Laser::ALL_LASERS;
	bool useRightLaser = m_laserSelection == Laser::RIGHT_LASER || m_laserSelection == Laser::ALL_LASERS;

	// Ensure that the camera images get released
	Image * image1 = NULL;
	Image * image2 = NULL;
	try
	{
		image1 = acquireImage();

		Image rightDebuggingImage;
		std::vector<PixelLocation> rightLocations(m_maxNumLocations);
		int numRightLocations = 0;
		if (useRightLaser)
		{
			m_laser->turnOn(Laser::RIGHT_LASER);
			delayAcquisitionForLaser();
			image2 = acquireImage();

			m_laser->turnOff(Laser::RIGHT_LASER);
			if (useLeftLaser)
			{
				m_laser->turnOn(Laser::LEFT_LASER);
			}
			delayAcquisitionForLaser();

			int firstRowLaserCol = m_camera->getImageWidth() * 0.5;
			int numRowsBadFromColor = 0;
			int numRowsBadFromNumRanges = 0;

			numRightLocations = m_imageProcessor.process(* image1,
														 * image2,
														 &rightDebuggingImage,
														 &rightLocations.front(),
														 m_maxNumLocations,
														 firstRowLaserCol,
														 numRowsBadFromColor,
														 numRowsBadFromNumRanges,
														 debuggingCsv.c_str());

			releaseImage(image2);

			// If we had major problems with this frame, try it again
			std::cout << "numRowsBadFromColor: " << numRowsBadFromColor << std::endl;
			std::cout << "numRowsBadFromNumRanges: " << numRowsBadFromNumRanges << std::endl;

			// Map the points so that points below the ground plane and outside the max object size are omitted
			int numLocationsMapped = 0;
			LocationMapper locMapper(setup->rightLaserLocation, setup->cameraLocation);

			if (setup->haveLaserPlaneNormals)
			{
				locMapper.setLaserPlaneNormal(setup->rightLaserPlaneNormal);
			}

			locMapper.mapPoints(&rightLocations.front(), image1, m_columnPoints, numRightLocations, numLocationsMapped);
			numRightLocations = numLocationsMapped;
		}

		std::vector<PixelLocation> leftLocations (m_maxNumLocations);
		int numLeftLocations = 0;
		Image leftDebuggingImage;
		if (useLeftLaser)
		{
			if (!m_laser->isOn(Laser::LEFT_LASER))
			{
				m_laser->turnOn(Laser::LEFT_LASER);
				delayAcquisitionForLaser();
			}
			image2 = acquireImage();

			m_laser->turnOff(Laser::LEFT_LASER);
			delayAcquisitionForLaser();

			int firstRowLaserCol = m_camera->getImageWidth() * 0.5;
			int numRowsBadFromColor = 0;
			int numRowsBadFromNumRanges = 0;

			numLeftLocations = m_imageProcessor.process(* image1,
														* image2,
														&leftDebuggingImage,
														&leftLocations.front(),
														m_maxNumLocations,
														firstRowLaserCol,
														numRowsBadFromColor,
														numRowsBadFromNumRanges,
														debuggingCsv.c_str());

			releaseImage(image2);

			// If we had major problems with this frame, try it again
			std::cout << "numRowsBadFromColor: " << numRowsBadFromColor << std::endl;
			std::cout << "numRowsBadFromNumRanges: " << numRowsBadFromNumRanges << std::endl;

			// Map the points so that points below the ground plane and outside the max object size are omitted
			int numLocationsMapped = 0;
			LocationMapper locMapper(setup->leftLaserLocation, setup->cameraLocation);
			if (setup->haveLaserPlaneNormals)
			{
				locMapper.setLaserPlaneNormal(setup->leftLaserPlaneNormal);
			}

			locMapper.mapPoints(&leftLocations.front(), image1, m_columnPoints, numLeftLocations, numLocationsMapped);
			numLeftLocations = numLocationsMapped;
		}

		releaseImage(image1);

		//
		// Merge the two debugging images
		//
		Image debuggingImage;
		mergeDebuggingImages(debuggingImage, leftDebuggingImage, rightDebuggingImage, laserSide);

		std::string baseFilename = std::string(DEBUG_OUTPUT_DIR) + "/";

		// Show the image center line
		std::vector<PixelLocation> centerLocs (debuggingImage.getHeight());
		for (size_t iLoc = 0; iLoc < centerLocs.size(); iLoc++)
		{
			if ((iLoc % 4) != 0 && ((iLoc + 2) % 4) != 0)
			{
				PixelLocation loc;
				loc.x = debuggingImage.getWidth() / 2;
				loc.y = static_cast<real>(iLoc);
				centerLocs.push_back(loc);
			}
		}

		Image::overlayPixels(debuggingImage, &centerLocs.front(), (int) centerLocs.size(), 0, 0, 255);

		// Overlay the pixels onto the debug image and write that as a new image
		PixelLocationWriter locWriter;
		Image::overlayPixels(debuggingImage, &rightLocations.front(), numRightLocations, 255, 0, 0);
		Image::overlayPixels(debuggingImage, &leftLocations.front(), numLeftLocations, 255, 0, 0);

		if (setup->haveLaserPlaneNormals)
		{
			// Left
			std::vector<PixelLocation> left;
			PixelLocation leftTop;
			leftTop.x = setup->leftLaserCalibrationTop.x * debuggingImage.getWidth();
			leftTop.y = setup->leftLaserCalibrationTop.y * debuggingImage.getHeight();
			highlightPixel(leftTop, left);

			PixelLocation leftBottom;
			leftBottom.x = setup->leftLaserCalibrationBottom.x * debuggingImage.getWidth();
			leftBottom.y = setup->leftLaserCalibrationBottom.y * debuggingImage.getHeight();
			highlightPixel(leftBottom, left);

			Image::overlayPixels(debuggingImage, &left.front(), (int) left.size(), 255, 255, 0);

			// Right
			std::vector<PixelLocation> right;
			PixelLocation rightTop;
			rightTop.x = setup->rightLaserCalibrationTop.x * debuggingImage.getWidth();
			rightTop.y = setup->rightLaserCalibrationTop.y * debuggingImage.getHeight();
			highlightPixel(rightTop, right);

			PixelLocation rightBottom;
			rightBottom.x = setup->rightLaserCalibrationBottom.x * debuggingImage.getWidth();
			rightBottom.y = setup->rightLaserCalibrationBottom.y * debuggingImage.getHeight();
			highlightPixel(rightBottom, right);

			Image::overlayPixels(debuggingImage, &right.front(), (int) right.size(), 0, 255, 0);

			std::cout << "Laser plane data is available" << std::endl;
		}
		else
		{
			std::cout << "No laser plane normal data is available" << std::endl;
		}

		locWriter.writeImage(debuggingImage, debuggingImage.getWidth(), debuggingImage.getHeight(), baseFilename + "5.png");
	}
	catch (...)
	{
		releaseImage(image1);
		releaseImage(image2);
		throw;
	}

	m_progress.setPercent(100);

	m_status.enter();
	m_running = false;
	m_status.leave();
	std::cout << "Done." << std::endl;
}

void Scanner::highlightPixel(const PixelLocation& inPixel, std::vector<PixelLocation>& outPixels)
{
	PixelLocation pixel = inPixel;

	pixel.x = inPixel.x - 5;
	for (int i = 0; i <= 10; i++)
	{
		outPixels.push_back(pixel);
		pixel.x++;
	}

	pixel = inPixel;
	pixel.y = inPixel.y - 5;
	for (int i = 0; i <= 10; i++)
	{
		outPixels.push_back(pixel);
		pixel.y++;
	}
}

void Scanner::mergeDebuggingImages(Image& outImage, Image& leftDebuggingImage, Image& rightDebuggingImage, Laser::LaserSide laserSide)
{

	if (laserSide == Laser::RIGHT_LASER)
	{
		memcpy(outImage.getPixels(), rightDebuggingImage.getPixels(), outImage.getPixelBufferSize());
	}
	else if (laserSide == Laser::LEFT_LASER)
	{
		memcpy(outImage.getPixels(), leftDebuggingImage.getPixels(), outImage.getPixelBufferSize());
	}
	else if (laserSide == Laser::ALL_LASERS)
	{
		//
		// Merge the two images with a MAX operator
		//
		unsigned char * left = leftDebuggingImage.getPixels();
		unsigned char * right = rightDebuggingImage.getPixels();
		unsigned numPixels = leftDebuggingImage.getWidth() * leftDebuggingImage.getHeight();
		unsigned char * out = outImage.getPixels();
		for (unsigned iPx = 0; iPx < numPixels; iPx++)
		{
			int lr = (int) left[0];
			int lg = (int) left[1];
			int lb = (int) left[2];
			int leftMagSq = lr * lr + lg * lg + lb * lb;

			int rr = (int) right[0];
			int rg = (int) right[1];
			int rb = (int) right[2];
			int rightMagSq = rr * rr + rg * rg + rb * rb;

			if (leftMagSq > rightMagSq)
			{
				out[0] = (unsigned char) lr;
				out[1] = (unsigned char) lg;
				out[2] = (unsigned char) lb;
			}
			else
			{
				out[0] = (unsigned char) rr;
				out[1] = (unsigned char) rg;
				out[2] = (unsigned char) rb;
			}

			left += 3;
			right += 3;
			out += 3;
		}
	}
	else
	{
		throw Exception("Unsupported LaserSide");
	}
}
void Scanner::singleScanTurnTable(int frame, float rotation, float frameRotation,
		                 LocationMapper& leftLocMapper, LocationMapper& rightLocMapper, TimingStats * timingStats)
{
	double time1 = GetTimeInSeconds();
	m_turnTable->rotate(frameRotation);
	timingStats->rotationTime += GetTimeInSeconds() - time1;

	Image * image1 = NULL;
	Image * image2 = NULL;

	bool useLeftLaser = m_laserSelection == Laser::LEFT_LASER || m_laserSelection == Laser::ALL_LASERS;
	bool useRightLaser = m_laserSelection == Laser::RIGHT_LASER || m_laserSelection == Laser::ALL_LASERS;

	// Ensure that the images get released back to the camera
	try
	{
		// Take a picture with the laser off
		time1 = GetTimeInSeconds();
		image1 = acquireImage();
		timingStats->imageAcquisitionTime += GetTimeInSeconds() - time1;

		// If this is the first image, save it as a thumbnail
		if (frame == 0)
		{
			std::string thumbnail = m_filename + ".png";

			PixelLocationWriter imageWriter;
			imageWriter.writeImage(* image1, 128, 96, thumbnail.c_str());
		}

		// Scan with the Right laser
		if (useRightLaser)
		{
			// Turn on the right laser
			time1 = GetTimeInSeconds();
			m_laser->turnOn(Laser::RIGHT_LASER);
			delayAcquisitionForLaser();
			timingStats->laserTime += GetTimeInSeconds() - time1;

			// Take a picture with the right laser on
			time1 = GetTimeInSeconds();
			image2 = acquireImage();
			timingStats->imageAcquisitionTime += GetTimeInSeconds() - time1;

			// Turn off the right laser
			time1 = GetTimeInSeconds();
			m_laser->turnOff(Laser::RIGHT_LASER);

			// Turn the left laser on now if it is a dual laser scan to save time later
			if (useLeftLaser)
			{
				m_laser->turnOn(Laser::LEFT_LASER);
			}

			delayAcquisitionForLaser();
			timingStats->laserTime += GetTimeInSeconds() - time1;

			// Process the right laser results
			processScan(image1, image2, m_rightLaserResults, frame, rotation, rightLocMapper, Laser::RIGHT_LASER, m_firstRowRightLaserCol, timingStats);

			releaseImage(image2);
		}

		// Scan with the Left laser
		if (useLeftLaser)
		{
			// Turn on the left laser
			time1 = GetTimeInSeconds();
			if (!m_laser->isOn(Laser::LEFT_LASER))
			{
				m_laser->turnOn(Laser::LEFT_LASER);
				delayAcquisitionForLaser();
			}
			timingStats->laserTime += GetTimeInSeconds() - time1;

			// Take a picture with the left laser on
			time1 = GetTimeInSeconds();
			image2 = acquireImage();
			timingStats->imageAcquisitionTime += GetTimeInSeconds() - time1;

			// Turn off the left laser
			time1 = GetTimeInSeconds();
			m_laser->turnOff(Laser::LEFT_LASER);
			delayAcquisitionForLaser();
			timingStats->laserTime += GetTimeInSeconds() - time1;

			// Process the left laser results
			processScan(image1, image2, m_leftLaserResults, frame, rotation, leftLocMapper, Laser::LEFT_LASER, m_firstRowLeftLaserCol, timingStats);

			releaseImage(image2);
		}

		// Release image 1 back to the camera
		releaseImage(image1);
	}
	catch (...)
	{
		releaseImage(image1);
		releaseImage(image2);
		throw;
	}
}
 
void Scanner::singleScanInPlace(int frame, float rotation, float frameRotation, LocationMapper& rightLocMapper, TimingStats * timingStats, Image * image1)
    {
        double time1 = GetTimeInSeconds();
        m_turnTable->rotate(frameRotation);
        timingStats->rotationTime += GetTimeInSeconds() - time1;
        
        Image * image2 = NULL;
        
        // Ensure that the images get released back to the camera
        try
        {
            // Take a picture with the laser off
            time1 = GetTimeInSeconds();
            timingStats->imageAcquisitionTime += GetTimeInSeconds() - time1;
            
            // If this is the first image, save it as a thumbnail
            if (frame == 0)
            {
                std::string thumbnail = m_filename + ".png";
                
                PixelLocationWriter imageWriter;
                imageWriter.writeImage(* image1, 128, 96, thumbnail.c_str());
            }
            
                // Turn on the right laser
                time1 = GetTimeInSeconds();
                timingStats->laserTime += GetTimeInSeconds() - time1;
                
                // Take a picture with the right laser on
                time1 = GetTimeInSeconds();
                image2 = acquireImage();
                timingStats->imageAcquisitionTime += GetTimeInSeconds() - time1;
            
                Vector3 vecUpdatedLaserNorm (sin(rotation), 0.0, -cos(rotation));
                rightLocMapper.setLaserPlaneNormal(vecUpdatedLaserNorm);
                
                // Process the right laser results
                processScan(image1, image2, m_rightLaserResults, frame, 0.0, rightLocMapper, Laser::RIGHT_LASER, m_firstRowRightLaserCol, timingStats);
                
                releaseImage(image2);
            
            
        }
        catch (...)
        {
            releaseImage(image1);
            releaseImage(image2);
            throw;
        }
    }

bool Scanner::processScan(Image * image1, Image * image2, std::vector<DataPoint> & results, int frame, float rotation, LocationMapper& locMapper, Laser::LaserSide laserSide, int & firstRowLaserCol, TimingStats * timingStats)
{
	int numLocationsMapped = 0;
	int numRowsBadFromColor = 0;
	int numRowsBadFromNumRanges = 0;

	// Send the pictures off for processing
	double time1 = GetTimeInSeconds();
	int numLocations = m_imageProcessor.process(* image1,
												* image2,
												NULL,
												m_laserLocations,
												m_maxNumLocations,
												firstRowLaserCol,
												numRowsBadFromColor,
												numRowsBadFromNumRanges,
												NULL);

	timingStats->imageProcessingTime += GetTimeInSeconds() - time1;

	// If we had major problems with this frame, try it again
	std::cout << "numRowsBadFromColor: " << numRowsBadFromColor << std::endl;
	std::cout << "numRowsBadFromNumRanges: " << numRowsBadFromNumRanges << std::endl;

	if (numRowsBadFromNumRanges > m_maxNumFailedRows)
	{
		std::cout << "!! Too many bad laser locations suspected"
				  << ", numRowsBadFromColor=" << numRowsBadFromColor
				  << ", numRowsBadFromNumRanges=" << numRowsBadFromNumRanges
				  << std::endl;
	}

	std::cout << "Detected " << numLocations << " laser pixels." << std::endl;

	// Lookup the 3D locations for the laser points
	numLocationsMapped = 0;

	if (numLocations > 0)
	{
		time1 = GetTimeInSeconds();
		locMapper.mapPoints(m_laserLocations, image1, m_columnPoints, numLocations, numLocationsMapped);
		timingStats->pointMappingTime += GetTimeInSeconds() - time1;

		if (numLocations != numLocationsMapped)
		{
			std::cout << "Discarded " << numLocations - numLocationsMapped << " points." << std::endl;
		}
	}
	else
	{
		// Stop here if we didn't detect the laser at all
		std::cerr << "!!! Could not detect laser at all" << std::endl;
		timingStats->numEmptyFrames++;
	}


	// Map the points if there was something to map
	if (numLocationsMapped > 0)
	{
		time1 = GetTimeInSeconds();

		if (m_writeRangeCsvEnabled)
		{
			writeRangePoints(m_columnPoints, numLocationsMapped, laserSide);
		}

		// Rotate the points
		rotatePoints(m_columnPoints, rotation, numLocationsMapped);

		std::vector<DataPoint>  rawResults;
		for (int iLoc = 0; iLoc < numLocationsMapped; iLoc++)
		{
			DataPoint record;
			record.pixel = m_laserLocations[iLoc];
			record.point = m_columnPoints[iLoc];
			record.rotation = laserSide == Laser::RIGHT_LASER ? rotation : rotation + m_radiansBetweenLaserPlanes;
			record.frame = frame;
			record.laserSide = (int) laserSide;
			rawResults.push_back(record);
		}

		uint32 maxNumRows = Camera::getInstance()->getImageHeight();
		uint32 numRowBins = MAX(400, m_maxFramesPerRevolution / 4);

		time1 = GetTimeInSeconds();

		// Reduce the number of result rows and filter out some of the noise
		m_results.enter();
		DataPoint::lowpassFilter(results, rawResults, maxNumRows, numRowBins);
		m_results.leave();

		timingStats->pointProcessingTime += GetTimeInSeconds() - time1;
	}

	return true;
}

void Scanner::rotatePoints(ColoredPoint * points, float theta, int numPoints)
{
	// Build the 2D rotation matrix to rotate in the XZ plane
	real c = cos(theta);
	real s = sin(theta);
	
	// Rotate
	for (int iPt = 0; iPt < numPoints; iPt++)
	{
		// Location
		real x = points[iPt].x * c + points[iPt].z * -s;
		real z = points[iPt].x * s + points[iPt].z * c;
		points[iPt].x = x;
		points[iPt].z = z;

		// Normal
		real nx = points[iPt].normal.x * c + points[iPt].normal.z * -s;
		real nz = points[iPt].normal.x * s + points[iPt].normal.z * c;
		points[iPt].normal.x = nx;
		points[iPt].normal.z = nz;
	}
}

void Scanner::writeRangePoints(ColoredPoint * points, int numLocationsMapped, Laser::LaserSide laserSide)
{
	Vector3 laserLoc;

	if (Laser::LEFT_LASER == laserSide)
	{
		laserLoc = m_leftLaserLoc;
	}
	else if (Laser::RIGHT_LASER == laserSide)
	{
		laserLoc = m_rightLaserLoc;
	}
	else
	{
		throw Exception("Invalid LaserSide given");
	}

	int iLoc = 0;
	while (iLoc < numLocationsMapped)
	{
		const ColoredPoint& point = points[iLoc];

		real32 dx = laserLoc.x - point.x;
		real32 dy = laserLoc.y - point.y;
		real32 dz = laserLoc.z - point.z;

		real32 distSq = dx * dx + dy * dy + dz * dz;

		if (iLoc > 0)
		{
			m_rangeFout << ",";
		}

		m_rangeFout << distSq;
		iLoc++;
	}

	Camera * camera = Camera::getInstance();
	int imageHeight = camera->getImageHeight();
	while (iLoc < imageHeight)
	{
		m_rangeFout << ",";
		iLoc++;
	}

	m_rangeFout << std::endl;
}

void Scanner::logTimingStats(std::ostream& out, const Scanner::TimingStats& stats)
{
	// Prevent divide by zero
	if (stats.numFrames == 0)
	{
		return;
	}

	double now = GetTimeInSeconds();
	double totalTime = now - stats.startTime;

	double accountedTime = stats.xyzWritingTime + stats.facetizationTime + stats.stlWritingTime + stats.imageAcquisitionTime + stats.imageProcessingTime + stats.pointMappingTime
			+ stats.pointProcessingTime + stats.rotationTime + stats.plyWritingTime + stats.laserTime + stats.laserMergeTime;

	double unaccountedTime = totalTime - accountedTime;
	double rate = totalTime / stats.numFrames;

	out << "Total Seconds per frame:\t" << rate << std::endl;
	out << "Unaccounted time:\t" << (100.0 * unaccountedTime / totalTime) << "%" << std::endl;
	out << "Image Acquisition:\t" << (100.0 * stats.imageAcquisitionTime / totalTime) << "%, " << (stats.imageAcquisitionTime / stats.numFrames) << " seconds per frame." << std::endl;
	out << "Image Processing:\t" << (100.0 * stats.imageProcessingTime / totalTime) << "%, " << (stats.imageProcessingTime / stats.numFrames) << " seconds per frame." << std::endl;
	out << "Laser Time:\t" << (100.0 * stats.laserTime / totalTime) << "%, " << (stats.laserTime / stats.numFrames) << " seconds per frame." << std::endl;
	out << "Point Mapping:\t" << (100.0 * stats.pointMappingTime / totalTime) << "%" << std::endl;
	out << "Point Processing:\t" << (100.0 * stats.pointProcessingTime / totalTime) << "%" << std::endl;
	out << "Table Rotation:\t" << (100.0 * stats.rotationTime / totalTime) << "%" << std::endl;
	out << "Laser Merging:\t" << (100.0 * stats.laserMergeTime / totalTime) << "%" << std::endl;
	out << "PLY Writing:\t" << (100.0 * stats.plyWritingTime / totalTime) << "%" << std::endl;
	out << "STL Writing:\t" << (100.0 * stats.stlWritingTime / totalTime) << "%" << std::endl;
	out << "XYZ Writing:\t" << (100.0 * stats.xyzWritingTime / totalTime) << "%" << std::endl;
	out << "Facetization:\t" << (100.0 * stats.facetizationTime / totalTime) << "%" << std::endl;
	out << "Num Frame Retries:\t" << stats.numFrameRetries << std::endl;
	out << "Num Frames:\t" << stats.numFrames << std::endl;
	out << "Num Empties:\t" << stats.numEmptyFrames << std::endl;

	out << "Point Memory:\t" << (sizeof(DataPoint) * (m_leftLaserResults.size() + m_rightLaserResults.size())) / 1024.0 / 1024.0 << " MB" << std::endl;

	out << "Total Time (min):\t" << (totalTime / 60.0) << std::endl << std::endl;
}


std::vector<ScanResult> Scanner::getPastScanResults()
{
	DIR * dirp = opendir(SCAN_OUTPUT_DIR.c_str());
	if (dirp == NULL)
	{
		throw Exception("Error opening scan directory");
	}

	struct dirent *dp = readdir(dirp);

	std::map<std::string, ScanResult> scanResultMap;

	while (dp != NULL)
	{
		std::string name = dp->d_name;
		size_t dotPos = name.find_last_of(".");

		if (name != "." && name != ".." && dotPos != std::string::npos)
		{
			std::string extension = name.substr(dotPos + 1);
			std::string base = name.substr(0, dotPos);

			std::string fullPath = std::string(SCAN_OUTPUT_DIR) + "/" + name;
			ScanResultFile file;

			struct stat st;
			if (stat(fullPath.c_str(), &st) == 0)
			{
				file.fileSize = st.st_size;
			}
			else
			{
				file.fileSize = 0;
				std::cerr << "Error obtaining stats on file: " << fullPath << ", error=" << strerror(errno) << std::endl;
			}

			file.extension = extension;
			file.creationTime = atol(base.c_str());

			scanResultMap[base].files.push_back(file);
		}

		dp = readdir(dirp);
	}

	closedir(dirp);

	//
	// Turn the map into a vector
	//
	std::vector<ScanResult> results;

	std::map<std::string, ScanResult>::iterator it = scanResultMap.begin();
	while (it != scanResultMap.end())
	{
		std::sort(it->second.files.begin(), it->second.files.end(), CompareScanResultFiles);
		results.push_back(it->second);
		it++;
	}

	// Sort the results
	std::sort(results.begin(), results.end(), CompareScanResults);

	return results;
}


Image * Scanner::acquireImage()
{
	Image * image = m_camera->acquireImage();
	if (image == NULL)
	{
		throw Exception("Camera returned NULL image");
	}

	return image;
}

void Scanner::releaseImage(Image * image)
{
	if (image != NULL)
	{
		m_camera->releaseImage(image);
	}
}

void Scanner::delayAcquisitionForLaser()
{
	m_camera->setAcquisitionDelay(m_laserDelaySec);
}

} // ns sdl

