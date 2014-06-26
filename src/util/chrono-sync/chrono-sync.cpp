/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2014 Regents of the University of California.
 * @author: Jeff Thompson <jefft0@remap.ucla.edu>
 * Derived from ChronoChat-js by Qiuhan Ding and Wentao Shang.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the additional exemption that
 * compiling, linking, and/or using OpenSSL is allowed.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * A copy of the GNU General Public License is in the file COPYING.
 */

#include "../logging.hpp"
#include "sync-state.pb.h"
#include "chrono-sync.hpp"

using namespace std;

namespace ndn {

int
ChronoSync::logfind(const std::string& digest)
{
  for (size_t i = 0; i < digest_log_.size(); ++i) {
    if (digest == digest_log_[i]->digest_)
      return i;
  }

  return -1;
};

void
ChronoSync::onInterest
  (const ptr_lib::shared_ptr<const Name>& prefix,
   const ptr_lib::shared_ptr<const Interest>& inst, Transport& transport,
   uint64_t registerPrefixId)
{
  // Search if the digest already exists in the digest log.
   //_LOG_DEBUG("Sync Interest received in callback.");
   //_LOG_DEBUG(inst->getName().toUri());

  string syncdigest = inst->getName().get(4).toEscapedString();
  if (inst->getName().size() == 6)
    syncdigest = inst->getName().get(5).toEscapedString();
   //_LOG_DEBUG("syncdigest: " + syncdigest);
  if (inst->getName().size() == 6 || syncdigest == "00")
    // Recovery interest or newcomer interest.
    processRecoveryInst(*inst, syncdigest, transport);
  else {
    if (syncdigest != digest_tree_.root_) {
      size_t index = logfind(syncdigest);
      if (index == -1) {
        ChronoSync& self = *this;
#if 0 // TODO: Set the timeout without using usleep.
        //Wait 2 seconds to see whether there is any data packet coming back
        setTimeout(function(){self.judgeRecovery(syncdigest, transport);},2000);
        //_LOG_DEBUG("set timer recover");
#else
        //_LOG_DEBUG("set timer recover");
        usleep(1000000);
        judgeRecovery(syncdigest, transport);
#endif
      }
      else
        // common interest processing
        processSyncInst(index, syncdigest, transport);
    }
  }
}

void
ChronoSync::onData
  (const ptr_lib::shared_ptr<const Interest>& inst,
   const ptr_lib::shared_ptr<Data>& co)
{
  //_LOG_DEBUG("Sync ContentObject received in callback");
  //_LOG_DEBUG("name: " + co->getName().toUri());
  Sync::SyncStateMsg content_t;
  content_t.ParseFromArray(co->getContent().buf(), co->getContent().size());
  const google::protobuf::RepeatedPtrField<Sync::SyncState >&content = content_t.ss();
  if (digest_tree_.root_ == "00") {
    flag_ = 1;
    //processing initial sync data
    initialOndata(content);
  }
  else {
    digest_tree_.update(content, *this);
    if (logfind(digest_tree_.root_) == -1)
      addDigestLogEntry(digest_tree_.root_, content_t.ss());
    if (inst->getName().size() == 6)
      flag_ = 1;
    else
      flag_ = 0;
  }

  // Send the Chat Interest to fetch the data. This can be changed to another
  //   function in other applications.
  sendChatInterest_(content);
  Name n(prefix_ + chatroom_ + "/" + digest_tree_.root_);
  Interest interest(n);
  interest.setInterestLifetimeMilliseconds(sync_lifetime_);
  face_.expressInterest
    (interest, bind(&ChronoSync::onData, this, _1, _2),
     bind(&ChronoSync::syncTimeout, this, _1));
  //_LOG_DEBUG("Syncinterest expressed:");
  //_LOG_DEBUG(n.toUri());
}

void
ChronoSync::processRecoveryInst
  (const Interest& inst, const string& syncdigest, Transport& transport)
{
  //_LOG_DEBUG("processRecoveryInst");
  if (logfind(syncdigest) != -1) {
    Sync::SyncStateMsg content_t;
    for (size_t i = 0; i < digest_tree_.digestnode_.size(); ++i) {
      Sync::SyncState* content = content_t.add_ss();
      content->set_name(digest_tree_.digestnode_[i]->prefix_name_);
      content->set_type(Sync::SyncState_ActionType_UPDATE);
      content->mutable_seqno()->set_seq(digest_tree_.digestnode_[i]->seqno_seq_);
      content->mutable_seqno()->set_session(digest_tree_.digestnode_[i]->seqno_session_);
    }

    if (content_t.ss_size() != 0) {
      ptr_lib::shared_ptr<vector<uint8_t> > array(new vector<uint8_t>(content_t.ByteSize()));
      content_t.SerializeToArray(&array->front(), array->size());
      Data co(inst.getName());
      co.setContent(Blob(array, false));
      keyChain_.sign(co, certificateName_);
      try {
        transport.send(*co.wireEncode());
        //_LOG_DEBUG("send recovery data back");
        //_LOG_DEBUG(inst.getName().toUri());
      }
      catch (std::exception& e) {
        _LOG_DEBUG(e.what());
      }
    }
  }
}

void
ChronoSync::processSyncInst
  (int index, const string& syncdigest_t, Transport& transport)
{
  vector<string> data_name;
  vector<int> data_seq;
  vector<int> data_ses;
  for (size_t j = index + 1; j < digest_log_.size(); ++j) {
    google::protobuf::RepeatedPtrField<Sync::SyncState>& temp =
      *digest_log_[j]->data_;
    for (size_t i = 0; i < temp.size(); ++i) {
      if (temp.Get(i).type() != 0)
        continue;

      if (digest_tree_.find(temp.Get(i).name(), temp.Get(i).seqno().session()) != -1) {
        int n = -1;
        for (size_t k = 0; k < data_name.size(); ++k) {
          if (data_name[k] == temp.Get(i).name()) {
            n = k;
            break;
          }
        }
        if (n == -1) {
          data_name.push_back(temp.Get(i).name());
          data_seq.push_back(temp.Get(i).seqno().seq());
          data_ses.push_back(temp.Get(i).seqno().session());
        }
        else {
          data_seq[n] = temp.Get(i).seqno().seq();
          data_ses[n] = temp.Get(i).seqno().session();
        }
      }
    }
  }

  Sync::SyncStateMsg content_t;
  for (size_t i = 0; i < data_name.size(); ++i) {
    Sync::SyncState* content = content_t.add_ss();
    content->set_name(data_name[i]);
    content->set_type(Sync::SyncState_ActionType_UPDATE);
    content->mutable_seqno()->set_seq(data_seq[i]);
    content->mutable_seqno()->set_session(data_ses[i]);
  }

  if (content_t.ss_size() != 0) {
    Name n(prefix_ + chatroom_ + "/" + syncdigest_t);
    ptr_lib::shared_ptr<vector<uint8_t> > array(new vector<uint8_t>(content_t.ByteSize()));
    content_t.SerializeToArray(&array->front(), array->size());
    Data co(n);
    co.setContent(Blob(array, false));
    keyChain_.sign(co, certificateName_);
    try {
      transport.send(*co.wireEncode());
      //_LOG_DEBUG("Sync Data send");
      //_LOG_DEBUG(n.toUri());
    } catch (std::exception& e) {
      _LOG_DEBUG(e.what());
    }
  }
}

void
ChronoSync::sendRecovery(const string& syncdigest_t)
{
  //_LOG_DEBUG("unknown digest: ");
  Name n(prefix_ + chatroom_ + "/recovery/" + syncdigest_t);
  Interest interest(n);
  interest.setInterestLifetimeMilliseconds(sync_lifetime_);
  face_.expressInterest
    (interest, bind(&ChronoSync::onData, this, _1, _2),
     bind(&ChronoSync::syncTimeout, this, _1));
  _LOG_DEBUG("Recovery Syncinterest expressed:");
  _LOG_DEBUG(n.toUri());
}

void
ChronoSync::judgeRecovery(const string& syncdigest_t, Transport& transport)
{
  int index2 = logfind(syncdigest_t);
  if (index2 != -1) {
    if (syncdigest_t != digest_tree_.root_)
      processSyncInst(index2, syncdigest_t, transport);
  }
  else
    sendRecovery(syncdigest_t);
}

void
ChronoSync::syncTimeout(const ptr_lib::shared_ptr<const Interest>& interest)
{
   //_LOG_DEBUG("Sync Interest time out.");
   //_LOG_DEBUG("Sync Interest name: " + interest->getName().toUri());
  string component = interest->getName().get(4).toEscapedString();
  if (component == digest_tree_.root_) {
    Name n(interest->getName());
    Interest retryInterest(interest->getName());
    retryInterest.setInterestLifetimeMilliseconds(sync_lifetime_);
    face_.expressInterest
      (retryInterest, bind(&ChronoSync::onData, this, _1, _2),
       bind(&ChronoSync::syncTimeout, this, _1));
     //_LOG_DEBUG("Syncinterest expressed:");
     //_LOG_DEBUG(n.toUri());
  }
}

void
ChronoSync::initialOndata
  (const google::protobuf::RepeatedPtrField<Sync::SyncState >& content)
{
  // The user is a new comer and receive data of all other people in the group.
  digest_tree_.update(content, *this);
  if (logfind(digest_tree_.root_) == -1)
    addDigestLogEntry(digest_tree_.root_, content);
  string digest_t = digest_tree_.root_;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content.Get(i).name() == chat_prefix_ && content.Get(i).seqno().session() == session_) {
      // If the user was an old comer, after add the static log he needs to increase his seqno by 1.
      Sync::SyncStateMsg content_t;
      Sync::SyncState* content2 = content_t.add_ss();
      content2->set_name(chat_prefix_);
      content2->set_type(Sync::SyncState_ActionType_UPDATE);
      content2->mutable_seqno()->set_seq(content.Get(i).seqno().seq() + 1);
      content2->mutable_seqno()->set_session(session_);
      digest_tree_.update(content_t.ss(), *this);
      if (logfind(digest_tree_.root_) == -1) {
        addDigestLogEntry(digest_tree_.root_, content_t.ss());
        initialChat_(usrseq_);
      }
    }
  }

  Sync::SyncStateMsg content2_t;
  if (usrseq_ >= 0) {
    // Send the data packet with the new seqno back.
    Sync::SyncState* content2 = content2_t.add_ss();
    content2->set_name(chat_prefix_);
    content2->set_type(Sync::SyncState_ActionType_UPDATE);
    content2->mutable_seqno()->set_seq(usrseq_);
    content2->mutable_seqno()->set_session(session_);
  }
  else {
    Sync::SyncState* content2 = content2_t.add_ss();
    content2->set_name(chat_prefix_);
    content2->set_type(Sync::SyncState_ActionType_UPDATE);
    content2->mutable_seqno()->set_seq(0);
    content2->mutable_seqno()->set_session(session_);
  }

  Name n(prefix_ + chatroom_ + "/" + digest_t);
  ptr_lib::shared_ptr<vector<uint8_t> > array(new vector<uint8_t>(content2_t.ByteSize()));
  content2_t.SerializeToArray(&array->front(), array->size());
  Data co(n);
  co.setContent(Blob(array));
  keyChain_.sign(co, certificateName_);
  //_LOG_DEBUG("initial update data sending back");
  //_LOG_DEBUG(n.toUri());
  try {
    pokeData(co);
  }
  catch (std::exception& e) {
    _LOG_DEBUG(e.what());
  }

  if (digest_tree_.find(chat_prefix_, session_) == -1) {
    // the user hasn't put himself in the digest tree.
    //_LOG_DEBUG("initial state")
    ++usrseq_;
    Sync::SyncStateMsg content_t;
    Sync::SyncState* content2 = content_t.add_ss();
    content2->set_name(chat_prefix_);
    content2->set_type(Sync::SyncState_ActionType_UPDATE);
    content2->mutable_seqno()->set_seq(usrseq_);
    content2->mutable_seqno()->set_session(session_);
    digest_tree_.update(content_t.ss(), *this);

    if (logfind(digest_tree_.root_) == -1) {
      addDigestLogEntry(digest_tree_.root_, content_t.ss());
      initialChat_(usrseq_);
    }
  }
}

