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

#include "integrationpluginzigbeeschneiderelectric.h"
#include "plugininfo.h"

#include <hardware/zigbee/zigbeehardwareresource.h>
#include <plugintimer.h>

#include <zigbeenodeendpoint.h>
#include <zcl/general/zigbeeclusteronoff.h>
#include <zcl/general/zigbeeclusterbinaryinput.h>

#include <QDebug>

IntegrationPluginZigbeeSchneiderElectric::IntegrationPluginZigbeeSchneiderElectric(): ZigbeeIntegrationPlugin(ZigbeeHardwareResource::HandlerTypeVendor, dcZigbeeSchneiderElectric())
{
}

QString IntegrationPluginZigbeeSchneiderElectric::name() const
{
    return "SchneiderElectric";
}

bool IntegrationPluginZigbeeSchneiderElectric::handleNode(ZigbeeNode *node, const QUuid &/*networkUuid*/)
{
    qCDebug(dcZigbeeSchneiderElectric()) << "Handle node:" << node->nodeDescriptor().manufacturerCode << "Manufacturer:" << node->manufacturerName() << "Model:" << node->modelName();

    if (node->manufacturerName() != "Schneider Electric") {
        qCDebug(dcZigbeeSchneiderElectric) << "Not a Schneider Electric device. Ignoring...";
        return false;
    }

    if (node->modelName() == "FLS/AIRLINK/4") {
        qCDebug(dcZigbeeSchneiderElectric()) << "Handling" << node->modelName();

        ZigbeeNodeEndpoint *endpoint21 = node->getEndpoint(21);
        ZigbeeNodeEndpoint *endpoint22 = node->getEndpoint(22);

        if (!endpoint21 || !endpoint22) {
            qCWarning(dcZigbeeSchneiderElectric()) << "Unable to get endpoints from device.";
            return false;
        }

        bindCluster(endpoint21, ZigbeeClusterLibrary::ClusterIdPowerConfiguration);
        bindCluster(endpoint21, ZigbeeClusterLibrary::ClusterIdOnOff);
        bindCluster(endpoint22, ZigbeeClusterLibrary::ClusterIdOnOff);

        bindCluster(endpoint21, ZigbeeClusterLibrary::ClusterIdLevelControl);
        bindCluster(endpoint22, ZigbeeClusterLibrary::ClusterIdLevelControl);

        createThing(flsAirlink4ThingClassId, node);
        return true;
    }

    return false;
}

void IntegrationPluginZigbeeSchneiderElectric::setupThing(ThingSetupInfo *info)
{
    qCDebug(dcZigbeeSchneiderElectric()) << "Setting up thing" << info->thing()->name();
    Thing *thing = info->thing();

    if (!manageNode(thing)) {
        qCWarning(dcZigbeeSchneiderElectric()) << "Failed to claim node during setup.";
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginZigbeeSchneiderElectric::createConnections(Thing *thing)
{
    ZigbeeNode *node = nodeForThing(thing);
    if (!node) {
        qCWarning(dcZigbeeSchneiderElectric()) << "Node for thing" << thing << "not found.";
        return;
    }


    if (thing->thingClassId() == flsAirlink4ThingClassId) {

        ZigbeeNodeEndpoint *endpoint21 = node->getEndpoint(21);
        ZigbeeNodeEndpoint *endpoint22 = node->getEndpoint(22);

        if (!endpoint21 || !endpoint22) {
            qCWarning(dcZigbeeSchneiderElectric()) << "Unable to get endpoints from device.";
            return;
        }

        connectToPowerConfigurationInputCluster(thing, endpoint21);
        connectToOnOffOutputCluster(thing, endpoint21, "Top on", "Top on");
        connectToLevelControlOutputCluster(thing, endpoint21, "Top up", "Top down");
        connectToOnOffOutputCluster(thing, endpoint22, "Bottom on", "Bottom off");
        connectToLevelControlOutputCluster(thing, endpoint22, "Bottom up", "Bottom down");

        return;
    }
}
