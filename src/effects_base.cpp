/*
 *  Copyright © 2017-2025 Wellington Wallace
 *
 *  This file is part of Easy Effects.
 *
 *  Easy Effects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Easy Effects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Easy Effects. If not, see <https://www.gnu.org/licenses/>.
 */

#include "effects_base.hpp"
#include <gsl/gsl_interp.h>
#include <gsl/gsl_spline.h>
#include <qcontainerfwd.h>
#include <qnamespace.h>
#include <qobjectdefs.h>
#include <qpoint.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <QSharedPointer>
#include <QString>
#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>
#include "autogain.hpp"
#include "bass_enhancer.hpp"
#include "bass_loudness.hpp"
#include "compressor.hpp"
#include "convolver.hpp"
#include "crossfeed.hpp"
#include "crystalizer.hpp"
#include "db_manager.hpp"
#include "deesser.hpp"
#include "delay.hpp"
#include "echo_canceller.hpp"
#include "exciter.hpp"
#include "expander.hpp"
#include "filter.hpp"
#include "gate.hpp"
#include "level_meter.hpp"
#include "limiter.hpp"
#include "loudness.hpp"
#include "maximizer.hpp"
#include "output_level.hpp"
#include "pipeline_type.hpp"
#include "pitch.hpp"
#include "plugin_base.hpp"
#include "pw_manager.hpp"
#include "reverb.hpp"
#include "rnnoise.hpp"
#include "spa/utils/defs.h"
#include "spectrum.hpp"
#include "speex.hpp"
#include "stereo_tools.hpp"
#include "tags_plugin_name.hpp"
#include "util.hpp"

// #include "deepfilternet.hpp"
// #include "equalizer.hpp"
// #include "multiband_compressor.hpp"
// #include "multiband_gate.hpp"

EffectsBase::EffectsBase(pw::Manager* pipe_manager, PipelineType pipe_type)
    : log_tag(pipe_type == PipelineType::output ? "soe: " : "sie: "), pm(pipe_manager), pipeline_type(pipe_type) {
  using namespace std::string_literals;

  output_level = std::make_shared<OutputLevel>(log_tag, pm, pipeline_type, "0");

  spectrum = std::make_shared<Spectrum>(log_tag, pm, pipeline_type, "0");

  if (!output_level->connected_to_pw) {
    output_level->connect_to_pw();
  }

  if (!spectrum->connected_to_pw) {
    spectrum->connect_to_pw();
  }

  create_filters_if_necessary();

  switch (pipeline_type) {
    case PipelineType::input:
      connect(db::StreamInputs::self(), &db::StreamInputs::pluginsChanged, [&]() { create_filters_if_necessary(); });
      break;
    case PipelineType::output:
      connect(db::StreamOutputs::self(), &db::StreamOutputs::pluginsChanged, [&]() { create_filters_if_necessary(); });
      break;
  }

  connect(db::Main::self(), &db::Main::lv2uiUpdateFrequencyChanged, [&]() {
    auto v = db::Main::lv2uiUpdateFrequency();

    for (auto& plugin : plugins | std::views::values) {
      plugin->set_native_ui_update_frequency(v);
    }
  });
}

EffectsBase::~EffectsBase() {
  util::debug("effects_base: destroyed");
}

