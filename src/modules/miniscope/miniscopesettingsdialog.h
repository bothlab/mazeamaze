/**
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QDialog>
#include <QList>

namespace MScope {
class Miniscope;
}
class MSControlWidget;
class QVBoxLayout;
namespace Ui {
class MiniscopeSettingsDialog;
}

class MiniscopeSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MiniscopeSettingsDialog(MScope::Miniscope *mscope, QWidget *parent = nullptr);
    ~MiniscopeSettingsDialog();

    void updateValues();
    void setRunning(bool running);

    void setDeviceType(const QString &devType);

private slots:
    void on_scopeTypeComboBox_currentIndexChanged(const QString &arg1);
    void on_sbCamId_valueChanged(int arg1);
    void on_cbExtRecTrigger_toggled(bool checked);
    void on_sbDisplayMax_valueChanged(int arg1);
    void on_sbDisplayMin_valueChanged(int arg1);
    void on_bgDivCheckBox_toggled(bool checked);
    void on_bgSubstCheckBox_toggled(bool checked);
    void on_accAlphaSpinBox_valueChanged(double arg1);

private:
    Ui::MiniscopeSettingsDialog *ui;
    QString m_recName;
    MScope::Miniscope *m_mscope;

    QList<MSControlWidget*> m_controls;
    QVBoxLayout *m_controlsLayout;
};
