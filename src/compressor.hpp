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

#pragma once

#include <pipewire/proxy.h>
#include <qtmetamacros.h>
#include <sys/types.h>
#include <QString>
#include <span>
#include <vector>
#include "easyeffects_db_compressor.h"
#include "pipeline_type.hpp"
#include "plugin_base.hpp"
#include "pw_manager.hpp"

class Compressor : public PluginBase {
  Q_OBJECT;

 public:
  Compressor(const std::string& tag, pw::Manager* pipe_manager, PipelineType pipe_type, QString instance_id);
  Compressor(const Compressor&) = delete;
  auto operator=(const Compressor&) -> Compressor& = delete;
  Compressor(const Compressor&&) = delete;
  auto operator=(const Compressor&&) -> Compressor& = delete;
  ~Compressor() override;

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

  void update_probe_links() override;

  Q_INVOKABLE [[nodiscard]] float getReductionLevelLeft() const;
  Q_INVOKABLE [[nodiscard]] float getReductionLevelRight() const;
  Q_INVOKABLE [[nodiscard]] float getSideChainLevelLeft() const;
  Q_INVOKABLE [[nodiscard]] float getSideChainLevelRight() const;
  Q_INVOKABLE [[nodiscard]] float getCurveLevelLeft() const;
  Q_INVOKABLE [[nodiscard]] float getCurveLevelRight() const;
  Q_INVOKABLE [[nodiscard]] float getEnvelopeLevelLeft() const;
  Q_INVOKABLE [[nodiscard]] float getEnvelopeLevelRight() const;

 private:
  uint latency_n_frames = 0U;

  float reduction_left = 0.0F, reduction_right = 0.0F;
  float sidechain_left = 0.0F, sidechain_right = 0.0F;
  float curve_left = 0.0F, curve_right = 0.0F;
  float envelope_left = 0.0F, envelope_right = 0.0F;

  db::Compressor* settings = nullptr;

  std::vector<pw_proxy*> list_proxies;

  void update_sidechain_links();
};