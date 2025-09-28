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
  Q_OBJECT

 public:
  explicit Modules(QObject* parent = nullptr);

  enum Roles { Id = Qt::UserRole, Serial, Name, Description, Filename };

  [[nodiscard]] int rowCount(const QModelIndex& /* parent */) const override;

  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

  void reset();

  void begin_reset();

  void end_reset();

  auto get_list() -> QList<ModuleInfo>;

  void append(const ModuleInfo& info);

  void remove_by_id(const uint& id);

  template <typename T>
  void update_field(const int& row, const Roles& role, const T& value) {
    auto model_index = this->index(row);

    auto it = std::next(list.begin(), row);

    switch (role) {
      case Roles::Id: {
        if constexpr (std::is_same_v<T, uint>) {
          it->id = value;
        }

        break;
      }
      case Roles::Serial: {
        if constexpr (std::is_same_v<T, uint64_t>) {
          it->serial = value;
        }

        break;
      }
      case Roles::Name: {
        if constexpr (std::is_same_v<T, QString>) {
          it->name = value;
        }

        break;
      }
      case Roles::Description: {
        if constexpr (std::is_same_v<T, QString>) {
          it->description = value;
        }

        break;
      }
      case Roles::Filename: {
        if constexpr (std::is_same_v<T, QString>) {
          it->filename = value;
        }

        break;
      }
      default:
        break;
    }

    Q_EMIT dataChanged(model_index, model_index, {role});
  }

 private:
  QList<ModuleInfo> list;
};

}  // namespace pw::models
