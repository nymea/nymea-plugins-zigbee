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

#ifndef ZIGBEEINTEGRATIONPLUGIN_H
#define ZIGBEEINTEGRATIONPLUGIN_H

#include "integrations/integrationplugin.h"
#include "hardware/zigbee/zigbeehandler.h"
#include "hardware/zigbee/zigbeehardwareresource.h"
#include "plugintimer.h"

#include <zcl/lighting/zigbeeclustercolorcontrol.h>
#include <zcl/ota/zigbeeclusterota.h>

#include <QFuture>

class FetchFirmwareReply;

class ZigbeeIntegrationPlugin: public IntegrationPlugin, public ZigbeeHandler
{
    Q_OBJECT

public:
    class FirmwareIndexEntry {
    public:
        quint16 manufacturerCode = 0;
        quint16 imageType = 0;
        quint32 fileVersion = 0;
        quint32 minFileVersion = 0;
        quint32 maxFileVersion = 0;
        quint32 fileSize = 0;
        QString modelId;
        QUrl url;
        QByteArray sha512;
    };

    explicit ZigbeeIntegrationPlugin(ZigbeeHardwareResource::HandlerType handlerType, const QLoggingCategory &loggingCategory);
    virtual ~ZigbeeIntegrationPlugin();

    virtual void init() override;
    virtual void handleRemoveNode(ZigbeeNode *node, const QUuid &networkUuid) override;
    virtual void thingRemoved(Thing *thing) override;

protected:
    ZigbeeNode *manageNode(Thing *thing);
    Thing *thingForNode(ZigbeeNode *node);
    ZigbeeNode *nodeForThing(Thing *thing);

    virtual Thing *createThing(const ThingClassId &thingClassId, ZigbeeNode *node, const ParamList &additionalParams = ParamList());
    virtual void createConnections(Thing *thing) = 0;


    void bindCluster(ZigbeeNodeEndpoint *endpoint, ZigbeeClusterLibrary::ClusterId clusterId, int retries = 3);

    void enrollIasZone(ZigbeeNodeEndpoint *endpoint, quint8 zoneId);

    void configurePowerConfigurationInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureThermostatClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureOnOffInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureLevelControlInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureColorControlInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureElectricalMeasurementInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureMeteringInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureTemperatureMeasurementInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureRelativeHumidityMeasurementInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureAnalogInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureIlluminanceMeasurementInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureOccupancySensingInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureFanControlInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureIasZoneInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureWindowCoveringInputClusterLiftPercentageAttributeReporting(ZigbeeNodeEndpoint *endpoint);
    void configureDoorLockInputClusterAttributeReporting(ZigbeeNodeEndpoint *endpoint);

