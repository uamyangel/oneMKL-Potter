#!/bin/bash
################################################################################
# Intel oneAPI Base Toolkit Installation Script
################################################################################
# Description: Download and install Intel oneAPI Base Toolkit 2025.2.1
# Usage: sudo ./install-oneapi.sh
################################################################################

set -e

# Configuration
ONEAPI_VERSION="2025.2.1.44"
ONEAPI_INSTALLER="intel-oneapi-base-toolkit-${ONEAPI_VERSION}_offline.sh"
ONEAPI_URL="https://registrationcenter-download.intel.com/akdlm/IRC_NAS/3b7a16b3-a7b0-460f-be16-de0d64fa6b1e/${ONEAPI_INSTALLER}"
INSTALL_DIR="/opt/intel/oneapi"

################################################################################
# Check if already installed
################################################################################

if [ -d "${INSTALL_DIR}" ] && [ -f "${INSTALL_DIR}/setvars.sh" ]; then
    echo "✓ Intel oneAPI is already installed at ${INSTALL_DIR}"
    echo ""
    echo "To use it, load the environment:"
    echo "  source ${INSTALL_DIR}/setvars.sh"
    echo ""
    echo "Then build Potter:"
    echo "  ./scripts/build-and-test.sh"
    exit 0
fi

################################################################################
# Install oneAPI
################################################################################

echo "Intel oneAPI not found. Starting installation..."
echo ""

# Check root permission
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: This script must be run as root (use sudo)"
    exit 1
fi

# Download installer if not exists
if [ ! -f "${ONEAPI_INSTALLER}" ]; then
    echo "Downloading Intel oneAPI Base Toolkit ${ONEAPI_VERSION} (~3GB)..."
    wget "${ONEAPI_URL}" -O "${ONEAPI_INSTALLER}"
else
    echo "Using existing installer: ${ONEAPI_INSTALLER}"
fi

# Make executable
chmod +x "${ONEAPI_INSTALLER}"

# Install
echo ""
echo "Installing to ${INSTALL_DIR} (this will take 5-10 minutes)..."
sh ./"${ONEAPI_INSTALLER}" -a --silent --cli --eula accept

# Verify
if [ -f "${INSTALL_DIR}/setvars.sh" ]; then
    echo ""
    echo "✓ Installation successful!"
    echo ""
    echo "Next steps:"
    echo "  1. Load environment: source ${INSTALL_DIR}/setvars.sh"
    echo "  2. Build and test:   ./scripts/build-and-test.sh"

    # Cleanup installer
    rm -f "${ONEAPI_INSTALLER}"
    echo ""
    echo "Installer file removed to save disk space."
else
    echo "ERROR: Installation failed"
    exit 1
fi
