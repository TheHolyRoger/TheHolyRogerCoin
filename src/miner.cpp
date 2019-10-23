// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <miner.h>

#include <amount.h>
#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/consensus.h>
#include <consensus/tx_verify.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <hash.h>
#include <crypto/scrypt.h>
#include <validation.h>
#include <net.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <timedata.h>
#include <util.h>
#include <utilmoneystr.h>
#include <validationinterface.h>
#include "utilstrencodings.h" // for BEGIN
#include "crypto/scrypt.h"
#include "wallet/wallet.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <utility>
#include <random>
#include <boost/thread.hpp>

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest fee rate of a transaction combined with all
// its ancestors.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockWeight = 0;

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);

    return nNewTime - nOldTime;
}

BlockAssembler::Options::Options() {
    blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
}

BlockAssembler::BlockAssembler(const CChainParams& params, const Options& options) : chainparams(params)
{
    blockMinFeeRate = options.blockMinFeeRate;
    // Limit weight to between 4K and MAX_BLOCK_WEIGHT-4K for sanity:
    nBlockMaxWeight = std::max<size_t>(4000, std::min<size_t>(MAX_BLOCK_WEIGHT - 4000, options.nBlockMaxWeight));
}

static BlockAssembler::Options DefaultOptions(const CChainParams& params)
{
    // Block resource limits
    // If -blockmaxweight is not given, limit to DEFAULT_BLOCK_MAX_WEIGHT
    BlockAssembler::Options options;
    options.nBlockMaxWeight = gArgs.GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT);
    if (gArgs.IsArgSet("-blockmintxfee")) {
        CAmount n = 0;
        ParseMoney(gArgs.GetArg("-blockmintxfee", ""), n);
        options.blockMinFeeRate = CFeeRate(n);
    } else {
        options.blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    }
    return options;
}

BlockAssembler::BlockAssembler(const CChainParams& params) : BlockAssembler(params, DefaultOptions(params)) {}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;
    fIncludeWitness = false;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, bool fMineWitnessTx)
{
    int64_t nTimeStart = GetTimeMicros();

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if(!pblocktemplate.get())
        return nullptr;
    pblock = &pblocktemplate->block; // pointer for convenience

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    LOCK2(cs_main, mempool.cs);
    CBlockIndex* pindexPrev = chainActive.Tip();
    assert(pindexPrev != nullptr);
    nHeight = pindexPrev->nHeight + 1;

    pblock->nVersion = ComputeBlockVersion(pindexPrev, chainparams.GetConsensus());
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = gArgs.GetArg("-blockversion", pblock->nVersion);

    pblock->nTime = GetAdjustedTime();
    const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                       ? nMedianTimePast
                       : pblock->GetBlockTime();

    // Decide whether to include witness transactions
    // This is only needed in case the witness softfork activation is reverted
    // (which would require a very deep reorganization) or when
    // -promiscuousmempoolflags is used.
    // TODO: replace this with a call to main to assess validity of a mempool
    // transaction (which in most cases can be a no-op).
    fIncludeWitness = IsWitnessEnabled(pindexPrev, chainparams.GetConsensus()) && fMineWitnessTx;

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    addPackageTxs(nPackagesSelected, nDescendantsUpdated);

    int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockWeight = nBlockWeight;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;
    coinbaseTx.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;
    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));
    pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus());
    pblocktemplate->vTxFees[0] = -nFees;

    LogPrintf("CreateNewBlock(): block weight: %u txs: %u fees: %ld sigops %d\n", GetBlockWeight(*pblock), nBlockTx, nFees, nBlockSigOpsCost);

    // Fill in header
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
    pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());
    pblock->nNonce         = 0;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);

    CValidationState state;
    if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
    }
    int64_t nTime2 = GetTimeMicros();

    LogPrint(BCLog::BENCH, "CreateNewBlock() packages: %.2fms (%d packages, %d updated descendants), validity: %.2fms (total %.2fms)\n", 0.001 * (nTime1 - nTimeStart), nPackagesSelected, nDescendantsUpdated, 0.001 * (nTime2 - nTime1), 0.001 * (nTime2 - nTimeStart));

    return std::move(pblocktemplate);
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        }
        else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost) const
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= nBlockMaxWeight)
        return false;
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST)
        return false;
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - premature witness (in case segwit transactions are added to mempool before
//   segwit activation)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    for (const CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeWitness && it->GetTx().HasWitness())
            return false;
    }
    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf("fee %s txid %s\n",
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

int BlockAssembler::UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded,
        indexed_modified_transaction_set &mapModifiedTx)
{
    int nDescendantsUpdated = 0;
    for (const CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc))
                continue;
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCostWithAncestors -= it->GetSigOpCost();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }
    return nDescendantsUpdated;
}