void EffectsBase::create_filters_if_necessary() {
  auto list = (pipeline_type == PipelineType::output ? db::StreamOutputs::plugins() : db::StreamInputs::plugins());

  if (list.empty()) {
    return;
  }

  for (const auto& name : list) {
    if (plugins.contains(name)) {
      continue;
    }

    auto instance_id = tags::plugin_name::get_id(name);

    std::shared_ptr<PluginBase> filter = nullptr;

    if (name.startsWith(tags::plugin_name::BaseName::autogain)) {
      filter = std::make_shared<Autogain>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::bassEnhancer)) {
      filter = std::make_shared<BassEnhancer>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::bassLoudness)) {
      filter = std::make_shared<BassLoudness>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::compressor)) {
      filter = std::make_shared<Compressor>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::convolver)) {
      filter = std::make_shared<Convolver>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::crossfeed)) {
      filter = std::make_shared<Crossfeed>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::crystalizer)) {
      filter = std::make_shared<Crystalizer>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::deepfilternet)) {
      //   filter = std::make_shared<DeepFilterNet>(log_tag, tags::schema::deepfilternet::id, path, pm, pipeline_type);
    } else if (name.startsWith(tags::plugin_name::BaseName::deesser)) {
      filter = std::make_shared<Deesser>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::delay)) {
      filter = std::make_shared<Delay>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::echoCanceller)) {
      filter = std::make_shared<EchoCanceller>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::exciter)) {
      filter = std::make_shared<Exciter>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::expander)) {
      filter = std::make_shared<Expander>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::equalizer)) {
      //   filter = std::make_shared<Equalizer>(
      //       log_tag, tags::schema::equalizer::id, path, tags::schema::equalizer::channel_id,
      //       schema_base_path + "equalizer/" + instance_id + "/leftchannel/",
      //       schema_base_path + "equalizer/" + instance_id + "/rightchannel/", pm, pipeline_type);
    } else if (name.startsWith(tags::plugin_name::BaseName::filter)) {
      filter = std::make_shared<Filter>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::gate)) {
      filter = std::make_shared<Gate>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::levelMeter)) {
      filter = std::make_shared<LevelMeter>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::limiter)) {
      filter = std::make_shared<Limiter>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::loudness)) {
      filter = std::make_shared<Loudness>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::maximizer)) {
      filter = std::make_shared<Maximizer>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::multibandCompressor)) {
      //   filter = std::make_shared<MultibandCompressor>(log_tag, tags::schema::multiband_compressor::id, path, pm,
      //                                                  pipeline_type);
    } else if (name.startsWith(tags::plugin_name::BaseName::multibandGate)) {
      //   filter = std::make_shared<MultibandGate>(log_tag, tags::schema::multiband_gate::id, path, pm, pipeline_type);
    } else if (name.startsWith(tags::plugin_name::BaseName::pitch)) {
      filter = std::make_shared<Pitch>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::reverb)) {
      filter = std::make_shared<Reverb>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::rnnoise)) {
      filter = std::make_shared<RNNoise>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::speex)) {
      filter = std::make_shared<Speex>(log_tag, pm, pipeline_type, instance_id);
    } else if (name.startsWith(tags::plugin_name::BaseName::stereoTools)) {
      filter = std::make_shared<StereoTools>(log_tag, pm, pipeline_type, instance_id);
    }

    if (filter != nullptr) {
      /*
        The filters inherit from QObject and we do not want QML to take owndership of them. Double free may happen
        in this case when closing the window or doing similar actions that trigger qml cleanup. The way to avoid this
        is making sure that the objects managed by the c++ backend already have a parent by the time they are used on
        QML.
      */
      filter->setParent(this);
    }

    plugins.insert(std::make_pair(name, filter));
  }
}

void EffectsBase::remove_unused_filters() {
  auto list = (pipeline_type == PipelineType::output ? db::StreamOutputs::plugins() : db::StreamInputs::plugins());

  if (list.empty()) {
    plugins.clear();

    return;
  }

  for (auto it = plugins.begin(); it != plugins.end();) {
    auto key = it->first;

    if (std::ranges::find(list, key) == list.end()) {
      auto plugin = it->second;

      if (plugin == nullptr) {
        it = plugins.erase(it);

        continue;
      }

      plugin->bypass = true;

      if (plugin->connected_to_pw) {
        plugin->disconnect_from_pw();
      }

      it = plugins.erase(it);
    } else {
      it++;
    }
  }
}

void EffectsBase::activate_filters() {
  for (auto& plugin : plugins | std::views::values) {
    plugin->set_active(true);
  }
}

void EffectsBase::deactivate_filters() {
  for (auto& plugin : plugins | std::views::values) {
    plugin->set_active(false);
  }
}

auto EffectsBase::get_plugins_map() -> std::map<QString, std::shared_ptr<PluginBase>> {
  return plugins;
}

QVariant EffectsBase::getPluginInstance(const QString& pluginName) {
  if (!plugins.contains(pluginName)) {
    return {};
  }

  auto p = plugins[pluginName];

  if (pluginName.startsWith(tags::plugin_name::BaseName::autogain)) {
    return QVariant::fromValue(dynamic_cast<Autogain*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::bassEnhancer)) {
    return QVariant::fromValue(dynamic_cast<BassEnhancer*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::bassLoudness)) {
    return QVariant::fromValue(dynamic_cast<BassLoudness*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::compressor)) {
    return QVariant::fromValue(dynamic_cast<Compressor*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::convolver)) {
    return QVariant::fromValue(dynamic_cast<Convolver*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::crossfeed)) {
    return QVariant::fromValue(dynamic_cast<Crossfeed*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::crystalizer)) {
    return QVariant::fromValue(dynamic_cast<Crystalizer*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::delay)) {
    return QVariant::fromValue(dynamic_cast<Delay*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::deesser)) {
    return QVariant::fromValue(dynamic_cast<Deesser*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::echoCanceller)) {
    return QVariant::fromValue(dynamic_cast<EchoCanceller*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::exciter)) {
    return QVariant::fromValue(dynamic_cast<Exciter*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::expander)) {
    return QVariant::fromValue(dynamic_cast<Expander*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::filter)) {
    return QVariant::fromValue(dynamic_cast<Filter*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::gate)) {
    return QVariant::fromValue(dynamic_cast<Gate*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::levelMeter)) {
    return QVariant::fromValue(dynamic_cast<LevelMeter*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::limiter)) {
    return QVariant::fromValue(dynamic_cast<Limiter*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::loudness)) {
    return QVariant::fromValue(dynamic_cast<Loudness*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::maximizer)) {
    return QVariant::fromValue(dynamic_cast<Maximizer*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::pitch)) {
    return QVariant::fromValue(dynamic_cast<Pitch*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::reverb)) {
    return QVariant::fromValue(dynamic_cast<Reverb*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::rnnoise)) {
    return QVariant::fromValue(dynamic_cast<RNNoise*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::speex)) {
    return QVariant::fromValue(dynamic_cast<Speex*>(p.get()));
  }

  if (pluginName.startsWith(tags::plugin_name::BaseName::stereoTools)) {
    return QVariant::fromValue(dynamic_cast<StereoTools*>(p.get()));
  }

  return {};
}

