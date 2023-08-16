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
#define _FILE_OFFSET_BITS 64

#include <time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>

#include "util.h"
#include "camera.h"

#include "savesettingswindow.h"
#include "playbackwindow.h"
#include "ui_playbackwindow.h"

#include <QTimer>
#include <QMessageBox>
#include <QSettings>
#include <QKeyEvent>

#define USE_AUTONAME_FOR_SAVE ""
#define MIN_FREE_SPACE 20000000

playbackWindow::playbackWindow(QWidget *parent, Camera * cameraInst, bool autosave) :
	QWidget(parent),
	ui(new Ui::playbackWindow)
{
	QSettings appSettings;
	VideoStatus vStatus;
	ui->setupUi(this);
	this->setWindowFlags(Qt::Dialog /*| Qt::WindowStaysOnTopHint*/ | Qt::FramelessWindowHint);
	
	ui->lblSave->setVisible(false);
	ui->progressBar->setVisible(false);
	ui->lblSave->setText("Processing");
	ui->progressBar->setValue(0);
	ui->guides->setVisible(false);

	camera = cameraInst;

	autoSaveFlag = autosave;
	autoRecordFlag = camera->get_autoRecord();
	// this->move(camera->ButtonsOnLeft? 0:600, 0);
	saveDoneTimer = NULL;
	saveAborted = false;
	saveAbortedAutomatically = false;
	
	camera->vinst->getStatus(&vStatus);

	camera->cinst->getImagerSettings(&is);
	gainDigital = is.digitalGain;
	on_digitalGain_valueChanged(gainDigital);
	camera->cinst->listen("digitalGain", this, SLOT(on_digitalGain_valueChanged(const QVariant &)));
	on_videoZoom_valueChanged(camera->vinst->videoZoom);
	camera->cinst->listen("videoZoom", this, SLOT(on_videoZoom_valueChanged(const QVariant &)));

	/* Configure the overlay. */
	camera->cinst->setString("overlayFormat", "%.6h/%.6z Sg=%g/%i T=%.8Ss");
	camera->cinst->setString("overlayPosition", "bottom");
	camera->cinst->setBool("overlayEnable", appSettings.value("camera/overlayEnabled", false).toBool());

	playFrame = 0;
	playLoop = false;
	totalFrames = vStatus.totalFrames;

	sw = new StatusWindow;

	connect(ui->cmdClose, SIGNAL(clicked()), this, SLOT(close()));

	ui->verticalSlider->setMinimum(0);
	ui->verticalSlider->setMaximum(totalFrames - 1);
	ui->verticalSlider->setValue(playFrame);
	ui->cmdLoop->setVisible(appSettings.value("camera/demoMode", false).toBool());
	markInFrame = 1;
	markOutFrame = totalFrames;
	ui->verticalSlider->setHighlightRegion(markInFrame, markOutFrame);
	ui->verticalSlider->setFocusProxy(this);

	camera->setPlayMode(true);
	camera->vinst->setPosition(0);
	connect(camera->vinst, SIGNAL(started(VideoState)), this, SLOT(videoStarted(VideoState)));
	connect(camera->vinst, SIGNAL(ended(VideoState, QString)), this, SLOT(videoEnded(VideoState, QString)));

	playbackExponent = 0;

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updatePlayFrame()));
    timer->start(200);

    timerStats = new QTimer(this);
    connect(timerStats, SIGNAL(timeout()), this, SLOT(updateStats()));
    timerStats->start(10000);

	updateStatusText();
	updateStats();

	settingsWindowIsOpen = false;

	if(autoSaveFlag) {
		strcpy(camera->cinst->filename, USE_AUTONAME_FOR_SAVE);

		on_cmdSave_clicked();
	} else {
		strcpy(camera->cinst->filename, appSettings.value("recorder/filename", "").toString().toAscii());
	}
}

playbackWindow::~playbackWindow()
{
	qDebug()<<"playbackwindow deconstructor";
	camera->setPlayMode(false);
	timer->stop();
	timerStats->stop();
	emit finishedSaving();
	delete sw;
	delete ui;
}

