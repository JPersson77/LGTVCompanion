"""Protocol-level tests: pairing handshake and SSAP requests against MockTV."""
from lgtv_easy.mock_tv import MockTV
from lgtv_easy.webos import WebOSClient, pair_with_fallback


class _FakeClient:
    """Records the order of ws/wss attempts; succeeds only on a chosen mode."""

    def __init__(self, succeed_on_secure):
        self.secure = False
        self.succeed_on_secure = succeed_on_secure
        self.attempts = []

    def connect(self, client_key="", on_prompt=None, prompt_timeout=0, log=None):
        self.attempts.append(self.secure)
        if self.secure == self.succeed_on_secure:
            return "CLIENT-KEY"
        raise OSError("Connection closed during handshake")

    def close(self):
        pass


def test_fallback_default_tries_plain_then_secure():
    c = _FakeClient(succeed_on_secure=True)  # only wss works (a C2-style TV)
    key = pair_with_fallback(c)
    assert key == "CLIENT-KEY"
    assert c.attempts == [False, True]  # tried 3000, then 3001


def test_fallback_prefer_secure_tries_secure_first():
    c = _FakeClient(succeed_on_secure=True)
    key = pair_with_fallback(c, prefer_secure=True)
    assert key == "CLIENT-KEY"
    assert c.attempts[0] is True  # went straight to 3001


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
