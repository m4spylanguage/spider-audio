#!/bin/sh
set -e

echo "🕷️ Installing Spider Audio..."

# Compile spider-pactl
make clean
make

# Install binaries and configs
echo "Installing spider-pactl to /usr/local/bin..."
sudo cp -f spider-pactl /usr/local/bin/spider-pactl
sudo ln -sf /usr/local/bin/spider-pactl /usr/local/bin/pactl
sudo ln -sf /usr/local/bin/spider-pactl /usr/bin/pactl 2>/dev/null || true

echo "Installing /etc/asound.conf..."
sudo cp -f conf/asound.conf /etc/asound.conf

if [ -d /etc/init.d ]; then
    echo "Installing OpenRC service /etc/init.d/spider-audio..."
    sudo cp -f init/spider-audio /etc/init.d/spider-audio
    sudo chmod 755 /etc/init.d/spider-audio
    if command -v rc-update >/dev/null 2>&1; then
        sudo rc-update add spider-audio boot 2>/dev/null || true
    fi
fi

echo "Setting audio device permissions..."
sudo chmod -R 0666 /dev/snd/* 2>/dev/null || true

echo "✅ Spider Audio successfully installed!"
