﻿/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
*
* Copyright 2013 - 2022, nymea GmbH
* Contact: contact@nymea.io

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

#include "integrationpluginzigbeelumi.h"
#include "plugininfo.h"

#include <math.h>

#include <zigbeenodeendpoint.h>
#include <hardware/zigbee/zigbeehardwareresource.h>

#include <zcl/general/zigbeeclusterbasic.h>
#include <zcl/general/zigbeeclusteronoff.h>
#include <zcl/general/zigbeeclustermultistateinput.h>
#include <zcl/general/zigbeeclusteranalogoutput.h>
#include <zcl/general/zigbeeclusteranaloginput.h>
#include <zcl/measurement/zigbeeclusteroccupancysensing.h>
#include <zcl/measurement/zigbeeclusterilluminancemeasurement.h>
#include <zcl/measurement/zigbeeclustertemperaturemeasurement.h>
#include <zcl/measurement/zigbeeclusterrelativehumiditymeasurement.h>
#include <zcl/measurement/zigbeeclusterpressuremeasurement.h>
#include <zcl/security/zigbeeclusteriaszone.h>
#include <zcl/general/zigbeeclusterpowerconfiguration.h>

#include <QDebug>
#include <QDataStream>

#define LUMI_CLUSTER_ID 0xfcc0
#define MANUFACTURER_CODE_XIAOMI 0x115f

IntegrationPluginZigbeeLumi::IntegrationPluginZigbeeLumi():
    ZigbeeIntegrationPlugin(ZigbeeHardwareResource::HandlerTypeVendor, dcZigbeeLumi())
{

    // Known model identifier
    m_knownLumiDevices.insert("lumi.sensor_magnet", lumiMagnetSensorThingClassId);
    m_knownLumiDevices.insert("lumi.sensor_switch", lumiButtonSensorThingClassId);
    // Check sensor_motion separate since the have the same name but different features
    //m_knownLumiDevices.insert("lumi.sensor_motion", lumiMotionSensorThingClassId);
    m_knownLumiDevices.insert("lumi.sensor_wleak", lumiWaterSensorThingClassId);
    m_knownLumiDevices.insert("lumi.weather", lumiWeatherSensorThingClassId);
    m_knownLumiDevices.insert("lumi.vibration", lumiVibrationSensorThingClassId);
    m_knownLumiDevices.insert("lumi.plug", lumiPowerSocketThingClassId);
    m_knownLumiDevices.insert("lumi.relay", lumiRelayThingClassId);
    m_knownLumiDevices.insert("lumi.remote", lumiRemoteThingClassId);
}

QString IntegrationPluginZigbeeLumi::name() const
{
    return "Lumi";
}

bool IntegrationPluginZigbeeLumi::handleNode(ZigbeeNode *node, const QUuid &/*networkUuid*/)
{
    // Note: Lumi / Xiaomi / Aquara devices are not in the specs, some older models do not
    // send the node descriptor or use a inconsistent manufacturer code. We use the model identifier
    // for verification since they seem to start always with lumi.
    foreach (ZigbeeNodeEndpoint *endpoint, node->endpoints()) {
        // Get the model identifier if present from the first endpoint. Also this is out of spec
        if (!endpoint->hasInputCluster(ZigbeeClusterLibrary::ClusterIdBasic)) {
            qCDebug(dcZigbeeLumi()) << "This lumi endpoint does not have the basic input cluster, so we skipp it" << endpoint;
            continue;
        }

        ThingClassId thingClassId;
        if (endpoint->modelIdentifier().startsWith("lumi.sensor_motion")) {
            // Check if this is a xiaomi or aquara motion sensor
            if (endpoint->hasInputCluster(ZigbeeClusterLibrary::ClusterIdIlluminanceMeasurement)) {
                thingClassId = lumiMotionSensorThingClassId;
            } else {
                thingClassId = xiaomiMotionSensorThingClassId;
            }

        } else if (endpoint->modelIdentifier() == "lumi.motion.ac02") {
            thingClassId = lumiMotionSensor2ThingClassId;

        } else if (endpoint->modelIdentifier() == "lumi.remote.b1acn01") {
            thingClassId = lumiLongpressButtonSensorThingClassId;

        } else if (endpoint->modelIdentifier() == "lumi.plug.maeu01" || endpoint->modelIdentifier() == "lumi.plug.mmeu01") {
            thingClassId = lumiPowerSocketThingClassId;

            bindCluster(endpoint, ZigbeeClusterLibrary::ClusterIdElectricalMeasurement);
            configureElectricalMeasurementInputClusterAttributeReporting(endpoint);

            bindCluster(endpoint, ZigbeeClusterLibrary::ClusterIdMetering);
            configureMeteringInputClusterAttributeReporting(endpoint);

        } else if (endpoint->modelIdentifier() == "lumi.sensor_ht" || endpoint->modelIdentifier() == "lumi.sens") {
            thingClassId = lumiHTSensorThingClassId;

        } else if (endpoint->modelIdentifier() == "lumi.airmonitor.acn01") {
            thingClassId = lumiAirMonitorThingClassId;

            bindCluster(endpoint, ZigbeeClusterLibrary::ClusterIdPowerConfiguration);
            configurePowerConfigurationInputClusterAttributeReporting(endpoint);

            bindCluster(endpoint, ZigbeeClusterLibrary::ClusterIdTemperatureMeasurement);
            configureTemperatureMeasurementInputClusterAttributeReporting(endpoint);

            bindCluster(endpoint, ZigbeeClusterLibrary::ClusterIdRelativeHumidityMeasurement);
            configureRelativeHumidityMeasurementInputClusterAttributeReporting(endpoint);

            bindCluster(endpoint, ZigbeeClusterLibrary::ClusterIdAnalogInput);
            configureAnalogInputClusterAttributeReporting(endpoint);


        } else {
            foreach (const QString &knownLumi, m_knownLumiDevices.keys()) {
                if (endpoint->modelIdentifier().startsWith(knownLumi)) {
                    thingClassId = m_knownLumiDevices.value(knownLumi);
                    break;
                }
            }
        }
        if (thingClassId.isNull()) {
            qCWarning(dcZigbeeLumi()) << "Unhandled Lumi device:" << endpoint->modelIdentifier();
            return false;
        }

        createThing(thingClassId, node);
        return true;
    }

    return false;
}

