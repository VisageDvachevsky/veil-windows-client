#!/usr/bin/env bash
#
# VEIL Client Setup Wizard
#
# This interactive script helps inexperienced users configure their VEIL VPN client
# by guiding them through the required configuration steps.
#
# Usage:
#   sudo ./setup-wizard.sh
#

set -e
set -u
set -o pipefail

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
BOLD='\033[1m'
NC='\033[0m'

# Configuration paths
CONFIG_DIR="/etc/veil"
CONFIG_FILE="$CONFIG_DIR/client.conf"
KEY_FILE="$CONFIG_DIR/client.key"
SEED_FILE="$CONFIG_DIR/obfuscation.seed"

# Functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Display banner
show_banner() {
    clear
    echo ""
    echo -e "${CYAN}╔════════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║                                                                        ║${NC}"
    echo -e "${CYAN}║                    VEIL VPN Client Setup Wizard                        ║${NC}"
    echo -e "${CYAN}║                                                                        ║${NC}"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${BOLD}Welcome! This wizard will help you configure your VEIL VPN client.${NC}"
    echo ""
}

# Check current status
check_status() {
    echo -e "${MAGENTA}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${MAGENTA}  Checking Current Configuration Status${NC}"
    echo -e "${MAGENTA}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo ""

    local all_ok=true

    # Check if VEIL client is installed
    if command -v veil-client &> /dev/null; then
        echo -e "  ${GREEN}✓${NC} VEIL client is installed"
    else
        echo -e "  ${RED}✗${NC} VEIL client is NOT installed"
        log_error "Please install VEIL client first using install_client.sh"
        exit 1
    fi

    # Check config directory
    if [[ -d "$CONFIG_DIR" ]]; then
        echo -e "  ${GREEN}✓${NC} Configuration directory exists: $CONFIG_DIR"
    else
        echo -e "  ${YELLOW}!${NC} Configuration directory missing (will create)"
        mkdir -p "$CONFIG_DIR"
        chmod 700 "$CONFIG_DIR"
    fi

    # Check configuration file
    if [[ -f "$CONFIG_FILE" ]]; then
        echo -e "  ${GREEN}✓${NC} Configuration file exists: $CONFIG_FILE"
        HAS_CONFIG=true
    else
        echo -e "  ${YELLOW}!${NC} Configuration file missing (will create)"
        HAS_CONFIG=false
        all_ok=false
    fi

    # Check key file
    if [[ -f "$KEY_FILE" ]]; then
        echo -e "  ${GREEN}✓${NC} Client key file exists: $KEY_FILE"
        HAS_KEY=true
    else
        echo -e "  ${RED}✗${NC} Client key file MISSING: $KEY_FILE"
        HAS_KEY=false
        all_ok=false
    fi

    # Check obfuscation seed
    if [[ -f "$SEED_FILE" ]]; then
        echo -e "  ${GREEN}✓${NC} Obfuscation seed exists: $SEED_FILE"
        HAS_SEED=true
    else
        echo -e "  ${RED}✗${NC} Obfuscation seed MISSING: $SEED_FILE"
        HAS_SEED=false
        all_ok=false
    fi

    echo ""

    if $all_ok; then
        log_success "All required files are present!"
        echo ""
        read -p "Do you want to reconfigure? [y/N]: " reconfigure
        if [[ ! "$reconfigure" =~ ^[Yy]$ ]]; then
            echo ""
            log_info "Configuration looks good. You're ready to connect!"
            show_management_commands
            exit 0
        fi
    fi
}

