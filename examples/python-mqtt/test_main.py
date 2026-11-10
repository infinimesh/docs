
import importlib.util
from pathlib import Path
from unittest.mock import MagicMock, patch

import paho.mqtt.client as mqtt


spec = importlib.util.spec_from_file_location(
    "mqtt_example",
    Path(__file__).with_name("main.py"),
)
app = importlib.util.module_from_spec(spec)
spec.loader.exec_module(app)


def test_mqtt_topic():
    assert app.TOPIC_REPORTED == (
        f"devices/{app.DEVICE_ID}/state/reported"
    )


def test_mqtt_port():
    assert app.MQTT_PORT == 8883


def test_successful_connection():
    app.connected.clear()
    app.connection_error.clear()

    reason_code = MagicMock()
    reason_code.is_failure = False

    app.on_connect(None, None, None, reason_code, None)

    assert app.connected.is_set()
    assert app.connection_error == []


def test_failed_connection():
    app.connected.clear()
    app.connection_error.clear()

    reason_code = MagicMock()
    reason_code.is_failure = True

    app.on_connect(None, None, None, reason_code, None)

    assert app.connected.is_set()
    assert len(app.connection_error) == 1


def test_client_uses_tls():
    with patch.object(app.ssl, "create_default_context") as mock_ssl:
        context = MagicMock()
        mock_ssl.return_value = context

        with patch.object(app.mqtt, "Client") as mock_client:
            client = app.make_client()

            context.load_cert_chain.assert_called_once_with(
                app.CERT_FILE, app.KEY_FILE
            )

            assert (
                context.minimum_version
                == app.ssl.TLSVersion.TLSv1_2
            )

            mock_client.assert_called_once_with(
                callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
                client_id=app.DEVICE_ID,
            )

            client.tls_set_context.assert_called_once_with(context)
            assert client.on_connect == app.on_connect