    void connectToPowerConfigurationInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint, qreal maxVoltage = 0, qreal minVoltage = 0);
    void connectToThermostatCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    void connectToOnOffInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint, const QString &stateName = "power", bool inverted = false);
    void connectToOnOffOutputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint, const QString &onButtonName = "ON", const QString &offButtonName = "OFF", const QString &toggleButtonName = "TOGGLE");
    void connectToLevelControlInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint, const QString &stateName);
    void connectToLevelControlOutputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint, const QString &buttonUp = "UP", const QString &buttonDown = "DOWN");
    void connectToColorControlInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    void connectToElectricalMeasurementCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    void connectToMeteringCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    void connectToTemperatureMeasurementInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    void connectToRelativeHumidityMeasurementInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    void connectToIasZoneInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint, const QString &alarmStateName, bool inverted = false);
    void connectToIlluminanceMeasurementInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    void connectToOccupancySensingInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    void connectToFanControlInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    void connectToOtaOutputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    void connectToAnalogInputCluster(Thing *thing, ZigbeeNodeEndpoint *endpoint, const QString &stateName);
    void connectToWindowCoveringInputClusterLiftPercentage(Thing *thing, ZigbeeNodeEndpoint *endpoint);

    void executePowerOnOffInputCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);
    void executeBrightnessLevelControlInputCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);
    void executeColorTemperatureColorControlInputCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);
    void executeColorColorControlInputCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);
    void executeIdentifyIdentifyInputCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);
    void executePowerFanControlInputCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);
    void executeFlowRateFanControlInputCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);
    void executeImageNotifyOtaOutputCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);
    void executeOpenWindowCoveringCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);
    void executeCloseWindowCoveringCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);
    void executeStopWindowCoveringCluster(ThingActionInfo *info, ZigbeeNodeEndpoint *endpoint);

    void readColorTemperatureRange(Thing *thing, ZigbeeNodeEndpoint *endpoint);
    quint16 mapScaledValueToColorTemperature(Thing *thing, int scaledColorTemperature);
    int mapColorTemperatureToScaledValue(Thing *thing, quint16 colorTemperature);

    void readAttributesDelayed(ZigbeeCluster *cluster, const QList<quint16> &attributes, quint16 manufacturerCode = 0x0000);
    void writeAttributesDelayed(ZigbeeCluster *cluster, const QList<ZigbeeClusterLibrary::WriteAttributeRecord> &records, quint16 manufacturerCode = 0x0000);

    // To support OTA updates, set the firmware update index url and override the parsing.
    // This base class will take care for fetching, caching and managing.
    void setFirmwareIndexUrl(const QUrl &url);
    virtual QList<FirmwareIndexEntry> firmwareIndexFromJson(const QByteArray &data) const;

    FirmwareIndexEntry checkFirmwareAvailability(const QList<FirmwareIndexEntry> &index, quint16 manufacturerCode, quint16 imageType, quint32 currentFileVersion, const QString &modelName) const;
    void enableFirmwareUpdate(Thing *thing);

private slots:
    virtual void updateFirmwareIndex();

private:
    void setupNode(ZigbeeNode *node, Thing *thing);
    FirmwareIndexEntry firmwareInfo(quint16 manufacturerId, quint16 imageType, quint32 fileVersion) const;
    QString firmwareFileName(const FirmwareIndexEntry &info) const;
    FetchFirmwareReply *fetchFirmware(const FirmwareIndexEntry &info);
    bool firmwareFileExists(const FirmwareIndexEntry &info) const;
    QByteArray extractImage(const FirmwareIndexEntry &info, const QByteArray &data) const;

private:
    QHash<Thing*, ZigbeeNode*> m_thingNodes;

    ZigbeeHardwareResource::HandlerType m_handlerType = ZigbeeHardwareResource::HandlerTypeVendor;
    QLoggingCategory m_dc;

    typedef struct ColorTemperatureRange {
        quint16 minValue = 250;
        quint16 maxValue = 450;
    } ColorTemperatureRange;

    QHash<Thing *, ColorTemperatureRange> m_colorTemperatureRanges;
    QHash<Thing *, ZigbeeClusterColorControl::ColorCapabilities> m_colorCapabilities;

    struct DelayedAttributeReadRequest {
        ZigbeeCluster *cluster;
        QList<quint16> attributes;
        quint16 manufacturerCode;
    };
    QHash<ZigbeeNode*, QList<DelayedAttributeReadRequest>> m_delayedReadRequests;

    struct DelayedAttributeWriteRequest {
        ZigbeeCluster *cluster;
        QList<ZigbeeClusterLibrary::WriteAttributeRecord> records;
        quint16 manufacturerCode;
    };
    QHash<ZigbeeNode*, QList<DelayedAttributeWriteRequest>> m_delayedWriteRequests;

    // OTA
    QList<Thing*> m_enabledFirmwareUpdates;
    QUrl m_firmwareIndexUrl = QUrl("https://raw.githubusercontent.com/Koenkk/zigbee-OTA/master/index.json");
    QList<FirmwareIndexEntry> m_firmwareIndex;
    QDateTime m_lastFirmwareIndexUpdate;
};

class FetchFirmwareReply: public QObject
{
    Q_OBJECT
public:
    FetchFirmwareReply(QObject *parent): QObject(parent) {
        connect(this, &FetchFirmwareReply::finished, this, &FetchFirmwareReply::deleteLater);
    }
signals:
    void finished();
};

#endif // INTEGRATIONPLUGINZIGBEEEUROTRONIC_H
