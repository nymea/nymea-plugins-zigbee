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

#include "integrationpluginzigbeeeurotronic.h"
#include "plugininfo.h"
#include "hardware/zigbee/zigbeehardwareresource.h"

#include "zcl/hvac/zigbeeclusterthermostat.h"

#include <QDebug>


IntegrationPluginZigbeeEurotronic::IntegrationPluginZigbeeEurotronic(): ZigbeeIntegrationPlugin(ZigbeeHardwareResource::HandlerTypeVendor, dcZigbeeEurotronic())
{
}

QString IntegrationPluginZigbeeEurotronic::name() const
{
    return "Eurotronic";
}

bool IntegrationPluginZigbeeEurotronic::handleNode(ZigbeeNode *node, const QUuid &networkUuid)
{
    Q_UNUSED(networkUuid)

    if (node->nodeDescriptor().manufacturerCode == 0x1037 && node->modelName() == "SPZB0001") {

        ZigbeeNodeEndpoint *endpoint = node->getEndpoint(0x01);

        bindCluster(endpoint, ZigbeeClusterLibrary::ClusterIdPowerConfiguration);
        configurePowerConfigurationInputClusterAttributeReporting(endpoint);

        bindCluster(endpoint, ZigbeeClusterLibrary::ClusterIdThermostat);
        configureThermostatClusterAttributeReporting(endpoint);

        createThing(spiritThingClassId, node);
        return true;
    }

    return false;
}


