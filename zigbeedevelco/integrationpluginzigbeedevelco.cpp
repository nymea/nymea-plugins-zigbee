﻿/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
*
* Copyright 2013 - 2021, nymea GmbH
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

#include "integrationpluginzigbeedevelco.h"
#include "plugininfo.h"

#include <hardware/zigbee/zigbeehardwareresource.h>
#include <zcl/general/zigbeeclusteronoff.h>
#include <zcl/general/zigbeeclusterbinaryinput.h>
#include <zcl/general/zigbeeclusterpowerconfiguration.h>
#include <zcl/general/zigbeeclusteridentify.h>
#include <zcl/measurement/zigbeeclustertemperaturemeasurement.h>
#include <zcl/measurement/zigbeeclusterrelativehumiditymeasurement.h>
#include <zcl/security/zigbeeclusteriaszone.h>
#include <zcl/security/zigbeeclusteriaswd.h>

#include <QDebug>

#include <zigbeeutils.h>

IntegrationPluginZigbeeDevelco::IntegrationPluginZigbeeDevelco():
    ZigbeeIntegrationPlugin(ZigbeeHardwareResource::HandlerTypeVendor, dcZigbeeDevelco())
{
}

QString IntegrationPluginZigbeeDevelco::name() const
{
    return "Develco";
}

bool IntegrationPluginZigbeeDevelco::handleNode(ZigbeeNode *node, const QUuid &networkUuid)
{
    // Not filtering for Develco manufacturer code as Develco allows to override that via branding.
    // Develco devices have an endpoint 0x01 with Develco profile, so we'll check for that
    ZigbeeNodeEndpoint *endpoint1 = node->getEndpoint(0x01);
    if (!endpoint1 || endpoint1->profile() != Zigbee::ZigbeeProfileDevelco) {
        return false;
    }

    // Not checking model name as Develco allows to override that via branding, we're determining the devices by their available endpoints
    if (node->hasEndpoint(IO_MODULE_EP_INPUT1) && node->hasEndpoint(IO_MODULE_EP_INPUT2) &&
            node->hasEndpoint(IO_MODULE_EP_INPUT3) && node->hasEndpoint(IO_MODULE_EP_INPUT4) &&
            node->hasEndpoint(IO_MODULE_EP_OUTPUT1 && node->hasEndpoint(IO_MODULE_EP_OUTPUT2))) {
        qCDebug(dcZigbeeDevelco()) << "Found IO module" << node << networkUuid.toString();
        initIoModule(node);
        createThing(ioModuleThingClassId, node);
        return true;

    } else if (node->hasEndpoint(DEVELCO_EP_TEMPERATURE_SENSOR)
               && node->getEndpoint(DEVELCO_EP_TEMPERATURE_SENSOR)->hasInputCluster(static_cast<ZigbeeClusterLibrary::ClusterId>(AIR_QUALITY_SENSOR_VOC_MEASUREMENT_CLUSTER_ID))) {
        qCDebug(dcZigbeeDevelco()) << "Found air quality sensor" << node << networkUuid.toString();
        createThing(airQualitySensorThingClassId, node);

        ZigbeeNodeEndpoint *endpoint = node->getEndpoint(DEVELCO_EP_TEMPERATURE_SENSOR);
        bindCluster(endpoint, ZigbeeClusterLibrary::ClusterIdTemperatureMeasurement);
        configureTemperatureMeasurementInputClusterAttributeReporting(endpoint);
        bindCluster(endpoint, ZigbeeClusterLibrary::ClusterIdRelativeHumidityMeasurement);
        configureRelativeHumidityMeasurementInputClusterAttributeReporting(endpoint);

        configureBattryVoltageReporting(node, endpoint);
        configureVocReporting(node, endpoint);

        return true;

    } else if (node->hasEndpoint(DEVELCO_EP_IAS_ZONE)) {
        ZigbeeNodeEndpoint *iasZoneEndpoint = node->getEndpoint(DEVELCO_EP_IAS_ZONE);

        // We need to read the Type attribute to determine what this actually is...
        ZigbeeClusterIasZone *iasZoneCluster = iasZoneEndpoint->inputCluster<ZigbeeClusterIasZone>(ZigbeeClusterLibrary::ClusterIdIasZone);
        if (!iasZoneCluster) {
            return false;
        }

        qCDebug(dcZigbeeDevelco()) << "Found IAS Zone sensor" << node;
        ZigbeeClusterReply *reply = iasZoneCluster->readAttributes({ZigbeeClusterIasZone::AttributeZoneType});
        connect(reply, &ZigbeeClusterReply::finished, this, [=](){
            if (reply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeDevelco()) << "Reading IAS Zone type attribute finished with error" << reply->error();
                return;
            }
            QList<ZigbeeClusterLibrary::ReadAttributeStatusRecord> attributeStatusRecords = ZigbeeClusterLibrary::parseAttributeStatusRecords(reply->responseFrame().payload);
            if (attributeStatusRecords.count() != 1 || attributeStatusRecords.first().attributeId != ZigbeeClusterIasZone::AttributeZoneType) {
                qCWarning(dcZigbeeDevelco()) << "Unexpected reply in reading IAS Zone device type:" << attributeStatusRecords;
                return;
            }

            ZigbeeClusterLibrary::ReadAttributeStatusRecord iasZoneTypeRecord = attributeStatusRecords.first();
            qCDebug(dcZigbeeDevelco()) << "IAS Zone device type:" << iasZoneTypeRecord.dataType.toUInt16();
            switch (iasZoneTypeRecord.dataType.toUInt16()) {
            case ZigbeeClusterIasZone::ZoneTypeFireSensor:
                qCInfo(dcZigbeeDevelco()) << "Fire sensor thing";
                createThing(smokeSensorThingClassId, node);
                break;
            case ZigbeeClusterIasZone::ZoneTypeWaterSensor:
                qCInfo(dcZigbeeDevelco()) << "Water sensor thing";
                createThing(waterSensorThingClassId, node);
                break;
            case ZigbeeClusterIasZone::ZoneTypeContactSwitch:
                qCInfo(dcZigbeeDevelco()) << "Door/window sensor thing";
                createThing(doorSensorThingClassId, node);
                break;
            case ZigbeeClusterIasZone::ZoneTypeMotionSensor:
                qCInfo(dcZigbeeDevelco()) << "Motion sensor thing";
                createThing(motionSensorThingClassId, node);
                break;
            default:
                qCWarning(dcZigbeeDevelco()) << "Unhandled IAS Zone device type:" << "0x" + QString::number(iasZoneTypeRecord.dataType.toUInt16(), 16);

            }

            bindCluster(iasZoneEndpoint, ZigbeeClusterLibrary::ClusterIdIasZone);
            configureIasZoneInputClusterAttributeReporting(iasZoneEndpoint);
            enrollIasZone(iasZoneEndpoint, 0x42);

            // IAS Zone devices (at least fire, water and sensors) have a temperature sensor endpoint too
            ZigbeeNodeEndpoint *temperatureSensorEndpoint = node->getEndpoint(DEVELCO_EP_TEMPERATURE_SENSOR);
            if (temperatureSensorEndpoint) {
                bindCluster(temperatureSensorEndpoint, ZigbeeClusterLibrary::ClusterIdTemperatureMeasurement);
                configureTemperatureMeasurementInputClusterAttributeReporting(temperatureSensorEndpoint);
            }

            // Some have a light sensor (at least the motion sensor)
            ZigbeeNodeEndpoint *lightSensorEndpoint = node->getEndpoint(DEVELCO_EP_LIGHT_SENSOR);
            if (lightSensorEndpoint) {
                bindCluster(lightSensorEndpoint, ZigbeeClusterLibrary::ClusterIdIlluminanceMeasurement);
                configureIlluminanceMeasurementInputClusterAttributeReporting(lightSensorEndpoint);
            }


        });
        return true;

    }

    return false;
}

