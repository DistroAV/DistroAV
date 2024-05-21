/*
obs-ndi
Copyright (C) 2016-2024 OBS-NDI Project <obsndi@obsndiproject.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/
#pragma once

#include <QDialog>

#include "ui_obsndi-update.h"

class ObsNdiUpdate : public QDialog {
	Q_OBJECT
public:
	explicit ObsNdiUpdate(const QJsonDocument &jsonResponse,
			      QWidget *parent = nullptr);
	~ObsNdiUpdate();

private:
	Ui::ObsNdiUpdate *ui;
};

void updateCheckStop();
void updateCheckStart(bool force = false);
