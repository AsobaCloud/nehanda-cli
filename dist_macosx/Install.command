#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
INSTALL_DIR="$HOME/.nehanda"

echo "Installing Nehanda CLI..."

# Copy files to user home directory
mkdir -p "$INSTALL_DIR"
cp -r "$DIR/nehanda-cli/"* "$INSTALL_DIR/"

# Install dependencies
cd "$INSTALL_DIR" && npm install --omit=dev --silent

# Create executable wrapper
cat << 'LAUNCHER' > "$INSTALL_DIR/bin/nehanda"
#!/bin/sh
exec node "$HOME/.nehanda/bin/agent.mjs" "$@"[cite: 1]
LAUNCHER
chmod +x "$INSTALL_DIR/bin/nehanda"

# Symlink to /usr/local/bin for immediate terminal availability
sudo ln -sf "$INSTALL_DIR/bin/nehanda" /usr/local/bin/nehanda

echo ""
echo "Installation complete! You can now run 'nehanda' in your terminal."
read -p "Press [Enter] to exit..."