void playbackWindow::videoStarted(VideoState state)
{
	char strCancelIcon[48];
	sprintf(strCancelIcon, ":/qss/assets/images/cancel.png");
	QPixmap pixmapCancel(strCancelIcon);
	QIcon ButtonIconCancel(pixmapCancel);

	char strRecordIcon[48];
	sprintf(strRecordIcon, ":/qss/assets/images/record.png");
	QPixmap pixmapRecord(strRecordIcon);
	QIcon ButtonIconRecord(pixmapRecord);

	/* When starting a filesave, increase the frame timing for maximum speed */
	if (state == VIDEO_STATE_FILESAVE) {
		camera->recordingData.hasBeenSaved = true;
		//TODO turn off sensor

		ui->cmdSave->setIcon(ButtonIconCancel);
		ui->cmdSave->setChecked(true);

        //saveDoneTimer = new QTimer(this);
        //connect(saveDoneTimer, SIGNAL(timeout()), this, SLOT(checkForSaveDone()));
        //saveDoneTimer->start(2000);
		saveAbortedAutomatically = false;

		/* Prevent the user from pressing the abort/save button just after the last frame,
		 * as that can make the camera try to save a 2nd video too soon, crashing the camapp.
		 * It is also disabled in checkForSaveDone(), but if the video is very short,
		 * that might not be called at all before the end of the video, so just disable the button right away.*/
		if(markOutFrame - markInFrame < 25 || (markOutFrame - markInFrame < 230 && getSaveFormat() == SAVE_MODE_H264)) {
			ui->cmdSave->setEnabled(false);
		} else {
			ui->cmdSave->setEnabled(true);
		}
	} else {

		ui->cmdSave->setChecked(false);
		ui->cmdSave->setEnabled(true);
		setControlEnable(true);
		emit enableSaveSettingsButtons(true);
	}
	/* TODO: Other start events might occur on HDMI hotplugs. */
}

void playbackWindow::videoEnded(VideoState state, QString err)
{
	if (state == VIDEO_STATE_FILESAVE) { //Filesave has just ended
		QMessageBox msg;

		/* When ending a filesave, restart the sensor and return to live display timing. */
		//TODO turn on sensor

		sw->close();
		ui->lblSave->setVisible(false);
		ui->progressBar->setVisible(false);
		
		if(saveAbortedAutomatically)
			QMessageBox::warning(this, "Warning - Insufficient free space", "Save aborted due to insufficient free space.");
		char strRecordIcon[48];
		sprintf(strRecordIcon, ":/qss/assets/images/record.png");
		QPixmap pixmapRecord(strRecordIcon);
		QIcon ButtonIconRecord(pixmapRecord);
		ui->cmdSave->setIcon(ButtonIconRecord);
		ui->cmdSave->setChecked(false);
		saveAborted = false;
		autoSaveFlag = false;
		
		if(saveDoneTimer){
			saveDoneTimer->stop();
			delete saveDoneTimer;
			saveDoneTimer = NULL;
			qDebug()<<"saveDoneTimer deleted";
		} else qDebug("cannot delete saveDoneTimer because it is null");

		/* If recording failed from an error. Tell the user about it. */
		if (!err.isNull()) {
			msg.setText("Recording Failed: " + err);
			msg.exec();
			return;
		}

		ui->verticalSlider->setHighlightRegion(markInFrame, markOutFrame);
		if(autoRecordFlag) {
			qDebug()<<"autoRecordFlag is true. closing";
			delete this;
		}
	}
}

void playbackWindow::on_verticalSlider_sliderMoved(int position)
{
	/* Note that a rate of zero will also pause playback. */
	stopPlayLoop();
	camera->vinst->setPosition(position);
}

void playbackWindow::on_verticalSlider_valueChanged(int value)
{

}

void playbackWindow::on_cmdPlayForward_pressed()
{
	int fps = (playbackExponent >= 0) ? (60 << playbackExponent) : 60 / (1 - playbackExponent);
	stopPlayLoop();
	camera->vinst->setPlayback(fps);
}

void playbackWindow::on_cmdPlayForward_released()
{
	stopPlayLoop();
	camera->vinst->setPlayback(0);
}

void playbackWindow::on_cmdPlayReverse_pressed()
{
	int fps = (playbackExponent >= 0) ? (60 << playbackExponent) : 60 / (1 - playbackExponent);
	stopPlayLoop();
	camera->vinst->setPlayback(-fps);
}

void playbackWindow::on_cmdPlayReverse_released()
{
	stopPlayLoop();
	camera->vinst->setPlayback(0);
}

