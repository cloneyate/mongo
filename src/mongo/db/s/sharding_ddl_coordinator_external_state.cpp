/**
 *    Copyright (C) 2023-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/db/s/sharding_ddl_coordinator_external_state.h"
#include "mongo/db/s/database_sharding_state.h"
#include "mongo/db/s/global_user_write_block_state.h"
#include "mongo/db/s/sharding_ddl_util.h"
#include "mongo/db/vector_clock_mutable.h"
#include "mongo/s/grid.h"

namespace mongo {

void ShardingDDLCoordinatorExternalStateImpl::checkShardedDDLAllowedToStart(
    OperationContext* opCtx, const NamespaceString& nss) const {
    GlobalUserWriteBlockState::get(opCtx)->checkShardedDDLAllowedToStart(opCtx, nss);
}

void ShardingDDLCoordinatorExternalStateImpl::waitForVectorClockDurable(
    OperationContext* opCtx) const {
    VectorClockMutable::get(opCtx)->waitForDurable().get(opCtx);
}

void ShardingDDLCoordinatorExternalStateImpl::assertIsPrimaryShardForDb(
    OperationContext* opCtx, const DatabaseName& dbName) const {
    // Check under the dbLock if this is still the primary shard for the database
    Lock::DBLock dbLock(opCtx, dbName, MODE_IS);
    const auto scopedDss = DatabaseShardingState::assertDbLockedAndAcquireShared(opCtx, dbName);
    scopedDss->assertIsPrimaryShardForDb(opCtx);
}

bool ShardingDDLCoordinatorExternalStateImpl::isShardedTimeseries(
    OperationContext* opCtx, const NamespaceString& bucketNss) const {
    try {
        const auto bucketColl = Grid::get(opCtx)->catalogClient()->getCollection(
            opCtx, bucketNss, repl::ReadConcernLevel::kMajorityReadConcern);
        return bucketColl.getTimeseriesFields().has_value();
    } catch (const ExceptionFor<ErrorCodes::NamespaceNotFound>&) {
        // if we don't find the bucket nss it means the collection is not
        // sharded.
        return false;
    }
}

void ShardingDDLCoordinatorExternalStateImpl::allowMigrations(OperationContext* opCtx,
                                                              NamespaceString nss,
                                                              bool allowMigrations) const {
    if (allowMigrations) {
        sharding_ddl_util::resumeMigrations(opCtx, nss, boost::none);
    } else {
        sharding_ddl_util::stopMigrations(opCtx, nss, boost::none);
    }
}

std::unique_ptr<ShardingDDLCoordinatorExternalState>
ShardingDDLCoordinatorExternalStateFactoryImpl::create() const {
    return std::make_unique<ShardingDDLCoordinatorExternalStateImpl>();
}

}  // namespace mongo
