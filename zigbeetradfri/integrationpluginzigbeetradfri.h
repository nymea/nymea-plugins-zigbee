/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright 2013 - 2020, nymea GmbH
* Contact: contact@nymea.io
*
* This file is part of nymea.
* This project including source code and documentation is protected by
* copyright law, and remains the property of nymea GmbH. All rights, including
* reproduction, publication, editing and translation, are reserved. The use of
* this project is subject to the terms of a license agreement to be concluded
* with nymea GmbH in accordance with the terms of use of nymea GmbH, available
* under https://nymea.io/license
*
* GNU Lesser General Public License Usage
* Alternatively, this project may be redistributed and/or modified under the
* terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 3. This project is distributed in the hope that
* it will be useful, but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this project. If not, see <https://www.gnu.org/licenses/>.
*
* For any further details and any questions please contact us under
* contact@nymea.io or see our FAQ/Licensing Information on
* https://nymea.io/license/faq
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef INTEGRATIONPLUGINZIGBEETRADFRI_H
#define INTEGRATIONPLUGINZIGBEETRADFRI_H

#include "extern-plugininfo.h"

#include <integrations/integrationplugin.h>
#include <hardware/zigbee/zigbeehandler.h>
#include <zcl/general/zigbeeclusterlevelcontrol.h>
#include <plugintimer.h>

#include <QTimer>

class IntegrationPluginZigbeeTradfri: public IntegrationPlugin, public ZigbeeHandler
{
    Q_OBJECT

    Q_PLUGIN_METADATA(IID "io.nymea.IntegrationPlugin" FILE "integrationpluginzigbeetradfri.json")
    Q_INTERFACES(IntegrationPlugin)

public:
    explicit IntegrationPluginZigbeeTradfri();

    QString name() const override;
    bool handleNode(ZigbeeNode *node, const QUuid &networkUuid) override;
    void handleRemoveNode(ZigbeeNode *node, const QUuid &networkUuid) override;

    void init() override;
    void setupThing(ThingSetupInfo *info) override;
    void executeAction(ThingActionInfo *info) override;
    void thingRemoved(Thing *thing) override;

private slots:
    void soundRemoteMove(Thing *thing, ZigbeeClusterLevelControl::MoveMode mode);

private:
    PluginTimer *m_presenceTimer = nullptr;
    QHash<Thing*, ZigbeeNode*> m_thingNodes;
    quint8 m_lastReceivedTransactionSequenceNumber = 0;

    QHash<ThingClassId, ParamTypeId> m_ieeeAddressParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_networkUuidParamTypeIds;
    QHash<ThingClassId, ParamTypeId> m_endpointIdParamTypeIds;

    QHash<ThingClassId, StateTypeId> m_connectedStateTypeIds;
    QHash<ThingClassId, StateTypeId> m_signalStrengthStateTypeIds;
    QHash<ThingClassId, StateTypeId> m_versionStateTypeIds;

    QHash<Thing*, QTimer*> m_soundRemoteMoveTimers;

    ZigbeeNodeEndpoint *findEndpoint(Thing *thing);
    void createThing(const ThingClassId &thingClassId, const QUuid &networkUuid, ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint);

    void initOnOffSwitch(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint);
    void initShortcutButton(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint);
    void initRemote(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint);
    void initMotionSensor(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint);

    bool isDuplicate(quint8 transactionSequenceNumber);
};

#endif // INTEGRATIONPLUGINZIGBEETRADFRI_H
