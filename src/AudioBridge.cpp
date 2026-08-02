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

#include "AudioBridge.h"

#include <media-io/audio-resampler.h>

#include <QDateTime>

#include <algorithm>
#include <cmath>
#include <vector>

AudioBridge::AudioBridge(QObject *parent) : QObject(parent) {}

AudioBridge::~AudioBridge()
{
	stop();
}

bool AudioBridge::start(const QString &sourceName)
{
	stop();

	obs_source_t *source = obs_get_source_by_name(sourceName.toUtf8().constData());
	if (!source)
		return false;

	m_source = source;
	m_lastAudioMs = QDateTime::currentMSecsSinceEpoch();

	obs_source_add_audio_capture_callback(m_source, &AudioBridge::audioCaptureCallback, this);
	return true;
}

void AudioBridge::stop()
{
	if (m_source) {
		obs_source_remove_audio_capture_callback(m_source, &AudioBridge::audioCaptureCallback, this);
		obs_source_release(m_source);
		m_source = nullptr;
	}

	if (m_resampler) {
		audio_resampler_destroy(m_resampler);
		m_resampler = nullptr;
	}
}

void AudioBridge::setAudioReadyCallback(AudioBridge::AudioReadyCallback callback)
{
	m_audioReadyCallback = std::move(callback);
}

qint64 AudioBridge::msSinceLastAudio() const
{
	if (!m_source)
		return 0;
	return QDateTime::currentMSecsSinceEpoch() - m_lastAudioMs.load();
}

void AudioBridge::audioCaptureCallback(void *param, obs_source_t *source, const struct audio_data *audioData,
					bool muted)
{
	auto *self = static_cast<AudioBridge *>(param);
	self->handleAudio(audioData, muted);
	(void)source;
}

void AudioBridge::handleAudio(const struct audio_data *audioData, bool muted)
{
	m_lastAudioMs = QDateTime::currentMSecsSinceEpoch();

	obs_audio_info info;
	if (!obs_get_audio_info(&info))
		return;

	const uint32_t planeChannels = get_audio_channels(info.speakers);

	if (!muted) {
		float peak = 0.0f;
		for (uint32_t ch = 0; ch < planeChannels && ch < MAX_AV_PLANES; ch++) {
			const float *samples = reinterpret_cast<const float *>(audioData->data[ch]);
			if (!samples)
				continue;
			for (uint32_t i = 0; i < audioData->frames; i++)
				peak = std::max(peak, std::fabs(samples[i]));
		}
		emit levelChanged(peak);
	} else {
		emit levelChanged(0.0f);
	}

	if (!m_audioReadyCallback)
		return;

	if (!m_resampler) {
		resample_info dst;
		dst.samples_per_sec = 16000;
		dst.format = AUDIO_FORMAT_16BIT;
		dst.speakers = SPEAKERS_MONO;

		resample_info src;
		src.samples_per_sec = info.samples_per_sec;
		src.format = AUDIO_FORMAT_FLOAT_PLANAR;
		src.speakers = info.speakers;

		m_resampler = audio_resampler_create(&dst, &src);
		if (!m_resampler)
			return;
	}

	uint8_t *output[MAX_AV_PLANES] = {};
	uint32_t outFrames = 0;
	uint64_t tsOffset = 0;

	bool ok = audio_resampler_resample(m_resampler, output, &outFrames, &tsOffset, audioData->data,
					    audioData->frames);
	if (!ok || outFrames == 0 || !output[0])
		return;

	if (muted) {
		static thread_local std::vector<uint8_t> silence;
		size_t byteCount = static_cast<size_t>(outFrames) * sizeof(int16_t);
		silence.assign(byteCount, 0);
		m_audioReadyCallback(silence.data(), byteCount);
		return;
	}

	m_audioReadyCallback(output[0], static_cast<size_t>(outFrames) * sizeof(int16_t));
}
