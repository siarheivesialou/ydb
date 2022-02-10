#pragma once

#include "hive.h"
#include "metrics.h"

namespace NKikimr {
namespace NHive {

struct TNodeInfo;
struct TLeaderTabletInfo;
struct TFollowerTabletInfo;

struct TCounters {
    TVector<ui64> Simple;
    TVector<i64> SimpleDelta;
    TVector<ui64> Cumulative;
    TVector<ui64> CumulativeDelta;

    void UpdateCounters(const NKikimrTabletCountersAggregator::TTabletCounters& counters) {
        {
            size_t size = counters.SimpleCountersSize();
            Simple.resize(size);
            SimpleDelta.resize(size);
            for (size_t i = 0; i < size; ++i) {
                ui64 newValue = counters.GetSimpleCounters(i);
                i64 deltaValue = newValue - Simple[i];
                Simple[i] = newValue;
                SimpleDelta[i] = deltaValue;
            }
        }
        {
            size_t size = counters.CumulativeCountersSize();
            Cumulative.resize(Max(size, Cumulative.size()), 0);
            CumulativeDelta.resize(Max(size, CumulativeDelta.size()), 0);
            for (size_t i = 0; i < size; ++i) {
                ui64 newValue = counters.GetCumulativeCounters(i);
                ui64 deltaValue = Cumulative[i] <= newValue ? newValue - Cumulative[i] : newValue;
                Cumulative[i] = newValue;
                CumulativeDelta[i] = deltaValue;
            }
        }
    }

    void UpdateCounters(const TCounters& counters) {
        {
            size_t size = counters.Simple.size();
            Simple.resize(size);
            SimpleDelta.resize(size);
            for (size_t i = 0; i < size; ++i) {
                i64 deltaValue = counters.SimpleDelta[i];
                Simple[i] += deltaValue;
                SimpleDelta[i] = deltaValue;
            }
        }
        {
            size_t size = counters.CumulativeDelta.size();
            Cumulative.resize(Max(size, Cumulative.size()), 0);
            CumulativeDelta.resize(Max(size, CumulativeDelta.size()), 0);
            for (size_t i = 0; i < size; ++i) {
                ui64 deltaValue = counters.CumulativeDelta[i];
                Cumulative[i] += deltaValue;
                CumulativeDelta[i] = deltaValue;
            }
        }
    }
};

struct TTabletCountersInfo {
    TCounters ExecutorCounters;
    TCounters AppCounters;

    void UpdateCounters(const NKikimrTabletCountersAggregator::TTabletCountersInfo& countersInfo) {
        if (countersInfo.HasExecutorCounters())
            ExecutorCounters.UpdateCounters(countersInfo.GetExecutorCounters());
        if (countersInfo.HasAppCounters())
            AppCounters.UpdateCounters(countersInfo.GetAppCounters());
    }
};

struct TTabletDebugState {
    ui32 NodesDead = 0;
    ui32 NodesDown = 0;
    ui32 NodesNotAllowed = 0;
    ui32 NodesInDatacentersNotAllowed = 0;
    bool LeaderNotRunning = false;
    ui32 NodesWithLeaderNotLocal = 0;
    ui32 NodesWithoutDomain = 0;
    ui32 NodesFilledWithDatacenterFollowers = 0;
    ui32 NodesWithoutResources = 0;
    ui32 NodesWithSomeoneFromOurFamily = 0;
    ui32 NodesWithoutLocation = 0;
};

struct TTabletInfo {
    friend class TTxMonEvent_TabletInfo;
public:
    using EVolatileState = NKikimrHive::ETabletVolatileState;

    enum class ETabletRole {
        Leader,
        Follower
    };

protected:
    EVolatileState VolatileState;
    ETabletRole TabletRole;
    TInstant VolatileStateChangeTime;
    TInstant LastBalancerDecisionTime;

public:
    static TString EVolatileStateName(EVolatileState value) {
        switch(value) {
        case EVolatileState::TABLET_VOLATILE_STATE_UNKNOWN: return "Unknown";
        case EVolatileState::TABLET_VOLATILE_STATE_STOPPED: return "Stopped";
        case EVolatileState::TABLET_VOLATILE_STATE_BOOTING: return "Booting";
        case EVolatileState::TABLET_VOLATILE_STATE_STARTING: return "Starting";
        case EVolatileState::TABLET_VOLATILE_STATE_RUNNING: return "Running";
        case EVolatileState::_TABLET_VOLATILE_STATE_BLOCKED: return "Blocked";
        default: return Sprintf("%d", static_cast<int>(value));
        }
    }

    static TString ETabletRoleName(ETabletRole value) {
        switch(value) {
        case ETabletRole::Leader: return "Leader";
        case ETabletRole::Follower: return "Follower";
        default: return Sprintf("%d", static_cast<int>(value));
        }
    }

    TInstant GetVolatileStateChangeTime() const {
        return VolatileStateChangeTime;
    }

    bool IsGoodForBalancer(TInstant now) const;