void IntegrationPluginZigbeeLumi::setupThing(ThingSetupInfo *info)
{
    Thing *thing = info->thing();
    if (!manageNode(thing)) {
        qCWarning(dcZigbeeLumi()) << "Failed to claim node during setup.";
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    ZigbeeNode *node = nodeForThing(thing);

    ZigbeeNodeEndpoint *endpoint = node->getEndpoint(0x01);
    if (!endpoint) {
        qCWarning(dcZigbeeLumi()) << "Zigbee endpoint 1 not found on" << thing;
        info->finish(Thing::ThingErrorSetupFailed);
        return;
    }

    info->finish(Thing::ThingErrorNoError);

    // Set the version
    thing->setStateValue("version", endpoint->softwareBuildId());

    // Thing specific setup
    if (thing->thingClassId() == lumiMagnetSensorThingClassId) {
        ZigbeeClusterOnOff *onOffCluster = endpoint->inputCluster<ZigbeeClusterOnOff>(ZigbeeClusterLibrary::ClusterIdOnOff);
        if (onOffCluster) {
            // Only set the state if the cluster actually has the attribute
            if (onOffCluster->hasAttribute(ZigbeeClusterOnOff::AttributeOnOff)) {
                thing->setStateValue(lumiMagnetSensorClosedStateTypeId, !onOffCluster->power());
            }

            connect(onOffCluster, &ZigbeeClusterOnOff::powerChanged, thing, [thing](bool power){
                qCDebug(dcZigbeeLumi()) << thing << "state changed" << (power ? "closed" : "open");
                thing->setStateValue(lumiMagnetSensorClosedStateTypeId, !power);
            });
        } else {
            qCDebug(dcZigbeeLumi()) << "Could not find the OnOff input cluster on" << thing->name() << endpoint;
            // The lumi.sensor_magnet does not reply to endpoint introspection so the OnOff cluster may not exist directly after
            // pairing yet. Once the sensor is actually opened/closed, it will create the cluster and we can connect to it.
            connect(endpoint, &ZigbeeNodeEndpoint::inputClusterAdded, thing, [thing](ZigbeeCluster *cluster){
                if (cluster->clusterId() == ZigbeeClusterLibrary::ClusterIdOnOff) {
                    qCDebug(dcZigbeeLumi()) << "OnOff cluster appeared on" << thing->name();
                    ZigbeeClusterOnOff *onOffCluster = qobject_cast<ZigbeeClusterOnOff*>(cluster);
                    if (onOffCluster->hasAttribute(ZigbeeClusterOnOff::AttributeOnOff)) {
                        thing->setStateValue(lumiMagnetSensorClosedStateTypeId, !onOffCluster->power());
                    }
                    connect(onOffCluster, &ZigbeeClusterOnOff::powerChanged, thing, [thing](bool power){
                        qCDebug(dcZigbeeLumi()) << thing << "state changed" << (power ? "closed" : "open");
                        thing->setStateValue(lumiMagnetSensorClosedStateTypeId, !power);
                    });
                }
            });
        }
    }

    if (thing->thingClassId() == lumiMotionSensorThingClassId) {
        QTimer *presenceTimer = new QTimer(thing);
        connect(presenceTimer, &QTimer::timeout, thing, [thing](){
            thing->setStateValue(lumiMotionSensorIsPresentStateTypeId, false);
        });

        ZigbeeClusterOccupancySensing *occupancyCluster = endpoint->inputCluster<ZigbeeClusterOccupancySensing>(ZigbeeClusterLibrary::ClusterIdOccupancySensing);
        if (occupancyCluster) {
            if (occupancyCluster->hasAttribute(ZigbeeClusterOccupancySensing::AttributeOccupancy)) {
                thing->setStateValue(lumiMotionSensorIsPresentStateTypeId, occupancyCluster->occupied());
                thing->setStateValue(lumiMotionSensorLastSeenTimeStateTypeId, QDateTime::currentMSecsSinceEpoch() / 1000);
            }

            connect(occupancyCluster, &ZigbeeClusterOccupancySensing::occupancyChanged, thing, [thing, presenceTimer](bool occupancy){
                qCDebug(dcZigbeeLumi()) << "occupancy changed" << occupancy;
                // Only change the state if the it changed to true, it will be disabled by the timer
                if (occupancy) {
                    thing->setStateValue(lumiMotionSensorIsPresentStateTypeId, occupancy);
                    int timeout = thing->setting(lumiMotionSensorSettingsTimeoutParamTypeId).toInt();
                    presenceTimer->start(timeout * 1000);
                }

                thing->setStateValue(lumiMotionSensorLastSeenTimeStateTypeId, QDateTime::currentMSecsSinceEpoch() / 1000);
            });

        } else {
            qCWarning(dcZigbeeLumi()) << "Occupancy cluster not found on" << thing->name();
        }

        ZigbeeClusterIlluminanceMeasurement *illuminanceCluster = endpoint->inputCluster<ZigbeeClusterIlluminanceMeasurement>(ZigbeeClusterLibrary::ClusterIdIlluminanceMeasurement);
        if (illuminanceCluster) {
            // Only set the state if the cluster actually has the attribute
            if (illuminanceCluster->hasAttribute(ZigbeeClusterIlluminanceMeasurement::AttributeMeasuredValue)) {
                thing->setStateValue(lumiMotionSensorLightIntensityStateTypeId, illuminanceCluster->illuminance());
            }

            connect(illuminanceCluster, &ZigbeeClusterIlluminanceMeasurement::illuminanceChanged, thing, [thing](quint16 illuminance){
                qCDebug(dcZigbeeLumi()) << thing << "light intensity changed" << illuminance << "lux";
                thing->setStateValue(lumiMotionSensorLightIntensityStateTypeId, illuminance);
            });
        } else {
            qCWarning(dcZigbeeLumi()) << "Illuminance cluster not found on" << thing->name();
        }
    }

    if (thing->thingClassId() == lumiMotionSensor2ThingClassId) {
        QTimer *presenceTimer = new QTimer(thing);
        connect(presenceTimer, &QTimer::timeout, thing, [thing](){
            thing->setStateValue(lumiMotionSensor2IsPresentStateTypeId, false);
        });

        // This one only sends an illuminage changed when a motion is detected.
        ZigbeeCluster *lumiCluster = endpoint->getInputCluster(static_cast<ZigbeeClusterLibrary::ClusterId>(LUMI_CLUSTER_ID));
        if (lumiCluster) {
            connect(lumiCluster, &ZigbeeCluster::attributeChanged, thing, [thing, presenceTimer](const ZigbeeClusterAttribute &attribute){
                switch (attribute.id()) {
                case 0x0112: {
                    quint32 lightIntensity = attribute.dataType().toUInt32();
                    qCDebug(dcZigbeeLumi()) << thing << "Light intensity" << lightIntensity;
                    thing->setStateValue(lumiMotionSensor2LightIntensityStateTypeId, lightIntensity);
                    thing->setStateValue(lumiMotionSensor2IsPresentStateTypeId, true);
                    thing->setStateValue(lumiMotionSensor2LastSeenTimeStateTypeId, QDateTime::currentMSecsSinceEpoch() / 1000);
                    int timeout = thing->setting(lumiMotionSensor2SettingsTimeoutParamTypeId).toInt();
                    presenceTimer->start(timeout * 1000);
                    break;
                }
                default:
                    qCDebug(dcZigbeeLumi()) << thing << "Unhandled attribute report:" << attribute;
                }
            });
            connect(thing, &Thing::settingChanged, lumiCluster, [this, lumiCluster] (const ParamTypeId &settingTypeId, const QVariant &value){
                if (settingTypeId == lumiMotionSensor2SettingsSensitivityParamTypeId) {
                    ZigbeeDataType dt(value.toUInt(), Zigbee::Uint8);
                    ZigbeeClusterLibrary::WriteAttributeRecord record;
                    record.attributeId = 0x010c;
                    record.dataType = dt.dataType();
                    record.data = dt.data();
                    writeAttributesDelayed(lumiCluster, {record}, MANUFACTURER_CODE_XIAOMI);
                }
            });
        }
    }

    if (thing->thingClassId() == xiaomiMotionSensorThingClassId) {
        QTimer *presenceTimer = new QTimer(thing);
        connect(presenceTimer, &QTimer::timeout, thing, [thing](){
            thing->setStateValue(xiaomiMotionSensorIsPresentStateTypeId, false);
        });

        ZigbeeClusterOccupancySensing *occupancyCluster = endpoint->inputCluster<ZigbeeClusterOccupancySensing>(ZigbeeClusterLibrary::ClusterIdOccupancySensing);
        if (occupancyCluster) {
            if (occupancyCluster->hasAttribute(ZigbeeClusterOccupancySensing::AttributeOccupancy)) {
                thing->setStateValue(xiaomiMotionSensorIsPresentStateTypeId, occupancyCluster->occupied());
                thing->setStateValue(xiaomiMotionSensorLastSeenTimeStateTypeId, QDateTime::currentMSecsSinceEpoch() / 1000);
            }

            connect(occupancyCluster, &ZigbeeClusterOccupancySensing::occupancyChanged, thing, [thing, presenceTimer](bool occupancy){
                qCDebug(dcZigbeeLumi()) << "occupancy changed" << occupancy;
                // Only change the state if the it changed to true, it will be disabled by the timer
                if (occupancy) {
                    thing->setStateValue(xiaomiMotionSensorIsPresentStateTypeId, occupancy);
                    int timeout = thing->setting(xiaomiMotionSensorSettingsTimeoutParamTypeId).toInt();
                    presenceTimer->start(timeout * 1000);
                }

                thing->setStateValue(xiaomiMotionSensorLastSeenTimeStateTypeId, QDateTime::currentMSecsSinceEpoch() / 1000);
            });

        } else {
            qCWarning(dcZigbeeLumi()) << "Occupancy cluster not found on" << thing->name();
        }
    }

    if (thing->thingClassId() == lumiHTSensorThingClassId) {
        connectToTemperatureMeasurementInputCluster(thing, endpoint);
        connectToRelativeHumidityMeasurementInputCluster(thing, endpoint);
    }

    if (thing->thingClassId() == lumiAirMonitorThingClassId) {
        connectToPowerConfigurationInputCluster(thing, endpoint, 3, 2.85);
        connectToTemperatureMeasurementInputCluster(thing, endpoint);
        connectToRelativeHumidityMeasurementInputCluster(thing, endpoint);
        connectToAnalogInputCluster(thing, endpoint, "voc");
        connectToOtaOutputCluster(thing, endpoint);

        // Device may add clusters delayed in which case the above connects fail
        connect(endpoint, &ZigbeeNodeEndpoint::inputClusterAdded, thing, [this, thing, endpoint](ZigbeeCluster *cluster) {
            switch (cluster->clusterId()) {
            case ZigbeeClusterLibrary::ClusterIdPowerConfiguration:
                connectToPowerConfigurationInputCluster(thing, endpoint, 3, 2.85);
                break;
            case ZigbeeClusterLibrary::ClusterIdTemperatureMeasurement:
                connectToTemperatureMeasurementInputCluster(thing, endpoint);
                break;
            case ZigbeeClusterLibrary::ClusterIdRelativeHumidityMeasurement:
                connectToRelativeHumidityMeasurementInputCluster(thing, endpoint);
                break;
            case ZigbeeClusterLibrary::ClusterIdAnalogInput:
                connectToAnalogInputCluster(thing, endpoint, "voc");
                break;
            case ZigbeeClusterLibrary::ClusterIdOtaUpgrade:
                connectToOtaOutputCluster(thing, endpoint);
                break;
            default:
                qCWarning(dcZigbeeLumi()) << "Unhandled cluster" << cluster->clusterId() << "appeared on" << thing;
            }
        });

        // FIXME: For testing
        ZigbeeClusterPowerConfiguration *powerCluster = endpoint->inputCluster<ZigbeeClusterPowerConfiguration>(ZigbeeClusterLibrary::ClusterIdPowerConfiguration);
        if (powerCluster) {
            connect(powerCluster, &ZigbeeClusterPowerConfiguration::attributeChanged, thing, [=](const ZigbeeClusterAttribute &attribute){
                qCDebug(dcZigbeeLumi()) << "***" << thing->name() << "Power configuration cluster attribute changed:" << attribute;
            });

            if (endpoint->node()->reachable()) {
                powerCluster->readAttributes({
                                                 ZigbeeClusterPowerConfiguration::AttributeBatteryPercentageRemaining,
                                                 ZigbeeClusterPowerConfiguration::AttributeBatteryVoltage,
                                                 ZigbeeClusterPowerConfiguration::AttributeBatteryAlarmState
                                             });
            }
            connect(endpoint->node(), &ZigbeeNode::reachableChanged, powerCluster, [powerCluster](bool reachable){
                if (reachable) {
                    powerCluster->readAttributes({
                                                     ZigbeeClusterPowerConfiguration::AttributeBatteryPercentageRemaining,
                                                     ZigbeeClusterPowerConfiguration::AttributeBatteryVoltage,
                                                     ZigbeeClusterPowerConfiguration::AttributeBatteryAlarmState
                                                 });
                }
            });
        }




        QHash<QString, uint> map = {
            {"mg/m³ - °C", 0x00},
            {"ppb - °C", 0x01},
            {"mg/m³ - °F",0x10},
            {"ppb³ - °F", 0x11}
        };
        ZigbeeCluster *lumiCluster = endpoint->getInputCluster(static_cast<ZigbeeClusterLibrary::ClusterId>(LUMI_CLUSTER_ID));
        if (lumiCluster) {
            connect(lumiCluster, &ZigbeeCluster::attributeChanged, thing, [thing, map](const ZigbeeClusterAttribute &attribute){
                switch (attribute.id()) {
                case 0x0114: {
                    quint8 displayUnit = attribute.dataType().toUInt8();
                    qCDebug(dcZigbeeLumi()) << thing << "Display unit" << displayUnit;
                    thing->setSettingValue(lumiAirMonitorSettingsDisplayUnitsParamTypeId, map.key(displayUnit));
                    break;
                }
                default:
                    qCDebug(dcZigbeeLumi()) << thing << "Unhandled attribute report:" << attribute;
                }
            });
            connect(thing, &Thing::settingChanged, lumiCluster, [lumiCluster, map](const ParamTypeId &settingId, const QVariant &value){
                if (settingId == lumiAirMonitorSettingsDisplayUnitsParamTypeId) {
                    ZigbeeClusterLibrary::WriteAttributeRecord record;
                    record.attributeId = 0x0114;
                    record.dataType = Zigbee::Uint8;
                    record.data = ZigbeeDataType(map.value(value.toString()), Zigbee::Uint8).data();
                    lumiCluster->writeAttributes({record}, MANUFACTURER_CODE_XIAOMI);
                }
            });
            lumiCluster->readAttributes({0x0114}, MANUFACTURER_CODE_XIAOMI);
        }
    }

    if (thing->thingClassId() == lumiWeatherSensorThingClassId) {
        connectToTemperatureMeasurementInputCluster(thing, endpoint);
        connectToRelativeHumidityMeasurementInputCluster(thing, endpoint);

        ZigbeeClusterPressureMeasurement *pressureCluster = endpoint->inputCluster<ZigbeeClusterPressureMeasurement>(ZigbeeClusterLibrary::ClusterIdPressureMeasurement);
        if (pressureCluster) {
            // Only set the state if the cluster actually has the attribute
            if (pressureCluster->hasAttribute(ZigbeeClusterPressureMeasurement::AttributeMeasuredValue)) {
                thing->setStateValue(lumiWeatherSensorPressureStateTypeId, pressureCluster->pressure() * 10);
            }

            connect(pressureCluster, &ZigbeeClusterPressureMeasurement::pressureChanged, thing, [thing](double pressure){
                thing->setStateValue(lumiWeatherSensorPressureStateTypeId, pressure * 10);
            });
        } else {
            qCWarning(dcZigbeeLumi()) << "Could not find the pressure measurement server cluster on" << thing << endpoint;
        }
    }

    if (thing->thingClassId() == lumiWaterSensorThingClassId) {
        connect(endpoint, &ZigbeeNodeEndpoint::clusterAttributeChanged, this, [thing](ZigbeeCluster *cluster, const ZigbeeClusterAttribute &attribute){
            if (cluster->clusterId() == ZigbeeClusterLibrary::ClusterIdIasZone) {
                if (attribute.id() == ZigbeeClusterIasZone::AttributeZoneState) {
                    bool valueOk = false;
                    ZigbeeClusterIasZone::ZoneStatusFlags zoneStatus = static_cast<ZigbeeClusterIasZone::ZoneStatusFlags>(attribute.dataType().toUInt16(&valueOk));
                    if (!valueOk) {
                        qCWarning(dcZigbeeLumi()) << thing << "failed to convert attribute data to uint16 flag. Not updating the states from" << attribute;
                    } else {
                        qCDebug(dcZigbeeLumi()) << thing << "zone status changed" << zoneStatus;

                        // Water detected gets indicated in the Alarm1 flag
                        if (zoneStatus.testFlag(ZigbeeClusterIasZone::ZoneStatusAlarm1)) {
                            thing->setStateValue(lumiWaterSensorWaterDetectedStateTypeId, true);
                        } else {
                            thing->setStateValue(lumiWaterSensorWaterDetectedStateTypeId, false);
                        }

                        // Battery alarm
                        if (zoneStatus.testFlag(ZigbeeClusterIasZone::ZoneStatusBattery)) {
                            thing->setStateValue(lumiWaterSensorBatteryCriticalStateTypeId, true);
                        } else {
                            thing->setStateValue(lumiWaterSensorBatteryCriticalStateTypeId, false);
                        }
                    }
                }
            }
        });
    }

    if (thing->thingClassId() == lumiVibrationSensorThingClassId) {

        connect(thing, &Thing::settingChanged, this, [this, thing, endpoint](const ParamTypeId &paramTypeId, const QVariant &value){
            qCDebug(dcZigbeeLumi()) << thing->thingClass().displayName() << "settings changed" << thing->thingClass().settingsTypes().findById(paramTypeId).displayName() << value;
            if (paramTypeId == lumiVibrationSensorSettingsSensitivityParamTypeId) {
                QString sensitivity = value.toString();
                quint8 sensitivityValue = 0x0b;
                if (sensitivity == "high") {
                    sensitivityValue = 0x01;
                } else if (sensitivity == "medium") {
                    sensitivityValue = 0x0b;
                } else {
                    sensitivityValue = 0x15;
                }

                ZigbeeClusterBasic *basicCluster = endpoint->inputCluster<ZigbeeClusterBasic>(ZigbeeClusterLibrary::ClusterIdBasic);
                if (!basicCluster) {
                    qCWarning(dcZigbeeLumi()) << "Failed to update sensitivity. The basic cluster could not be found.";
                    return;
                }

                ZigbeeClusterLibrary::WriteAttributeRecord record;
                record.attributeId = 0xFF0D;
                record.dataType = Zigbee::Uint8;
                record.data = QByteArray(1, sensitivityValue);
                writeAttributesDelayed(basicCluster, {record}, MANUFACTURER_CODE_XIAOMI);
            }
        });

        connect(endpoint, &ZigbeeNodeEndpoint::clusterAttributeChanged, this, [this, thing](ZigbeeCluster *cluster, const ZigbeeClusterAttribute &attribute){
            if (cluster->clusterId() == ZigbeeClusterLibrary::ClusterIdDoorLock) {
                // Note: the vibration sensor is using the door lock cluster, with undocumented attribitues.
                // The data interpretation has been stolen from: https://github.com/Koenkk/zigbee-herdsman-converters/blob/master/converters/fromZigbee.js
                bool valueOk = false;
                switch (attribute.id()) {
                case 0x0055: { // motion type
                    quint16 value = attribute.dataType().toUInt16(&valueOk);
                    if (!valueOk) {
                        qCWarning(dcZigbeeLumi()) << thing << "failed to convert attribute data to uint16." << attribute;
                        return;
                    }

                    switch (value) {
                    case 0x01:
                        qCDebug(dcZigbeeLumi()) << thing << "vibration detected";
                        emit emitEvent(Event(lumiVibrationSensorVibrationDetectedEventTypeId, thing->id()));
                        break;
                    case 0x02:
                        qCDebug(dcZigbeeLumi()) << thing << "tilt detected";
                        emit emitEvent(Event(lumiVibrationSensorTiltDetectedEventTypeId, thing->id()));
                        break;
                    case 0x03:
                        qCDebug(dcZigbeeLumi()) << thing << "drop detected";
                        emit emitEvent(Event(lumiVibrationSensorDropDetectedEventTypeId, thing->id()));
                        break;
                    default:
                        qCDebug(dcZigbeeLumi()) << thing << "unhandled movement type" << value;
                        break;
                    }

                    break;
                }
                case 0x0503: { // angle
                    quint16 value = attribute.dataType().toUInt16(&valueOk);
                    if (!valueOk) {
                        qCWarning(dcZigbeeLumi()) << thing << "failed to convert attribute data to uint16." << attribute;
                        return;
                    }

                    // Not sure which angle this is...maybe from the tilte
                    qCDebug(dcZigbeeLumi()) << thing << "angle" << value;
                    break;
                }
                case 0x0505: { // strength
                    quint16 value = attribute.dataType().toUInt16(&valueOk);
                    if (!valueOk) {
                        qCWarning(dcZigbeeLumi()) << thing << "failed to convert attribute data to uint16." << attribute;
                        return;
                    }

                    // So far not seen
                    qCDebug(dcZigbeeLumi()) << thing << "strength" << value;
                    break;
                }
                case 0x0508: { // rotation
                    if (attribute.dataType().dataLength() != 6) {
                        qCWarning(dcZigbeeLumi()) << thing << "received rotation data but with invalid size" << attribute.dataType().dataLength() << attribute.dataType().data().toHex();
                        return;
                    }

                    QDataStream stream(attribute.dataType().data());
                    stream.setByteOrder(QDataStream::LittleEndian);
                    qint16 x, y, z;
                    stream >> x >> y >> z;

                    int xSquare = x * x;
                    int ySquare = y * y;
                    int zSquare = y * y;
                    int angleX = qRound(atan(x / sqrt(ySquare + zSquare)) * 180 / M_PI);
                    int angleY = qRound(atan(y / sqrt(xSquare + zSquare)) * 180 / M_PI);
                    int angleZ = qRound(atan(z / sqrt(xSquare + ySquare)) * 180 / M_PI);

                    double r = sqrt(xSquare + ySquare + zSquare);
                    int absoluteX = qRound((acos(x / r)) * 180 / M_PI);
                    int absoluteY = qRound((acos(y / r)) * 180 / M_PI);

                    qCDebug(dcZigbeeLumi()) << thing << "angles:" << attribute.dataType().data().toHex();
                    qCDebug(dcZigbeeLumi()) << "x" << x << angleX;
                    qCDebug(dcZigbeeLumi()) << "y" << y << angleY;
                    qCDebug(dcZigbeeLumi()) << "z" << z << angleZ;
                    qCDebug(dcZigbeeLumi()) << "abs x" << absoluteX;
                    qCDebug(dcZigbeeLumi()) << "abs y" << absoluteY;

                    thing->setStateValue(lumiVibrationSensorAngleXStateTypeId, angleX);
                    thing->setStateValue(lumiVibrationSensorAngleYStateTypeId, angleY);
                    thing->setStateValue(lumiVibrationSensorAngleZStateTypeId, angleZ);
                    thing->setStateValue(lumiVibrationSensorAngleAbsoluteXStateTypeId, absoluteX);
                    thing->setStateValue(lumiVibrationSensorAngleAbsoluteYStateTypeId, absoluteY);
                    break;
                }
                default:
                    qCDebug(dcZigbeeLumi()) << thing << "unknown attribute change:" << attribute.id() << attribute.dataType().data().toHex();
                    break;
                }
            }
        });
    }

    if (thing->thingClassId() == lumiButtonSensorThingClassId) {
        ZigbeeClusterOnOff *onOffCluster = endpoint->inputCluster<ZigbeeClusterOnOff>(ZigbeeClusterLibrary::ClusterIdOnOff);
        if (onOffCluster) {
            connect(onOffCluster, &ZigbeeClusterOnOff::attributeChanged, thing, [thing](const ZigbeeClusterAttribute &attribute){
                qCDebug(dcZigbeeLumi()) << thing->name() << "Attribute changed:" << attribute;
                if (attribute.id() == ZigbeeClusterOnOff::AttributeOnOff && attribute.dataType().toUInt8() == 0x01) {
                    thing->emitEvent(lumiButtonSensorPressedEventTypeId, {Param(lumiButtonSensorPressedEventButtonNameParamTypeId, 1)});
                } else if (attribute.id() == 0x8000) {
                    quint8 count = attribute.dataType().toUInt8();
                    thing->emitEvent(lumiButtonSensorPressedEventTypeId, {Param(lumiButtonSensorPressedEventButtonNameParamTypeId, count)});
                }
            });
        } else {
            qCWarning(dcZigbeeLumi()) << "Could not find the OnOff input cluster on" << thing << endpoint;
        }
    }

    if (thing->thingClassId() == lumiLongpressButtonSensorThingClassId) {
        ZigbeeClusterMultistateInput *multistateInputCluster = endpoint->inputCluster<ZigbeeClusterMultistateInput>(ZigbeeClusterLibrary::ClusterIdMultistateInput);
        connect(multistateInputCluster, &ZigbeeClusterMultistateInput::attributeChanged, thing, [thing](const ZigbeeClusterAttribute &attribute){
            qCDebug(dcZigbeeLumi()) << thing->name() << "Attribute changed:" << attribute;
            if (attribute.id() == ZigbeeClusterMultistateInput::AttributePresentValue) {
                quint16 value = attribute.dataType().toUInt16();
                switch (value) {
                case 0:
                    thing->emitEvent("longPressed", {Param(lumiLongpressButtonSensorLongPressedEventButtonNameParamTypeId, "1")});
                    break;
                case 1:
                    thing->emitEvent("pressed", {Param(lumiLongpressButtonSensorPressedEventButtonNameParamTypeId, "1")});
                    break;
                case 2:
                    thing->emitEvent("pressed", {Param(lumiLongpressButtonSensorPressedEventButtonNameParamTypeId, "2")});
                    break;
                    // 0xff would be released, but nymeas longpressbutton interface doesn't use that
                }
            }
        });
    }

    if (thing->thingClassId() == lumiPowerSocketThingClassId) {

        connectToOtaOutputCluster(thing, endpoint);
        connectToOnOffInputCluster(thing, endpoint);

        if (node->modelName() == "lumi.plug.maeu01" || node->modelName() == "lumi.plug.mmeu01") {
            // Those seem to have the electricalmeasurement and metering clusters, but at least for some models/version
            // they don't seem to report anything but some instead offer analogInput clusters that report the values...
            connectToElectricalMeasurementCluster(thing, endpoint);
            connectToMeteringCluster(thing, endpoint);

            if (node->hasEndpoint(0x15)) {
                connectToAnalogInputCluster(thing, node->getEndpoint(0x15), "currentPower");
            }
            if (node->hasEndpoint(0x16)) {
                connectToAnalogInputCluster(thing, node->getEndpoint(0x16), "totalEnergyConsumed");
            }

        } else if (node->modelName() == "lumi.plug") {
            if (node->hasEndpoint(0x02)) {
                connectToAnalogInputCluster(thing, node->getEndpoint(0x02), "currentPower");
            }
            if (node->hasEndpoint(0x03)) {
                connectToAnalogInputCluster(thing, node->getEndpoint(0x03), "totalEnergyConsumed");
            }
        }
    }

    if (thing->thingClassId() == lumiRemoteThingClassId) {
        // Since we are here again out of spec, we just can react on cluster and endpoint signals
        connect(node, &ZigbeeNode::endpointClusterAttributeChanged, thing, [this, thing](ZigbeeNodeEndpoint *endpoint, ZigbeeCluster *cluster, const ZigbeeClusterAttribute &attribute){
            switch (endpoint->endpointId()) {
            case 0x01:
                if (cluster->clusterId() == ZigbeeClusterLibrary::ClusterIdMultistateInput && attribute.id() == ZigbeeClusterMultistateInput::AttributePresentValue) {
                    quint16 value = attribute.dataType().toUInt16();
                    if (value == 1) {
                        emit emitEvent(Event(lumiRemotePressedEventTypeId, thing->id(), ParamList() << Param(lumiRemotePressedEventButtonNameParamTypeId, "1")));
                    } else {
                        emit emitEvent(Event(lumiRemoteLongPressedEventTypeId, thing->id(), ParamList() << Param(lumiRemoteLongPressedEventButtonNameParamTypeId, "1")));
                    }
                }
                break;
            case 0x02:
                if (cluster->clusterId() == ZigbeeClusterLibrary::ClusterIdMultistateInput && attribute.id() == ZigbeeClusterMultistateInput::AttributePresentValue) {
                    quint16 value = attribute.dataType().toUInt16();
                    if (value == 1) {
                        emit emitEvent(Event(lumiRemotePressedEventTypeId, thing->id(), ParamList() << Param(lumiRemotePressedEventButtonNameParamTypeId, "2")));
                    } else {
                        emit emitEvent(Event(lumiRemoteLongPressedEventTypeId, thing->id(), ParamList() << Param(lumiRemoteLongPressedEventButtonNameParamTypeId, "2")));
                    }
                }
                break;
            case 0x03:
                if (cluster->clusterId() == ZigbeeClusterLibrary::ClusterIdMultistateInput && attribute.id() == ZigbeeClusterMultistateInput::AttributePresentValue) {
                    quint16 value = attribute.dataType().toUInt16();
                    if (value == 1) {
                        emit emitEvent(Event(lumiRemotePressedEventTypeId, thing->id(), ParamList() << Param(lumiRemotePressedEventButtonNameParamTypeId, "1+2")));
                    } else {
                        emit emitEvent(Event(lumiRemoteLongPressedEventTypeId, thing->id(), ParamList() << Param(lumiRemoteLongPressedEventButtonNameParamTypeId, "1+2")));
                    }
                }
                break;
            default:
                qCWarning(dcZigbeeLumi()) << "Received attribute changed signal from unhandled endpoint" << thing << endpoint << cluster << attribute;
                break;
            }
        });
    }

    if (thing->thingClassId() == lumiRelayThingClassId) {
        // Get the 2 endpoints
        ZigbeeNodeEndpoint *endpoint1 = node->getEndpoint(0x01);
        if (endpoint1) {
            connectToOnOffInputCluster(thing, endpoint1, "relay1");
        } else {
            qCWarning(dcZigbeeLumi()) << "Could not find endpoint 1 on" << thing << node;
        }

        ZigbeeNodeEndpoint *endpoint2 = node->getEndpoint(0x02);
        if (endpoint2) {
            connectToOnOffInputCluster(thing, endpoint2, "relay2");
        } else {
            qCWarning(dcZigbeeLumi()) << "Could not find endpoint 2 on" << thing << node;
        }
    }

}

void IntegrationPluginZigbeeLumi::executeAction(ThingActionInfo *info)
{
    Thing *thing = info->thing();
    ZigbeeNode *node = nodeForThing(info->thing());

    if (thing->thingClassId() == lumiPowerSocketThingClassId) {
        ZigbeeNodeEndpoint *endpoint = node->getEndpoint(0x01);
        if (!endpoint) {
            qCWarning(dcZigbeeLumi()) << "Unable to get the endpoint from node" << node << "for" << thing;
            info->finish(Thing::ThingErrorSetupFailed);
            return;
        }

        if (info->action().actionTypeId() == lumiPowerSocketPerformUpdateActionTypeId) {
            enableFirmwareUpdate(thing);
            executeImageNotifyOtaOutputCluster(info, endpoint);
            return;
        }

        if (info->action().actionTypeId() == lumiPowerSocketPowerActionTypeId) {
            executePowerOnOffInputCluster(info, endpoint);
            return;
        }

        if (info->action().actionTypeId() == lumiPowerSocketAlertActionTypeId) {
            executeIdentifyIdentifyInputCluster(info, endpoint);
            return;
        }
    }

    if (thing->thingClassId() == lumiRelayThingClassId) {

        if (info->action().actionTypeId() == lumiRelayRelay1ActionTypeId) {
            ZigbeeNodeEndpoint *endpoint = node->getEndpoint(0x01);
            if (!endpoint) {
                qCWarning(dcZigbeeLumi()) << "Unable to get the endpoint from node" << node << "for" << thing;
                info->finish(Thing::ThingErrorSetupFailed);
                return;
            }

            executePowerOnOffInputCluster(info, endpoint);
            return;
        }

        if (info->action().actionTypeId() == lumiRelayRelay2ActionTypeId) {
            ZigbeeNodeEndpoint *endpoint = node->getEndpoint(0x02);
            if (!endpoint) {
                qCWarning(dcZigbeeLumi()) << "Unable to get the endpoint from node" << node << "for" << thing;
                info->finish(Thing::ThingErrorSetupFailed);
                return;
            }

            executePowerOnOffInputCluster(info, endpoint);
        }
    }

    if (thing->thingClassId() == lumiAirMonitorThingClassId) {
        if (info->action().actionTypeId() == lumiAirMonitorPerformUpdateActionTypeId) {
            enableFirmwareUpdate(info->thing());
            executeImageNotifyOtaOutputCluster(info, node->getEndpoint(1));
            return;
        }

    }
    info->finish(Thing::ThingErrorUnsupportedFeature);
}
