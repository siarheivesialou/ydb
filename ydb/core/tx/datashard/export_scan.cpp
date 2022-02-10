#include "export_common.h"
#include "export_scan.h"

#include <ydb/core/base/appdata.h>
#include <ydb/core/base/counters.h>
#include <ydb/core/tablet_flat/flat_row_state.h>
#include <ydb/core/tablet_flat/flat_scan_spent.h>
#include <library/cpp/actors/core/hfunc.h>

#include <util/generic/bitmap.h>
#include <util/string/builder.h>

namespace NKikimr {
namespace NDataShard {

using namespace NActors;
using namespace NExportScan;
using namespace NTable;

class TExportScan: private NActors::IActor, public NTable::IScan {
    enum EStateBits {
        ES_REGISTERED = 0, // Actor is registered
        ES_INITIALIZED, // Seek(...) was called
        ES_UPLOADER_READY,
        ES_BUFFER_SENT,
        ES_NO_MORE_DATA,

        ES_COUNT,
    };

    struct TStats {
        ui64 Rows;
        ui64 BytesRead;
        ui64 BytesSent;

        TStats()
            : Rows(0)
            , BytesRead(0)
            , BytesSent(0)
        {
            auto counters = GetServiceCounters(AppData()->Counters, "tablets")->GetSubgroup("subsystem", "store_to_yt");

            MonRows = counters->GetCounter("Rows", true);
            MonBytesRead = counters->GetCounter("BytesRead", true);
            MonBytesSent = counters->GetCounter("BytesSent", true);
        }

        void Aggr(ui64 rows, ui64 bytesRead, ui64 bytesSent) {
            Rows += rows;
            BytesRead += bytesRead;
            BytesSent += bytesSent;

            *MonRows += rows;
            *MonBytesRead += bytesRead;
            *MonBytesSent += bytesSent;
        }

        void Aggr(IBuffer const* buffer) {
            Aggr(buffer->GetRows(), buffer->GetBytesRead(), buffer->GetBytesSent());
        }

        TString ToString() const {
            return TStringBuilder()
                << "Stats { "
                    << " Rows: " << Rows
                    << " BytesRead: " << BytesRead
                    << " BytesSent: " << BytesSent
                << " }";
        }

    private:
        NMonitoring::TDynamicCounters::TCounterPtr MonRows;
        NMonitoring::TDynamicCounters::TCounterPtr MonBytesRead;
        NMonitoring::TDynamicCounters::TCounterPtr MonBytesSent;
    };

    bool IsReady() const {
        return State.Test(ES_REGISTERED) && State.Test(ES_INITIALIZED);
    }

    void MaybeReady() {
        if (IsReady()) {
            Send(Uploader, new TEvExportScan::TEvReady());
        }
    }

    EScan MaybeSendBuffer() {
        const bool noMoreData = State.Test(ES_NO_MORE_DATA);

        if (!noMoreData && !Buffer->IsFilled()) {
            return EScan::Feed;
        }

        if (!State.Test(ES_UPLOADER_READY) || State.Test(ES_BUFFER_SENT)) {
            Spent->Alter(false);
            return EScan::Sleep;
        }

        State.Set(ES_BUFFER_SENT);
        Stats->Aggr(Buffer.Get());
        Send(Uploader, Buffer->PrepareEvent(noMoreData));

        if (noMoreData) {
            Spent->Alter(false);
            return EScan::Sleep;
        }

        return EScan::Feed;
    }

    void Handle(TEvExportScan::TEvReset::TPtr&) {
        Y_VERIFY(IsReady());

        EXPORT_LOG_D("Handle TEvExportScan::TEvReset"
            << ": self# " << SelfId());

        Stats.Reset(new TStats);
        State.Reset(ES_UPLOADER_READY).Reset(ES_BUFFER_SENT).Reset(ES_NO_MORE_DATA);
        Spent->Alter(true);
        Driver->Touch(EScan::Reset);
    }