// Skip entries in mapTx that are already in a block or are present
// in mapModifiedTx (which implies that the mapTx ancestor state is
// stale due to ancestor inclusion in the block)
// Also skip transactions that we've already failed to add. This can happen if
// we consider a transaction in mapModifiedTx and it fails: we can then
// potentially consider it again while walking mapTx.  It's currently
// guaranteed to fail again, but as a belt-and-suspenders check we put it in
// failedTx and avoid re-evaluation, since the re-evaluation would be using
// cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx, CTxMemPool::setEntries &failedTx)
{
    assert (it != mempool.mapTx.end());
    return mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it);
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, CTxMemPool::txiter entry, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(int &nPackagesSelected, int &nDescendantsUpdated)
{
    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors
    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty())
    {
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool.mapTx.get<ancestor_score>().end() &&
                SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareTxMemPoolEntryByAncestorFee()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                    nBlockMaxWeight - 4000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, iter, sortedEntries);

        for (size_t i=0; i<sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}


//
// Internal miner
//

static std::unique_ptr< boost::thread_group > minerThreads ;

size_t HowManyMiningThreads()
{
    return ( minerThreads != nullptr ) ? minerThreads->size() : 0 ;
}

//
// ScanSHA256Hash scans nonces looking for a sha256 hash with many zero bits
//
/*
bool static ScanSHA256Hash( const CBlockHeader * const blockHeader, uint32_t & nNonce, uint256 * phash,
                            const arith_uint256 & solutionHash, uint32_t & hashesScanned )
{
    uint256 uint256solution = ArithToUint256( solutionHash ) ;
    size_t firstLEZeroByte = 32 ;
    for ( ; firstLEZeroByte > 1 ; -- firstLEZeroByte )
        if ( ((uint8_t*)&uint256solution)[ firstLEZeroByte - 1 ] != 0 )
            break ;

    if ( firstLEZeroByte == 32 ) return true ; // nothing to look for

    // Write the first 76 bytes of the block header to a double-SHA256
    CHash256 hasher ;
    CDataStream ss( SER_NETWORK, PROTOCOL_VERSION ) ;
    ss << *blockHeader ;
    assert( ss.size() == 80 ) ;
    hasher.Write( (unsigned char*)&ss[0], 76 ) ;

    while ( true )
    {
        // Write the last 4 bytes of the block header (the nonce) to a copy of
        // the double-SHA256 state, and compute the result
        CHash256( hasher ).Write( (unsigned char*)&nNonce, 4 ).Finalize( (unsigned char*)phash ) ;

        hashesScanned ++ ;
        nNonce ++ ;

        // Return the nonce if the hash has at least as many zero bytes as the solution has,
        // then caller will check for hash <= solution
        bool found = true ;
        for ( size_t j = firstLEZeroByte ; j < 32 ; ++ j )
            if ( ((uint8_t*)phash)[ j ] != 0 ) { found = false ; break ; }

        if ( found )
        {
            LogPrintf( "ScanSHA256Hash with nonce 0x%x found %s, solution is %s\n", nNonce, phash->GetHex(), solutionHash.GetHex() ) ;
            return true ;
        }

        // Not found after trying for a while
        if ( ( nNonce & 0xffff ) == 0 )
            return false ;
    }
}
*/

