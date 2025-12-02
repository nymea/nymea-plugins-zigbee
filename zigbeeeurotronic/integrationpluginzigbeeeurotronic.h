// SPDX-License-Identifier: GPL-3.0-or-later

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright (C) 2013 - 2024, nymea GmbH
* Copyright (C) 2024 - 2025, chargebyte austria GmbH
*
* This file is part of nymea-plugins-zigbee.
*
* nymea-plugins-zigbee is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* nymea-plugins-zigbee is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with nymea-plugins-zigbee. If not, see <https://www.gnu.org/licenses/>.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INTEGRATIONPLUGINZIGBEEEUROTRONIC_H
#define INTEGRATIONPLUGINZIGBEEEUROTRONIC_H

#include "../common/zigbeeintegrationplugin.h"

#include "extern-plugininfo.h"

#include <QQueue>

class IntegrationPluginZigbeeEurotronic: public ZigbeeIntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginzigbeeeurotronic.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    enum ModeFlag {
        ModeFlagNone = 0x0001,
        ModeFlagMirrorDisplay = 0x0002,
        ModeFlagOn = 0x0004,
        ModeFlagAuto = 0x0010,
        ModeFlagOff = 0x0020,
        ModeFlagChildProtection = 0x0080
    };
    Q_DECLARE_FLAGS(ModeFlags, ModeFlag)
    Q_FLAG(ModeFlags)
    explicit IntegrationPluginZigbeeEurotronic();

    QString name() const override;
    bool handleNode(ZigbeeNode *node, const QUuid &networkUuid) override;

    void setupThing(ThingSetupInfo *info) override;
    void executeAction(ThingActionInfo *info) override;

private:
    void createConnections(Thing *thing) override;

    void sendNextAction();

private:
    QQueue<ThingActionInfo*> m_actionQueue;
    ThingActionInfo *m_pendingAction = nullptr;
};

#endif // INTEGRATIONPLUGINZIGBEEEUROTRONIC_H
