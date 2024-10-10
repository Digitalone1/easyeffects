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

#include <qmap.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include "easyeffects_db.h"                // IWYU pragma: export
#include "easyeffects_db_spectrum.h"       // IWYU pragma: export
#include "easyeffects_db_streaminputs.h"   // IWYU pragma: export
#include "easyeffects_db_streamoutputs.h"  // IWYU pragma: export

namespace db {

class Manager : public QObject {
  Q_OBJECT;

  Q_PROPERTY(db::Main* main MEMBER main NOTIFY mainChanged)
  Q_PROPERTY(db::Spectrum* spectrum MEMBER spectrum NOTIFY spectrumChanged)
  Q_PROPERTY(db::StreamInputs* streamInputs MEMBER streamInputs NOTIFY streamInputsChanged)
  Q_PROPERTY(db::StreamOutputs* streamOutputs MEMBER streamOutputs NOTIFY streamOutputsChanged)

  Q_PROPERTY(QMap<QString, QVariant> soePluginsDB MEMBER soePluginsDB NOTIFY soePluginsDBChanged)
  Q_PROPERTY(QMap<QString, QVariant> siePluginsDB MEMBER siePluginsDB NOTIFY siePluginsDBChanged)

 public:
  Manager();
  Manager(const Manager&) = delete;
  auto operator=(const Manager&) -> Manager& = delete;
  Manager(const Manager&&) = delete;
  auto operator=(const Manager&&) -> Manager& = delete;
  ~Manager() override;

  static Manager& self() {
    static Manager m;
    return m;
  }

  Q_INVOKABLE
  void saveAll() const;

  Q_INVOKABLE
  void resetAll() const;

  db::Main* main;
  db::Spectrum* spectrum;
  db::StreamInputs* streamInputs;
  db::StreamOutputs* streamOutputs;

  QMap<QString, QVariant> soePluginsDB;
  QMap<QString, QVariant> siePluginsDB;

 signals:
  void mainChanged();
  void spectrumChanged();
  void streamInputsChanged();
  void streamOutputsChanged();
  void soePluginsDBChanged();
  void siePluginsDBChanged();

 private:
  void create_plugin_db(const QString& parentGroup, const auto& plugins_list, QMap<QString, QVariant>& plugins_map);
};

}  // namespace db