void playbackWindow::on_cmdSave_clicked()
{
	UInt32 ret;
	QMessageBox msg;
	char parentPath[1000];
	struct statfs statfsBuf;
	uint64_t estimatedSize;
	QSettings appSettings;
	
	autoRecordFlag = camera->autoRecord = camera->get_autoRecord();

	//Build the parent path of the save directory, to determine if it's a mount point
	strcpy(parentPath, camera->cinst->fileDirectory);
	strcat(parentPath, "/..");

	if(camera->vinst->getStatus(NULL) != VIDEO_STATE_FILESAVE)
	{
		save_mode_type format = getSaveFormat();
		FrameGeometry frame;

		camera->cinst->getResolution(&frame);

		//If no directory set, complain to the user
		if(strlen(camera->cinst->fileDirectory) == 0)
		{
			msg.setText("No save location set! Set save location in Settings");
			msg.exec();
			return;
		}

		if (!statfs(camera->cinst->fileDirectory, &statfsBuf)) {

			unsigned int numFrames = (markOutFrame - markInFrame + 3);
			uint64_t freeSpace = statfsBuf.f_bsize * (uint64_t)statfsBuf.f_bfree;
			uint64_t fileOverhead = 4096;
			double bpp = appSettings.value("recorder/bitsPerPixel", camera->vinst->bitsPerPixel).toDouble();
			bool fileOverMaxSize = (statfsBuf.f_type == 0x4d44); // Check for file size limits for FAT32 only.

			/* Estimate bits per pixel, by format. */
			switch(format) {
			case SAVE_MODE_H264:
				bpp *= 1.2;	/* Add a fudge factor. */
				break;
			case SAVE_MODE_DNG:
			case SAVE_MODE_TIFF_RAW:
				fileOverMaxSize = false;
				fileOverhead *= numFrames;
			case SAVE_MODE_RAW16:
				bpp = 16;
				break;
			case SAVE_MODE_RAW12:
				bpp = 12;
				break;
			case SAVE_MODE_TIFF:
				fileOverMaxSize = false;
				fileOverhead *= numFrames;
				if (camera->getIsColor()) {
					bpp = 24;
				} else {
					bpp = 8;
				}
				break;

			default:
				bpp = 16;
				break;
			}

			// calculate estimated file size
			estimatedSize = (frame.pixels() * numFrames);
			estimatedSize = ((double)estimatedSize * bpp) / 8;
			estimatedSize += fileOverhead;

			qDebug("===================================");
			qDebug("Resolution: %d x %d", frame.hRes, frame.vRes);
			qDebug("Bits/pixel: %f", bpp);
			qDebug("Frames: %d", markOutFrame - markInFrame + 1);
			qDebug("Free space: %llu", freeSpace);
			qDebug("Estimated file size: %llu", estimatedSize);
			qDebug("===================================");

			fileOverMaxSize = fileOverMaxSize && (estimatedSize > 4294967296);
			insufficientFreeSpaceEstimate = (estimatedSize > freeSpace);

			//If amount of free space is below both 10MB and below the estimated size of the video, do not allow the save to start
			if(insufficientFreeSpaceEstimate && (MIN_FREE_SPACE > freeSpace)) {
				QMessageBox::warning(this, "Warning - Insufficient free space", "Cannot save a video because of insufficient free space", QMessageBox::Ok);
				return;
			}

			if(!autoSaveFlag){
				if (fileOverMaxSize && !insufficientFreeSpaceEstimate) {
					QMessageBox::StandardButton reply;
					reply = QMessageBox::warning(this, "Warning - File size over limit", "Estimated file size is larger than the 4GB limit for the the filesystem.\nAttempt to save anyway?", QMessageBox::Yes|QMessageBox::No);
					if(QMessageBox::Yes != reply)
						return;
				}
				
				if (insufficientFreeSpaceEstimate && !fileOverMaxSize) {
					QMessageBox::StandardButton reply;
					reply = QMessageBox::warning(this, "Warning - Insufficient free space", "Estimated file size is larger than free space on drive.\nAttempt to save anyway?", QMessageBox::Yes|QMessageBox::No);
					if(QMessageBox::Yes != reply)
						return;
				}
				
				if (fileOverMaxSize && insufficientFreeSpaceEstimate){
					QMessageBox::StandardButton reply;
					reply = QMessageBox::warning(this, "Warning - File size over limits", "Estimated file size is larger than free space on drive.\nEstimated file size is larger than the 4GB limit for the the filesystem.\nAttempt to save anyway?", QMessageBox::Yes|QMessageBox::No);
					if(QMessageBox::Yes != reply)
						return;
				}
			}
		}

		//Check that the path exists
		struct stat sb;
		struct stat sbP;

        qDebug() << stat(camera->cinst->fileDirectory, &sb);
        perror("stat");

        if (stat(camera->cinst->fileDirectory, &sb) == 0 && S_ISDIR(sb.st_mode) &&
            stat(parentPath, &sbP) == 0 && sb.st_dev != sbP.st_dev)		//If location is directory and is a mount point (device ID of parent is different from device ID of path)
		{
			UInt32 bppBitrate = camera->vinst->bitsPerPixel * frame.pixels() * camera->vinst->framerate;
			UInt32 realBitrate = min(bppBitrate, min(60000000, (UInt32)(camera->vinst->maxBitrate * 1000000.0)));

			ret = camera->cinst->saveRecording(markInFrame - 1, markOutFrame - markInFrame + 1, format, camera->vinst->framerate, realBitrate);
			if (RECORD_FILE_EXISTS == ret) {
				msg.setText("File already exists. Rename then try saving again.");
				msg.exec();
				return;
			}
			else if (RECORD_FOLDER_DOES_NOT_EXIST == ret) {
				qDebug() << "RECORD_FOLDER_DOES_NOT_EXIST";
				if(autoSaveFlag) {
					strncpy(camera->cinst->fileFolder, "\0", 1);
					ret = camera->cinst->saveRecording(markInFrame - 1, markOutFrame - markInFrame + 1, format, camera->vinst->framerate, realBitrate);
				} else {
					msg.setText("Save folder does not exist.");
					msg.exec();
					return;
				}
			}
			else if (RECORD_DIRECTORY_NOT_WRITABLE == ret) {
				if(autoSaveFlag) {
					strncpy(camera->cinst->fileFolder, "\0", 1);
					ret = camera->cinst->saveRecording(markInFrame - 1, markOutFrame - markInFrame + 1, format, camera->vinst->framerate, realBitrate);
				} else {
					msg.setText("Save directory is not writable.");
					msg.exec();
					return;
				}
			}
			else if (RECORD_ERROR == ret) {
				if(autoSaveFlag) {
					strncpy(camera->cinst->fileFolder, "\0", 1);
					ret = camera->cinst->saveRecording(markInFrame - 1, markOutFrame - markInFrame + 1, format, camera->vinst->framerate, realBitrate);
				} else {
					msg.setText("Record Error");
					msg.exec();
					return;
				}
			}
			else if (RECORD_INSUFFICIENT_SPACE == ret) {
				msg.setText("Selected device does not have sufficient free space.");
				msg.exec();
				return;
			}

			ui->cmdSave->setEnabled(false);
			setControlEnable(false);
			// sw->setText(" Saving... ");
			// sw->show();
			ui->lblSave->setText("<p>Preparing to Save</p>");
			ui->progressBar->setValue(0);
			ui->lblSave->setVisible(true);
			ui->progressBar->setVisible(true);

			ui->verticalSlider->appendRegionToList();
			ui->verticalSlider->setHighlightRegion(markOutFrame, markOutFrame);
			//both arguments should be markout because a new rectangle will be drawn,
			//and it should not overlap the one that was just appended
			emit enableSaveSettingsButtons(false);
		}
		else {
			msg.setText(QString("Save location ") + QString(camera->cinst->fileDirectory) + " not found, set save location in Settings");
			msg.exec();
			return;
		}
	}
	else {
		//This block is executed when Abort is clicked
		//or when save is automatically aborted due to full storage
		camera->vinst->stopRecording();
		ui->cmdSave->setEnabled(false);
		ui->verticalSlider->removeLastRegionFromList();
		ui->verticalSlider->setHighlightRegion(markInFrame, markOutFrame);
		saveAborted = true;
		autoRecordFlag = false;
	}
}

