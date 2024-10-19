/*
 *  Copyright © 2017-2024 Wellington Wallace
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

#include "output_level.hpp"
#include <algorithm>
#include <span>
#include <string>
#include "pipeline_type.hpp"
#include "plugin_base.hpp"
#include "pw_manager.hpp"
#include "tags_plugin_name.hpp"
#include "util.hpp"

OutputLevel::OutputLevel(const std::string& tag, pw::Manager* pipe_manager, PipelineType pipe_type, QString instance_id)
    : PluginBase(tag,
                 tags::plugin_name::BaseName::outputLevel,
                 tags::plugin_package::Package::ee,
                 instance_id,
                 pipe_manager,
                 pipe_type) {}

OutputLevel::~OutputLevel() {
  if (connected_to_pw) {
    disconnect_from_pw();
  }

  util::debug(log_tag + name.toStdString() + " destroyed");
}

void OutputLevel::setup() {
  util::debug(log_tag + name.toStdString() + ": PipeWire blocksize: " + util::to_string(n_samples, ""));
  util::debug(log_tag + name.toStdString() + ": PipeWire sampling rate: " + util::to_string(rate, ""));
}

void OutputLevel::process(std::span<float>& left_in,
                          std::span<float>& right_in,
                          std::span<float>& left_out,
                          std::span<float>& right_out) {
  std::copy(left_in.begin(), left_in.end(), left_out.begin());
  std::copy(right_in.begin(), right_in.end(), right_out.begin());

  get_peaks(left_in, right_in, left_out, right_out);
}

void OutputLevel::process([[maybe_unused]] std::span<float>& left_in,
                          [[maybe_unused]] std::span<float>& right_in,
                          [[maybe_unused]] std::span<float>& left_out,
                          [[maybe_unused]] std::span<float>& right_out,
                          [[maybe_unused]] std::span<float>& probe_left,
                          [[maybe_unused]] std::span<float>& probe_right) {}

auto OutputLevel::get_latency_seconds() -> float {
  return 0.0F;
}