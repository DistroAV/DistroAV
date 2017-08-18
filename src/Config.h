/*
obs-ndi (NDI I/O in OBS Studio)
Copyright (C) 2016 Stéphane Lepin <stephane.lepin@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see <https://www.gnu.org/licenses/>
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <obs-module.h>

class Config {
  public:
    Config();
    static void OBSSaveCallback(obs_data_t* save_data,
        bool saving, void* private_data);
    static Config* Current();
    void Load();
    void Save();

    bool OutputEnabled;
    QString OutputName;

  private:
    static Config* _instance;
};

#endif // CONFIG_H