void playbackWindow::on_cmdGainDigital_toggled(bool checked)
{
	if (checked) {
		ui->cmdFrameRate->setChecked(false);
	} else {
		ui->verticalSlider->setFocus(Qt::OtherFocusReason);
	}
	char strGainDigitalIcon[48];
	sprintf(strGainDigitalIcon, ":/qss/assets/images/gain-digital-%s.png", checked ? "checked" : "unchecked");
	QPixmap pixmapGainDigital(strGainDigitalIcon);
	QIcon ButtonIconGainDigital(pixmapGainDigital);
	ui->cmdGainDigital->setIcon(ButtonIconGainDigital);
}

void playbackWindow::on_cmdFrameRate_toggled(bool checked)
{
	if (checked) {
		ui->cmdGainDigital->setChecked(false);
	} else {
		ui->verticalSlider->setFocus(Qt::OtherFocusReason);
	}
}

void playbackWindow::addDotsToString(QString* abc)
{
	periodsToAdd = (periodsToAdd + 1) % 4;
	if (periodsToAdd == 0)     abc->append("   ");
	else if(periodsToAdd == 1) abc->append(".  ");
	else if(periodsToAdd == 2) abc->append(".. ");
	else if(periodsToAdd == 3) abc->append("...");
}

void playbackWindow::on_cmdSaveSettings_clicked()
{
	saveSettingsWindow *w = new saveSettingsWindow(NULL, camera);
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->show();
	
	settingsWindowIsOpen = true;
	if(camera->ButtonsOnLeft) w->move(201, 0);
	ui->cmdSaveSettings->setEnabled(false);
	ui->cmdClose->setEnabled(false);
	connect(w, SIGNAL(destroyed()), this, SLOT(saveSettingsClosed()));
	connect(this, SIGNAL(enableSaveSettingsButtons(bool)), w, SLOT(setControlEnable(bool)));
	connect(this, SIGNAL(destroyed(QObject*)), w, SLOT(close()));
}