void IntegrationPluginZigbeeDevelco::setupThing(ThingSetupInfo *info)
{
    qCDebug(dcZigbeeDevelco()) << "Setup" << info->thing();

    Thing *thing = info->thing();
    if (!manageNode(thing)) {
        qCWarning(dcZigbeeDevelco()) << "Failed to claim node during setup.";
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }
    ZigbeeNode *node = nodeForThing(thing);

    if (thing->thingClassId() == ioModuleThingClassId) {
        // Set the version from the manufacturer specific attribute in base cluster
        ZigbeeNodeEndpoint *primaryEndpoint = node->getEndpoint(IO_MODULE_EP_INPUT1);
        if (!primaryEndpoint) {
            qCWarning(dcZigbeeDevelco()) << "Failed to set up IO module" << thing << ". Could not find endpoint for version parsing.";
            info->finish(Thing::ThingErrorSetupFailed);
            return;
        }

        if (primaryEndpoint->hasInputCluster(ZigbeeClusterLibrary::ClusterIdBasic)) {
            ZigbeeCluster *basicCluster = primaryEndpoint->getInputCluster(ZigbeeClusterLibrary::ClusterIdBasic);
            if (basicCluster->hasAttribute(DEVELCO_BASIC_ATTRIBUTE_SW_VERSION)) {
                thing->setStateValue(ioModuleVersionStateTypeId, parseDevelcoVersionString(primaryEndpoint));
            } else {
                readDevelcoFirmwareVersion(node, primaryEndpoint);
            }

            connect(basicCluster, &ZigbeeCluster::attributeChanged, this, [=](const ZigbeeClusterAttribute &attribute){
                if (attribute.id() == DEVELCO_BASIC_ATTRIBUTE_SW_VERSION) {
                    thing->setStateValue(ioModuleVersionStateTypeId, parseDevelcoVersionString(primaryEndpoint));
                }
            });
        }

        // Handle reachable state from node
        connect(node, &ZigbeeNode::reachableChanged, thing, [=](bool reachable){
            if (reachable) {
                readDevelcoFirmwareVersion(node, node->getEndpoint(IO_MODULE_EP_INPUT1));
                readIoModuleOutputPowerStates(thing);
                readIoModuleInputPowerStates(thing);
            }
        });


        // Output 1
        ZigbeeNodeEndpoint *output1Endpoint = node->getEndpoint(IO_MODULE_EP_OUTPUT1);
        if (!output1Endpoint) {
            qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for output 1 on" << thing << node;
        } else {
            ZigbeeClusterOnOff *onOffCluster = output1Endpoint->inputCluster<ZigbeeClusterOnOff>(ZigbeeClusterLibrary::ClusterIdOnOff);
            if (!onOffCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find On/Off cluster on" << thing << node << output1Endpoint;
            } else {
                if (onOffCluster->hasAttribute(ZigbeeClusterOnOff::AttributeOnOff)) {
                    thing->setStateValue(ioModuleOutput1StateTypeId, onOffCluster->power());
                }
                connect(onOffCluster, &ZigbeeClusterOnOff::powerChanged, thing, [thing](bool power){
                    qCDebug(dcZigbeeDevelco()) << thing << "output 1 power changed to" << power;
                    thing->setStateValue(ioModuleOutput1StateTypeId, power);
                });
            }
        }

        // Output 2
        ZigbeeNodeEndpoint *output2Endpoint = node->getEndpoint(IO_MODULE_EP_OUTPUT2);
        if (!output2Endpoint) {
            qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for output 2 on" << thing << node;
        } else {
            ZigbeeClusterOnOff *onOffCluster = output2Endpoint->inputCluster<ZigbeeClusterOnOff>(ZigbeeClusterLibrary::ClusterIdOnOff);
            if (!onOffCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find On/Off cluster on" << thing << node << output2Endpoint;
            } else {
                if (onOffCluster->hasAttribute(ZigbeeClusterOnOff::AttributeOnOff)) {
                    thing->setStateValue(ioModuleOutput2StateTypeId, onOffCluster->power());
                }
                connect(onOffCluster, &ZigbeeClusterOnOff::powerChanged, thing, [thing](bool power){
                    qCDebug(dcZigbeeDevelco()) << thing << "output 2 power changed to" << power;
                    thing->setStateValue(ioModuleOutput2StateTypeId, power);
                });
            }
        }

        // Input 1
        ZigbeeNodeEndpoint *input1Endpoint = node->getEndpoint(IO_MODULE_EP_INPUT1);
        if (!input1Endpoint) {
            qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for input 1 on" << thing << node;
        } else {
            ZigbeeClusterBinaryInput *binaryInputCluster = input1Endpoint->inputCluster<ZigbeeClusterBinaryInput>(ZigbeeClusterLibrary::ClusterIdBinaryInput);
            if (!binaryInputCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find BinaryInput cluster on" << thing << node << input1Endpoint;
            } else {
                if (binaryInputCluster->hasAttribute(ZigbeeClusterLibrary::ClusterIdBinaryInput)) {
                    thing->setStateValue(ioModuleInput1StateTypeId, binaryInputCluster->presentValue());
                }
                connect(binaryInputCluster, &ZigbeeClusterBinaryInput::presentValueChanged, thing, [thing](bool presentValue){
                    qCDebug(dcZigbeeDevelco()) << thing << "input 1 changed to" << presentValue;
                    thing->setStateValue(ioModuleInput1StateTypeId, presentValue);
                });
            }
        }

        // Input 2
        ZigbeeNodeEndpoint *input2Endpoint = node->getEndpoint(IO_MODULE_EP_INPUT2);
        if (!input2Endpoint) {
            qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for input 2 on" << thing << node;
        } else {
            ZigbeeClusterBinaryInput *binaryInputCluster = input2Endpoint->inputCluster<ZigbeeClusterBinaryInput>(ZigbeeClusterLibrary::ClusterIdBinaryInput);
            if (!binaryInputCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find BinaryInput cluster on" << thing << node << input2Endpoint;
            } else {
                if (binaryInputCluster->hasAttribute(ZigbeeClusterLibrary::ClusterIdBinaryInput)) {
                    thing->setStateValue(ioModuleInput2StateTypeId, binaryInputCluster->presentValue());
                }
                connect(binaryInputCluster, &ZigbeeClusterBinaryInput::presentValueChanged, thing, [thing](bool presentValue){
                    qCDebug(dcZigbeeDevelco()) << thing << "input 2 changed to" << presentValue;
                    thing->setStateValue(ioModuleInput2StateTypeId, presentValue);
                });
            }
        }

        // Input 3
        ZigbeeNodeEndpoint *input3Endpoint = node->getEndpoint(IO_MODULE_EP_INPUT3);
        if (!input3Endpoint) {
            qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for input 3 on" << thing << node;
        } else {
            ZigbeeClusterBinaryInput *binaryInputCluster = input3Endpoint->inputCluster<ZigbeeClusterBinaryInput>(ZigbeeClusterLibrary::ClusterIdBinaryInput);
            if (!binaryInputCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find BinaryInput cluster on" << thing << node << input3Endpoint;
            } else {
                if (binaryInputCluster->hasAttribute(ZigbeeClusterLibrary::ClusterIdBinaryInput)) {
                    thing->setStateValue(ioModuleInput3StateTypeId, binaryInputCluster->presentValue());
                }
                connect(binaryInputCluster, &ZigbeeClusterBinaryInput::presentValueChanged, thing, [thing](bool presentValue){
                    qCDebug(dcZigbeeDevelco()) << thing << "input 3 changed to" << presentValue;
                    thing->setStateValue(ioModuleInput3StateTypeId, presentValue);
                });
            }
        }

        // Input 4
        ZigbeeNodeEndpoint *input4Endpoint = node->getEndpoint(IO_MODULE_EP_INPUT4);
        if (!input4Endpoint) {
            qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for input 4 on" << thing << node;
        } else {
            ZigbeeClusterBinaryInput *binaryInputCluster = input4Endpoint->inputCluster<ZigbeeClusterBinaryInput>(ZigbeeClusterLibrary::ClusterIdBinaryInput);
            if (!binaryInputCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find BinaryInput cluster on" << thing << node << input4Endpoint;
            } else {
                if (binaryInputCluster->hasAttribute(ZigbeeClusterLibrary::ClusterIdBinaryInput)) {
                    thing->setStateValue(ioModuleInput4StateTypeId, binaryInputCluster->presentValue());
                }
                connect(binaryInputCluster, &ZigbeeClusterBinaryInput::presentValueChanged, thing, [thing](bool presentValue){
                    qCDebug(dcZigbeeDevelco()) << thing << "input 4 changed to" << presentValue;
                    thing->setStateValue(ioModuleInput4StateTypeId, presentValue);
                });
            }
        }
    } else if (thing->thingClassId() == airQualitySensorThingClassId) {
        ZigbeeNodeEndpoint *sensorEndpoint = node->getEndpoint(DEVELCO_EP_TEMPERATURE_SENSOR);
        if (!sensorEndpoint) {
            qCWarning(dcZigbeeDevelco()) << "Failed to set up air quality sensor" << thing << ". Could not find endpoint for version parsing.";
            info->finish(Thing::ThingErrorSetupFailed);
            return;
        }


        // Version state
        if (sensorEndpoint->hasInputCluster(ZigbeeClusterLibrary::ClusterIdBasic)) {
            ZigbeeCluster *basicCluster = sensorEndpoint->getInputCluster(ZigbeeClusterLibrary::ClusterIdBasic);
            if (basicCluster->hasAttribute(DEVELCO_BASIC_ATTRIBUTE_SW_VERSION)) {
                thing->setStateValue(airQualitySensorVersionStateTypeId, parseDevelcoVersionString(sensorEndpoint));
            }

            connect(basicCluster, &ZigbeeCluster::attributeChanged, this, [=](const ZigbeeClusterAttribute &attribute){
                if (attribute.id() == DEVELCO_BASIC_ATTRIBUTE_SW_VERSION) {
                    thing->setStateValue(airQualitySensorVersionStateTypeId, parseDevelcoVersionString(sensorEndpoint));
                }
            });
        }

        connectToTemperatureMeasurementInputCluster(thing, sensorEndpoint);
        connectToRelativeHumidityMeasurementInputCluster(thing, sensorEndpoint);

        // Battery voltage
        ZigbeeClusterPowerConfiguration *powerConfigurationCluster = sensorEndpoint->inputCluster<ZigbeeClusterPowerConfiguration>(ZigbeeClusterLibrary::ClusterIdPowerConfiguration);
        if (!powerConfigurationCluster) {
            qCWarning(dcZigbeeDevelco()) << "Could not find the power configuration server cluster on" << thing << sensorEndpoint;
        } else {
            // Only set the state if the cluster actually has the attribute
            if (powerConfigurationCluster->hasAttribute(ZigbeeClusterPowerConfiguration::AttributeBatteryVoltage)) {
                int batteryVoltage = powerConfigurationCluster->attribute(ZigbeeClusterPowerConfiguration::AttributeBatteryVoltage).dataType().toUInt8() * 100;
                qCDebug(dcZigbeeDevelco()) << thing << "battery voltage" << batteryVoltage << "mV";
                thing->setStateValue(airQualitySensorBatteryCriticalStateTypeId, batteryVoltage < 2500);
            }
            connect(powerConfigurationCluster, &ZigbeeClusterPowerConfiguration::attributeChanged, thing, [=](const ZigbeeClusterAttribute &attribute){
                if (attribute.id() == ZigbeeClusterPowerConfiguration::AttributeBatteryVoltage) {
                    int batteryVoltage = attribute.dataType().toUInt8() * 100;
                    qCDebug(dcZigbeeDevelco()) << thing << "battery voltage" << batteryVoltage << "mV";
                    thing->setStateValue(airQualitySensorBatteryCriticalStateTypeId, batteryVoltage < 2500);
                }
            });
        }

        // VOC
        ZigbeeCluster *vocCluster = sensorEndpoint->getInputCluster(static_cast<ZigbeeClusterLibrary::ClusterId>(AIR_QUALITY_SENSOR_VOC_MEASUREMENT_CLUSTER_ID));
        if (!vocCluster) {
            qCWarning(dcZigbeeDevelco()) << "Could not find the VOC measurement server cluster on" << thing << sensorEndpoint;
        } else {
            // Only set the state if the cluster actually has the attribute
            if (vocCluster->hasAttribute(AIR_QUALITY_SENSOR_VOC_MEASUREMENT_ATTRIBUTE_MEASURED_VALUE)) {
                ZigbeeClusterAttribute measuredValueAttribute = vocCluster->attribute(AIR_QUALITY_SENSOR_VOC_MEASUREMENT_ATTRIBUTE_MEASURED_VALUE);
                bool valueOk = false;
                quint16 value = measuredValueAttribute.dataType().toUInt16(&valueOk);
                if (valueOk) {
                    if (value == 0xFFFF) {
                        qCWarning(dcZigbeeDevelco()) << "Received invalid VOC measurment. The sensor is not ready yet.";
                    } else {
                        qCDebug(dcZigbeeDevelco()) << thing << "VOC changed" << value << "ppm";
                        thing->setStateValue(airQualitySensorVocStateTypeId, value);
                    }
                } else {
                    qCWarning(dcZigbeeDevelco()) << "Failed to convert VOC measurment value" << measuredValueAttribute;
                }
            }

            connect(vocCluster, &ZigbeeCluster::attributeChanged, thing, [=](const ZigbeeClusterAttribute &attribute){
                if (attribute.id() == AIR_QUALITY_SENSOR_VOC_MEASUREMENT_ATTRIBUTE_MEASURED_VALUE) {
                    bool valueOk = false;
                    quint16 value = attribute.dataType().toUInt16(&valueOk);
                    if (valueOk) {
                        if (value == 0xFFFF) {
                            qCWarning(dcZigbeeDevelco()) << "Received invalid VOC measurment. The sensor is not ready yet.";
                        } else {
                            qCDebug(dcZigbeeDevelco()) << thing << "VOC changed" << value << "ppm";
                            thing->setStateValue(airQualitySensorVocStateTypeId, value);
                        }
                    } else {
                        qCWarning(dcZigbeeDevelco()) << "Failed to convert VOC measurment value" << attribute;
                    }
                }
            });
        }
    } else if (thing->thingClassId() == smokeSensorThingClassId) {
        ZigbeeNodeEndpoint *iasZoneEndpoint = node->getEndpoint(DEVELCO_EP_IAS_ZONE);
        ZigbeeNodeEndpoint *temperatureSensorEndpoint = node->getEndpoint(DEVELCO_EP_TEMPERATURE_SENSOR);
        connectToIasZoneInputCluster(thing, iasZoneEndpoint, "fireDetected");
        connectToTemperatureMeasurementInputCluster(thing, temperatureSensorEndpoint);
    } else if (thing->thingClassId() == waterSensorThingClassId) {
        ZigbeeNodeEndpoint *iazZoneEndpoint = node->getEndpoint(DEVELCO_EP_IAS_ZONE);
        ZigbeeNodeEndpoint *temperatureSensorEndpoint = node->getEndpoint(DEVELCO_EP_TEMPERATURE_SENSOR);
        connectToIasZoneInputCluster(thing, iazZoneEndpoint, "waterDetected");
        connectToTemperatureMeasurementInputCluster(thing, temperatureSensorEndpoint);
    } else if (thing->thingClassId() == doorSensorThingClassId) {
        ZigbeeNodeEndpoint *iazZoneEndpoint = node->getEndpoint(DEVELCO_EP_IAS_ZONE);
        ZigbeeNodeEndpoint *temperatureSensorEndpoint = node->getEndpoint(DEVELCO_EP_TEMPERATURE_SENSOR);
        connectToIasZoneInputCluster(thing, iazZoneEndpoint, "closed", true);
        connectToTemperatureMeasurementInputCluster(thing, temperatureSensorEndpoint);
    } else if (thing->thingClassId() == motionSensorThingClassId) {
        ZigbeeNodeEndpoint *iazZoneEndpoint = node->getEndpoint(DEVELCO_EP_IAS_ZONE);
        ZigbeeNodeEndpoint *temperatureSensorEndpoint = node->getEndpoint(DEVELCO_EP_TEMPERATURE_SENSOR);
        ZigbeeNodeEndpoint *illuminanceSensorEndpoint = node->getEndpoint(DEVELCO_EP_LIGHT_SENSOR);
        connectToIasZoneInputCluster(thing, iazZoneEndpoint, "isPresent");
        connectToTemperatureMeasurementInputCluster(thing, temperatureSensorEndpoint);
        connectToIlluminanceMeasurementInputCluster(thing, illuminanceSensorEndpoint);
    }

    info->finish(Thing::ThingErrorNoError);
}

