/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <app-common/zap-generated/cluster-objects.h>
#include <app/AttributeAccessInterface.h>
#include <app/CommandHandlerInterface.h>
#include <app/ConcreteAttributePath.h>
#include <app/InteractionModelEngine.h>
#include <app/MessageDef/StatusIB.h>
#include <app/reporting/reporting.h>
#include <app/util/attribute-storage.h>
#include <lib/core/CHIPError.h>

#include <list>
#include <string>

namespace chip {
namespace app {
namespace Clusters {
namespace EnergyCalendar {

// Spec-defined constants
constexpr uint32_t kMaximumNameLenght            = 12;
constexpr uint32_t kMinimumCalendarPeriodsLength = 1;
constexpr uint32_t kMaximumCalendarPeriodsLength = 4;
constexpr uint32_t kMaximumSpecialDaysLength     = 50;

constexpr int kNumSupportedEndpoints = 1;

class EnergyCalendarServer;

/** @brief
 * CalendarProvider is interface of the Calendar Provider
 */
class CalendarProvider
{
public:
    CalendarProvider(EndpointId ep) : mEndpoint(ep), mName(nullptr) {}
    virtual ~CalendarProvider() = default;

    EndpointId endpoint() const { return mEndpoint; };
    void SetServer(EnergyCalendarServer *srv) { mServer = srv; }

    CHIP_ERROR SetCalendarID(DataModel::Nullable<uint32_t> calendarID);
    CHIP_ERROR SetName(const char *name);
    CHIP_ERROR SetProviderID(DataModel::Nullable<uint32_t> providerID);
    CHIP_ERROR SetEventID(DataModel::Nullable<uint32_t> eventID);

    CHIP_ERROR SetCalendarPeriod(DataModel::Nullable<uint32_t> startDate,
                                 DataModel::List<Structs::CalendarPeriodStruct::Type> calendarPeriods);

    CHIP_ERROR SetSpecialDays(DataModel::List<Structs::DayStruct::Type> specialDays);

    CHIP_ERROR SetCurrentAndNextDays(DataModel::Nullable<Structs::DayStruct::Type> currentDay,
                                     DataModel::Nullable<Structs::DayStruct::Type> nextDay);

    CHIP_ERROR SetPeakPeriods(DataModel::Nullable<Structs::PeakPeriodStruct::Type> currentPeakPeriod,
                              DataModel::Nullable<Structs::PeakPeriodStruct::Type> nextPeakPeriod);

    CHIP_ERROR UpdateDays(uint32_t day);

    virtual DataModel::Nullable<Structs::DayStruct::Type> GetDay(uint32_t day) = 0;

    virtual void ErrorMessage(EndpointId ep, const char * msg, ...) = 0;

    DataModel::Nullable<uint32_t> GetCalendarID(void) { return mCalendarID; }
    char *GetName(void) { return mName; }
    DataModel::Nullable<uint32_t> GetProviderID(void) { return mProviderID; }
    DataModel::Nullable<uint32_t> GetEventID(void) { return mEventID; }
    DataModel::Nullable<uint32_t> GetStartDate(void) { return mStartDate; }
    DataModel::List<Structs::CalendarPeriodStruct::Type> GetCalendarPeriods(void) { return mCalendarPeriods; }
    DataModel::List<Structs::DayStruct::Type> GetSpecialDays(void) { return mSpecialDays; }
    DataModel::Nullable<Structs::DayStruct::Type> GetCurrentDay(void) { return mCurrentDay; }
    DataModel::Nullable<Structs::DayStruct::Type> GetNextDay(void) { return mNextDay; }
    DataModel::Nullable<Structs::PeakPeriodStruct::Type> GetCurrentPeakPeriod(void) { return mCurrentPeakPeriod; }
    DataModel::Nullable<Structs::PeakPeriodStruct::Type> GetNextPeakPeriod(void) { return mNextPeakPeriod; }

private:
    EndpointId mEndpoint;
    EnergyCalendarServer *mServer;
    uint32_t mCurrentDate;

    DataModel::Nullable<uint32_t> mCalendarID;
    char * mName;
    DataModel::Nullable<uint32_t> mProviderID;
    DataModel::Nullable<uint32_t> mEventID;
    DataModel::Nullable<uint32_t> mStartDate;
    DataModel::List<Structs::CalendarPeriodStruct::Type> mCalendarPeriods;
    DataModel::List<Structs::DayStruct::Type> mSpecialDays;
    DataModel::Nullable<Structs::DayStruct::Type> mCurrentDay;
    DataModel::Nullable<Structs::DayStruct::Type> mNextDay;
    DataModel::Nullable<Structs::PeakPeriodStruct::Type> mCurrentPeakPeriod;
    DataModel::Nullable<Structs::PeakPeriodStruct::Type> mNextPeakPeriod;

    bool CheckPeriods(DataModel::List<Structs::CalendarPeriodStruct::Type> &periods);
    bool CheckSpecialDays(DataModel::List<Structs::DayStruct::Type> &specialDays);
    bool CheckDay(const Structs::DayStruct::Type & day);
    bool CheckPeakPeriod(const Structs::PeakPeriodStruct::Type & peakPeriod);
};

/** @brief
 * EnergyCalendarServer implements both Attributes and Commands
 */
class EnergyCalendarServer : public AttributeAccessInterface
{
public:
    EnergyCalendarServer();
    EnergyCalendarServer(Feature aFeature);
    ~EnergyCalendarServer() { Shutdown(); }

    CHIP_ERROR Init();
    void Shutdown();

    bool HasFeature(Feature aFeature) const;

    CHIP_ERROR AddCalendarProvider(CalendarProvider * provider);

    //(...)
    // Attributes
    CHIP_ERROR Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder) override;
    CHIP_ERROR Write(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder) override;

    // Commands
    // void InvokeCommand(HandlerContext & ctx) override;

private:
    BitMask<Feature> feature;
    CalendarProvider * calendars[kNumSupportedEndpoints] = { 0 };

    void UpdateCurrentAttrs(void);
    CalendarProvider * GetProvider(EndpointId ep);
    DataModel::Nullable<Structs::TransitionStruct::Type> GetTransition(EndpointId ep);

    static void MidnightTimerCallback(chip::System::Layer *, void * callbackContext);
};

} // namespace EnergyCalendar
} // namespace Clusters
} // namespace app
} // namespace chip