void playbackWindow::saveSettingsClosed(){
	settingsWindowIsOpen = false;
	if(camera->vinst->getStatus(NULL) != VIDEO_STATE_FILESAVE) {
		/* Only enable these buttons if the camera is not saving a video */
		ui->cmdSaveSettings->setEnabled(true);
		ui->cmdClose->setEnabled(true);
	}
}

void playbackWindow::on_cmdMarkIn_clicked()
{
	markInFrame = playFrame + 1;
	if(markOutFrame < markInFrame)
		markOutFrame = markInFrame;
	ui->verticalSlider->setHighlightRegion(markInFrame, markOutFrame);
	updateStatusText();
}

void playbackWindow::on_cmdMarkOut_clicked()
{
	markOutFrame = playFrame + 1;
	if(markInFrame > markOutFrame)
		markInFrame = markOutFrame;
	ui->verticalSlider->setHighlightRegion(markInFrame, markOutFrame);
	updateStatusText();
}

void playbackWindow::keyPressEvent(QKeyEvent *ev)
{
	if(ui->cmdFrameRate->isChecked()) {
		switch (ev->key()) {
			case Qt::Key_Up:
				if(playbackExponent < 5)
					playbackExponent++;
				break;

			case Qt::Key_Down:
				if(playbackExponent > -5)
					playbackExponent--;
				break;

			case Qt::Key_PageUp:
				if(playbackExponent < 5)
					playbackExponent++;
				break;

			case Qt::Key_PageDown:
				if(playbackExponent > -5)
					playbackExponent--;
				break;
		}
		int fps = (playbackExponent >= 0) ? (60 << playbackExponent) : 60 / (1 - playbackExponent);
		if(ui->cmdLoop->isChecked()) {
			camera->vinst->setFrameRate(fps);
		}
	} else if(ui->cmdGainDigital->isChecked()) {
		switch (ev->key())
		{
			case Qt::Key_Up:
			case Qt::Key_PageUp:
				if (gainDigital <= 16)
				{
					gainDigital+=0.1;
				}
				break;
			case Qt::Key_Down:
			case Qt::Key_PageDown:
				if (gainDigital > 1)
				{
					gainDigital-=0.1;
				}
				break;
		}
		camera->cinst->setProperty("digitalGain", gainDigital);
	} else {
		unsigned int skip = 10;
		if (playbackExponent > 0) {
			skip <<= playbackExponent;
		}

		switch (ev->key()) {
		case Qt::Key_Up:
			camera->vinst->seekFrame(1);
			break;

		case Qt::Key_Down:
			camera->vinst->seekFrame(-1);
			break;

		case Qt::Key_PageUp:
			camera->vinst->seekFrame(skip);
			break;

		case Qt::Key_PageDown:
			camera->vinst->seekFrame(-skip);
			break;
		}
	}
}

void playbackWindow::updateStatusText()
{
	// char text[100];
    // sprintf(text, "Mark start %d\r\nMark end %d", markInFrame, markOutFrame);
    // //sprintf(text, "Frame %d/%d\r\nMark start %d\r\nMark end %d", playFrame + 1, totalFrames, markInFrame, markOutFrame);
	// ui->lblInfo->setText(text);
    ui->cmdMarkIn->setText(QString::number(markInFrame));
    ui->cmdMarkOut->setText(QString::number(markOutFrame));

    // char frame[30];
    // sprintf(frame, "  %d/%d", playFrame + 1, totalFrames);
    ui->cmdCurrentFrame->setText(QString::number(playFrame));
    ui->cmdTotalFrames->setText(QString::number(totalFrames));
}

void playbackWindow::updateSWText(){
	QString statusWindowText;
	if (!saveAborted) {
        int percent = int(float(playFrame) / float(markOutFrame) * 100);
		statusWindowText = QString("<p>Saving</p><p>%1%</p>").arg(percent);
	} else if (saveAbortedAutomatically) {
		statusWindowText = QString(" Storage is now full; Aborting ");
	} else {
		statusWindowText = QString(" Aborting Save ");
	}
	// addDotsToString(&statusWindowText);
	sw->setText(statusWindowText);
}

