/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "genericcameramodule.h"

#include <QDebug>
#include <QMutexLocker>
#include <QMessageBox>
#include <opencv2/opencv.hpp>

#include "camera.h"
#include "videoviewwidget.h"
#include "genericcamerasettingsdialog.h"

#include "modules/videorecorder/videowriter.h"

QString GenericCameraModuleInfo::id() const
{
    return QStringLiteral("generic-camera");
}

QString GenericCameraModuleInfo::name() const
{
    return QStringLiteral("Generic Camera");
}

QString GenericCameraModuleInfo::description() const
{
    return QStringLiteral("Capture a video with a regular, Linux-compatible camera.");
}

QPixmap GenericCameraModuleInfo::pixmap() const
{
    return QPixmap(":/module/generic-camera");
}

AbstractModule *GenericCameraModuleInfo::createModule(QObject *parent)
{
    return new GenericCameraModule(parent);
}

GenericCameraModule::GenericCameraModule(QObject *parent)
    : ImageSourceModule(parent),
      m_camera(nullptr),
      m_videoView(nullptr),
      m_camSettingsWindow(nullptr)
{
    m_camera = new Camera;

    m_frameRing = boost::circular_buffer<Frame>(64);
    m_outStream = registerOutputPort<Frame>("Video");

    m_videoView = new VideoViewWidget;
    m_camSettingsWindow = new GenericCameraSettingsDialog(m_camera);
    addDisplayWindow(m_videoView);
    addSettingsWindow(m_camSettingsWindow);

    // set initial window titles
    setName(name());
}

GenericCameraModule::~GenericCameraModule()
{
    stop();
}

void GenericCameraModule::setName(const QString &name)
{
    ImageSourceModule::setName(name);
    m_videoView->setWindowTitle(name);
    m_camSettingsWindow->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
}

ModuleFeatures GenericCameraModule::features() const
{
    return ModuleFeature::RUN_EVENTS |
           ModuleFeature::RUN_THREADED |
           ModuleFeature::SHOW_DISPLAY |
           ModuleFeature::SHOW_SETTINGS;
}

void GenericCameraModule::attachVideoWriter(VideoWriter *vwriter)
{
    m_vwriters.append(vwriter);
}

int GenericCameraModule::selectedFramerate() const
{
    return m_camSettingsWindow->framerate();
}

cv::Size GenericCameraModule::selectedResolution() const
{
    return m_camSettingsWindow->resolution();
}

bool GenericCameraModule::prepare()
{
    setState(ModuleState::PREPARING);

    if (m_camera->camId() < 0) {
        raiseError("Unable to continue: No valid camera was selected!");
        return false;
    }

    statusMessage("Connecting camera...");
    if (!m_camera->connect()) {
        raiseError(QStringLiteral("Unable to connect camera: %1").arg(m_camera->lastError()));
        return false;
    }
    m_camera->setResolution(m_camSettingsWindow->resolution());
    statusMessage("Launching DAQ thread...");

    m_camSettingsWindow->setRunning(true);
    m_fps = m_camSettingsWindow->framerate();
    m_running = true;
    statusMessage("Waiting.");

    setState(ModuleState::READY);
    return true;
}

void GenericCameraModule::start()
{
    m_camera->setStartTime(m_timer->startTime());
    statusMessage("Acquiring frames...");
    setState(ModuleState::RUNNING);
}

bool GenericCameraModule::runEvent()
{
    m_mutex.lock();
    if (m_frameRing.empty()) {
        m_mutex.unlock();
        return true;
    }

    auto statusText = QStringLiteral("<html>Display buffer: %1/%2").arg(m_frameRing.size()).arg(m_frameRing.capacity());

    auto frame = m_frameRing.front();
    m_videoView->showImage(frame.mat);
    m_frameRing.pop_front();

    // END OF SAFE ZONE
    m_mutex.unlock();

    // warn if there is a bigger framerate drop
    if (m_currentFps < (m_fps - 2))
        statusText = QStringLiteral("%1 - <font color=\"red\"><b>Framerate is too low!</b></font>").arg(statusText);
    statusMessage(statusText);

    // send frame away to connected image sinks, and hope they are
    // handling this efficiently and don't block the loop
    emit newFrame(frame);

    // show framerate directly in the window title, to make reduced framerate very visible
    m_videoView->setWindowTitle(QStringLiteral("%1 (%2 fps)").arg(m_name).arg(m_currentFps));

    return true;
}

void GenericCameraModule::runThread(OptionalWaitCondition *waitCondition)
{
    m_currentFps = m_fps;
    auto frameRecordFailedCount = 0;

    // wait until we actually start
    waitCondition->wait(this);

    while (m_running) {
        const auto cycleStartTime = currentTimePoint();

        cv::Mat frame;
        std::chrono::milliseconds time;
        if (!m_camera->recordFrame(&frame, &time)) {
            frameRecordFailedCount++;
            if (frameRecordFailedCount > 32) {
                m_running = false;
                raiseError(QStringLiteral("Too many attempts to record frames from this camera have failed. Is the camera connected properly?"));
            }
            continue;
        }

        // record this frame, if we have any video writers registered
        Q_FOREACH(auto vwriter, m_vwriters)
            vwriter->pushFrame(frame, time);

        m_mutex.lock();
        m_frameRing.push_back(Frame(frame, time));
        m_mutex.unlock();

        // wait a bit if necessary, to keep the right framerate
        const auto cycleTime = timeDiffToNowMsec(cycleStartTime);
        const auto extraWaitTime = std::chrono::milliseconds((1000 / m_fps) - cycleTime.count());
        if (extraWaitTime.count() > 0)
            std::this_thread::sleep_for(extraWaitTime);

        const auto totalTime = timeDiffToNowMsec(cycleStartTime);
        m_currentFps = static_cast<int>(1 / (totalTime.count() / static_cast<double>(1000)));
    }
}

void GenericCameraModule::stop()
{
    // ensure we unregister all video writers before starting another run,
    // and after finishing the current one, as the modules they belong to
    // may meanwhile have been removed
    m_vwriters.clear();

    statusMessage("Cleaning up...");
    m_running = false;

    m_camera->disconnect();
    m_camSettingsWindow->setRunning(false);
    statusMessage("Camera disconnected.");
}

QByteArray GenericCameraModule::serializeSettings(const QString &confBaseDir)
{
    Q_UNUSED(confBaseDir)
    QJsonObject jsettings;
    jsettings.insert("camera", m_camera->camId());
    jsettings.insert("width", m_camSettingsWindow->resolution().width);
    jsettings.insert("height", m_camSettingsWindow->resolution().height);
    jsettings.insert("fps", m_camSettingsWindow->framerate());
    jsettings.insert("gain", m_camera->gain());
    jsettings.insert("exposure", m_camera->exposure());

    return jsonObjectToBytes(jsettings);
}

bool GenericCameraModule::loadSettings(const QString &confBaseDir, const QByteArray &data)
{
    Q_UNUSED(confBaseDir)

    auto jsettings = jsonObjectFromBytes(data);
    m_camera->setCamId(jsettings.value("camera").toInt());
    m_camera->setResolution(cv::Size(jsettings.value("width").toInt(), jsettings.value("height").toInt()));
    m_camera->setExposure(jsettings.value("exposure").toDouble());
    m_camera->setGain(jsettings.value("gain").toDouble());
    m_camSettingsWindow->setFramerate(jsettings.value("fps").toInt());

    m_camSettingsWindow->updateValues();
    return true;
}
