// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>

#include <assert.h>
#include <memory>

#include <chainparamsseeds.h>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * python2 ./genesis.py -a scrypt -z "The Telegraph 27/May/2018 Kim Jong-un commits to Trump summit and denuclearisation" -p "04B99291FD5963B92F914F3ADB533605ECE67A9F4A4120CED4D8CB2E68C74AD5150D4EB228AE2103A693D4A4088A49A0526AE873D8EFD8CB84CF86246F9B8CF948" -n 2084524493
 *   04ffff001d01044c525468652054656c6567726170682032372f4d61792f32303138204b696d204a6f6e672d756e20636f6d6d69747320746f205472756d702073756d6d697420616e642064656e75636c65617269736174696f6e
 *   algorithm: scrypt
 *   merkle hash: 7295c791d735679ec7578c74881458876ac8e696d6683311b482862306879544
 *   pszTimestamp: The Telegraph 27/May/2018 Kim Jong-un commits to Trump summit and denuclearisation
 *   pubkey: 04B99291FD5963B92F914F3ADB533605ECE67A9F4A4120CED4D8CB2E68C74AD5150D4EB228AE2103A693D4A4088A49A0526AE873D8EFD8CB84CF86246F9B8CF948
 *   time: 1527430983
 *   bits: 0x1e0ffff0
 *   Searching for genesis hash..
 *     nonce: 2085075971
 *     genesis hash: efdcf5b2eece13c846fad57cef0eb5cbc07076dc90cf125a1b78625bda84b99f
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "The Telegraph 27/May/2018 Kim Jong-un commits to Trump summit and denuclearisation";
    const CScript genesisOutputScript = CScript() << ParseHex("04B99291FD5963B92F914F3ADB533605ECE67A9F4A4120CED4D8CB2E68C74AD5150D4EB228AE2103A693D4A4088A49A0526AE873D8EFD8CB84CF86246F9B8CF948") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.BIP16Height = 0; // 73474adb2b764dcf94f4a633e0ff47234d69e3780a220531a766296f4a407412 - October 1, 2012
        consensus.BIP34Height = 76;
        consensus.BIP34Hash = uint256S("678f04de7ac4f992071d75557dfed5da6cb716938c52fcb965d019392d3eced9");
        consensus.BIP65Height = 76; // 678f04de7ac4f992071d75557dfed5da6cb716938c52fcb965d019392d3eced9
        consensus.BIP66Height = 76; // 678f04de7ac4f992071d75557dfed5da6cb716938c52fcb965d019392d3eced9
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); 
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // 3.5 days
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 6048; // 75% of 8064
        consensus.nMinerConfirmationWindow = 8064; // nPowTargetTimespan / nPowTargetSpacing * 4
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000001d2c47a2bb3ed4");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x8cd00f1222fece10d64aa629d28fa41d6bcd36723b507f4dbcc995f2f5bebb39"); //80380 (fix bad sigs before this block)

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xfc;
        pchMessageStart[1] = 0xc2;
        pchMessageStart[2] = 0xc4;
        pchMessageStart[3] = 0xeb;
        nDefaultPort = 9663;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1527430983, 2085075971, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0xefdcf5b2eece13c846fad57cef0eb5cbc07076dc90cf125a1b78625bda84b99f"));
        assert(genesis.hashMerkleRoot == uint256S("0x7295c791d735679ec7578c74881458876ac8e696d6683311b482862306879544"));

        // Note that of those with the service bits flag, most only support a subset of possible options
       vSeeds.emplace_back("dnsseed.theholyroger.com");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,61);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,13);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,70);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,176);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "rog";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        //CHANGE CHECKPOINT
        checkpointData = {
            {
                {  1337, uint256S("0xf6718e7cd32a60061f8242a4043fd0aea04a7d41218994b29f9bde7fdecc5ee6")},
                {  3333, uint256S("0xdb8bc36fe4575d4175920ad09320ab539095c8a75e8fcc58af223365cf726fc4")},
                {  6969, uint256S("0x64d63a2a9cb4b7ff8ebbbcce1b003215e015adef4a0f3901c40c80432de1c342")},
                {  10847, uint256S("0xea203105addb8bdf95d7c03e844b1753afb8a72a31662aa2ea0bbcca6ec1e189")},
                {  20469, uint256S("0xf95e323399592516d9b505e35cf47934b4965c108e75e9595bae94c4135527d0")},
                {  31593, uint256S("0x265ebefeba471cb65090d50f75d9877446dc01686875145f3624fa30be73835c")},
                {  40000, uint256S("0xf21a7c923218278d5bd39246e580aa9803142e1e02d4ecdc105d49cb20b5b3c3")},
                {  50000, uint256S("0x354b3fcd273a1c08f368bbf1a7ca17510793393bb7df4e1a4c840cfc07c4af3e")},
                {  60000, uint256S("0x1a426ad7f8319e1e0bff08f69071e65c3bffdfc76e3c8309e68f051cfe5457ab")},
                {  70000, uint256S("0x4500c1a66f12043eb8cc600d8f2e6223f95348456bab8b48fb40e54bd1f5abbb")},
                {  80186, uint256S("0x0dff661a60a1cdc697b5b3ff785c2d47a03f27774927c04329a637db7de29725")}, // COINBASE_MATURITY CHANGE
                {  80430, uint256S("0xdc6c52f4fb74c6226ba10031893e4a58afc2f5a045e90b7437f92d6f95bba41c")},
                {  82580, uint256S("0x2fd61b8fc052d044eb7d35c905e07d5786237688a698636dfe209ecd541e18af")},
            }
        };

        chainTxData = ChainTxData{
            // Data as of block fb5783255c10026aeaaaa582e5706d0c3a6fe53ae8fdecd7fe2d98976c2d4ac4 (height 52000).
           1533233077, // * UNIX timestamp of last known number of transactions
           80448,  // * total number of transactions between genesis and that timestamp
                   //   (the tx=... number in the SetBestChain debug.log lines)
           0.015     // * estimated number of transactions per second after that timestamp
        };
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.BIP16Height = 0; // always enforce P2SH BIP16 on regtest
        consensus.BIP34Height = 76;
        consensus.BIP34Hash = uint256S("8075c771ed8b495ffd943980a95f702ab34fce3c8c54e379548bda33cc8c0573");
        consensus.BIP65Height = 76; // 8075c771ed8b495ffd943980a95f702ab34fce3c8c54e379548bda33cc8c0573
        consensus.BIP66Height = 76; // 8075c771ed8b495ffd943980a95f702ab34fce3c8c54e379548bda33cc8c0573
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // 3.5 days
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1483228800; // January 1, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1517356801; // January 31st, 2018

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1483228800; // January 1, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1526867723; // January 31st, 2018

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000100010");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x6ce8bfdfb510688eac56bb79b0df193ae8e5f76490697269ff45072680867101"); //343833

        pchMessageStart[0] = 0xff;
        pchMessageStart[1] = 0xd5;
        pchMessageStart[2] = 0xc1;
        pchMessageStart[3] = 0xf5;
        nDefaultPort = 19665;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1527431244, 3280353, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x6ce8bfdfb510688eac56bb79b0df193ae8e5f76490697269ff45072680867101"));
        assert(genesis.hashMerkleRoot == uint256S("0x7295c791d735679ec7578c74881458876ac8e696d6683311b482862306879544"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("localhost");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "trog";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

        checkpointData = (CCheckpointData) {
            {
                {  0, uint256S("0x8e37fbf9a820b9b7fd55b1e5ebfbd8370134523c5c09714c04dc26afdf2f65eb")},
//                {2056, uint256S("17748a31ba97afdc9a4f86837a39d287e3e7c7290a08a1d816c5969c78a83289")},
            }
        };

        chainTxData = ChainTxData{
            // Data as of block 0 (height 0)
            0,
            0,
            0
        };

    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP16Height = 0; // always enforce P2SH BIP16 on regtest
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xfe;
        pchMessageStart[1] = 0xbc;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xdd;
        nDefaultPort = 19774;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1538348721, 1414558, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0xa058f45da46f31c4e4cb29e57b00f66f9438821c991bcd05dbd476b119fc8119"));
        assert(genesis.hashMerkleRoot == uint256S("0x7295c791d735679ec7578c74881458876ac8e696d6683311b482862306879544"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true; 

        checkpointData = {
           {
                {  0, uint256S("0xa058f45da46f31c4e4cb29e57b00f66f9438821c991bcd05dbd476b119fc8119")},
           }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "rrog";
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