//Periodically check if the play frame is updated
void playbackWindow::updatePlayFrame()
{
	VideoStatus st;
	char playRateStr[100];
	camera->vinst->getStatus(&st);

    /* Update the position */
	playFrame = st.position;
	ui->verticalSlider->setValue(st.position);
	updateStatusText();
    if (st.state == VIDEO_STATE_FILESAVE)
    {
        struct statvfs statvfsBuf;
        statvfs(camera->cinst->fileDirectory, &statvfsBuf);
        qDebug("Free space: %llu  (%lu * %lu)", statvfsBuf.f_bsize * (uint64_t)statvfsBuf.f_bfree, statvfsBuf.f_bsize, statvfsBuf.f_bfree);

        /* Prevent the user from pressing the abort/save button just after the last frame,
         * as that can make the camera try to save a 2nd video too soon, crashing the camapp.*/
        UInt32 framerate = (UInt32) st.framerate;
        if(playFrame + framerate > markOutFrame)
            ui->cmdSave->setEnabled(false);

        /*Abort the save if insufficient free space,
        but not if the save has already been aborted,
        or if the save button is not enabled(unsafe to abort at that time)(except if save mode is RAW)*/
        bool insufficientFreeSpaceCurrent = (MIN_FREE_SPACE > statvfsBuf.f_bsize * (uint64_t)statvfsBuf.f_bfree);
        if(insufficientFreeSpaceCurrent &&
           insufficientFreeSpaceEstimate &&
           !saveAborted &&
            (ui->cmdSave->isEnabled() || getSaveFormat() != SAVE_MODE_H264)
           )
        {
            saveAbortedAutomatically = true;
            on_cmdSave_clicked();
        }

        // updateSWText();
		QString saveText;
		if (!saveAborted) {
			if (playFrame < markOutFrame && st.framerate < 25000) {
				int percent = int(float(playFrame - markInFrame) / float(markOutFrame - markInFrame) * 100);
				int eta = int(float(markOutFrame - playFrame) / float(st.framerate));
				saveText = QString("<p>Saving @%1 fps<br>ETA: %2 seconds<br>Frame %3 of %4</p>").arg(int(st.framerate)).arg(eta).arg(playFrame).arg(markOutFrame);
				ui->progressBar->setValue(percent);
			} else {
				saveText = QString("Processing");
			}
			ui->cmdSaveSettings->setText(QString("%1gb Available").arg(int(double(statvfsBuf.f_bsize * (uint64_t)statvfsBuf.f_bfree) / 1073741824.0)));
		} else if (saveAbortedAutomatically) {
			saveText = QString("Storage is now full; Aborting");
		} else {
			saveText = QString("Aborting Save");
		}
		ui->lblSave->setText(saveText);
    }
    else
    {
        if (markOutFrame - markInFrame < 10 && getSaveFormat() == SAVE_MODE_H264) ui->cmdSave->setEnabled(false);
        else
        {
            if (ui->cmdMarkIn->isEnabled())
                ui->cmdSave->setEnabled(true);
        }
    }

    /* Update the framerate. */
	if (st.state != VIDEO_STATE_FILESAVE) {
		st.framerate = (playbackExponent >= 0) ? (60 << playbackExponent) : 60.0 / (1 - playbackExponent);
		sprintf(playRateStr, "%.0f fps", st.framerate);
		ui->cmdFrameRate->setText(playRateStr);
	}
}

//Once save is done, re-enable the window
void playbackWindow::checkForSaveDone()
{
	VideoStatus st;
    camera->vinst->getStatus(&st);

	if(st.state == VIDEO_STATE_FILESAVE) {
		setControlEnable(false);

		struct statvfs statvfsBuf;
		statvfs(camera->cinst->fileDirectory, &statvfsBuf);
		qDebug("Free space: %llu  (%lu * %lu)", statvfsBuf.f_bsize * (uint64_t)statvfsBuf.f_bfree, statvfsBuf.f_bsize, statvfsBuf.f_bfree);
		
		/* Prevent the user from pressing the abort/save button just after the last frame,
		 * as that can make the camera try to save a 2nd video too soon, crashing the camapp.*/
		UInt32 framerate = (UInt32) st.framerate;
		if(playFrame + framerate > markOutFrame)
			ui->cmdSave->setEnabled(false);
		
		/*Abort the save if insufficient free space,
		but not if the save has already been aborted,
		or if the save button is not enabled(unsafe to abort at that time)(except if save mode is RAW)*/
		bool insufficientFreeSpaceCurrent = (MIN_FREE_SPACE > statvfsBuf.f_bsize * (uint64_t)statvfsBuf.f_bfree);
		if(insufficientFreeSpaceCurrent &&
		   insufficientFreeSpaceEstimate &&
		   !saveAborted &&
				(ui->cmdSave->isEnabled() ||
				getSaveFormat() != SAVE_MODE_H264)
		   ) {
			saveAbortedAutomatically = true;
			on_cmdSave_clicked();
		}
		
		updateSWText();
	}
}

