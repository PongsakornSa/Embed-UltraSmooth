// secrets.example.h - copy this file to secrets.h and fill in your values.
// secrets.h is in .gitignore: NEVER commit real passwords or tokens.
#pragma once

// Wi-Fi (use the phone hotspot on demo day)
#define WIFI_SSID "your-hotspot"
#define WIFI_PASSWORD "your-password"

// NETPIE 2020 device credentials (Device -> Key)
#define NETPIE_CLIENT_ID "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
#define NETPIE_TOKEN "your-device-token"
#define NETPIE_SECRET "your-device-secret"

// Root CA of mqtt.netpie.io in PEM format, for verified TLS (E5).
// Get it with:  openssl s_client -connect mqtt.netpie.io:8883 -showcerts
// and paste the LAST certificate of the chain (the root) here.
// Leave empty ("") to encrypt without certificate verification (weaker).
#define NETPIE_ROOT_CA ""

// Telegram bot (talk to @BotFather to create one, then get your chat id)
#define TELEGRAM_BOT_TOKEN "123456:ABC..."
#define TELEGRAM_CHAT_ID "123456789"

// Password for OTA firmware upload (E6)
#define OTA_PASSWORD "change-me"