    void MakeBalancerDecision(TInstant now) {
        LastBalancerDecisionTime = now;
    }

    THive& Hive;
    TNodeId PreferredNodeId;
    TNodeId NodeId;
    TNodeInfo* Node;
    TNodeId LastNodeId;
    TTabletCountersInfo Counters;
    NKikimrHive::TTabletStatistics Statistics;

    TString GetLogPrefix() const;

protected:
    NKikimrTabletBase::TMetrics ResourceValues; // current values of various metrics
    TTabletMetricsAggregates ResourceMetricsAggregates;

public:
    TVector<TActorId> ActorsToNotify; // ...OnCreation persistent
    TVector<TActorId> ActorsToNotifyOnRestart; // volatile
    double Weight;
    mutable TString BootState;
    TInstant PostponedStart;

    TTabletInfo(ETabletRole role, THive& hive);
    TTabletInfo(const TTabletInfo&) = delete;
    TTabletInfo(TTabletInfo&&) = delete;
    TTabletInfo& operator =(const TTabletInfo&) = delete;
    TTabletInfo& operator =(TTabletInfo&&) = delete;

    bool operator ==(const TTabletInfo& tablet) const {
        return this == &tablet;
    }

    EVolatileState GetVolatileState() const {
        return VolatileState;
    }

    bool IsLeader() const {
        return TabletRole == ETabletRole::Leader;
    }

    bool IsFollower() const {
        return TabletRole == ETabletRole::Follower;
    }

    const TLeaderTabletInfo& GetLeader() const;
    TLeaderTabletInfo& GetLeader();
    TLeaderTabletInfo& AsLeader();
    const TLeaderTabletInfo& AsLeader() const;
    TFollowerTabletInfo& AsFollower();
    const TFollowerTabletInfo& AsFollower() const;
    std::pair<TTabletId, TFollowerId> GetFullTabletId() const;
    TObjectId GetObjectId() const;
    TTabletTypes::EType GetTabletType() const;
    TString ToString() const;
    TString StateString() const;
    TString FamilyString() const;
    void ChangeVolatileState(EVolatileState state);

    bool IsReadyToBoot() const {
        return NodeId == 0 && VolatileState == EVolatileState::TABLET_VOLATILE_STATE_STOPPED;
    }

    bool IsReadyToStart(TInstant now) const;
    bool IsStarting() const;
    bool IsStartingOnNode(TNodeId nodeId) const;
    bool IsRunning() const;
    bool IsBooting() const;
    bool IsAlive() const;
    bool CanBeAlive() const; // IsAlive() + <Unknown>

    bool IsAliveOnLocal(const TActorId& local) const;
    bool IsStopped() const;
    bool InitiateBoot();
    bool BecomeStarting(TNodeId nodeId);
    bool BecomeRunning(TNodeId nodeId);
    bool BecomeStopped();

    void SendStopTablet(const TActorId& local, TFullTabletId tabletId);

    bool InitiateStop();

    void BecomeUnknown(TNodeInfo* node);
    bool Kick();
    void Kill();
    const TVector<i64>& GetTabletAllowedMetricIds() const;

    void UpdateResourceUsage(const NKikimrTabletBase::TMetrics& metrics);
    TResourceRawValues GetResourceCurrentValues() const;
    TResourceRawValues GetResourceMaximumValues() const;
    static i64 GetCounterValue(const NKikimrTabletBase::TMetrics& metrics, const TVector<i64>& allowedMetricIds);
    void FilterRawValues(TResourceRawValues& values) const;
    void FilterRawValues(TResourceNormalizedValues& values) const;

    template <typename ResourcesType>
    static double GetUsage(const ResourcesType& current, const ResourcesType& maximum) {
        return max(NormalizeRawValues(current, maximum));
    }

    void UpdateWeight() { 
        TResourceRawValues current = GetResourceCurrentValues();
        TResourceRawValues maximum = GetResourceMaximumValues();
        FilterRawValues(current);
        FilterRawValues(maximum);

        Weight = GetUsage(current, maximum); 
    }

    void PostponeStart(TInstant nextStart) {
        PostponedStart = nextStart;
    }

    const TVector<TNodeId>& GetAllowedNodes() const;
    const TVector<TDataCenterId>& GetAllowedDataCenters() const;
    bool InitiateStart(TNodeInfo* node);

    const NKikimrTabletBase::TMetrics& GetResourceValues() const {
        return ResourceValues;
    }

    void InitTabletMetrics() {
        ResourceValues.SetCounter(1);
    }

    const TTabletMetricsAggregates& GetResourceMetricsAggregates() const {
        return ResourceMetricsAggregates;
    }

    TTabletMetricsAggregates& MutableResourceMetricsAggregates() {
        return ResourceMetricsAggregates;
    }

    /*NKikimrTabletBase::TMetrics& GetMutableResourceValues() {
        return ResourceValues;
    }*/

    void ActualizeTabletStatistics(TInstant now);
    ui64 GetRestartsPerPeriod(TInstant barrier);
};


} // NHive
} // NKikimr