void playbackWindow::setControlEnable(bool en)
{
	//While settings window is open, don't let the user
	//close the playback window or open another settings window.
	if(!settingsWindowIsOpen){
		ui->cmdClose->setEnabled(en);
		ui->cmdSaveSettings->setEnabled(en);
	}
	ui->cmdMarkIn->setEnabled(en);
	ui->cmdMarkOut->setEnabled(en);
	ui->cmdPlayForward->setEnabled(en);
	ui->cmdPlayReverse->setEnabled(en);
	ui->cmdFrameRate->setEnabled(en);
	ui->cmdLoop->setEnabled(en);
	ui->verticalSlider->setEnabled(en);
}

void playbackWindow::stopPlayLoop(void)
{
	playLoop = false;
	ui->cmdLoop->setChecked(false);
	char strPlayIcon[48];
	sprintf(strPlayIcon, ":/qss/assets/images/play.png");
	QPixmap pixmapPlay(strPlayIcon);
	QIcon ButtonIconPlay(pixmapPlay);
	ui->cmdLoop->setIcon(ButtonIconPlay);
}

void playbackWindow::on_cmdClose_clicked()
{
	camera->recordingData.hasBeenViewed = true;
	camera->autoRecord = false;
}

save_mode_type playbackWindow::getSaveFormat()
{
	QSettings appSettings;

	switch (appSettings.value("recorder/saveFormat", SAVE_MODE_H264).toUInt()) {
	case 0:  return SAVE_MODE_H264;
	case 1:  return SAVE_MODE_RAW16;
	case 2:  return SAVE_MODE_RAW12;
	case 3:  return SAVE_MODE_DNG;
	case 4:  return SAVE_MODE_TIFF;
	case 5:  return SAVE_MODE_TIFF_RAW;
	default: return SAVE_MODE_H264;
	}
}

void playbackWindow::on_cmdLoop_clicked()
{
	if (playLoop) {
		stopPlayLoop();
		camera->vinst->setPlayback(0);
	}
	else {
		int fps = (playbackExponent >= 0) ? (60 << playbackExponent) : 60 / (1 - playbackExponent);
		unsigned int count = (markOutFrame - markInFrame + 1);
		playLoop = true;
		ui->cmdLoop->setChecked(true);
		char strStopIcon[48];
		sprintf(strStopIcon, ":/qss/assets/images/stop.png");
		QPixmap pixmapStop(strStopIcon);
		QIcon ButtonIconStop(pixmapStop);
		ui->cmdLoop->setIcon(ButtonIconStop);
		camera->vinst->loopPlayback(markInFrame - 1, count, fps);
	}
}

void playbackWindow::on_cmdBattery_toggled()
{
	updateBatteryData();
}

void playbackWindow::updateStats()
{
	updateBatteryData();
	updateSensorTemperature();
}

void playbackWindow::updateBatteryData()
{
	QStringList names = {
		"externalPower",
		"batteryPresent",
		"batteryVoltage",
		"batteryChargePercent"};
    QVariantMap battData = camera->cinst->getPropertyGroup(names);

	if (!battData.isEmpty())
	{
		externalPower = battData["externalPower"].toBool();
		batteryPresent = battData["batteryPresent"].toBool();
		batteryVoltage = battData["batteryVoltage"].toDouble();
		batteryPercent = battData["batteryChargePercent"].toDouble();
	}

	char strBattery[10];
	char strIconBattery[40];
	if (batteryPresent)
	{
		if (ui->cmdBattery->isChecked())
		{
			sprintf(strBattery, "%.2fv", batteryVoltage);
			ui->cmdBattery->setText(strBattery);
		}
		else
		{
			sprintf(strBattery, "%.0f%%", batteryPercent);
			ui->cmdBattery->setText(strBattery);
		}
		if (externalPower)
		{
			if (batteryPercent >= 90)
			{
				sprintf(strIconBattery, ":/qss/assets/images/battery-100-charging.png");
			}
			if (batteryPercent < 90 && batteryPercent > 75)
			{
				sprintf(strIconBattery, ":/qss/assets/images/battery-75-charging.png");
			}
			if (batteryPercent < 75 && batteryPercent > 50)
			{
				sprintf(strIconBattery, ":/qss/assets/images/battery-50-charging.png");
			}
			if (batteryPercent < 50 && batteryPercent > 25)
			{
				sprintf(strIconBattery, ":/qss/assets/images/battery-25-charging.png");
			}
			if (batteryPercent <= 25)
			{
				sprintf(strIconBattery, ":/qss/assets/images/battery-0-charging.png");
			}
		}
		else
		{
			if (batteryPercent >= 90)
			{
				sprintf(strIconBattery, ":/qss/assets/images/battery-100.png");
			}
			if (batteryPercent < 90 && batteryPercent > 75)
			{
				sprintf(strIconBattery, ":/qss/assets/images/battery-75.png");
			}
			if (batteryPercent < 75 && batteryPercent > 50)
			{
				sprintf(strIconBattery, ":/qss/assets/images/battery-50.png");
			}
			if (batteryPercent < 50 && batteryPercent > 25)
			{
				sprintf(strIconBattery, ":/qss/assets/images/battery-25.png");
			}
			if (batteryPercent <= 25)
			{
				sprintf(strIconBattery, ":/qss/assets/images/battery-0.png");
			}
		}
	}
	else
	{
		sprintf(strBattery, "%s", "A/C");
		ui->cmdBattery->setText(strBattery);
		sprintf(strIconBattery, ":/qss/assets/images/battery-missing.png");
	}
	QPixmap pixmap(strIconBattery);
	QIcon ButtonIcon(pixmap);
	ui->cmdBattery->setIcon(ButtonIcon);
}

