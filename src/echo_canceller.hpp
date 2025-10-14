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

#pragma once

#include <qobject.h>
#include <speex/speex_echo.h>
#include <climits>
#include <span>
#include <string>
#include <vector>
#include "easyeffects_db_echo_canceller.h"
#include "pipeline_type.hpp"
#include "plugin_base.hpp"
#include "pw_manager.hpp"

#include <speex/speex_preprocess.h>
#include <speex/speexdsp_config_types.h>
#include <sys/types.h>

class EchoCanceller : public PluginBase {
 public:
  EchoCanceller(const std::string& tag, pw::Manager* pipe_manager, PipelineType pipe_type, QString instance_id);
  EchoCanceller(const EchoCanceller&) = delete;
  auto operator=(const EchoCanceller&) -> EchoCanceller& = delete;
  EchoCanceller(const EchoCanceller&&) = delete;
  auto operator=(const EchoCanceller&&) -> EchoCanceller& = delete;
  ~EchoCanceller() override;

  void reset() override;

  void setup() override;

  void process(std::span<float>& left_in,
               std::span<float>& right_in,
               std::span<float>& left_out,
               std::span<float>& right_out) override;

  void process(std::span<float>& left_in,
               std::span<float>& right_in,
               std::span<float>& left_out,
               std::span<float>& right_out,
               std::span<float>& probe_left,
               std::span<float>& probe_right) override;

  auto get_latency_seconds() -> float override;

 private:
  db::EchoCanceller* settings = nullptr;

  bool notify_latency = false;
  bool ready = false;

  uint latency_n_frames = 0U;

  const float inv_short_max = 1.0F / (SHRT_MAX + 1.0F);

  std::vector<spx_int16_t> data;
  std::vector<spx_int16_t> probe;
  std::vector<spx_int16_t> filtered;
  std::vector<spx_int16_t> channel;

  SpeexEchoState* echo_state = nullptr;

  SpeexPreprocessState* state[2] = {nullptr, nullptr};

  void free_speex();

  void init_speex();
};
