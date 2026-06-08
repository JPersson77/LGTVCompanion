"""Protocol-level tests: pairing handshake and SSAP requests against MockTV."""
from lgtv_easy.mock_tv import MockTV
from lgtv_easy.webos import WebOSClient


def _client_for(tv: MockTV) -> WebOSClient:
    c = WebOSClient("127.0.0.1")
    c._url = lambda: tv.url  # point the client at the mock's random port
    return c


def test_first_time_pairing_returns_key():
    with MockTV(require_pairing=True) as tv:
        client = _client_for(tv)
        prompts = []
        key = client.connect(client_key="", on_prompt=lambda: prompts.append(1))
        assert key == tv.known_key
        assert tv.pair_prompts == 1
        assert prompts == [1], "the UI should be told to accept on the TV once"
        client.close()


def test_reconnect_with_saved_key_skips_prompt():
    with MockTV(require_pairing=True) as tv:
        client = _client_for(tv)
        called = []
        key = client.connect(client_key=tv.known_key,
                             on_prompt=lambda: called.append(1))
        assert key == tv.known_key
        assert tv.pair_prompts == 0
        assert called == [], "no prompt when already paired"
        client.close()


def test_screen_off_and_on():
    with MockTV(require_pairing=False) as tv:
        client = _client_for(tv)
        client.connect()
        assert tv.screen_on is True
        client.screen_off()
        assert tv.screen_on is False
        client.screen_on()
        assert tv.screen_on is True
        client.close()


def test_mute_request():
    with MockTV(require_pairing=False) as tv:
        client = _client_for(tv)
        client.connect()
        client.set_mute(True)
        assert tv.muted is True
        client.set_mute(False)
        assert tv.muted is False
        client.close()


def test_power_state_response():
    with MockTV(require_pairing=False) as tv:
        client = _client_for(tv)
        client.connect()
        resp = client.get_power_state()
        assert resp["payload"]["state"] == "Active"
        client.close()