void playbackWindow::updateSensorTemperature()
{
	int sensorTemp = camera->cinst->getProperty("sensorTemperature", 0.0).toInt();
	char strTemperature[10];
	sprintf(strTemperature, "%u\xb0", sensorTemp);
	ui->cmdTemperature->setText(strTemperature);

	char strIcon[10];
	if (sensorTemp > 49)
	{
		sprintf(strIcon, ":/qss/assets/images/temperature-%s.png", "hot");
	}
	else if (sensorTemp < 44)
	{
		sprintf(strIcon, ":/qss/assets/images/temperature-%s.png", "cold");
	}
	else
	{
		sprintf(strIcon, ":/qss/assets/images/temperature-%s.png", "normal");
	}
	QPixmap pixmap(strIcon);
	QIcon ButtonIcon(pixmap);
	ui->cmdTemperature->setIcon(ButtonIcon);
}

void playbackWindow::on_digitalGain_valueChanged(const QVariant &value) 
{
	char strGainDigital[10];
	sprintf(strGainDigital, "x%.1f", value.toDouble());
	ui->cmdGainDigital->setText(strGainDigital);
}


void playbackWindow::on_cmdGuides_toggled(bool checked)
{
	ui->guides->setVisible(checked);
}

void playbackWindow::on_cmdToggleUI_toggled(bool checked)
{
	toggleUI(checked);
}

void playbackWindow::toggleUI(bool en)
{
	// bottom
	ui->cmdBattery->setVisible(en);
	ui->cmdClose->setVisible(en);
	ui->cmdFrameRate->setVisible(en);
	ui->cmdGainDigital->setVisible(en);
	ui->cmdSaveSettings->setVisible(en);
	ui->cmdTemperature->setVisible(en);
	ui->cmdTotalFrames->setVisible(en);
	// top
	ui->cmdCurrentFrame->setVisible(en);
	ui->cmdGuides->setVisible(en);
	ui->cmdLoop->setVisible(en);
	ui->cmdMarkIn->setVisible(en);
	ui->cmdMarkOut->setVisible(en);
	ui->cmdPlayForward->setVisible(en);
	ui->cmdPlayReverse->setVisible(en);
	ui->cmdSave->setVisible(en);
	ui->cmdZoom->setVisible(en);
	ui->verticalSlider->setVisible(en);

	if (!en)
	{
		ui->cmdGainDigital->setChecked(false);
		// ui->cmdFrameRate->setChecked(false);
	}

	// QCoreApplication::processEvents();
}

void playbackWindow::on_videoZoom_valueChanged(const QVariant &value)
{
	videoZoom = value.toDouble();
	char strZoom[10];
	sprintf(strZoom, "%.1fx", videoZoom);
	ui->cmdZoom->setText(strZoom);
	ui->cmdZoom->setChecked(videoZoom > 1.0);
	ui->cmdZoom->setEnabled(true);
}

void playbackWindow::on_cmdZoom_clicked()
{
	ui->cmdZoom->setEnabled(false);
	double hScale = is.geometry.hRes / 800.0;
	double vScale = is.geometry.vRes / 480.0;
	double oneToOne = hScale < vScale ? hScale : vScale;
	if (videoZoom == 1)
	{
		videoZoom = oneToOne;
	}
	else if (videoZoom == oneToOne)
	{
		videoZoom = oneToOne * 2.0;
	}
	else
	{
		videoZoom = 1;
	}
	camera->vinst->setZoom(videoZoom);
}
