/**
 * Copyright © 2017-2025 Wellington Wallace
 *
 * This file is part of Easy Effects.
 *
 * Easy Effects is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Easy Effects is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Easy Effects. If not, see <https://www.gnu.org/licenses/>.
 */

#include "crystalizer.hpp"
#include <qlist.h>
#include <qnamespace.h>
#include <qobjectdefs.h>
#include <qtypes.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>
#include "db_manager.hpp"
#include "easyeffects_db_crystalizer.h"
#include "fir_filter_bandpass.hpp"
#include "pipeline_type.hpp"
#include "plugin_base.hpp"
#include "pw_manager.hpp"
#include "tags_plugin_name.hpp"
#include "util.hpp"

// NOLINTNEXTLINE
#define BIND_BAND(index)                                                                                    \
  {                                                                                                         \
    band_intensity.at(index) = util::db_to_linear(settings->intensityBand##index());                        \
    band_mute.at(index) = settings->muteBand##index();                                                      \
    band_bypass.at(index) = settings->bypassBand##index();                                                  \
    connect(settings, &db::Crystalizer::intensityBand##index##Changed,                                      \
            [this]() { band_intensity.at(index) = util::db_to_linear(settings->intensityBand##index()); }); \
    connect(settings, &db::Crystalizer::muteBand##index##Changed,                                           \
            [this]() { band_mute.at(index) = settings->muteBand##index(); });                               \
    connect(settings, &db::Crystalizer::bypassBand##index##Changed,                                         \
            [this]() { band_bypass.at(index) = settings->bypassBand##index(); });                           \
  }

Crystalizer::Crystalizer(const std::string& tag, pw::Manager* pipe_manager, PipelineType pipe_type, QString instance_id)
    : PluginBase(tag,
                 tags::plugin_name::BaseName::crystalizer,
                 tags::plugin_package::Package::ee,
                 instance_id,
                 pipe_manager,
                 pipe_type),
      settings(db::Manager::self().get_plugin_db<db::Crystalizer>(
          pipe_type,
          tags::plugin_name::BaseName::crystalizer + "#" + instance_id)),
      adaptive_intensities(nbands, 1.0F) {
  for (uint n = 0U; n < nbands; n++) {
    filters.at(n) = std::make_unique<FirFilterBandpass>(log_tag + name.toStdString() + " band" + util::to_string(n));
  }

  std::ranges::fill(band_mute, false);
  std::ranges::fill(band_bypass, false);
  std::ranges::fill(band_intensity, 1.0F);
  std::ranges::fill(band_previous_L, 0.0F);
  std::ranges::fill(band_previous_R, 0.0F);
  std::ranges::fill(env_kurtosis_L, 3.0F);
  std::ranges::fill(env_kurtosis_R, 3.0F);
  std::ranges::fill(env_crest_L, 1.0F);
  std::ranges::fill(env_crest_R, 1.0F);

  auto f_edges = make_geometric_edges(20, 20000);

  freq_centers = compute_band_centers(f_edges);

  for (uint n = 0; n <= nbands; n++) {
    frequencies[n] = f_edges[n];
  }

  for (uint n = 0; n < nbands; n++) {
    freq_scaling[n] = std::sqrt(freq_ref / freq_centers[n]);
  }

  init_common_controls<db::Crystalizer>(settings);

  BIND_BAND(0);
  BIND_BAND(1);
  BIND_BAND(2);
  BIND_BAND(3);
  BIND_BAND(4);
  BIND_BAND(5);
  BIND_BAND(6);
  BIND_BAND(7);
  BIND_BAND(8);
  BIND_BAND(9);
  BIND_BAND(10);
  BIND_BAND(11);
  BIND_BAND(12);
}

Crystalizer::~Crystalizer() {
  if (connected_to_pw) {
    disconnect_from_pw();
  }

  settings->disconnect();

  data_mutex.lock();

  filters_are_ready = false;

  data_mutex.unlock();

  util::debug(std::format("{}{} destroyed", log_tag, name.toStdString()));
}

void Crystalizer::reset() {
  settings->setDefaults();
}

void Crystalizer::setup() {
  data_mutex.lock();

  filters_are_ready = false;

  data_mutex.unlock();

  block_time = static_cast<float>(n_samples) / static_cast<float>(rate);

  attack_coeff = std::exp(-block_time / attack_time);
  release_coeff = std::exp(-block_time / release_time);

  /**
   * As zita uses fftw we have to be careful when reinitializing it.
   * The thread that creates the fftw plan has to be the same that destroys it.
   * Otherwise segmentation faults can happen. As we do not want to do this
   * initializing in the plugin realtime thread we send it to the main thread.
   */

  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)

  QMetaObject::invokeMethod(
      this,
      [this] {
        if (filters_are_ready) {
          return;
        }

        blocksize = n_samples;

        n_samples_is_power_of_2 = (n_samples & (n_samples - 1U)) == 0 && n_samples != 0U;

        if (!n_samples_is_power_of_2) {
          while ((blocksize & (blocksize - 1U)) != 0 && blocksize > 2U) {
            blocksize--;
          }
        }

        util::debug(std::format("{}{} blocksize: {}", log_tag, name.toStdString(), blocksize));

        notify_latency = true;
        is_first_buffer = true;

        latency_n_frames = 0U;

        buf_in_L.clear();
        buf_in_R.clear();
        buf_out_L.clear();
        buf_out_R.clear();

        data_L.resize(blocksize);
        data_R.resize(blocksize);

        for (uint n = 0U; n < nbands; n++) {
          band_data_L.at(n).resize(blocksize);
          band_data_R.at(n).resize(blocksize);

          band_second_derivative_L.at(n).resize(blocksize);
          band_second_derivative_R.at(n).resize(blocksize);
        }

        for (uint n = 0U; n < nbands; n++) {
          filters.at(n)->set_n_samples(blocksize);
          filters.at(n)->set_rate(rate);

          filters.at(n)->set_min_frequency(frequencies.at(n));
          filters.at(n)->set_max_frequency(frequencies.at(n + 1U));

          filters.at(n)->setup();
        }

        data_mutex.lock();

        filters_are_ready = true;

        data_mutex.unlock();
      },
      Qt::QueuedConnection);

  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

void Crystalizer::process(std::span<float>& left_in,
                          std::span<float>& right_in,
                          std::span<float>& left_out,
                          std::span<float>& right_out) {
  std::scoped_lock<std::mutex> lock(data_mutex);

  if (bypass || !filters_are_ready) {
    std::ranges::copy(left_in, left_out.begin());
    std::ranges::copy(right_in, right_out.begin());

    return;
  }

  if (input_gain != 1.0F) {
    apply_gain(left_in, right_in, input_gain);
  }

  if (n_samples_is_power_of_2 && blocksize == n_samples) {
    std::ranges::copy(left_in, left_out.begin());
    std::ranges::copy(right_in, right_out.begin());

    enhance_peaks(left_out, right_out);
  } else {
    buf_in_L.insert(buf_in_L.end(), left_in.begin(), left_in.end());
    buf_in_R.insert(buf_in_R.end(), right_in.begin(), right_in.end());

    while (buf_in_L.size() >= blocksize) {
      util::copy_bulk(buf_in_L, data_L);
      util::copy_bulk(buf_in_R, data_R);

      enhance_peaks(data_L, data_R);

      buf_out_L.insert(buf_out_L.end(), data_L.begin(), data_L.end());
      buf_out_R.insert(buf_out_R.end(), data_R.begin(), data_R.end());
    }

    // copying the processed samples to the output buffers

    if (buf_out_L.size() >= n_samples) {
      util::copy_bulk(buf_out_L, left_out);
      util::copy_bulk(buf_out_R, right_out);
    } else {
      const uint offset = n_samples - buf_out_L.size();

      if (offset != latency_n_frames) {
        latency_n_frames = offset;

        notify_latency = true;
      }

      // Fill beginning with zeros
      std::fill_n(left_out.begin(), offset, 0.0F);
      std::fill_n(right_out.begin(), offset, 0.0F);

      std::ranges::copy(buf_out_L, left_out.begin() + offset);
      std::ranges::copy(buf_out_R, right_out.begin() + offset);

      buf_out_L.clear();
      buf_out_R.clear();
    }
  }

  if (output_gain != 1.0F) {
    apply_gain(left_out, right_out, output_gain);
  }

  if (notify_latency) {
    latency_value = static_cast<float>(latency_n_frames) / static_cast<float>(rate);

    util::debug(std::format("{}{} latency: {} s", log_tag, name.toStdString(), latency_value));

    update_filter_params();

    notify_latency = false;
  }

  if (updateLevelMeters) {
    get_peaks(left_in, right_in, left_out, right_out);
  }
}

void Crystalizer::process([[maybe_unused]] std::span<float>& left_in,
                          [[maybe_unused]] std::span<float>& right_in,
                          [[maybe_unused]] std::span<float>& left_out,
                          [[maybe_unused]] std::span<float>& right_out,
                          [[maybe_unused]] std::span<float>& probe_left,
                          [[maybe_unused]] std::span<float>& probe_right) {}

auto Crystalizer::get_latency_seconds() -> float {
  return this->latency_value;
}

auto Crystalizer::compute_kurtosis(float* data) const -> float {
  float mean = 0.0F;

  for (uint i = 0; i < blocksize; i++) {
    mean += data[i];
  }

  mean /= blocksize;

  // Calculate second and fourth moments

  float m2 = 0.0F;
  float m4 = 0.0F;

  for (uint i = 0; i < blocksize; i++) {
    float deviation = data[i] - mean;
    float deviation2 = deviation * deviation;

    m2 += deviation2;
    m4 += deviation2 * deviation2;
  }

  m2 /= blocksize;
  m4 /= blocksize;

  return (m2 > 1e-6F) ? m4 / (m2 * m2) : 3.0F;
}

auto Crystalizer::compute_crest(float* data) const -> float {
  float rms = 0.0;

  float peak = 0.0F;

  for (uint i = 0; i < blocksize; i++) {
    float v = std::fabs(data[i]);

    rms += v * v;

    peak = std::max(v, peak);
  }

  rms = std::sqrt(rms / blocksize);

  return (rms > 1e-6F) ? (peak / rms) : 1.0F;
}

void Crystalizer::compute_buffer_crest(float* data, const bool& isLeft) {
  float crest = compute_crest(data);

  auto& env_crest = isLeft ? buffer_crest_L : buffer_crest_R;

  float alpha = (crest > env_crest) ? attack_coeff : release_coeff;

  env_crest = (alpha * env_crest) + ((1.0F - alpha) * crest);
}

void Crystalizer::compute_buffer_kurtosis(float* data, const bool& isLeft) {
  float kurtosis = compute_kurtosis(data);

  auto& env_kurtosis = isLeft ? buffer_kurtosis_L : buffer_kurtosis_R;

  float alpha = (kurtosis > env_kurtosis) ? attack_coeff : release_coeff;

  env_kurtosis = (alpha * env_kurtosis) + ((1.0F - alpha) * kurtosis);
}

auto Crystalizer::compute_adaptive_intensity(const uint& band_index,
                                             float base_intensity,
                                             float* band_data,
                                             const bool& isLeft) -> float {
  // kurtosis calculation

  float kurtosis = compute_kurtosis(band_data);

  auto& env_kurtosis = isLeft ? env_kurtosis_L[band_index] : env_kurtosis_R[band_index];

  float alpha = (kurtosis > env_kurtosis) ? attack_coeff : release_coeff;

  env_kurtosis = (alpha * env_kurtosis) + ((1.0F - alpha) * kurtosis);

  float kurtosis_ratio = isLeft ? env_kurtosis / buffer_kurtosis_L : env_kurtosis / buffer_kurtosis_R;

  // crest calculation

  float crest = compute_crest(band_data);

  auto& env_crest = isLeft ? env_crest_L[band_index] : env_crest_R[band_index];

  float alpha_crest = (crest > env_crest) ? attack_coeff : release_coeff;

  env_crest = (alpha_crest * env_crest) + ((1.0F - alpha_crest) * crest);

  float crest_ratio = isLeft ? env_crest / buffer_crest_L : env_crest / buffer_crest_R;

  // intensity calculation

  auto intensity = base_intensity * std::sqrtf(crest_ratio * kurtosis_ratio) * freq_scaling[band_index];

  // util::warning(std::format("n = {}, intensity = {}, kurtosis_r = {}, crest_r = {}", band_index, intensity,
  //                           kurtosis_ratio, crest_ratio));

  return intensity;
}

auto Crystalizer::extrapolate_next(const std::vector<float>& x) -> float {
  size_t n = x.size();

  if (x.size() < 2) {
    return x.back();
  }

  float xm1 = x[n - 2];
  float xm = x[n - 1];

  return (2.0F * xm) - xm1;
}

auto Crystalizer::make_geometric_edges(float fmin, float fmax) -> std::array<float, nbands + 1U> {
  std::array<float, nbands + 1U> edges;

  float gamma = 0.413F;

  float r = fmax / fmin;

  for (unsigned i = 0; i <= nbands; i++) {
    float t = static_cast<float>(i) / nbands;

    edges[i] = fmin * std::pow(r, std::pow(t, gamma));
  }

  return edges;
}

auto Crystalizer::compute_band_centers(const std::array<float, nbands + 1U>& edges) -> std::array<float, nbands> {
  std::array<float, nbands> centers;

  for (size_t i = 0; i < nbands; i++) {
    float low = edges[i];
    float high = edges[i + 1];

    centers[i] = 0.5F * (high + low);
  }

  return centers;
}

float Crystalizer::getBandFrequency(const int& index) {
  return freq_centers[index];
}

QList<float> Crystalizer::getAdaptiveIntensities() {
  return adaptive_intensities;
}