/*
 * Copyright (C) 2016-2017 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QJsonObject>

typedef struct
{
    bool videoEnabled;
    bool trackingEnabled;
    bool ephysEnabled;
    bool ioEnabled;

    QString toHumanString();
    QString toString();
    QJsonObject toJson();
    void fromJson(const QJsonObject& json);

    bool isAnyEnabled();
    void enableAll();
} ExperimentFeatures;

time_t getMsecEpoch();

#endif // UTILS_H
