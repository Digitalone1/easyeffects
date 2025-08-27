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

#include "reverb.hpp"
#include <qobject.h>
#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include "db_manager.hpp"
#include "easyeffects_db_reverb.h"
#include "lv2_macros.hpp"
#include "lv2_wrapper.hpp"
#include "pipeline_type.hpp"
#include "plugin_base.hpp"
#include "pw_manager.hpp"
#include "tags_plugin_name.hpp"
#include "util.hpp"

Reverb::Reverb(const std::string& tag, pw::Manager* pipe_manager, PipelineType pipe_type, QString instance_id)
    : PluginBase(tag,
                 tags::plugin_name::BaseName::reverb,
                 tags::plugin_package::Package::calf,
                 instance_id,
                 pipe_manager,
                 pipe_type),
      settings(db::Manager::self().get_plugin_db<db::Reverb>(pipe_type,
                                                             tags::plugin_name::BaseName::reverb + "#" + instance_id)) {
  lv2_wrapper = std::make_unique<lv2::Lv2Wrapper>("http://calf.sourceforge.net/plugins/Reverb");

  package_installed = lv2_wrapper->found_plugin;

  if (!package_installed) {
    util::debug(log_tag + "http://calf.sourceforge.net/plugins/Reverb is not installed");
  }

  init_common_controls<db::Reverb>(settings);

  BIND_LV2_PORT("decay_time", decayTime, setDecayTime, db::Reverb::decayTimeChanged);
  BIND_LV2_PORT("hf_damp", hfDamp, setHfDamp, db::Reverb::hfDampChanged);
  BIND_LV2_PORT("diffusion", diffusion, setDiffusion, db::Reverb::diffusionChanged);
  BIND_LV2_PORT("predelay", predelay, setPredelay, db::Reverb::predelayChanged);
  BIND_LV2_PORT("bass_cut", bassCut, setBassCut, db::Reverb::bassCutChanged);
  BIND_LV2_PORT("treble_cut", trebleCut, setTrebleCut, db::Reverb::trebleCutChanged);
  BIND_LV2_PORT("room_size", roomSize, setRoomSize, db::Reverb::roomSizeChanged);

  // dB controls with -inf mode.

  BIND_LV2_PORT_DB("amount", amount, setAmount, db::Reverb::amountChanged, true);
  BIND_LV2_PORT_DB("dry", dry, setDry, db::Reverb::dryChanged, true);
}

Reverb::~Reverb() {
  if (connected_to_pw) {
    disconnect_from_pw();
  }

  settings->disconnect();

  util::debug(log_tag + name.toStdString() + " destroyed");
}

void Reverb::reset() {
  settings->setDefaults();
}

void Reverb::setup() {
  if (!lv2_wrapper->found_plugin) {
    return;
  }

  lv2_wrapper->set_n_samples(n_samples);

  if (lv2_wrapper->get_rate() != rate) {
    lv2_wrapper->create_instance(rate);
  }
}

void Reverb::process(std::span<float>& left_in,
                     std::span<float>& right_in,
                     std::span<float>& left_out,
                     std::span<float>& right_out) {
  if (!lv2_wrapper->found_plugin || !lv2_wrapper->has_instance() || bypass) {
    std::ranges::copy(left_in, left_out.begin());
    std::ranges::copy(right_in, right_out.begin());

    return;
  }

  if (input_gain != 1.0F) {
    apply_gain(left_in, right_in, input_gain);
  }

  lv2_wrapper->connect_data_ports(left_in, right_in, left_out, right_out);
  lv2_wrapper->run();

  if (output_gain != 1.0F) {
    apply_gain(left_out, right_out, output_gain);
  }

  get_peaks(left_in, right_in, left_out, right_out);
}

void Reverb::process([[maybe_unused]] std::span<float>& left_in,
                     [[maybe_unused]] std::span<float>& right_in,
                     [[maybe_unused]] std::span<float>& left_out,
                     [[maybe_unused]] std::span<float>& right_out,
                     [[maybe_unused]] std::span<float>& probe_left,
                     [[maybe_unused]] std::span<float>& probe_right) {}

auto Reverb::get_latency_seconds() -> float {
  return 0.0F;
}