# Explain what's needed
explain_requirements() {
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}  What You Need to Connect to VEIL VPN${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "To successfully connect to a VEIL VPN server, you need:"
    echo ""
    echo "  1. ${BOLD}Server Information${NC}"
    echo "     • Server IP address or hostname"
    echo "     • Server port (usually 4433)"
    echo ""
    echo "  2. ${BOLD}Cryptographic Key Files${NC} (from server administrator)"
    echo "     • ${BOLD}client.key${NC} - Pre-shared encryption key (32 bytes)"
    echo "     • ${BOLD}obfuscation.seed${NC} - Traffic obfuscation seed (32 bytes)"
    echo ""
    echo -e "${CYAN}Important:${NC} The key files MUST be obtained from your VEIL server"
    echo "administrator. They are cryptographic secrets that enable secure connection."
    echo ""
    echo -e "${YELLOW}⚠  Never share these files via insecure channels (email, chat, etc.)${NC}"
    echo ""
    read -p "Press Enter to continue..."
    echo ""
}

# Configure server settings
configure_server() {
    echo -e "${MAGENTA}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${MAGENTA}  Step 1: Server Configuration${NC}"
    echo -e "${MAGENTA}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo ""

    # Read current config if exists
    local current_server=""
    local current_port="4433"
    if [[ -f "$CONFIG_FILE" ]]; then
        current_server=$(grep "^server_address" "$CONFIG_FILE" 2>/dev/null | cut -d'=' -f2 | xargs || echo "")
        current_port=$(grep "^server_port" "$CONFIG_FILE" 2>/dev/null | cut -d'=' -f2 | xargs || echo "4433")
    fi

    # Ask for server address
    if [[ -n "$current_server" ]]; then
        read -p "Enter server IP or hostname [$current_server]: " SERVER_ADDRESS
        SERVER_ADDRESS="${SERVER_ADDRESS:-$current_server}"
    else
        read -p "Enter server IP or hostname: " SERVER_ADDRESS
        while [[ -z "$SERVER_ADDRESS" ]]; do
            echo -e "${RED}Server address cannot be empty!${NC}"
            read -p "Enter server IP or hostname: " SERVER_ADDRESS
        done
    fi

    # Ask for port
    read -p "Enter server port [$current_port]: " SERVER_PORT
    SERVER_PORT="${SERVER_PORT:-$current_port}"

    # Validate port
    if ! [[ "$SERVER_PORT" =~ ^[0-9]+$ ]] || [ "$SERVER_PORT" -lt 1 ] || [ "$SERVER_PORT" -gt 65535 ]; then
        log_warn "Invalid port number, using default: 4433"
        SERVER_PORT=4433
    fi

    echo ""
    log_success "Server configured: ${SERVER_ADDRESS}:${SERVER_PORT}"
    echo ""
}

# Guide for obtaining key files
guide_key_files() {
    echo -e "${MAGENTA}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${MAGENTA}  Step 2: Obtain Cryptographic Key Files${NC}"
    echo -e "${MAGENTA}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo ""

    echo -e "${BOLD}You need two key files from your VEIL server:${NC}"
    echo ""
    echo "  1. ${BOLD}$KEY_FILE${NC}"
    echo "     (copy from server: /etc/veil/server.key)"
    echo ""
    echo "  2. ${BOLD}$SEED_FILE${NC}"
    echo "     (copy from server: /etc/veil/obfuscation.seed)"
    echo ""

    if $HAS_KEY && $HAS_SEED; then
        log_info "Key files are already present. Skipping..."
        return
    fi

    echo -e "${CYAN}How to transfer files from your server:${NC}"
    echo ""
    echo "Option A - Using SCP (secure copy):"
    echo "  ${BOLD}# On this client machine, run:${NC}"
    echo "  scp user@${SERVER_ADDRESS}:/etc/veil/server.key $KEY_FILE"
    echo "  scp user@${SERVER_ADDRESS}:/etc/veil/obfuscation.seed $SEED_FILE"
    echo "  chmod 600 $KEY_FILE $SEED_FILE"
    echo ""
    echo "Option B - From the server (if you have access):"
    echo "  ${BOLD}# On the server, run:${NC}"
    echo "  scp /etc/veil/server.key user@<THIS_CLIENT_IP>:~/client.key"
    echo "  scp /etc/veil/obfuscation.seed user@<THIS_CLIENT_IP>:~/obfuscation.seed"
    echo ""
    echo "  ${BOLD}# Then on this client, run:${NC}"
    echo "  sudo mv ~/client.key $KEY_FILE"
    echo "  sudo mv ~/obfuscation.seed $SEED_FILE"
    echo "  sudo chmod 600 $KEY_FILE $SEED_FILE"
    echo ""

    read -p "Press Enter when you have transferred the key files..."
    echo ""

    # Verify files were transferred
    if [[ ! -f "$KEY_FILE" ]]; then
        log_error "Key file still missing: $KEY_FILE"
        echo ""
        echo "Please transfer the file and run this wizard again."
        exit 1
    fi

    if [[ ! -f "$SEED_FILE" ]]; then
        log_error "Obfuscation seed still missing: $SEED_FILE"
        echo ""
        echo "Please transfer the file and run this wizard again."
        exit 1
    fi

    # Set correct permissions
    chmod 600 "$KEY_FILE" "$SEED_FILE"

    log_success "Key files are in place and secured!"
    echo ""
}

