/****************************************************************************
 *  Copyright (C) 2013-2017 Kron Technologies Inc <http://www.krontech.ca>. *
 *                                                                          *
 *  This program is free software: you can redistribute it and/or modify    *
 *  it under the terms of the GNU General Public License as published by    *
 *  the Free Software Foundation, either version 3 of the License, or       *
 *  (at your option) any later version.                                     *
 *                                                                          *
 *  This program is distributed in the hope that it will be useful,         *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *  GNU General Public License for more details.                            *
 *                                                                          *
 *  You should have received a copy of the GNU General Public License       *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 ****************************************************************************/
#ifndef CAMERA_H
#define CAMERA_H

#include <pthread.h>
#include <semaphore.h>

#include "errorCodes.h"
#include "defines.h"

#include "video.h"
#include "control.h"
#include "sensor.h"
#include "userInterface.h"
#include "string.h"
#include "types.h"

#define RECORD_DATA_LENGTH		2048		//Number of record data entries for the record sequencer data
#define MAX_FRAME_WORDS			0x19000
#define CAL_REGION_START		0x0
#define CAL_REGION_FRAMES		3
#define LIVE_REGION_START		(CAL_REGION_START + MAX_FRAME_WORDS * CAL_REGION_FRAMES)
#define LIVE_REGION_FRAMES		3
#define REC_REGION_START		(LIVE_REGION_START + MAX_FRAME_WORDS * LIVE_REGION_FRAMES)
#define FRAME_ALIGN_WORDS		64			//Align to 256 byte boundaries (8 32-byte words)
#define RECORD_LENGTH_MIN       1           //Minimum number of frames in the record region
#define SEGMENT_COUNT_MAX       (32*1024)   //Maximum number of record segments in segmented mode
#define FPN_ADDRESS				CAL_REGION_START

#define MAX_FRAME_SIZE_H		1952
#define MAX_FRAME_SIZE_V		1080
#define MAX_FRAME_SIZE          (MAX_FRAME_SIZE_H * MAX_FRAME_SIZE_V * 8 / 12)
#define BITS_PER_PIXEL			12
#define BYTES_PER_WORD			32

#define MAX_LIVE_FRAMERATE      60
#define MAX_RECORD_FRAMERATE    230

#define FPN_AVERAGE_FRAMES		16	//Number of frames to average to get FPN correction data

#define SENSOR_DATA_WIDTH		12
#define COLOR_MATRIX_INT_BITS	3

#define IMAGE_GAIN_FUDGE_FACTOR 1.0		//Multiplier to make sure clipped ADC value actually clips image
#define COL_OFFSET_FOOTROOM		32		// Train ADC to not-quite zero to give footroom for noise.
#define COL_GAIN_FRAC_BITS		12		// 2-point column gain fractional bits.
#define COL_CURVE_FRAC_BITS		21		// 3-point column curvature factional bits.

#define SETTING_FLAG_TEMPORARY  1
#define SETTING_FLAG_USESAVED   2

#define FREE_SPACE_MARGIN_MULTIPLIER 1.1    //Drive must have at least this factor more free space than the estimated file size to allow saving

#define CAMERA_MAX_EXPOSURE_TARGET 3584

#define TRIGGERDELAY_TIME_RATIO 0
#define TRIGGERDELAY_SECONDS 1
#define TRIGGERDELAY_FRAMES 2

/*
typedef enum CameraErrortype
{
	CAMERA_ERROR_NONE = 0,
	CAMERA_THREAD_ERROR,
	CAMERA_ALREADY_RECORDING,
	CAMERA_NOT_RECORDING,
	CAMERA_NO_RECORDING_PRESENT,
	CAMERA_IN_PLAYBACK_MODE,
	CAMERA_ERROR_SENSOR,
	CAMERA_INVALID_IMAGER_SETTINGS,
	CAMERA_FILE_NOT_FOUND,
	CAMERA_FILE_ERROR,
	CAMERA_ERROR_IO,
	CAMERA_INVALID_SETTINGS
} CameraErrortype;
*/

typedef enum CameraRecordModes
{
    RECORD_MODE_NORMAL = 0,
    RECORD_MODE_SEGMENTED,
    RECORD_MODE_GATED_BURST,
    RECORD_MODE_FPN
} CameraRecordModeType;

typedef union SeqPrmMemWord_t
{
	struct settings_t
	{
	unsigned termRecTrig	: 1;
	unsigned termRecMem		: 1;
	unsigned termRecBlkEnd	: 1;
	unsigned termBlkFull	: 1;
	unsigned termBlkLow		: 1;
	unsigned termBlkHigh	: 1;
	unsigned termBlkFalling	: 1;
	unsigned termBlkRising	: 1;
	unsigned next			: 4;
	unsigned blkSize		: 32;
	unsigned pad			: 20;
	} __attribute__ ((__packed__)) settings;

	struct data_t
	{
		UInt32 low;
		UInt32 high;
	} data;
} SeqPgmMemWord;