uint EffectsBase::getPipeLineRate() const {
  switch (pipeline_type) {
    case PipelineType::input:
      if (auto node = pm->model_nodes.get_node_by_name(db::StreamInputs::inputDevice());
          node.serial != SPA_ID_INVALID) {
        return node.rate * 0.001F;
      }
      return 0.0F;
    case PipelineType::output: {
      if (auto node = pm->model_nodes.get_node_by_name(db::StreamOutputs::outputDevice());
          node.serial != SPA_ID_INVALID) {
        return node.rate * 0.001F;
      }
      return 0.0F;
    }
    default:
      return 0;
  }
}

uint EffectsBase::getPipeLineLatency() {
  auto list = (pipeline_type == PipelineType::output ? db::StreamOutputs::plugins() : db::StreamInputs::plugins());

  auto v = 0.0F;

  for (const auto& name : list) {
    if (plugins.contains(name) && plugins[name] != nullptr) {
      v += plugins[name]->get_latency_seconds();
    }
  }

  return v * 1000.0F;
}

float EffectsBase::getOutputLevelLeft() const {
  return output_level->output_peak_left;
}

float EffectsBase::getOutputLevelRight() const {
  return output_level->output_peak_right;
}

void EffectsBase::requestSpectrumData() {
  /*
    Technically we can do the same as the other Q_INVOKABLE methods and run the whole thing in the QML thread. But in
    this case we have some heavy operations that need to be done. It is probably better to do them in the main thread
    and deliver the spectrum list to QML through a signal.
  */
  QMetaObject::invokeMethod(
      this,
      [this] {
        auto [rate, list] = spectrum->compute_magnitudes();

        if (list.empty() || rate == 0) {
          return;
        }

        const qsizetype n_bands = list.size();

        QList<double> frequencies(n_bands);

        for (qsizetype n = 0; n < n_bands; n++) {
          frequencies[n] = 0.5F * static_cast<float>(rate) * static_cast<float>(n) / static_cast<float>(n_bands);
        }

        const auto min_freq = static_cast<float>(db::Spectrum::minimumFrequency());
        const auto max_freq = static_cast<float>(db::Spectrum::maximumFrequency());

        if (min_freq > (max_freq - 100.0F)) {
          return;
        }

        std::vector<float> x_axis;

        if (db::Spectrum::logarithimicHorizontalAxis()) {
          x_axis = util::logspace(min_freq, max_freq, db::Spectrum::nPoints());
        } else {
          x_axis = util::linspace(min_freq, max_freq, db::Spectrum::nPoints());
        }

        auto* acc = gsl_interp_accel_alloc();
        auto* spline = gsl_spline_alloc(gsl_interp_steffen, n_bands);

        gsl_spline_init(spline, frequencies.data(), list.data(), n_bands);

        QList<double> spectrum_mag(x_axis.size());

        for (size_t n = 0; n < x_axis.size(); n++) {
          spectrum_mag[n] = gsl_spline_eval(spline, x_axis[n], acc);
        }

        gsl_spline_free(spline);
        gsl_interp_accel_free(acc);

        std::ranges::for_each(spectrum_mag, [](auto& v) {
          v = 10.0F * std::log10(v);

          if (!std::isinf(v)) {
            v = (v > util::minimum_db_level) ? v : util::minimum_db_level;
          } else {
            v = util::minimum_db_level;
          }
        });

        QList<QPointF> output_data(spectrum_mag.size());

        for (qsizetype n = 0; n < spectrum_mag.size(); n++) {
          output_data[n] = QPointF(x_axis[n], spectrum_mag[n]);
        }

        Q_EMIT newSpectrumData(output_data);
      },
      Qt::QueuedConnection);
}