void IntegrationPluginZigbeeDevelco::postSetupThing(Thing *thing)
{
    if (thing->thingClassId() == ioModuleThingClassId) {
        if (nodeForThing(thing)->reachable()) {
            readIoModuleOutputPowerStates(thing);
            readIoModuleInputPowerStates(thing);
        }
    }
}

void IntegrationPluginZigbeeDevelco::executeAction(ThingActionInfo *info)
{
    if (!hardwareManager()->zigbeeResource()->available()) {
        info->finish(Thing::ThingErrorHardwareNotAvailable);
        return;
    }

    Thing *thing = info->thing();
    ZigbeeNode *node = nodeForThing(info->thing());

    if (thing->thingClassId() == ioModuleThingClassId) {
        // Identify
        if (info->action().actionTypeId() == ioModuleAlertActionTypeId) {
            ZigbeeNodeEndpoint *primaryEndpoint = node->getEndpoint(IO_MODULE_EP_INPUT1);
            if (!primaryEndpoint) {
                qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for execute action on" << thing << node;
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            ZigbeeClusterIdentify *identifyCluster = primaryEndpoint->inputCluster<ZigbeeClusterIdentify>(ZigbeeClusterLibrary::ClusterIdIdentify);
            if (!identifyCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find identify cluster for" << thing << "in" << node;
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            // Send the command trough the network
            ZigbeeClusterReply *reply = identifyCluster->identify(2);
            connect(reply, &ZigbeeClusterReply::finished, this, [reply, info](){
                // Note: reply will be deleted automatically
                if (reply->error() != ZigbeeClusterReply::ErrorNoError) {
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    info->finish(Thing::ThingErrorNoError);
                }
            });
            return;
        }

        // Output 1
        if (info->action().actionTypeId() == ioModuleOutput1ActionTypeId) {
            bool power = info->action().paramValue(ioModuleOutput1ActionOutput1ParamTypeId).toBool();
            qCDebug(dcZigbeeDevelco()) << "Set output 1 power of" << thing << "to" << power;

            ZigbeeNodeEndpoint *output1Endpoint = node->getEndpoint(IO_MODULE_EP_OUTPUT1);
            if (!output1Endpoint) {
                qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for output 1 on" << thing << node;
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            ZigbeeClusterOnOff *onOffCluster = output1Endpoint->inputCluster<ZigbeeClusterOnOff>(ZigbeeClusterLibrary::ClusterIdOnOff);
            if (!onOffCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find On/Off cluster on" << thing << node << output1Endpoint;
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            ZigbeeClusterReply *reply = (power ? onOffCluster->commandOn() : onOffCluster->commandOff());
            connect(reply, &ZigbeeClusterReply::finished, info, [=](){
                // Note: reply will be deleted automatically
                if (reply->error() != ZigbeeClusterReply::ErrorNoError) {
                    qCWarning(dcZigbeeDevelco()) << "Failed to set power for output 1 on" << thing << reply->error();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    info->finish(Thing::ThingErrorNoError);
                    qCDebug(dcZigbeeDevelco()) << "Set power on output 1 finished successfully for" << thing;
                    thing->setStateValue(ioModuleOutput1StateTypeId, power);
                }
            });
            return;
        }

        // Output 2
        if (info->action().actionTypeId() == ioModuleOutput2ActionTypeId) {
            bool power = info->action().paramValue(ioModuleOutput2ActionOutput2ParamTypeId).toBool();
            qCDebug(dcZigbeeDevelco()) << "Set output 2 power of" << thing << "to" << power;

            ZigbeeNodeEndpoint *output2Endpoint = node->getEndpoint(IO_MODULE_EP_OUTPUT2);
            if (!output2Endpoint) {
                qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for output 2 on" << thing << node;
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            ZigbeeClusterOnOff *onOffCluster = output2Endpoint->inputCluster<ZigbeeClusterOnOff>(ZigbeeClusterLibrary::ClusterIdOnOff);
            if (!onOffCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find On/Off cluster on" << thing << node << output2Endpoint;
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            ZigbeeClusterReply *reply = (power ? onOffCluster->commandOn() : onOffCluster->commandOff());
            connect(reply, &ZigbeeClusterReply::finished, info, [=](){
                // Note: reply will be deleted automatically
                if (reply->error() != ZigbeeClusterReply::ErrorNoError) {
                    qCWarning(dcZigbeeDevelco()) << "Failed to set power for output 2 on" << thing << reply->error();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    info->finish(Thing::ThingErrorNoError);
                    qCDebug(dcZigbeeDevelco()) << "Set power on output 2 finished successfully for" << thing;
                    thing->setStateValue(ioModuleOutput2StateTypeId, power);
                }
            });
            return;
        }

        // Impulse action
        if (info->action().actionTypeId() == ioModuleImpulseOutput1ActionTypeId || info->action().actionTypeId() == ioModuleImpulseOutput2ActionTypeId) {
            // Uint for time is 1/10 s
            uint impulseDuration = thing->settings().paramValue(ioModuleSettingsImpulseDurationParamTypeId).toUInt();
            quint16 impulseDurationScaled = static_cast<quint16>(qRound(impulseDuration / 100.0));

            ZigbeeNodeEndpoint *endpoint = nullptr;
            if (info->action().actionTypeId() == ioModuleImpulseOutput1ActionTypeId) {
                endpoint = node->getEndpoint(IO_MODULE_EP_OUTPUT1);
                qCDebug(dcZigbeeDevelco()) << "Execute output 1 impulse with" << impulseDurationScaled * 100 << "ms";
            } else if (info->action().actionTypeId() == ioModuleImpulseOutput2ActionTypeId) {
                endpoint = node->getEndpoint(IO_MODULE_EP_OUTPUT2);
                qCDebug(dcZigbeeDevelco()) << "Execute output 2 impulse with" << impulseDurationScaled * 100 << "ms";
            }

            if (!endpoint) {
                qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for impulse action on" << thing << node;
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            ZigbeeClusterOnOff *onOffCluster = endpoint->inputCluster<ZigbeeClusterOnOff>(ZigbeeClusterLibrary::ClusterIdOnOff);
            if (!onOffCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find On/Off cluster on" << thing << node << endpoint;
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }

            ZigbeeClusterReply *reply = onOffCluster->commandOnWithTimedOff(false, impulseDurationScaled, 0);
            connect(reply, &ZigbeeClusterReply::finished, info, [=](){
                // Note: reply will be deleted automatically
                if (reply->error() != ZigbeeClusterReply::ErrorNoError) {
                    qCWarning(dcZigbeeDevelco()) << "Failed to set on with timed off on" << thing << endpoint << reply->error();
                    info->finish(Thing::ThingErrorHardwareFailure);
                } else {
                    info->finish(Thing::ThingErrorNoError);
                    qCDebug(dcZigbeeDevelco()) << "Set on with timed off on finished successfully for" << thing << endpoint;
                }
            });
            return;
        }
    }
    if (thing->thingClassId() == smokeSensorThingClassId) {
        if (info->action().actionTypeId() == smokeSensorAlarmActionTypeId) {
            ZigbeeNodeEndpoint *iazZoneEndpoint = node->getEndpoint(DEVELCO_EP_IAS_ZONE);
            ZigbeeClusterIasWd *iasWdCluster = iazZoneEndpoint->inputCluster<ZigbeeClusterIasWd>(ZigbeeClusterLibrary::ClusterIdIasWd);
            if (!iasWdCluster) {
                qCWarning(dcZigbeeDevelco()) << "Could not find IAS WD cluster for" << thing << "in" << node;
                info->finish(Thing::ThingErrorHardwareFailure);
                return;
            }
            uint duration = info->action().paramValue(smokeSensorAlarmActionDurationParamTypeId).toUInt();
            ZigbeeClusterReply *reply = iasWdCluster->startWarning(ZigbeeClusterIasWd::WarningModeFire, true, ZigbeeClusterIasWd::SirenLevelHigh, duration, 50, ZigbeeClusterIasWd::StrobeLevelMedium);
            connect(reply, &ZigbeeClusterReply::finished, info, [reply, info]() {
                info->finish(reply->error() == ZigbeeClusterReply::ErrorNoError ? Thing::ThingErrorNoError:  Thing::ThingErrorHardwareFailure);
            });
            return;
        }
    }

    info->finish(Thing::ThingErrorUnsupportedFeature);
}

QString IntegrationPluginZigbeeDevelco::parseDevelcoVersionString(ZigbeeNodeEndpoint *endpoint)
{
    QString versionString;
    ZigbeeCluster *basicCluster = endpoint->getInputCluster(ZigbeeClusterLibrary::ClusterIdBasic);
    if (!basicCluster) {
        qCWarning(dcZigbeeDevelco()) << "Could not find basic cluster on" << endpoint << "for version parsing";
        return versionString;
    }

    if (!basicCluster->hasAttribute(DEVELCO_BASIC_ATTRIBUTE_SW_VERSION)) {
        qCWarning(dcZigbeeDevelco()) << "Could not find manufacturer specific develco software version attribute in basic cluster on" << endpoint;
        return versionString;
    }

    ZigbeeClusterAttribute versionAttribute = basicCluster->attribute(DEVELCO_BASIC_ATTRIBUTE_SW_VERSION);
    // 1 Byte octet string length, 3 byte version infromation
    if (versionAttribute.dataType().data().length() < 4 || versionAttribute.dataType().data().at(0) != 3) {
        qCWarning(dcZigbeeDevelco()) << "Failed to parse version string from manufacturer specific develco software version attribute" << versionAttribute;
        return versionString;
    }

    int majorVersion = static_cast<int>(versionAttribute.dataType().data().at(1));
    int minorVersion = static_cast<int>(versionAttribute.dataType().data().at(2));
    int patchVersion = static_cast<int>(versionAttribute.dataType().data().at(3));
    versionString = QString("%1.%2.%3").arg(majorVersion).arg(minorVersion).arg(patchVersion);
    //qCDebug(dcZigbeeDevelco()) << versionAttribute << ZigbeeUtils::convertByteArrayToHexString(versionAttribute.dataType().data()) << versionString;

    return versionString;
}

void IntegrationPluginZigbeeDevelco::initIoModule(ZigbeeNode *node)
{
    qCDebug(dcZigbeeDevelco()) << "Start initializing IO Module" << node;
    readDevelcoFirmwareVersion(node, node->getEndpoint(IO_MODULE_EP_INPUT1));

    // Binding and reporting outputs
    configureOnOffPowerReporting(node, node->getEndpoint(IO_MODULE_EP_OUTPUT1));
    configureOnOffPowerReporting(node, node->getEndpoint(IO_MODULE_EP_OUTPUT2));

    // Binding and reporting inputs
    configureBinaryInputReporting(node, node->getEndpoint(IO_MODULE_EP_INPUT1));
    configureBinaryInputReporting(node, node->getEndpoint(IO_MODULE_EP_INPUT2));
    configureBinaryInputReporting(node, node->getEndpoint(IO_MODULE_EP_INPUT3));
    configureBinaryInputReporting(node, node->getEndpoint(IO_MODULE_EP_INPUT4));
}

void IntegrationPluginZigbeeDevelco::configureOnOffPowerReporting(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint)
{
    qCDebug(dcZigbeeDevelco()) << "Bind on/off cluster to coordinator IEEE address" << node << endpoint;
    ZigbeeDeviceObjectReply * zdoReply = node->deviceObject()->requestBindIeeeAddress(endpoint->endpointId(), ZigbeeClusterLibrary::ClusterIdOnOff, hardwareManager()->zigbeeResource()->coordinatorAddress(node->networkUuid()), 0x01);
    connect(zdoReply, &ZigbeeDeviceObjectReply::finished, node, [=](){
        if (zdoReply->error() != ZigbeeDeviceObjectReply::ErrorNoError) {
            qCWarning(dcZigbeeDevelco()) << "Failed to bind on/off cluster to coordinator" << zdoReply->error();
        } else {
            qCDebug(dcZigbeeDevelco()) << "Bind on/off cluster to coordinator finished successfully";
        }

        // Configure attribute reporting
        ZigbeeClusterLibrary::AttributeReportingConfiguration reportingConfig;
        reportingConfig.attributeId = ZigbeeClusterOnOff::AttributeOnOff;
        reportingConfig.minReportingInterval = 0;
        reportingConfig.maxReportingInterval = 600;
        reportingConfig.dataType = Zigbee::Bool;

        qCDebug(dcZigbeeDevelco()) << "Configure attribute reporting for on/off cluster" << node << endpoint;
        ZigbeeClusterReply *reportingReply = endpoint->getInputCluster(ZigbeeClusterLibrary::ClusterIdOnOff)->configureReporting({reportingConfig});
        connect(reportingReply, &ZigbeeClusterReply::finished, this, [=](){
            if (reportingReply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeDevelco()) << "Failed configure attribute reporting on on/off cluster" << reportingReply->error();
            } else {
                qCDebug(dcZigbeeDevelco()) << "Attribute reporting configuration finished for on/off cluster" << ZigbeeClusterLibrary::parseAttributeReportingStatusRecords(reportingReply->responseFrame().payload);
            }
        });
    });
}

void IntegrationPluginZigbeeDevelco::configureBinaryInputReporting(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint)
{
    qCDebug(dcZigbeeDevelco()) << "Bind binary input cluster to coordinator IEEE address" << node << endpoint;
    ZigbeeDeviceObjectReply * zdoReply = node->deviceObject()->requestBindIeeeAddress(endpoint->endpointId(), ZigbeeClusterLibrary::ClusterIdBinaryInput, hardwareManager()->zigbeeResource()->coordinatorAddress(node->networkUuid()), 0x01);
    connect(zdoReply, &ZigbeeDeviceObjectReply::finished, node, [=](){
        if (zdoReply->error() != ZigbeeDeviceObjectReply::ErrorNoError) {
            qCWarning(dcZigbeeDevelco()) << "Failed to bind binary input cluster to coordinator" << zdoReply->error();
        } else {
            qCDebug(dcZigbeeDevelco()) << "Bind binary input cluster to coordinator finished successfully";
        }

        // Configure attribute reporting
        ZigbeeClusterLibrary::AttributeReportingConfiguration reportingConfig;
        reportingConfig.attributeId = ZigbeeClusterBinaryInput::AttributePresentValue;
        reportingConfig.minReportingInterval = 0;
        reportingConfig.maxReportingInterval = 600;
        reportingConfig.dataType = Zigbee::Bool;

        qCDebug(dcZigbeeDevelco()) << "Configure attribute reporting for binary input cluster" << node << endpoint;
        ZigbeeClusterReply *reportingReply = endpoint->getInputCluster(ZigbeeClusterLibrary::ClusterIdBinaryInput)->configureReporting({reportingConfig});
        connect(reportingReply, &ZigbeeClusterReply::finished, this, [=](){
            if (reportingReply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeDevelco()) << "Failed configure attribute reporting on binary input cluster" << reportingReply->error();
            } else {
                qCDebug(dcZigbeeDevelco()) << "Attribute reporting configuration finished for on binary input cluster" << ZigbeeClusterLibrary::parseAttributeReportingStatusRecords(reportingReply->responseFrame().payload);
            }
        });
    });
}

void IntegrationPluginZigbeeDevelco::configureTemperatureReporting(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint)
{
    // Note: don not read initial value since it returns an invalid masurement. The device needs some time for initial measuring

    qCDebug(dcZigbeeDevelco()) << "Bind temperature measurement cluster to coordinator IEEE address" << node << endpoint;
    ZigbeeDeviceObjectReply * zdoReply = node->deviceObject()->requestBindIeeeAddress(endpoint->endpointId(), ZigbeeClusterLibrary::ClusterIdTemperatureMeasurement, hardwareManager()->zigbeeResource()->coordinatorAddress(node->networkUuid()), 0x01);
    connect(zdoReply, &ZigbeeDeviceObjectReply::finished, node, [=](){
        if (zdoReply->error() != ZigbeeDeviceObjectReply::ErrorNoError) {
            qCWarning(dcZigbeeDevelco()) << "Failed to bind temperature measurement cluster to coordinator" << zdoReply->error();
        } else {
            qCDebug(dcZigbeeDevelco()) << "Bind temperature measurement cluster to coordinator finished successfully";
        }

        // Configure attribute reporting
        ZigbeeClusterLibrary::AttributeReportingConfiguration reportingConfig;
        reportingConfig.attributeId = ZigbeeClusterTemperatureMeasurement::AttributeMeasuredValue;
        reportingConfig.dataType = Zigbee::Int16;
        reportingConfig.minReportingInterval = 60;
        reportingConfig.maxReportingInterval = 300;
        reportingConfig.reportableChange = ZigbeeDataType(static_cast<qint16>(10)).data();

        qCDebug(dcZigbeeDevelco()) << "Configure attribute reporting for temperature measurement cluster" << node << endpoint;
        ZigbeeClusterReply *reportingReply = endpoint->getInputCluster(ZigbeeClusterLibrary::ClusterIdTemperatureMeasurement)->configureReporting({reportingConfig});
        connect(reportingReply, &ZigbeeClusterReply::finished, this, [=](){
            if (reportingReply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeDevelco()) << "Failed configure attribute reporting on temperature measurement cluster" << reportingReply->error();
            } else {
                qCDebug(dcZigbeeDevelco()) << "Attribute reporting configuration finished for on temperature measurement cluster" << ZigbeeClusterLibrary::parseAttributeReportingStatusRecords(reportingReply->responseFrame().payload);
            }
        });
    });
}

void IntegrationPluginZigbeeDevelco::configureHumidityReporting(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint)
{
    // Note: don not read initial value since it returns an invalid masurement. The device needs some time for initial measuring
    qCDebug(dcZigbeeDevelco()) << "Bind humidity measurement cluster to coordinator IEEE address" << node << endpoint;
    ZigbeeDeviceObjectReply * zdoReply = node->deviceObject()->requestBindIeeeAddress(endpoint->endpointId(), ZigbeeClusterLibrary::ClusterIdRelativeHumidityMeasurement, hardwareManager()->zigbeeResource()->coordinatorAddress(node->networkUuid()), 0x01);
    connect(zdoReply, &ZigbeeDeviceObjectReply::finished, node, [=](){
        if (zdoReply->error() != ZigbeeDeviceObjectReply::ErrorNoError) {
            qCWarning(dcZigbeeDevelco()) << "Failed to bind humidity measurement cluster to coordinator" << zdoReply->error();
        } else {
            qCDebug(dcZigbeeDevelco()) << "Bind humidity measurement cluster to coordinator finished successfully";
        }

        // Configure attribute reporting
        ZigbeeClusterLibrary::AttributeReportingConfiguration reportingConfig;
        reportingConfig.attributeId = ZigbeeClusterRelativeHumidityMeasurement::AttributeMeasuredValue;
        reportingConfig.dataType = Zigbee::Uint16;
        reportingConfig.minReportingInterval = 60;
        reportingConfig.maxReportingInterval = 300;
        reportingConfig.reportableChange = ZigbeeDataType(static_cast<quint16>(10)).data();

        qCDebug(dcZigbeeDevelco()) << "Configure attribute reporting for humidity measurement cluster" << node << endpoint;
        ZigbeeClusterReply *reportingReply = endpoint->getInputCluster(ZigbeeClusterLibrary::ClusterIdRelativeHumidityMeasurement)->configureReporting({reportingConfig});
        connect(reportingReply, &ZigbeeClusterReply::finished, this, [=](){
            if (reportingReply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeDevelco()) << "Failed configure attribute reporting on humidity measurement cluster" << reportingReply->error();
            } else {
                qCDebug(dcZigbeeDevelco()) << "Attribute reporting configuration finished for on humidity measurement cluster" << ZigbeeClusterLibrary::parseAttributeReportingStatusRecords(reportingReply->responseFrame().payload);
            }
        });
    });
}

void IntegrationPluginZigbeeDevelco::configureBattryVoltageReporting(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint)
{
    // Note: don not read initial value since it returns an invalid masurement. The device needs some time for initial measuring
    qCDebug(dcZigbeeDevelco()) << "Bind power configuration cluster to coordinator IEEE address" << node << endpoint;
    ZigbeeDeviceObjectReply * zdoReply = node->deviceObject()->requestBindIeeeAddress(endpoint->endpointId(), ZigbeeClusterLibrary::ClusterIdPowerConfiguration, hardwareManager()->zigbeeResource()->coordinatorAddress(node->networkUuid()), 0x01);
    connect(zdoReply, &ZigbeeDeviceObjectReply::finished, node, [=](){
        if (zdoReply->error() != ZigbeeDeviceObjectReply::ErrorNoError) {
            qCWarning(dcZigbeeDevelco()) << "Failed to bind power configuration cluster to coordinator" << zdoReply->error();
        } else {
            qCDebug(dcZigbeeDevelco()) << "Bind power configuration cluster to coordinator finished successfully";
        }

        // Configure attribute reporting
        ZigbeeClusterLibrary::AttributeReportingConfiguration reportingConfig;
        reportingConfig.attributeId = ZigbeeClusterPowerConfiguration::AttributeBatteryVoltage;
        reportingConfig.dataType = Zigbee::Uint8;
        reportingConfig.minReportingInterval = 60;
        reportingConfig.maxReportingInterval = 300;
        reportingConfig.reportableChange = ZigbeeDataType(static_cast<quint8>(1)).data();

        qCDebug(dcZigbeeDevelco()) << "Configure attribute reporting for power configuration cluster" << node << endpoint;
        ZigbeeClusterReply *reportingReply = endpoint->getInputCluster(ZigbeeClusterLibrary::ClusterIdPowerConfiguration)->configureReporting({reportingConfig});
        connect(reportingReply, &ZigbeeClusterReply::finished, this, [=](){
            if (reportingReply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeDevelco()) << "Failed configure attribute reporting on power configuration cluster" << reportingReply->error();
            } else {
                qCDebug(dcZigbeeDevelco()) << "Attribute reporting configuration finished for on power configuration cluster" << ZigbeeClusterLibrary::parseAttributeReportingStatusRecords(reportingReply->responseFrame().payload);
            }
        });
    });
}

void IntegrationPluginZigbeeDevelco::configureVocReporting(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint)
{
    // Note: don not read initial value since it returns an invalid masurement. The device needs some time for initial measuring

    qCDebug(dcZigbeeDevelco()) << "Bind VOC measurement cluster to coordinator IEEE address" << node << endpoint;
    ZigbeeDeviceObjectReply * zdoReply = node->deviceObject()->requestBindIeeeAddress(endpoint->endpointId(), AIR_QUALITY_SENSOR_VOC_MEASUREMENT_CLUSTER_ID, hardwareManager()->zigbeeResource()->coordinatorAddress(node->networkUuid()), 0x01);
    connect(zdoReply, &ZigbeeDeviceObjectReply::finished, node, [=](){
        if (zdoReply->error() != ZigbeeDeviceObjectReply::ErrorNoError) {
            qCWarning(dcZigbeeDevelco()) << "Failed to bind VOC measurement cluster to coordinator" << zdoReply->error();
        } else {
            qCDebug(dcZigbeeDevelco()) << "Bind VOC measurement cluster to coordinator finished successfully";
        }

        // Configure attribute reporting
        ZigbeeClusterLibrary::AttributeReportingConfiguration reportingConfig;
        reportingConfig.attributeId = AIR_QUALITY_SENSOR_VOC_MEASUREMENT_ATTRIBUTE_MEASURED_VALUE;
        reportingConfig.dataType = Zigbee::Uint16;
        reportingConfig.minReportingInterval = 60;
        reportingConfig.maxReportingInterval = 300;
        reportingConfig.reportableChange = ZigbeeDataType(static_cast<quint16>(10)).data();

        qCDebug(dcZigbeeDevelco()) << "Configure attribute reporting for VOC measurement cluster" << node << endpoint;
        ZigbeeClusterReply *reportingReply = endpoint->getInputCluster(static_cast<ZigbeeClusterLibrary::ClusterId>(AIR_QUALITY_SENSOR_VOC_MEASUREMENT_CLUSTER_ID))->configureReporting({reportingConfig}, Zigbee::Develco);
        connect(reportingReply, &ZigbeeClusterReply::finished, this, [=](){
            if (reportingReply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeDevelco()) << "Failed configure attribute reporting on VOC measurement cluster" << reportingReply->error();
            } else {
                qCDebug(dcZigbeeDevelco()) << "Attribute reporting configuration finished for on VOC measurement cluster" << ZigbeeClusterLibrary::parseAttributeReportingStatusRecords(reportingReply->responseFrame().payload);
            }
        });
    });
}

void IntegrationPluginZigbeeDevelco::readDevelcoFirmwareVersion(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint)
{
    // Read manufacturer specific basic cluster attribute 0x8000
    ZigbeeClusterBasic *basicCluster = endpoint->inputCluster<ZigbeeClusterBasic>(ZigbeeClusterLibrary::ClusterIdBasic);
    if (!basicCluster) {
        qCWarning(dcZigbeeDevelco()) << "Could not find basic cluster for manufacturer specific attribute reading on" << node << endpoint;
        return;
    }

    // We have to read the color capabilities
    ZigbeeClusterReply *reply = basicCluster->readAttributes({DEVELCO_BASIC_ATTRIBUTE_SW_VERSION}, Zigbee::Develco);
    connect(reply, &ZigbeeClusterReply::finished, node, [=](){
        if (reply->error() != ZigbeeClusterReply::ErrorNoError) {
            qCWarning(dcZigbeeDevelco()) << "Failed to read manufacturer specific version attribute on" << node << endpoint << basicCluster;
            return;
        }

        qCDebug(dcZigbeeDevelco()) << "Reading develco manufacturer specific version attributes finished successfully";
    });
}

void IntegrationPluginZigbeeDevelco::readOnOffPowerAttribute(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint)
{
    qCDebug(dcZigbeeDevelco()) << "Reading power states of" << node << "on" << endpoint;

    ZigbeeClusterOnOff *onOffCluster = endpoint->inputCluster<ZigbeeClusterOnOff>(ZigbeeClusterLibrary::ClusterIdOnOff);
    if (!onOffCluster) {
        qCWarning(dcZigbeeDevelco()) << "Could not find On/Off cluster on" << node << endpoint;
    } else {
        ZigbeeClusterReply *reply = onOffCluster->readAttributes({ZigbeeClusterOnOff::AttributeOnOff});
        connect(reply, &ZigbeeClusterReply::finished, node, [=](){
            if (reply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeDevelco()) << "Failed to read On/Off power attribute from" << node << endpoint << onOffCluster;
            }
            // Will be updated trough the attribute changed signal
        });
    }
}