typedef struct {
	FrameGeometry geometry;		//Frame geometry.
	UInt32 exposure;            //10ns increments
	UInt32 period;              //Frame period in 10ns increments
	UInt32 gain;
	UInt32 recRegionSizeFrames; //Number of frames in the entire record region
	CameraRecordModeType mode;  //Recording mode
	UInt32 segments;            //Number of segments in segmented mode
	UInt32 segmentLengthFrames; //Length of segment in segmented mode
	UInt32 prerecordFrames;     //Number of frames to record before each burst in Gated Burst mode

	struct {
		unsigned temporary : 1; // set this to disable saving of state
		unsigned disableRingBuffer : 1; //Set this to disable the ring buffer (record ends when at the end of memory rather than rolling over to the beginning)
	};
} ImagerSettings_t;


typedef struct {
	ImagerSettings_t is;
	bool valid;
	bool hasBeenSaved;
} RecordSettings_t;

typedef struct {
	UInt32 discardCount;
	UInt32 inputXRes;
	UInt32 inputYRes;
	UInt32 outputXRes;
	UInt32 outputYRes;
	UInt32 xScale;
	UInt32 yScale;
	UInt32 leftOffset;
	UInt32 topFracOffset;
} ScalerSettings_t;

typedef struct {
	const char *name;
	double matrix[9];
} ColorMatrix_t;

//class Camera
//
class Camera : public QObject {
	Q_OBJECT

public:
	Camera();
	~Camera();
	CameraErrortype init(Video * vinstInst, Control * cinstInst, ImageSensor * sensorInst, UserInterface * userInterface, UInt32 ramSizeVal, bool color);
	Int32 startRecording(void);
	Int32 setRecSequencerModeNormal();
	Int32 stopRecording(void);
	bool getIsRecording(void);
	Video * vinst;
    Control * cinst;
	ImageSensor * sensor;
	UserInterface * ui;

	RecordSettings_t recordingData;
	ImagerSettings_t getImagerSettings() { return imagerSettings; }
	UInt32 getRecordLengthFrames(ImagerSettings_t settings);
    CameraData cData;
    SensorData sData;
    SensorLimits sLimits;
    SensorSettings sSettings;
	CameraStatus cStatus;

	unsigned short getTriggerDelayConstant();
	void setTriggerDelayConstant(unsigned short value);
	void setTriggerDelayValues(double ratio, double seconds, UInt32 frames);
	void updateTriggerValues(ImagerSettings_t settings);
	unsigned short triggerDelayConstant;
	double triggerTimeRatio;
	double triggerPostSeconds;
	UInt32 triggerPostFrames;
	double maxPostFramesRatio;

	bool liveLoopActive;
	UInt32 liveLoopTime = 2000;	//in milliseconds

	void getSensorInfo(Control *c);
	UInt32 setImagerSettings(ImagerSettings_t settings);
	UInt32 setIntegrationTime(double intTime, FrameGeometry *geometry, Int32 flags);
	UInt32 setPlayMode(bool playMode);
	Int32 autoColGainCorrection(void);
	Int32 takeWhiteReferences(void);

	void setCCMatrix(const double *matrix);
	void setWhiteBalance(const double *rgb);
	int autoWhiteBalance(unsigned int x, unsigned int y);
	void setFocusAid(bool enable);
	bool getFocusAid();
	int blackCalAllStdRes(bool factory = false);

	void setBncDriveLevel(UInt32 level);
	void setTriggerDelayFrames(UInt32 delayFrames);

	bool getFocusPeakEnable(void);
	void setFocusPeakEnable(bool en);
	bool getZebraEnable(void);
	void setZebraEnable(bool en);
	int getUnsavedWarnEnable(void);
	void setUnsavedWarnEnable(int newSetting);
	int getAutoPowerMode(void);
	void setAutoPowerMode(int newSetting);
	bool getFanDisable(void);
	void setFanDisable(bool dis);

	char * getSerialNumber(void) {return serialNumber;}
	void setSerialNumber(const char * sn) {strcpy(serialNumber, sn);}
	bool getIsColor() {return isColor;}

	UInt32 getFrameSizeWords(FrameGeometry *geometry);
	UInt32 getMaxRecordRegionSizeFrames(FrameGeometry *geometry);