    void Handle(TEvExportScan::TEvFeed::TPtr&) {
        Y_VERIFY(IsReady());

        EXPORT_LOG_D("Handle TEvExportScan::TEvFeed"
            << ": self# " << SelfId());

        State.Set(ES_UPLOADER_READY).Reset(ES_BUFFER_SENT);
        Spent->Alter(true);
        if (EScan::Feed == MaybeSendBuffer()) {
            Driver->Touch(EScan::Feed);
        }
    }

    void Handle(TEvExportScan::TEvFinish::TPtr& ev) {
        Y_VERIFY(IsReady());

        EXPORT_LOG_D("Handle TEvExportScan::TEvFinish"
            << ": self# " << SelfId()
            << ", msg# " << ev->Get()->ToString());

        Success = ev->Get()->Success;
        Error = ev->Get()->Error;
        Driver->Touch(EScan::Final);
    }

public:
    static constexpr TStringBuf LogPrefix() {
        return "scanner"sv;
    }

    explicit TExportScan(std::function<IActor*()>&& createUploaderFn, IBuffer::TPtr buffer)
        : IActor(static_cast<TReceiveFunc>(&TExportScan::StateWork), NKikimrServices::TActivity::EXPORT_SCAN_ACTOR)
        , CreateUploaderFn(std::move(createUploaderFn))
        , Buffer(std::move(buffer))
        , Stats(new TStats)
        , Driver(nullptr)
        , Success(false)
    {
    }

    void Describe(IOutputStream& o) const noexcept override {
        o << "ExportScan { "
              << "Uploader: " << Uploader
              << Stats->ToString() << " "
              << "Success: " << Success
              << "Error: " << Error
          << " }";
    }

    IScan::THello Prepare(IDriver* driver, TIntrusiveConstPtr<TScheme> scheme) noexcept override {
        TlsActivationContext->AsActorContext().RegisterWithSameMailbox(this);

        Driver = driver;
        Scheme = std::move(scheme);
        Spent = new TSpent(TAppData::TimeProvider.Get());
        Buffer->ColumnsOrder(Scheme->Tags());

        return {EScan::Feed, {}};
    }

    void Registered(TActorSystem* sys, const TActorId&) override {
        Uploader = sys->Register(CreateUploaderFn(), TMailboxType::HTSwap, AppData()->BatchPoolId);

        State.Set(ES_REGISTERED);
        MaybeReady();
    }

    EScan Seek(TLead& lead, ui64) noexcept override {
        lead.To(Scheme->Tags(), {}, ESeek::Lower);
        Buffer->Clear();

        State.Set(ES_INITIALIZED);
        MaybeReady();

        Spent->Alter(true);
        return EScan::Feed;
    }

    EScan Feed(TArrayRef<const TCell>, const TRow& row) noexcept override {
        Buffer->Collect(row);
        return MaybeSendBuffer();
    }

    EScan Exhausted() noexcept override {
        State.Set(ES_NO_MORE_DATA);
        return MaybeSendBuffer();
    }

    TAutoPtr<IDestructable> Finish(EAbort abort) noexcept override {
        const bool success = (abort == EAbort::None) && Success;

        if (abort != EAbort::None) {
            Error = "Aborted by scan host env";
            Send(Uploader, new TEvents::TEvPoisonPill());
        }

        PassAway();
        return new TExportScanProduct(success, Error, Stats->BytesRead, Stats->Rows);
    }

    STATEFN(StateWork) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvExportScan::TEvReset, Handle);
            hFunc(TEvExportScan::TEvFeed, Handle);
            hFunc(TEvExportScan::TEvFinish, Handle);
        }
    }

private:
    std::function<IActor*()> CreateUploaderFn;
    IBuffer::TPtr Buffer;

    TActorId Uploader;
    THolder<TStats> Stats;

    IDriver* Driver;
    TIntrusiveConstPtr<TScheme> Scheme;
    TAutoPtr<TSpent> Spent;

    TBitMap<EStateBits::ES_COUNT> State;
    bool Success;
    TString Error;

}; // TExportScan

NTable::IScan* CreateExportScan(IBuffer::TPtr buffer, std::function<IActor*()>&& createUploaderFn) {
    return new TExportScan(std::move(createUploaderFn), std::move(buffer));
}

} // NDataShard
} // NKikimr
