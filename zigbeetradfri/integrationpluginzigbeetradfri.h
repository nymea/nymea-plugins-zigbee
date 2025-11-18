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

#ifndef INTEGRATIONPLUGINZIGBEETRADFRI_H
#define INTEGRATIONPLUGINZIGBEETRADFRI_H

#include "extern-plugininfo.h"
#include "../common/zigbeeintegrationplugin.h"

#include <integrations/integrationplugin.h>
#include <hardware/zigbee/zigbeehandler.h>
#include <zcl/general/zigbeeclusterlevelcontrol.h>
#include <plugintimer.h>

#include <QTimer>

class IntegrationPluginZigbeeTradfri: public ZigbeeIntegrationPlugin
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginzigbeetradfri.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginZigbeeTradfri();

    QString name() const override;
    bool handleNode(ZigbeeNode *node, const QUuid &networkUuid) override;

    void setupThing(ThingSetupInfo *info) override;
    void executeAction(ThingActionInfo *info) override;
    void thingRemoved(Thing *thing) override;

protected:
    void createConnections(Thing *thing) override;
    QList<FirmwareIndexEntry> firmwareIndexFromJson(const QByteArray &data) const override;

private slots:
    void soundRemoteMove(Thing *thing, ZigbeeClusterLevelControl::MoveMode mode);

private:
    PluginTimer *m_presenceTimer = nullptr;
    quint8 m_lastReceivedTransactionSequenceNumber = 0;

    QHash<Thing*, QTimer*> m_soundRemoteMoveTimers;

    bool isDuplicate(quint8 transactionSequenceNumber);

    void configureAirPurifierAttributeReporting(ZigbeeNodeEndpoint *endpoint);
};

#endif // INTEGRATIONPLUGINZIGBEETRADFRI_H
