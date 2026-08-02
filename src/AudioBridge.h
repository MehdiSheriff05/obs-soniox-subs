/*
Plugin Name
Copyright (C) 2026 Mehdi Sheriff mehdirsheriff@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>

#include <obs.h>

struct audio_resampler;

class AudioBridge : public QObject {
	Q_OBJECT

public:
	using AudioReadyCallback = std::function<void(const uint8_t *data, size_t byteCount)>;

	explicit AudioBridge(QObject *parent = nullptr);
	~AudioBridge() override;

	bool start(const QString &sourceName);
	void stop();

	bool isRunning() const { return m_source != nullptr; }

	void setAudioReadyCallback(AudioReadyCallback callback);

	qint64 msSinceLastAudio() const;

signals:
	void levelChanged(float peakLevel);

private:
	static void audioCaptureCallback(void *param, obs_source_t *source, const struct audio_data *audioData,
					  bool muted);
	void handleAudio(const struct audio_data *audioData, bool muted);

	obs_source_t *m_source = nullptr;
	audio_resampler *m_resampler = nullptr;

	AudioReadyCallback m_audioReadyCallback;
	std::atomic<qint64> m_lastAudioMs{0};
};
