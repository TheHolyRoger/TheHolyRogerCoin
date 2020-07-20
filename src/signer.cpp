// Copyright (c) 2014-2015 The Darkcoin developers
// Copyright (c) 2015-2020 The Neutron Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <signer.h>
#include <init.h>
#include <util.h>
#include <utiltime.h>
#include <ui_interface.h>
#include <base58.h>
#include <validation.h>
#include <net.h>

#include <string>
#include <openssl/rand.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <algorithm>
#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost;

CCriticalSection cs_darksend;
CSporkSigner sporkSigner;

// count peers we've requested the list from
int requestedMasterNodeList = 0;
bool isMasternodeListSynced = false;

bool CSporkSigner::SetKey(std::string strSecret, std::string& errorMessage, CKey& key, CPubKey& pubkey)
{
    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(strSecret);

    if (!fGood)
    {
        errorMessage = _("Invalid private key.");
        return false;
    }

    key = vchSecret.GetKey();
    pubkey = key.GetPubKey();

    return true;
}

bool CSporkSigner::SignMessage(std::string strMessage, std::string& errorMessage, vector<unsigned char>& vchSig, CKey key)
{
    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    if (!key.Sign(ss.GetHash(), vchSig))
    {
        errorMessage = _("Signing failed.");
        return false;
    }

    return true;
}

bool CSporkSigner::VerifyMessage(CPubKey pubkey, vector<unsigned char>& vchSig, std::string strMessage, std::string& errorMessage)
{
    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

/*    CKey key;
    key.SetPubKey(pubkey);
    return key.Verify(ss.GetHash(), vchSig);*/

    CPubKey pubkey2;

    if (!pubkey2.RecoverCompact(ss.GetHash(), vchSig)) {
        errorMessage = _("Error recovering public key.");
        return false;
    }

    if (pubkey2.GetID() != pubkey.GetID())
        LogPrintf("%s -- keys don't match: %s %s\n", __func__, pubkey2.GetID().ToString(), pubkey.GetID().ToString());

    return pubkey2.GetID() == pubkey.GetID();

}

/*bool CDarksendQueue::Sign()
{
    if (!fMasterNode)
        return false;

    std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nDenom) + boost::lexical_cast<std::string>(time) +
                             boost::lexical_cast<std::string>(ready);
    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if (!darkSendSigner.SetKey(strMasterNodePrivKey, errorMessage, key2, pubkey2))
    {
        LogPrintf("CDarksendQueue():Relay - ERROR: Invalid masternodeprivkey: '%s'\n", errorMessage.c_str());
        return false;
    }

    if (!darkSendSigner.SignMessage(strMessage, errorMessage, vchSig, key2))
    {
        LogPrintf("CDarksendQueue():Relay - Sign message failed");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubkey2, vchSig, strMessage, errorMessage))
    {
        LogPrintf("CDarksendQueue():Relay - Verify message failed");
        return false;
    }

    return true;
}*/

/*void ThreadCheckDarkSend(CConnman& connman)
{

    static bool fOneThread;

    if (fOneThread)
        return;

    fOneThread = true;

    // Make this thread recognisable as the wallet flushing thread
    RenameThread("neutron-darksend");

    unsigned int nTick = 0;

    bool waitMnSyncStarted = false;
    int64_t nMnSyncWaitTime = GetTime();

    while (true)
    {
        if (!IsInitialBlockDownload())
        {
            nTick++;

            // every 5 ticks we try to send some requests (roughly 2.5 seconds)
            if (nTick % 5 == 0)
            {
                LOCK(cs_vNodes);

                if (!vNodes.empty())
                {
                    // randomly clear a node in order to get constant syncing of the lists
                    int index = GetRandInt(vNodes.size());
                    vNodes[index]->ClearFulfilledRequest("getspork");
                }

                int sentRequests = 0;

                BOOST_FOREACH(CNode* pnode, vNodes)
                {
                    if (!pnode->HasFulfilledRequest("getspork"))
                    {
                        pnode->FulfilledRequest("getspork");
                        pnode->PushMessage(NetMsgType::GETSPORKS); // get current network sporks
                        sentRequests++;
                    }

                    if (sentRequests >= MAX_REQUESTS_PER_TICK_CYCLE)
                        break;
                }
            }
        }

	MilliSleep(500); // Sleep for half a second before the next tick
    }
}*/
