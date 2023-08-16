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
#ifndef PLAYBACKWINDOW_H
#define PLAYBACKWINDOW_H

#include <QWidget>
#include <QDebug>

#include "statuswindow.h"
#include "util.h"
#include "camera.h"

namespace Ui {
class playbackWindow;
}

class playbackWindow : public QWidget
{
	Q_OBJECT
	
public:
	explicit playbackWindow(QWidget *parent = 0, Camera * cameraInst = NULL, bool autosave = false);
	~playbackWindow();

private slots:
	void on_verticalSlider_sliderMoved(int position);

	void on_verticalSlider_valueChanged(int value);

	void on_cmdPlayForward_pressed();

	void on_cmdPlayForward_released();

	void on_cmdPlayReverse_pressed();

	void on_cmdPlayReverse_released();

	void on_cmdSave_clicked();

	void on_cmdSaveSettings_clicked();

	void on_cmdMarkIn_clicked();

	void on_cmdMarkOut_clicked();

	void on_cmdBattery_toggled();
	void on_cmdGuides_toggled(bool checked);
	void on_cmdGainDigital_toggled(bool checked);
	void on_cmdFrameRate_toggled(bool checked);
	void on_cmdToggleUI_toggled(bool checked);
	void on_cmdZoom_clicked();
	void on_digitalGain_valueChanged(const QVariant &value);
	void on_videoZoom_valueChanged(const QVariant &value);
	void toggleUI(bool en);
	void updateBatteryData();
	void updateSensorTemperature();
	void updateStats();

	void updatePlayFrame();

	void checkForSaveDone();

	void keyPressEvent(QKeyEvent *ev);
	void on_cmdClose_clicked();
	void saveSettingsClosed();

	void on_cmdLoop_clicked();

	/* Video class signals */
	void videoStarted(VideoState state);
	void videoEnded(VideoState state, QString err);

private:
	Ui::playbackWindow *ui;
	Camera * camera;
	StatusWindow * sw;
	void stopPlayLoop();
	void updateStatusText();
	void setControlEnable(bool en);

	UInt32 markInFrame;
	UInt32 markOutFrame;
	UInt32 totalFrames;
	UInt32 playFrame;
	bool playLoop;
	QTimer * timer;
	QTimer * timerStats;
	QTimer * saveDoneTimer;
	Int32 playbackExponent;
	bool autoSaveFlag, autoRecordFlag;
	bool settingsWindowIsOpen;
	bool saveAborted;
	bool saveAbortedAutomatically;
	bool insufficientFreeSpaceEstimate;
	short periodsToAdd = 0;
	
	double batteryPercent = 0;
	double batteryVoltage = 0;
	bool batteryPresent = false;
	bool externalPower = false;
	ImagerSettings_t is;
	double gainDigital = 1.0;
	double videoZoom = 1.0;

	save_mode_type getSaveFormat();

	void addDotsToString(QString* abc);
	void updateSWText();

signals:
	void finishedSaving();
	void enableSaveSettingsButtons(bool);
};

#endif // PLAYBACKWINDOW_H