void IntegrationPluginZigbeeEurotronic::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();

    if (!manageNode(thing)) {
        qCWarning(dcZigbeeEurotronic()) << "Failed to claim node during setup.";
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginZigbeeEurotronic::createConnections(Thing *thing)
{
    ZigbeeNode *node = nodeForThing(thing);
    if (!node) {
        qCWarning(dcZigbeeEurotronic()) << "Node for thing" << thing << "not found.";
        return;
    }

    ZigbeeNodeEndpoint *endpoint = node->getEndpoint(0x01);
    thing->setStateValue("currentVersion", endpoint->deviceVersion());

    connectToPowerConfigurationInputCluster(thing, endpoint);
    connectToThermostatCluster(thing, endpoint);

    ZigbeeClusterThermostat *thermostatCluster = endpoint->inputCluster<ZigbeeClusterThermostat>(ZigbeeClusterLibrary::ClusterIdThermostat);
    if (!thermostatCluster) {
        qCWarning(dcZigbeeEurotronic()) << "Failed to read thermostat cluster";
        return;
    }

    connect(thermostatCluster, &ZigbeeClusterThermostat::attributeChanged, thing, [thing](const ZigbeeClusterAttribute &attribute){
        qCDebug(dcZigbeeEurotronic()) << "Thermostat attribute changed" << thing->name() << attribute.id() << attribute.dataType();
        if (attribute.id() == 0x4008) {
            ModeFlags flags = static_cast<ModeFlags>(attribute.dataType().toUInt32());
            qCDebug(dcZigbeeEurotronic()) << "flags:" << flags;
            // This thing seems broken... We need to write ModeFlagOff to turn it off and ModeFlagAuto to set it to auto mode
            // However, when it reports the flag, it will report ModeAuto when it's off, and nothing at all when it's in auto mode
            thing->setStateValue(spiritWindowOpenStateTypeId, flags.testFlag(ModeFlagAuto));
            thing->setStateValue(spiritBoostStateTypeId, flags.testFlag(ModeFlagOn));
            thing->setStateValue(spiritChildLockStateTypeId, flags.testFlag(ModeFlagChildProtection));
            thing->setStateValue(spiritMirrorDisplayStateTypeId, flags.testFlag(ModeFlagMirrorDisplay));
        }
    });
    thermostatCluster->readAttributes({0x4008}, 0x1037);
}

void IntegrationPluginZigbeeEurotronic::executeAction(ThingActionInfo *info)
{
    m_actionQueue.enqueue(info);
    connect(info, &ThingActionInfo::finished, this, [=](){
        m_actionQueue.removeAll(info);
        if (m_pendingAction == info) {
            m_pendingAction = nullptr;
            sendNextAction();
        }
    });
    sendNextAction();
}

void IntegrationPluginZigbeeEurotronic::sendNextAction()
{
    if (m_actionQueue.isEmpty()) {
        return;
    }

    if (m_pendingAction) {
        qCDebug(dcZigbeeEurotronic()) << "An action is already pending... enqueueing action.";
        return;
    }

    ThingActionInfo *info = m_actionQueue.dequeue();

    if (!hardwareManager()->zigbeeResource()->available()) {
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    ZigbeeNode *node = nodeForThing(info->thing());
    if (!node) {
        qCWarning(dcZigbeeEurotronic()) << "Node for thing" << info->thing() << "not found.";
        info->finish(Thing::ThingErrorHardwareNotAvailable, QT_TR_NOOP("ZigBee node not found in network."));
        return;
    }
    ZigbeeNodeEndpoint *endpoint = node->getEndpoint(0x01);

    ZigbeeClusterThermostat *thermostatCluster = endpoint->inputCluster<ZigbeeClusterThermostat>(ZigbeeClusterLibrary::ClusterIdThermostat);
    if (!thermostatCluster) {
        qCWarning(dcZigbeeEurotronic()) << "Thermostat cluster not found on thing" << info->thing()->name();
        info->finish(Thing::ThingErrorHardwareFailure);
        return;
    }

    if (info->action().actionTypeId() == spiritTargetTemperatureActionTypeId) {
        qint16 targetTemp = qRound(info->action().paramValue(spiritTargetTemperatureActionTargetTemperatureParamTypeId).toDouble() * 10) * 10;
        qCDebug(dcZigbeeEurotronic()) << "setting target temp" << targetTemp;

        ZigbeeClusterReply *reply = thermostatCluster->setOccupiedHeatingSetpoint(targetTemp);
        connect(reply, &ZigbeeClusterReply::finished, info, [info, reply](){
            if (reply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeEurotronic()) << "Error setting target temperture:" << reply->error();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }
            info->thing()->setStateValue(spiritTargetTemperatureStateTypeId, info->action().paramValue(spiritTargetTemperatureActionTargetTemperatureParamTypeId));
            info->finish(Thing::ThingErrorNoError);
        });
        return;
    }
    if (info->action().actionTypeId() == spiritMirrorDisplayActionTypeId
            || info->action().actionTypeId() == spiritChildLockActionTypeId
            || info->action().actionTypeId() == spiritBoostActionTypeId
            || info->action().actionTypeId() == spiritWindowOpenActionTypeId) {

        ActionTypeId actionTypeId = info->action().actionTypeId();
        bool actionParam = info->action().paramValue(actionTypeId).toBool();
        ModeFlags flags = ModeFlagNone;
        flags |= info->thing()->stateValue(spiritMirrorDisplayStateTypeId).toBool() ? ModeFlagMirrorDisplay : ModeFlagNone;
        flags |= info->thing()->stateValue(spiritChildLockStateTypeId).toBool() ? ModeFlagChildProtection : ModeFlagNone;
        flags |= info->thing()->stateValue(spiritBoostStateTypeId).toBool() ? ModeFlagOn : ModeFlagNone;
        flags |= info->thing()->stateValue(spiritWindowOpenStateTypeId).toBool() ? ModeFlagOff : ModeFlagNone;
        flags |= !info->thing()->stateValue(spiritBoostStateTypeId).toBool() && ! info->thing()->stateValue(spiritWindowOpenActionTypeId).toBool() ? ModeFlagAuto : ModeFlagNone;

        if (actionTypeId == spiritMirrorDisplayActionTypeId) {
            if (actionParam) {
                flags |= ModeFlagMirrorDisplay;
            } else {
                flags &= ~ModeFlagMirrorDisplay;
            }
        }

        if (actionTypeId == spiritChildLockActionTypeId) {
            if (actionParam) {
                flags |= ModeFlagChildProtection;
            } else {
                flags &= ~ModeFlagChildProtection;
            }
        }

        if (actionTypeId == spiritBoostActionTypeId) {
            if (actionParam) {
                flags |= ModeFlagOn;
                flags &= ~ModeFlagAuto;
                flags &= ~ModeFlagOff;
            } else {
                flags &= ~ModeFlagOn;
            }
        }

        if (actionTypeId == spiritWindowOpenActionTypeId) {
            if (actionParam) {
                flags |= ModeFlagOff;
                flags &= ~ModeFlagAuto;
                flags &= ~ModeFlagOn;
            } else {
                flags &= ~ModeFlagOff;
            }
        }
        if (!flags.testFlag(ModeFlagOn) && !flags.testFlag(ModeFlagOff)) {
            flags |= ModeFlagAuto;
        }

        ZigbeeDataType dataType(flags, Zigbee::Uint24);
        QList<ZigbeeClusterLibrary::WriteAttributeRecord> attributes;
        ZigbeeClusterLibrary::WriteAttributeRecord eurotronicAttribute;
        eurotronicAttribute.attributeId = 0x4008;
        eurotronicAttribute.dataType = dataType.dataType();
        eurotronicAttribute.data = dataType.data();
        attributes.append(eurotronicAttribute);
        qCDebug(dcZigbeeEurotronic()) << "Writing eurotronic mode flags:" << flags << (qint32)flags << (1 << 7);
        ZigbeeClusterReply *reply = thermostatCluster->writeAttributes(attributes, 0x1037);

        connect(reply, &ZigbeeClusterReply::finished, info, [info, reply, flags](){
            qCDebug(dcZigbeeEurotronic()) << "Action finished";
            if (reply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeEurotronic()) << "Error setting target temperture:" << reply->error();
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }
            info->thing()->setStateValue(spiritMirrorDisplayStateTypeId, flags.testFlag(ModeFlagMirrorDisplay));
            info->thing()->setStateValue(spiritChildLockStateTypeId, flags.testFlag(ModeFlagChildProtection));
            info->thing()->setStateValue(spiritBoostStateTypeId, flags.testFlag(ModeFlagOn));
            info->thing()->setStateValue(spiritWindowOpenStateTypeId, flags.testFlag(ModeFlagOff));
            info->finish(Thing::ThingErrorNoError);
        });

        return;
    }

    if (info->action().actionTypeId() == spiritPerformUpdateActionTypeId) {
        enableFirmwareUpdate(info->thing());
        executeImageNotifyOtaOutputCluster(info, endpoint);
        return;
    }

    info->finish(Thing::ThingErrorUnsupportedFeature);
}