//
// ScanScryptHash scans nonces looking for a scrypt hash with many zero bits
//
bool static ScanScryptHash( CBlockHeader blockHeader, uint32_t & nNonce, uint256 * phash,
                            const arith_uint256 & solutionHash, uint32_t & hashesScanned )
{
    uint256 uint256solution = ArithToUint256( solutionHash ) ;
    size_t firstLEZeroByte = 32 ;
    for ( ; firstLEZeroByte > 1 ; -- firstLEZeroByte )
        if ( ((uint8_t*)&uint256solution)[ firstLEZeroByte - 1 ] != 0 )
            break ;

    if ( firstLEZeroByte == 32 ) return true ; // nothing to look for

    while ( true )
    {
        blockHeader.nNonce = nNonce ;
        scrypt_1024_1_1_256( /* const char* input */ BEGIN(blockHeader), /* char* output */ BEGIN(*phash) ) ;
        hashesScanned ++ ;
        nNonce ++ ;

        // Return the nonce if the hash has at least as many zero bytes as the solution has,
        // then caller will check for hash <= solution
        bool found = true ;
        for ( size_t j = firstLEZeroByte ; j < 32 ; ++ j )
            if ( ((uint8_t*)phash)[ j ] != 0 ) { found = false ; break ; }

        if ( found )
        {
            LogPrintf( "ScanScryptHash with nonce 0x%x found %s, solution is %s\n", nNonce, phash->GetHex(), solutionHash.GetHex() ) ;
            return true ;
        }

        // Not found after trying for a while
        if ( ( nNonce & 0xfff ) == 0 )
            return false ;
    }
}

static bool ProcessBlockFound( const CBlock * const block, const CChainParams & chainparams )
{
    // Found a solution
    LogPrintf( "%s\n", block->ToString() ) ;
    LogPrintf( "generated %s\n", FormatMoney( block->vtx[0]->vout[0].nValue ) ) ;

    {
        LOCK( cs_main );
        if ( block->hashPrevBlock != chainActive.Tip()->GetBlockHash() )
            return error( "ProcessBlockFound: generated block is stale" ) ;
    }

    // Process this block the same as if it were received from another node
    if ( ! ProcessNewBlock( chainparams, std::make_shared< const CBlock >( *block ), true, NULL ) )
        return error( "ProcessBlockFound: ProcessNewBlock, block not accepted" ) ;

    return true ;
}

