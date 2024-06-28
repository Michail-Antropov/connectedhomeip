/*
 *    Copyright (c) 2024 Project CHIP Authors
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

#include <app/AttributeAccessInterface.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/ConcreteAttributePath.h>
#include <app/InteractionModelEngine.h>
#include <app/util/attribute-storage.h>

#include "energy-calendar-instance.h"
#include <lib/support/logging/TextOnlyLogging.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::EnergyCalendar;
using namespace chip::app::Clusters::EnergyCalendar::Attributes;

constexpr uint32_t kSecInOneDay = 60 * 60 * 24;

static void ParceDay(uint32_t date, uint16_t & year, uint8_t & month, uint8_t & dayOfMonth, uint8_t & weekDay)
{
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t dayOfYear;

    ChipEpochToCalendarTime(date, year, month, dayOfMonth, hour, minute, second);
    uint8_t weekDayOffeset = FirstWeekdayOfYear(year);
    CalendarDateToOrdinalDate(year, month, dayOfMonth, dayOfYear);
    weekDay = static_cast<uint8_t>((dayOfYear -1 + weekDayOffeset) % kDaysPerWeek);
}

namespace chip::app::Clusters::EnergyCalendar {
    static void RestartServer(uint32_t features);
}

#if 0
static uint64_t GetCurrentDateTime(void)
{
    System::Clock::Timestamp time = System::SystemClock().GetMonotonicTimestamp();
    using cast                    = std::chrono::duration<std::uint64_t>;
    uint64_t msec                 = std::chrono::duration_cast<cast>(time).count();

    return msec / 1000;
}
#endif

chip::app::Clusters::EnergyCalendar::CalendarProviderInstance::~CalendarProviderInstance()
{
    FreeMemoryCalendarPeriodStructList(mCalendarPeriods);
    FreeMemoryDayStructList(mSpecialDays);
}

void CalendarProviderInstance::Init(void)
{
    ChipLogProgress(Zcl, "CalendarProviderInstance::Init");
    SetDefault();
}

void CalendarProviderInstance::SetDefault(void)
{
    SetCalendarID(std::nullopt);
    SetName(nullptr);
    SetProviderID(std::nullopt);
    SetEventID(std::nullopt);

    FreeMemoryCalendarPeriodStructList(mCalendarPeriods);
    SetCalendarPeriod(std::nullopt, mCalendarPeriods);

    FreeMemoryDayStructList(mSpecialDays);    
    SetSpecialDays(mSpecialDays);

    SetCurrentAndNextDays(std::nullopt, std::nullopt);
    SetPeakPeriods(std::nullopt, std::nullopt);
}

CHIP_ERROR CalendarProviderInstance::LoadJson(Json::Value & root)
{
    Json::Value value;
    DataModel::Nullable<uint32_t> calendarID;
    DataModel::Nullable<uint32_t> providerID;
    DataModel::Nullable<uint32_t> eventID;
    DataModel::Nullable<uint32_t> startDate;
    Structs::PeakPeriodStruct::Type peak;
    DataModel::Nullable<Structs::PeakPeriodStruct::Type> currentPeak;
    DataModel::Nullable<Structs::PeakPeriodStruct::Type> nextPeak;


    if (root.isMember("Features"))
    {
        value = root.get("Features", Json::Value());
        if (value.isUInt() && value.asUInt() <= 0x0F)
        {
            RestartServer(value.asUInt());
            return CHIP_NO_ERROR;
        }
    }

    if (root.isMember("CalendarID"))
    {
        value = root.get("CalendarID", Json::Value());
        if (value.isUInt() && value.asUInt() < static_cast<uint32_t>(-1)) // ignore 0xFF..FF value
        {
            calendarID.SetNonNull(value.asUInt());
        }
        else
        {
            calendarID.SetNull();
        }
        SetCalendarID(calendarID);
    }

    if (root.isMember("CalendarName"))
    {
        value = root.get("CalendarName", Json::Value());
        if (value.isString())
        {
           SetName(value.asCString());
        }
        else
        {
            SetName(nullptr);
        }
    }

    if (root.isMember("ProviderID"))
    {
        value = root.get("ProviderID", Json::Value());
        if (value.isUInt() && value.asUInt() < static_cast<uint32_t>(-1))
        {
            providerID.SetNonNull(value.asUInt());
        }
        else
        {
            providerID.SetNull();
        }
        SetProviderID(providerID);
    }

    if (root.isMember("StartDate") || root.isMember("CalendarPeriods"))
    {
        if (root.isMember("StartDate"))
        {
            value = root.get("StartDate", Json::Value());
            if (value.isUInt() && value.asUInt() < static_cast<uint32_t>(-1))
            {
                startDate.SetNonNull(value.asUInt());
            }
            else
            {
                startDate.SetNull();
            }
        }

        if (root.isMember("CalendarPeriods"))
        {
            FreeMemoryCalendarPeriodStructList(mCalendarPeriods);
            value = root.get("CalendarPeriods", Json::Value());
            if (value.isArray())
            {
                JsonToCalendarPeriodStructList(value, mCalendarPeriods);
            }
        }
        SetCalendarPeriod(startDate, mCalendarPeriods);
    }

    if (root.isMember("SpecialDays"))
    {
        FreeMemoryDayStructList(mSpecialDays);
        value = root.get("SpecialDays", Json::Value());
        if (value.isArray())
        {
            JsonToDayStructList(value, mSpecialDays);
        }
        SetSpecialDays(mSpecialDays);
    }

    if (root.isMember("CurrentPeak") || root.isMember("NextPeak")) 
    {
        if (root.isMember("CurrentPeak"))
        {
            value = root.get("CurrentPeak", Json::Value());
            if (!value.empty())
            {
                JsonToPeakPeriodStruct(value, peak);
                currentPeak.SetNonNull(peak);
            }
            else
            {
                currentPeak.SetNull();
            }
        }

        if (root.isMember("NextPeak"))
        {
            value = root.get("NextPeak", Json::Value());
            if (!value.empty())
            {
                JsonToPeakPeriodStruct(value, peak);
                nextPeak.SetNonNull(peak);
            }
            else
            {
                nextPeak.SetNull();
            }
        }

        SetPeakPeriods(currentPeak, nextPeak);
    }

    if (root.isMember("EventID"))
    {
        value = root.get("EventID", Json::Value());
        if (value.isUInt() && value.asUInt() < static_cast<uint32_t>(-1))
        {
            eventID.SetNonNull(value.asUInt());
        }
        else
        {
            eventID.SetNull();
        }
        SetEventID(eventID);
    }

    return CHIP_NO_ERROR;
}

void chip::app::Clusters::EnergyCalendar::CalendarProviderInstance::ErrorMessage(EndpointId ep, const char * msg, ...)
{
    va_list v;
    va_start(v, msg);
    ChipLogError(NotSpecified, msg, v);
    va_end(v);
}

DataModel::Nullable<Structs::DayStruct::Type> CalendarProviderInstance::GetDay(uint32_t date)
{
    DataModel::Nullable<Structs::DayStruct::Type> result = std::nullopt;
    uint32_t dt;

    uint16_t year;
    uint8_t month;
    uint8_t dayOfMonth;
    uint8_t dayOfWeek;

    ParceDay(date, year, month, dayOfMonth, dayOfWeek);

    for (auto & day : GetSpecialDays())
    {
        if (day.date.HasValue())
        {
            dt = static_cast<uint32_t>(day.date.ValueOr(0) - (day.date.ValueOr(0) % kSecInOneDay));
            if (dt == date)
            {
                return day;
            }
        }
    }

    if (GetStartDate().ValueOr(0) > date)
    {
        return result;
    }

    for (auto & period : GetCalendarPeriods())
    {
        dt = static_cast<uint32_t>(period.startDate.ValueOr(0) - (period.startDate.ValueOr(0) % kSecInOneDay));
        if (dt > date)
        {
            break;
        }

        bool calendarByWeek = period.days.size() > 0 && period.days[0].daysOfWeek.HasValue();
        if (calendarByWeek)
        {
            for (auto & day : period.days)
            {
                if (day.daysOfWeek.ValueOr(0).Has(static_cast<TransitionDayOfWeekBitmap>(1 << dayOfWeek)))
                {
                    result = day;
                    break;
                }
            }
        }
        else // loop calendar
        {
            uint32_t index = static_cast<uint32_t>((date - dt) / kSecondsPerDay % period.days.size());
            result = period.days[index];
        }
    }

    return result;
}

void CalendarProviderInstance::JsonToCalendarPeriodStruct(Json::Value & root, Structs::CalendarPeriodStruct::Type & value)
{
    Json::Value t = root.get("StartDate", Json::Value());
    if (!t.empty() && t.isUInt() && t.asUInt() < static_cast<uint32_t>(-1))
    {
        value.startDate.SetNonNull(t.asUInt());
    }

    t = root.get("Days", Json::Value());
    if (!t.empty() && t.isArray())
    {
        DataModel::List<Structs::DayStruct::Type> * days = (DataModel::List<Structs::DayStruct::Type> *) &value.days;
        JsonToDayStructList(t, *days);
    }
}

void CalendarProviderInstance::JsonToDayStruct(Json::Value & root, Structs::DayStruct::Type & value)
{
    Json::Value t = root.get("Date", Json::Value());
    if (!t.empty() && t.isUInt())
    {
        value.date.SetValue(t.asUInt());
    }

    t = root.get("DaysOfWeek", Json::Value());
    if (!t.empty() && t.isUInt() && t.asUInt() < 0x80)
    {
        value.daysOfWeek.SetValue(chip::BitMask<TransitionDayOfWeekBitmap>((uint8_t) t.asUInt()));
    }

    t = root.get("Transitions", Json::Value());
    if (!t.empty() && t.isArray())
    {
        DataModel::List<Structs::TransitionStruct::Type> * transitions =
            (DataModel::List<Structs::TransitionStruct::Type> *) &value.transitions;
        JsonToTransitionStructList(t, *transitions);
    }

    t = root.get("CalendarID", Json::Value());
    if (!t.empty() && t.isUInt() && t.asUInt() < static_cast<uint32_t>(-1))
    {
        value.calendarID.SetValue(t.asUInt());
    }
}

void CalendarProviderInstance::JsonToPeakPeriodStruct(Json::Value & root, Structs::PeakPeriodStruct::Type & value)
{
    Json::Value t = root.get("Severity", Json::Value());
    if (!t.empty() && t.isUInt() && t.asUInt() < static_cast<uint32_t>(PeakPeriodSeverityEnum::kUnknownEnumValue))
    {
        value.severity = static_cast<PeakPeriodSeverityEnum>(t.asUInt());
    }

    t = root.get("PeakPeriod", Json::Value());
    if (!t.empty() && t.isUInt() && t.asUInt() <= static_cast<uint16_t>(-1))
    {
        value.peakPeriod = static_cast<uint16_t>(t.asUInt());
    }

    t = root.get("StartTime", Json::Value());
    if (!t.empty() && t.isUInt() && t.asUInt() < static_cast<uint32_t>(-1))
    {
        value.startTime.SetNonNull(t.asUInt());
    }

    t = root.get("EndTime", Json::Value());
    if (!t.empty() && t.isUInt() && t.asUInt() < static_cast<uint32_t>(-1))
    {
        value.endTime.SetNonNull(t.asUInt());
    }
}

void CalendarProviderInstance::JsonToCalendarPeriodStructList(Json::Value & root,
                                                              DataModel::List<Structs::CalendarPeriodStruct::Type> & value)
{
    Structs::CalendarPeriodStruct::Type * buffer = (Structs::CalendarPeriodStruct::Type *) chip::Platform::MemoryCalloc(
        root.size(), sizeof(Structs::CalendarPeriodStruct::Type));

    value = Span<Structs::CalendarPeriodStruct::Type>(buffer, root.size());

    for (Json::ArrayIndex i = 0; i < root.size(); ++i)
    {
        Json::Value v = root[i];
        JsonToCalendarPeriodStruct(root[i], value[i]);
    }
}

void CalendarProviderInstance::JsonToDayStructList(Json::Value & root, DataModel::List<Structs::DayStruct::Type> & value)
{
    Structs::DayStruct::Type * buffer =
        (Structs::DayStruct::Type *) chip::Platform::MemoryCalloc(root.size(), sizeof(Structs::DayStruct::Type));

    value = Span<Structs::DayStruct::Type>(buffer, root.size());

    for (Json::ArrayIndex i = 0; i < root.size(); ++i)
    {
        Json::Value v = root[i];
        JsonToDayStruct(root[i], value[i]);
    }
}

void CalendarProviderInstance::JsonToTransitionStructList(Json::Value & root,
                                                          DataModel::List<Structs::TransitionStruct::Type> & value)
{
    Structs::TransitionStruct::Type * buffer =
        (Structs::TransitionStruct::Type *) chip::Platform::MemoryCalloc(root.size(), sizeof(Structs::TransitionStruct::Type));

    value = Span<Structs::TransitionStruct::Type>(buffer, root.size());

    for (Json::ArrayIndex i = 0; i < root.size(); ++i)
    {
        Json::Value v = root[i];

        Json::Value t = v.get("TransitionTime", Json::Value());
        if (!t.empty() && t.isUInt() && t.asUInt() <= static_cast<uint16_t>(-1))
        {
            value[i].transitionTime = static_cast<uint16_t>(t.asUInt());
        }

        t = v.get("PriceTier", Json::Value());
        if (!t.empty() && t.isUInt() && t.asUInt() < static_cast<uint32_t>(-1))
        {
            value[i].priceTier.SetValue(t.asUInt());
        }

        t = v.get("FriendlyCredit", Json::Value());
        if (!t.empty() && t.isBool())
        {
            value[i].friendlyCredit.SetValue(t.asBool());
        }

        t = v.get("AuxiliaryLoad", Json::Value());
        if (!t.empty() && t.isUInt() && t.asUInt() < 0x100)
        {
            value[i].auxiliaryLoad.SetValue(chip::BitMask<AuxiliaryLoadBitmap>((uint8_t) t.asUInt()));
        }
    }
}

void CalendarProviderInstance::FreeMemoryDayStruct(Structs::DayStruct::Type & value)
{
    DataModel::List<const Structs::TransitionStruct::Type> tmp;
    std::swap(tmp, value.transitions);
    chip::Platform::MemoryFree((void *) tmp.data());
}

void CalendarProviderInstance::FreeMemoryDayStructList(DataModel::List<Structs::DayStruct::Type> & value)
{
    for (auto & item : value)
    {
        FreeMemoryDayStruct(item);
    }
    chip::Platform::MemoryFree(value.data());
    value = Span<Structs::DayStruct::Type>();
}

void CalendarProviderInstance::FreeMemoryCalendarPeriodStruct(Structs::CalendarPeriodStruct::Type & value)
{
    for (auto & item : value.days)
    {
        Structs::DayStruct::Type * day = (Structs::DayStruct::Type *) &item;
        FreeMemoryDayStruct(*day);
    }

    DataModel::List<const Structs::DayStruct::Type> tmp;
    std::swap(tmp, value.days);
    chip::Platform::MemoryFree((void *) tmp.data());
}

void CalendarProviderInstance::FreeMemoryCalendarPeriodStructList(DataModel::List<Structs::CalendarPeriodStruct::Type> & value)
{
    for (auto & item : value)
    {
        FreeMemoryCalendarPeriodStruct(item);
    }
    chip::Platform::MemoryFree(value.data());
    value = Span<Structs::CalendarPeriodStruct::Type>();
}

static std::unique_ptr<CalendarProviderInstance> gMIDelegate;
static std::unique_ptr<EnergyCalendarServer> gMIInstance;
static BitMask<Feature> gMIFeature = BitMask<Feature, uint32_t>(
        Feature::kPricingTier, Feature::kFriendlyCredit, Feature::kAuxiliaryLoad, Feature::kPeakPeriod);

void emberAfEnergyCalendarClusterInitCallback(chip::EndpointId endpointId)
{
    VerifyOrDie(endpointId == 1); // this cluster is only enabled for endpoint 1.
    VerifyOrDie(!gMIInstance);

    gMIDelegate = std::make_unique<CalendarProviderInstance>(endpointId);
    if (gMIDelegate)
    {
        gMIDelegate->Init();

        gMIInstance = std::make_unique<EnergyCalendarServer>(gMIFeature);

        gMIInstance->Init();

        CHIP_ERROR err = gMIInstance->AddCalendarProvider(&(*gMIDelegate));
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(NotSpecified, "Failed to add Calendar provider: %s", err.AsString());
        }
    }
}

CalendarProviderInstance * chip::app::Clusters::EnergyCalendar::GetProvider()
{
    return &(*gMIDelegate);
}

void chip::app::Clusters::EnergyCalendar::RestartServer(uint32_t features)
{
    gMIFeature = static_cast<BitMask<Feature>>(features);

    VerifyOrDie(gMIInstance);
    gMIInstance.reset();

    gMIInstance = std::make_unique<EnergyCalendarServer>(gMIFeature);
    gMIInstance->Init();

    CHIP_ERROR err = gMIInstance->AddCalendarProvider(&(*gMIDelegate));
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Failed to add Calendar provider: %s", err.AsString());
    }
}
