/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "runcmdmodule.h"

#include <QMessageBox>

QString RunCmdModuleInfo::id() const
{
    return QStringLiteral("runcmd");
}

QString RunCmdModuleInfo::name() const
{
    return QStringLiteral("Run Command");
}

QString RunCmdModuleInfo::description() const
{
    return QStringLiteral("Run external commands at various stages of the experiment.");
}

QPixmap RunCmdModuleInfo::pixmap() const
{
    return QPixmap(":/module/runcmd");
}

AbstractModule *RunCmdModuleInfo::createModule(QObject *parent)
{
    return new RunCmdModule(parent);
}

RunCmdModule::RunCmdModule(QObject *parent)
    : AbstractModule(parent)
{
}

RunCmdModule::~RunCmdModule()
{

}

bool RunCmdModule::prepare(const QString &, const TestSubject &)
{
    return true;
}

void RunCmdModule::stop()
{

}
