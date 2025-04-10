#pragma once
#include "ui/layers/box_content.h"
#include "data/data_peer.h"
#include "base/bytes.h"
#include "base/openssl_help.h"
#include "calls/calls_call.h"
#include <memory>

namespace Core {
class App;
}; // namespace Core

namespace Main {
class Session;
}; // namespace Main

class NewSecretChatBox : public Ui::BoxContent {
public:
    NewSecretChatBox(
        QWidget*, 
        not_null<Main::Session*> session,
        not_null<PeerData*> peer);
protected:
    void prepare() override;

private:
    void submit();
    void dhStart();
    void dhSendGA();
    bytes::const_span updateDhConfig(const MTPmessages_DhConfig &data);

	  const not_null<Main::Session*> _session;
    not_null<PeerData*> _peer;
    openssl::BigNum A;
    bool useCustomEncryption;
    std::unique_ptr<Calls::DhConfig> _dhConfig;
};
