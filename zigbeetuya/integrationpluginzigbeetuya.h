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

#ifndef INTEGRATIONPLUGINZIGBEETUYA_H
#define INTEGRATIONPLUGINZIGBEETUYA_H

#include "../common/zigbeeintegrationplugin.h"
#include "extern-plugininfo.h"
#include "dpvalue.h"

#include <plugintimer.h>

#include <QTimer>


class IntegrationPluginZigbeeTuya: public ZigbeeIntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginzigbeetuya.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginZigbeeTuya();

    QString name() const override;
    bool handleNode(ZigbeeNode *node, const QUuid &networkUuid) override;

    void setupThing(ThingSetupInfo *info) override;
    void executeAction(ThingActionInfo *info) override;
    void thingRemoved(Thing *thing) override;

protected:
    void createConnections(Thing *thing) override;

private slots:
    void pollEnergyMeters();

private:
    bool match(ZigbeeNode *node, const QString &modelName, const QStringList &manufacturerNames);

    void writeDpDelayed(ZigbeeCluster *cluster, const DpValue &dp);

private:
    struct DelayedDpWrite {
        DpValue dp;
        ZigbeeCluster *cluster;
    };

    PluginTimer *m_energyPollTimer = nullptr;
    quint16 m_seq = 0;
    QList<DelayedDpWrite> m_delayedDpWrites;
};

#endif // INTEGRATIONPLUGINZIGBEETUYA_H