# Create configuration file
create_config() {
    echo -e "${MAGENTA}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${MAGENTA}  Step 3: Creating Configuration File${NC}"
    echo -e "${MAGENTA}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo ""

    if [[ -f "$CONFIG_FILE" ]]; then
        log_info "Backing up existing configuration..."
        cp "$CONFIG_FILE" "$CONFIG_FILE.backup.$(date +%s)"
    fi

    cat > "$CONFIG_FILE" <<EOF
# VEIL Client Configuration
# Generated by setup wizard on $(date)

[client]
# Server connection settings
server_address = ${SERVER_ADDRESS}
server_port = ${SERVER_PORT}

# Auto-reconnect settings
auto_reconnect = true
reconnect_delay_sec = 5
max_reconnect_attempts = 10

[tun]
device_name = veil0
mtu = 1400

[crypto]
preshared_key_file = $KEY_FILE

[obfuscation]
profile_seed_file = $SEED_FILE

[logging]
level = info
log_file = /var/log/veil/veil-client.log
EOF

    chmod 600 "$CONFIG_FILE"

    log_success "Configuration file created: $CONFIG_FILE"
    echo ""
}

# Show management commands
show_management_commands() {
    echo -e "${GREEN}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  Setup Complete! Your VEIL Client is Ready${NC}"
    echo -e "${GREEN}═══════════════════════════════════════════════════════════════════════════${NC}"
    echo ""

    echo -e "${BOLD}Quick Start Commands:${NC}"
    echo ""
    echo "  ${BOLD}Test connection (foreground):${NC}"
    echo "    sudo veil-client -s ${SERVER_ADDRESS} -p ${SERVER_PORT} -k $KEY_FILE"
    echo ""
    echo "  ${BOLD}Start as a service:${NC}"
    echo "    sudo systemctl start veil-client"
    echo ""
    echo "  ${BOLD}Check connection status:${NC}"
    echo "    sudo systemctl status veil-client"
    echo ""
    echo "  ${BOLD}View live logs:${NC}"
    echo "    sudo journalctl -u veil-client -f"
    echo ""
    echo "  ${BOLD}Enable auto-start on boot:${NC}"
    echo "    sudo systemctl enable veil-client"
    echo ""
    echo "  ${BOLD}Stop the client:${NC}"
    echo "    sudo systemctl stop veil-client"
    echo ""

    if command -v veil-client-gui &> /dev/null; then
        echo -e "${CYAN}GUI Available:${NC}"
        echo "    veil-client-gui"
        echo ""
    fi

    echo -e "${YELLOW}Troubleshooting:${NC}"
    echo "  • If connection fails, check logs: sudo journalctl -u veil-client"
    echo "  • Verify key files match the server"
    echo "  • Ensure server address and port are correct"
    echo "  • Check firewall allows UDP traffic on port ${SERVER_PORT}"
    echo ""
}

# Main wizard flow
main() {
    check_root
    show_banner
    check_status
    explain_requirements
    configure_server
    guide_key_files
    create_config
    show_management_commands

    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║           Thank you for using VEIL VPN! Happy secure browsing!         ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

# Run the wizard
main "$@"