void
ChronoSync::initialTimeOut(const ptr_lib::shared_ptr<const Interest>& interest)
{
  //_LOG_DEBUG("initial sync timeout");
  //_LOG_DEBUG("no other people");
  digest_tree_.initial(*this);
  ++usrseq_;
  initialChat_(usrseq_);
  Sync::SyncStateMsg content_t;
  Sync::SyncState* content = content_t.add_ss();
  content->set_name(chat_prefix_);
  content->set_type(Sync::SyncState_ActionType_UPDATE);
  content->mutable_seqno()->set_seq(usrseq_);
  content->mutable_seqno()->set_session(session_);

  addDigestLogEntry(digest_tree_.root_, content_t.ss());
  Name n(prefix_ + chatroom_ + "/" + digest_tree_.root_);
  Interest retryInterest(n);
  retryInterest.setInterestLifetimeMilliseconds(sync_lifetime_);
  face_.expressInterest
    (retryInterest, bind(&ChronoSync::onData, this, _1, _2),
     bind(&ChronoSync::syncTimeout, this, _1));
  //_LOG_DEBUG("Syncinterest expressed:");
  //_LOG_DEBUG(n.toUri());
}

ChronoSync::DigestLogEntry::DigestLogEntry
  (const std::string& digest,
   const google::protobuf::RepeatedPtrField<Sync::SyncState>& data)
  : digest_(digest),
   data_(new google::protobuf::RepeatedPtrField<Sync::SyncState>(data))
{
}

}