	bool getRecording(void);
	UInt8 getFocusPeakColorLL(void);
	void setFocusPeakColorLL(UInt8 color);
	void setFocusPeakThresholdLL(UInt32 thresh);
	UInt32 getFocusPeakThresholdLL(void);
	Int32 getRamSizeGB(UInt32 * stick0SizeGB, UInt32 * stick1SizeGB);
	Int32 readSerialNumber(char * dest);
	Int32 writeSerialNumber(char * src);
	UInt16 getFPGAVersion(void);
	UInt16 getFPGASubVersion(void);
	bool ButtonsOnLeft;
	bool UpsideDownDisplay;

private:
	friend void* recDataThread(void *arg);

	volatile bool recording;
	bool playbackMode;

	ImagerSettings_t imagerSettings;
	bool isColor;

	double imgGain;
	bool focusPeakEnabled;
	int focusPeakColorIndex;
	bool zebraEnabled;
	bool fanDisabled;
	char serialNumber[SERIAL_NUMBER_MAX_LEN+1];

	QString fpColors[7] = {"blue", "green", "cyan", "red", "magenta", "yellow", "white"};


public:
	// Actual white balance applied at runtime.
	double whiteBalMatrix[3] = { 1.0, 1.0, 1.0 };
	double colorCalMatrix[9] = {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0
	};

	/* Color Matrix Presets */
	const ColorMatrix_t ccmPresets[2] = {
		{ "CIECAM16/D55", {
			  +1.9147, -0.5768, -0.2342,
			  -0.3056, +1.3895, -0.0969,
			  +0.1272, -0.9531, +1.6492,
		  }
		},
		{ "CIECAM02/D55", {
			  +1.2330, +0.6468, -0.7764,
			  -0.3219, +1.6901, -0.3811,
			  -0.0614, -0.6409, +1.5258,
		  }
		}
	};

	UInt8 getWBIndex();
	void  setWBIndex(UInt8 index);
	int unsavedWarnEnabled;
	int autoPowerMode;
	bool videoHasBeenReviewed;
	bool autoSave;
	int autoSavePercent;
	bool autoSavePercentEnabled;
	bool shippingMode;

	void set_autoSave(bool state);
	bool get_autoSave();

	bool autoRecord;
	void set_autoRecord(bool state);
	bool get_autoRecord();

	bool demoMode;
	void set_demoMode(bool state);
	bool get_demoMode();

	bool getButtonsOnLeft();
	void setButtonsOnLeft(bool en);
	bool getUpsideDownDisplay();
	void setUpsideDownDisplay(bool en);
	bool RotationArgumentIsSet();
	void upsideDownTransform(int rotation);
	void updateVideoPosition();
	int getFocusPeakColor();
	void setFocusPeakColor(int value);

	void on_chkLiveLoop_stateChanged(int arg1);
	void on_spinLiveLoopTime_valueChanged(int arg1);
	int getAutoSavePercent(void);
	void setAutoSavePercent(int newSetting);
	bool getAutoSavePercentEnabled(void);
	void setAutoSavePercentEnabled(bool newSetting);
	bool getShippingMode(void);
	void setShippingMode(int newSetting);


private:
	bool lastRecording;
	bool terminateRecDataThread;
	UInt32 ramSize;
	pthread_t recDataThreadID;
	QTimer * loopTimer;
	bool loopTimerEnabled = false;


protected slots:
	void apiDoSetFramePeriod(UInt32 period);
	void apiDoSetCurrentIso(UInt32 iso);
	void apiDoSetCurrentGain(UInt32 gain );
	void apiDoSetPlaybackPosition(UInt32 frame);
	void apiDoSetPlaybackStart(UInt32 frame);
	void apiDoSetPlaybackLength(UInt32 frames);
	void apiDoSetWbTemperature(UInt32 temp);
	void apiDoSetRecMaxFrames(UInt32 frames);
	void apiDoSetRecSegments(UInt32 seg);
	void apiDoSetRecPreBurst(UInt32 frames);
	void apiDoSetExposurePeriod(UInt32 period);

	void apiDoSetExposurePercent(double percent);
	void apiDoSetExposureNormalized(double norm);
	void apiDoSetIoDelayTime(double delay);
	void apiDoSetFrameRate(double rate);
	void apiDoSetShutterAngle(double angle);

	void apiDoSetExposureMode(QString mode);
	void apiDoSetCameraTallyMode(QString mode);
	void apiDoSetCameraDescription(QString desc);
	void apiDoSetNetworkHostname(QString name);

	void apiDoSetWbMatrix(QVariant wb);
	void apiDoSetColorMatrix(QVariant color);
	void apiDoSetResolution(QVariant res);

	//void apiDoSetInt(QString param, UInt32 value);

	void onLoopTimer();

};

#endif // CAMERA_H