void IntegrationPluginZigbeeDevelco::readBinaryInputPresentValueAttribute(ZigbeeNode *node, ZigbeeNodeEndpoint *endpoint)
{
    ZigbeeClusterBinaryInput *binaryInputCluster = endpoint->inputCluster<ZigbeeClusterBinaryInput>(ZigbeeClusterLibrary::ClusterIdBinaryInput);
    if (!binaryInputCluster) {
        qCWarning(dcZigbeeDevelco()) << "Could not find BinaryInput cluster on" << node << endpoint;
    } else {
        ZigbeeClusterReply *reply = binaryInputCluster->readAttributes({ZigbeeClusterBinaryInput::AttributePresentValue});
        connect(reply, &ZigbeeClusterReply::finished, node, [=](){
            if (reply->error() != ZigbeeClusterReply::ErrorNoError) {
                qCWarning(dcZigbeeDevelco()) << "Failed to read binary input value attribute from" << node << endpoint << binaryInputCluster;
            }
            // Will be updated trough the attribute changed signal
        });
    }
}

void IntegrationPluginZigbeeDevelco::readIoModuleOutputPowerStates(Thing *thing)
{
    ZigbeeNode *node = nodeForThing(thing);
    if (!node) {
        qCWarning(dcZigbeeDevelco()) << "Could not find zigbee node for" << thing;
        return;
    }

    qCDebug(dcZigbeeDevelco()) << "Start reading power states of" << thing << node;

    // Read output 1 power state
    ZigbeeNodeEndpoint *output1Endpoint = node->getEndpoint(IO_MODULE_EP_OUTPUT1);
    if (!output1Endpoint) {
        qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for output 1 on" << thing << node;
    } else {
        readOnOffPowerAttribute(node, output1Endpoint);
    }

    // Read output 2 power state
    ZigbeeNodeEndpoint *output2Endpoint = node->getEndpoint(IO_MODULE_EP_OUTPUT2);
    if (!output2Endpoint) {
        qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for output 2 on" << thing << node;
    } else {
        readOnOffPowerAttribute(node, output2Endpoint);
    }
}

