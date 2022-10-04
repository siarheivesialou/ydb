#include "schemeshard__operation_db_changes.h"
#include "schemeshard_impl.h"

#include <ydb/core/tx/tx_processing.h>

namespace NKikimr::NSchemeShard {

void TStorageChanges::Apply(TSchemeShard* ss, NTabletFlatExecutor::TTransactionContext& txc, const TActorContext&) {
    NIceDb::TNiceDb db(txc.DB);

    for (const auto& pId : Pathes) {
        ss->PersistPath(db, pId);
    }

    for (const auto& pId : AlterUserAttrs) {
        ss->PersistAlterUserAttributes(db, pId);
    }

    for (const auto& pId : ApplyUserAttrs) {
        ss->ApplyAndPersistUserAttrs(db, pId);
    }

    for (const auto& pId : AlterIndexes) {
        ss->PersistTableIndexAlterData(db, pId);
    }

    for (const auto& pId : ApplyIndexes) {
        ss->PersistTableIndex(db, pId);
    }

    for (const auto& pId : AlterCdcStreams) {
        ss->PersistCdcStreamAlterData(db, pId);
    }

    for (const auto& pId : ApplyCdcStreams) {
        ss->PersistCdcStream(db, pId);
    }

    for (const auto& pId : Tables) {
        ss->PersistTable(db, pId);
    }

    for (const auto& [pId, snapshotTxId] : TableSnapshots) {
        ss->PersistSnapshotTable(db, snapshotTxId, pId);
    }

    for (const auto& shardIdx : Shards) {
        const TShardInfo& shardInfo = ss->ShardInfos.at(shardIdx);
        const TPathId& pId = shardInfo.PathId;
        const TTableInfo::TPtr tableInfo =  ss->Tables.at(pId);

        ss->PersistShardMapping(db, shardIdx, shardInfo.TabletID, pId, shardInfo.CurrentTxId, shardInfo.TabletType);
        ss->PersistChannelsBinding(db, shardIdx, shardInfo.BindedChannels);

        if (tableInfo->PerShardPartitionConfig.contains(shardIdx)) {
            ss->PersistAddTableShardPartitionConfig(db, shardIdx, tableInfo->PerShardPartitionConfig.at(shardIdx));
        }
    }

    for (const auto& opId : TxStates) {
        ss->PersistTxState(db, opId);
    }

    ss->PersistUpdateNextPathId(db);
    ss->PersistUpdateNextShardIdx(db);
}

}
