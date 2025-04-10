#include "boxes/new_secret_chat_box.h"
#include "new_secret_chat_box.h"
#include "lang/lang_keys.h"
#include "ui/widgets/checkbox.h"
#include "styles/style_layers.h"
#include "main/main_session.h"
#include "apiwrap.h"
#include "calls/calls_instance.h"
#include "calls/calls_call.h"
#include "core/application.h"
#include "base/bytes.h"
#include "base/openssl_help.h"
#include "data/data_user.h"
#include "mtproto/mtproto_dh_utils.h"

#include <memory>
#include <random>
#include <climits>
#include <ctime>
#include <chrono>
#include <thread>

#include <iostream>
#include <sstream>
#include <vector>

using random_bytes_engine = std::independent_bits_engine<
    std::default_random_engine, CHAR_BIT, unsigned char>;
using namespace openssl;

NewSecretChatBox::NewSecretChatBox(
    QWidget*,
    not_null<Main::Session*> session,
    not_null<PeerData*>peer):
  _session(session),
  _peer(peer),
  useCustomEncryption(false),
  _dhConfig(std::make_unique<Calls::DhConfig>())
  {}

std::string toHex(bytes::vector v) {
  std::ostringstream s;
  for (auto c: v)
    s << std::hex << int(c);
  return s.str();
}

void NewSecretChatBox::prepare() {
  setTitle(tr::lng_create_secret_chat_with());
  
  const auto encTypeBox = Ui::CreateChild<Ui::Checkbox>(
      this,
      tr::lng_secret_chat_qenc()
    );
  encTypeBox->addClickHandler([&] { 
      useCustomEncryption = !useCustomEncryption;
    });

  addLeftButton(tr::lng_close(), [=] { closeBox(); });
  addButton(tr::lng_create_group_create(), [=] { submit(); });
  setDimensions(
      st::boxWidth,
      encTypeBox->height());
  encTypeBox->moveToLeft(st::boxPadding.left(), 0);
}


bytes::const_span NewSecretChatBox::updateDhConfig(
		const MTPmessages_DhConfig &data) {
	const auto validRandom = [](const QByteArray & random) {
		if (random.size() != MTP::ModExpFirst::kRandomPowerSize) {
			return false;
		}
		return true;
	};
	return data.match([&](const MTPDmessages_dhConfig &data)
	-> bytes::const_span {
		auto primeBytes = bytes::make_vector(data.vp().v);
		if (!MTP::IsPrimeAndGood(primeBytes, data.vg().v)) {
			LOG(("API Error: bad p/g received in dhConfig."));
			return {};
		} else if (!validRandom(data.vrandom().v)) {
			return {};
		}
    std::cout << "primeBytes: ";
    for (int i = 0; i < 10; i++)
      std::cout << int(primeBytes[i]) << ",";
    std::cout << std::endl;
		_dhConfig->g = data.vg().v;
		_dhConfig->p = std::move(primeBytes);
		_dhConfig->version = data.vversion().v;
		return bytes::make_span(data.vrandom().v);
	}, [&](const MTPDmessages_dhConfigNotModified &data)
	-> bytes::const_span {
		if (!_dhConfig->g || _dhConfig->p.empty()) {
			LOG(("API Error: dhConfigNotModified on zero version."));
			return {};
		} else if (!validRandom(data.vrandom().v)) {
			return {};
		}
		return bytes::make_span(data.vrandom().v);
	});
}

void NewSecretChatBox::dhStart() {
  _session->api().request(MTPmessages_GetDhConfig(
		MTP_int(_dhConfig->version),
		MTP_int(MTP::ModExpFirst::kRandomPowerSize)
	)).done([=](const MTPmessages_DhConfig &result) {
		updateDhConfig(result);
    dhSendGA();
	}).fail([=] (const MTP::Error &error) {
    std::cout << "refreshDhConfig() failed!" << std::endl;
    std::cout << "type :"
              << error.type().toUtf8().constData()
              << std::endl;
	}).send();
}

void NewSecretChatBox::dhSendGA() {
  random_bytes_engine rbe; 
  rbe.seed(std::time(0));
  std::uniform_int_distribution<int> int_dist;
  std::vector<unsigned char> data(2048);
  std::generate(begin(data), end(data), std::ref(rbe));
  const unsigned char* pData = &data[0];
  // let's hope that it actually copies :)
  this->A = BigNum(bytes::make_span(pData, 2048));
  std::cout << "generated a: ";
  for (int i = 0; i < 8; i++)
    std::cout << int(data[i]) << ",";
  std::cout << "..." << std::endl;
  const auto g_a = BigNum::ModExp(
      BigNum(_dhConfig->g),
      this->A,
      BigNum(_dhConfig->p)
    );
  const auto random_id = int_dist(rbe);
  std::cout << "computed g ^ a mod p..." << std::endl;
  std::cout << "requestEncryption()..." << std::endl;
  std::cout << "  random id: " << random_id << std::endl;
  _session->api().request(
      MTPmessages_RequestEncryption(
        _peer->asUser()->inputUser,
        MTP_int(random_id),
        MTP_bytes(g_a.getBytes())
      )
    ).done([&] (const MTPencryptedChat &result) {
      std::cout << "  success!" << std::endl;
    }).fail([&] (const MTP::Error &error) {
      std::cout << "  fail!" << std::endl;
      std::cout << "  type: " 
                << error.type().toUtf8().constData()
                << std::endl;
      std::cout << "g: " << _dhConfig->g << std::endl;
      std::cout << "p: " 
        << toHex(BigNum(_dhConfig->p).getBytes())
        << std::endl;
      std::cout << "g_a: " 
        << toHex(g_a.getBytes())
        << std::endl;
    }).send();

}

void NewSecretChatBox::submit() {
  dhStart();
}