void IntegrationPluginZigbeeDevelco::readIoModuleInputPowerStates(Thing *thing)
{
    ZigbeeNode *node = nodeForThing(thing);
    if (!node) {
        qCWarning(dcZigbeeDevelco()) << "Could not find zigbee node for" << thing;
        return;
    }

    // Read input 1 state
    ZigbeeNodeEndpoint *input1Endpoint = node->getEndpoint(IO_MODULE_EP_INPUT1);
    if (!input1Endpoint) {
        qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for input 1 on" << thing << node;
    } else {
        readBinaryInputPresentValueAttribute(node, input1Endpoint);
    }

    // Read input 2 state
    ZigbeeNodeEndpoint *input2Endpoint = node->getEndpoint(IO_MODULE_EP_INPUT2);
    if (!input2Endpoint) {
        qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for input 2 on" << thing << node;
    } else {
        readBinaryInputPresentValueAttribute(node, input2Endpoint);
    }

    // Read input 3 state
    ZigbeeNodeEndpoint *input3Endpoint = node->getEndpoint(IO_MODULE_EP_INPUT3);
    if (!input3Endpoint) {
        qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for input 3 on" << thing << node;
    } else {
        readBinaryInputPresentValueAttribute(node, input3Endpoint);
    }

    // Read input 4 state
    ZigbeeNodeEndpoint *input4Endpoint = node->getEndpoint(IO_MODULE_EP_INPUT4);
    if (!input4Endpoint) {
        qCWarning(dcZigbeeDevelco()) << "Could not find endpoint for input 4 on" << thing << node;
    } else {
        readBinaryInputPresentValueAttribute(node, input4Endpoint);
    }
}