void static CoinMiner( const CChainParams & chainparams, char threadChar )
{
    LogPrintf( "CoinMiner (%c) started\n", threadChar ) ;
    RenameThread( strprintf( "coinminer-%c", threadChar ).c_str() ) ;

    std::shared_ptr< CReserveScript > coinbaseScript ;
    if ( ::vpwallets.size() > 0 )
        ::vpwallets[0]->GetScriptForMining( coinbaseScript ) ;

    try {
        // Throw an error if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL
        if ( coinbaseScript == nullptr || coinbaseScript->reserveScript.empty() )
            throw std::runtime_error( "No coinbase script available (mining requires a wallet)" ) ;

        std::random_device randomDevice ;
        std::mt19937 randomNumber( randomDevice() ) ;

        int64_t threadBeginsMillis = GetTimeMillis() ;
        uint64_t allHashesByThread = 0 ;

        uint32_t nExtraNonce = 0 ;

        while ( true )
        {
            // Busy-wait for the network to come online so we don't waste time mining
            // on an obsolete chain
            do {
                if ( g_connman->GetNodeCount( CConnman::CONNECTIONS_ALL ) > 0 && ! IsInitialBlockDownload() )
                    break ;
                MilliSleep( 1000 ) ;
            } while ( true ) ;

            //
            // Create new block
            //

            /* unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated() ; */
            CBlockIndex * pindexPrev = chainActive.Tip() ;

            std::unique_ptr< BlockAssembler > assembler( new BlockAssembler( chainparams ) ) ;
            std::unique_ptr< CBlockTemplate > pblocktemplate( assembler->CreateNewBlock( coinbaseScript->reserveScript ) ) ;
            if ( ! pblocktemplate.get() )
            {
                LogPrintf( "Keypool ran out, please invoke keypoolrefill before restarting the mining thread\n" ) ;
                return ;
            }

            CBlock * pblock = &pblocktemplate->block ;
            IncrementExtraNonce( pblock, pindexPrev, nExtraNonce ) ;

            //
            // Search
            //

            uint32_t hashesScanned = 0 ;
            int64_t scanBeginsMillis = GetTimeMillis() ;
            const CBlockHeader * const blockHeader = pblock ;
            arith_uint256 solutionHash = arith_uint256().SetCompact( blockHeader->nBits ) ;
            uint256 hash ;
            uint32_t nNonce = randomNumber() ;

            LogPrintf(
                "Running CoinMiner (%c) with %u transactions in block (%u bytes), looking for scrypt hash <= %s, random initial nonce 0x%x\n",
                threadChar,
                pblock->vtx.size(),
                ::GetSerializeSize( *pblock, SER_NETWORK, PROTOCOL_VERSION ),
                solutionHash.GetHex(), nNonce
            ) ;

            while ( true )
            {
                // Check if something found
                if ( ScanScryptHash( *blockHeader, nNonce, &hash, solutionHash, hashesScanned ) )
                {
                    if ( UintToArith256( hash ) <= solutionHash )
                    {
                        // Found a solution
                        pblock->nNonce = nNonce ;
                        if ( hash != pblock->GetPoWHash() ) {
                            LogPrintf( "CoinMiner (%c): oops! ScanScryptHash found %s but block with nonce 0x%x has scrypt hash %s\n",
                                       threadChar, hash.GetHex(), pblock->nNonce, pblock->GetPoWHash().GetHex() ) ;
                            throw std::runtime_error( "hash != pblock->GetPoWHash()" );
                        }

                        LogPrintf( "CoinMiner (%c):\n", threadChar ) ;
                        LogPrintf( "proof-of-work found with nonce 0x%x\n   scrypt hash %s\n   <= solution %s\n",
                                   nNonce, hash.GetHex(), solutionHash.GetHex() ) ;

                        /* bool ok = */ ProcessBlockFound( pblock, chainparams ) ;
                        coinbaseScript->KeepScript() ;

                        // for regression testing, stop mining after a block is found
                        if ( chainparams.MineBlocksOnDemand() )
                            throw boost::thread_interrupted() ;

                        break ;
                    }
                    else
                    {
                        nNonce = randomNumber() ;
                    }
                }

                // Check if block needs to be rebuilt
                boost::this_thread::interruption_point() ;
                /* if ( mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast
                           && GetTimeMillis() - scanBeginsMillis > 60999 )
                    break ; */
                if ( pindexPrev != chainActive.Tip() )
                    break ;

                uint32_t blockHeight = chainActive.Tip()->nHeight + 1 ;

                // recreate the block if the clock has run backwards, to get the actual time
                if ( UpdateTime( pblock, chainparams.GetConsensus(), pindexPrev ) < 0 )
                    break ;

                if ( chainparams.GetConsensus().fPowAllowMinDifficultyBlocks )
                {
                    // Changing pblock->nTime can change work required on testnet
                    solutionHash.SetCompact( pblock->nBits ) ;
                }
            }

            allHashesByThread += hashesScanned ;
            double blockHashesPerMillisecond = (double)hashesScanned / ( GetTimeMillis() - scanBeginsMillis ) ;
            double allHashesPerMillisecond = (double)allHashesByThread / ( GetTimeMillis() - threadBeginsMillis ) ;
            LogPrintf( "CoinMiner (%c) scanned %d hashes for current block (%.3f hashes/s), %ld hashes overall (%.3f hashes/s)\n",
                       threadChar, hashesScanned, blockHashesPerMillisecond * 1000, allHashesByThread, allHashesPerMillisecond * 1000 ) ;
        }
    } catch ( const boost::thread_interrupted & ) {
        LogPrintf( "CoinMiner (%c) interrupted\n", threadChar ) ;
        throw ;
    } catch ( const std::runtime_error & e ) {
        LogPrintf( "CoinMiner (%c) runtime error: %s\n", threadChar, e.what() ) ;
        return ;
    }
}

void GenerateCoins( bool fGenerate, int nThreads, const CChainParams & chainparams )
{
    if ( nThreads < 0 )
        nThreads = GetNumCores() ;

    if ( minerThreads != nullptr )
    {
        minerThreads->interrupt_all() ;
        minerThreads.reset( nullptr ) ;
    }

    if ( nThreads == 0 || ! fGenerate )
        return ;

    minerThreads.reset( new boost::thread_group() ) ;
    for ( unsigned int i = 0 ; i < nThreads ; i++ )
        minerThreads->create_thread( boost::bind( &CoinMiner, boost::cref(chainparams), '1' + i ) ) ;
}
