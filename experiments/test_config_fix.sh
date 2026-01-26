#!/usr/bin/env bash
#
# Test script to verify the configuration validation and fix logic
#

set -e

# Create a temporary directory for testing
TEST_DIR=$(mktemp -d)
echo "Test directory: $TEST_DIR"

# Create a mock config file with the issue (max_clients > IP pool size)
cat > "$TEST_DIR/server.conf" <<'EOF'
# VEIL Server Configuration

[server]
listen_address = 0.0.0.0
listen_port = 4433

[sessions]
max_clients = 256
session_timeout = 300

[ip_pool]
start = 10.8.0.2
end = 10.8.0.254

EOF

echo "Created test config with max_clients=256, IP pool 10.8.0.2-10.8.0.254"
echo ""

# Calculate IP pool size function (from install_veil.sh)
calculate_ip_pool_size() {
    local start_ip="$1"
    local end_ip="$2"

    # Convert IP address to integer
    ip_to_int() {
        local ip=$1
        local a b c d
        IFS=. read -r a b c d <<< "$ip"
        echo "$((a * 256 ** 3 + b * 256 ** 2 + c * 256 + d))"
    }

    local start_int=$(ip_to_int "$start_ip")
    local end_int=$(ip_to_int "$end_ip")

    # Calculate pool size (inclusive)
    echo "$((end_int - start_int + 1))"
}

# Extract values from config
config_file="$TEST_DIR/server.conf"
ip_pool_start=$(grep -E '^\s*start\s*=' "$config_file" | grep -v '^#' | sed 's/.*=\s*//' | tr -d ' ')
ip_pool_end=$(grep -E '^\s*end\s*=' "$config_file" | grep -v '^#' | sed 's/.*=\s*//' | tr -d ' ')
max_clients=$(grep -E '^\s*max_clients\s*=' "$config_file" | grep -v '^#' | sed 's/.*=\s*//' | tr -d ' ')

echo "Extracted from config:"
echo "  IP pool start: $ip_pool_start"
echo "  IP pool end: $ip_pool_end"
echo "  max_clients: $max_clients"
echo ""

# Calculate pool size
pool_size=$(calculate_ip_pool_size "$ip_pool_start" "$ip_pool_end")
echo "Calculated IP pool size: $pool_size"
echo ""

# Check if fix is needed
if [[ "$max_clients" -gt "$pool_size" ]]; then
    echo "ISSUE DETECTED: max_clients ($max_clients) > IP pool size ($pool_size)"
    echo "Applying fix..."

    # Create backup
    cp "$config_file" "${config_file}.backup"

    # Fix the config
    sed -i "s/^\s*max_clients\s*=.*/max_clients = ${pool_size}/" "$config_file"

    # Verify the fix
    new_max_clients=$(grep -E '^\s*max_clients\s*=' "$config_file" | grep -v '^#' | sed 's/.*=\s*//' | tr -d ' ')

    echo "Updated max_clients to: $new_max_clients"
    echo ""

    if [[ "$new_max_clients" == "$pool_size" ]]; then
        echo "✓ Fix successful!"
    else
        echo "✗ Fix failed!"
        exit 1
    fi
else
    echo "✓ Configuration is valid"
fi

echo ""
echo "Config after fix:"
grep -E '^\s*max_clients\s*=' "$config_file"

echo ""
echo "Backup created at: ${config_file}.backup"

# Cleanup
echo ""
echo "Test completed successfully!"
echo "Cleaning up test directory: $TEST_DIR"
rm -rf "$TEST_DIR"

echo "✓ All tests passed!"
