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

#include <qabstractitemmodel.h>
#include <qhash.h>
#include <qhashfunctions.h>
#include <qlist.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qstringview.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvariant.h>
#include <cstdint>
#include <iterator>
#include "pw_objects.hpp"

namespace pw::models {

class Modules : public QAbstractListModel {
  Q_OBJECT;

 public:
  explicit Modules(QObject* parent = nullptr);

  enum Roles { Id = Qt::UserRole, Serial, Name, Description, Filename };

  [[nodiscard]] int rowCount(const QModelIndex& /*parent*/) const override;

  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

  void reset();

  void begin_reset();

  void end_reset();

  auto get_list() -> QList<ModuleInfo>;

  void append(const ModuleInfo& info);

  void remove_by_id(const uint& id);

  template <typename T>
  void update_field(const QModelIndex& index, const Roles& role, const T& value) {
    auto it = std::next(list.begin(), index.row());

    switch (role) {
      case Roles::Id: {
        if constexpr (std::is_same_v<T, uint>) {
          it->id = value;
          emit dataChanged(index, index, {Roles::Id});
        }

        break;
      }
      case Roles::Serial: {
        if constexpr (std::is_same_v<T, uint64_t>) {
          it->serial = value;
          emit dataChanged(index, index, {Roles::Serial});
        }

        break;
      }
      case Roles::Name: {
        if constexpr (std::is_same_v<T, QString>) {
          it->name = value;
          emit dataChanged(index, index, {Roles::Name});
        }

        break;
      }
      case Roles::Description: {
        if constexpr (std::is_same_v<T, QString>) {
          it->description = value;
          emit dataChanged(index, index, {Roles::Description});
        }

        break;
      }
      case Roles::Filename: {
        if constexpr (std::is_same_v<T, QString>) {
          it->filename = value;
          emit dataChanged(index, index, {Roles::Filename});
        }

        break;
      }
      default:
        break;
    }
  }

 private:
  QList<ModuleInfo> list;
};

}  // namespace pw::models