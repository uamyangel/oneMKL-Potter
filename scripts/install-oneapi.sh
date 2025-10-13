#!/bin/bash
################################################################################
# Intel oneAPI Base Toolkit Installation Script
################################################################################
# Description: Download and install Intel oneAPI Base Toolkit 2025.2.1
# Author: oneMKL Optimization Team
# Usage: sudo ./install-oneapi.sh
################################################################################

set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# oneAPI version and download URL
ONEAPI_VERSION="2025.2.1.44"
ONEAPI_INSTALLER="intel-oneapi-base-toolkit-${ONEAPI_VERSION}_offline.sh"
ONEAPI_URL="https://registrationcenter-download.intel.com/akdlm/IRC_NAS/3b7a16b3-a7b0-460f-be16-de0d64fa6b1e/${ONEAPI_INSTALLER}"
INSTALL_DIR="/opt/intel/oneapi"

################################################################################
# Helper functions
################################################################################

print_header() {
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
}

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        print_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

check_disk_space() {
    local required_gb=15
    local available_gb=$(df / | awk 'NR==2 {print int($4/1024/1024)}')

    if [ "$available_gb" -lt "$required_gb" ]; then
        print_warning "Low disk space: ${available_gb}GB available, ${required_gb}GB recommended"
        read -p "Continue anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    else
        print_info "Disk space check passed: ${available_gb}GB available"
    fi
}

################################################################################
# Main installation steps
################################################################################

install_oneapi() {
    print_header "Intel oneAPI Base Toolkit Installation"

    # Check prerequisites
    print_info "Checking prerequisites..."
    check_root
    check_disk_space

    # Check if already installed
    if [ -d "${INSTALL_DIR}" ] && [ -f "${INSTALL_DIR}/setvars.sh" ]; then
        print_warning "Intel oneAPI appears to be already installed at ${INSTALL_DIR}"
        read -p "Reinstall? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Installation cancelled"
            verify_installation
            exit 0
        fi
    fi

    # Download installer
    print_info "Downloading Intel oneAPI Base Toolkit ${ONEAPI_VERSION}..."
    print_info "Download URL: ${ONEAPI_URL}"
    print_info "This may take several minutes (installer size: ~3GB)..."

    if [ -f "${ONEAPI_INSTALLER}" ]; then
        print_warning "Installer already exists: ${ONEAPI_INSTALLER}"
        read -p "Use existing file? (Y/n): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            print_info "Using existing installer"
        else
            rm -f "${ONEAPI_INSTALLER}"
            wget "${ONEAPI_URL}" -O "${ONEAPI_INSTALLER}"
        fi
    else
        wget "${ONEAPI_URL}" -O "${ONEAPI_INSTALLER}"
    fi

    if [ $? -ne 0 ]; then
        print_error "Failed to download installer"
        exit 1
    fi

    print_info "Download completed: ${ONEAPI_INSTALLER}"
    ls -lh "${ONEAPI_INSTALLER}"

    # Make installer executable
    chmod +x "${ONEAPI_INSTALLER}"

    # Run installation
    print_info "Starting installation (silent mode)..."
    print_info "Installation directory: ${INSTALL_DIR}"
    print_warning "This will take 5-10 minutes. Please be patient..."

    sh ./"${ONEAPI_INSTALLER}" -a --silent --cli --eula accept

    if [ $? -ne 0 ]; then
        print_error "Installation failed"
        exit 1
    fi

    print_info "Installation completed successfully"

    # Cleanup installer
    read -p "Remove installer file to save disk space? (Y/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Nn]$ ]]; then
        rm -f "${ONEAPI_INSTALLER}"
        print_info "Installer removed"
    fi

    # Verify installation
    verify_installation

    # Print usage instructions
    print_usage
}

################################################################################
# Verification
################################################################################

verify_installation() {
    print_header "Verifying Installation"

    if [ ! -f "${INSTALL_DIR}/setvars.sh" ]; then
        print_error "Installation verification failed: setvars.sh not found"
        exit 1
    fi

    print_info "✓ setvars.sh found at ${INSTALL_DIR}/setvars.sh"

    # Source environment and check MKL
    source "${INSTALL_DIR}/setvars.sh" > /dev/null 2>&1

    if [ -z "$MKLROOT" ]; then
        print_warning "MKLROOT not set after sourcing setvars.sh"
    else
        print_info "✓ MKLROOT is set to: ${MKLROOT}"
    fi

    if [ -d "${MKLROOT}/lib/intel64" ] || [ -d "${MKLROOT}/lib" ]; then
        print_info "✓ MKL libraries found"

        # List some key libraries
        if [ -d "${MKLROOT}/lib/intel64" ]; then
            local mkl_lib_dir="${MKLROOT}/lib/intel64"
        else
            local mkl_lib_dir="${MKLROOT}/lib"
        fi

        if [ -f "${mkl_lib_dir}/libmkl_core.so" ] || [ -f "${mkl_lib_dir}/libmkl_core.dylib" ]; then
            print_info "✓ libmkl_core found"
        fi

        if [ -f "${mkl_lib_dir}/libmkl_intel_lp64.so" ] || [ -f "${mkl_lib_dir}/libmkl_intel_lp64.dylib" ]; then
            print_info "✓ libmkl_intel_lp64 found"
        fi

        if [ -f "${mkl_lib_dir}/libmkl_gnu_thread.so" ] || [ -f "${mkl_lib_dir}/libmkl_gnu_thread.dylib" ]; then
            print_info "✓ libmkl_gnu_thread found"
        fi
    else
        print_warning "MKL library directory not found"
    fi

    if [ -d "${MKLROOT}/include" ]; then
        print_info "✓ MKL headers found"
        if [ -f "${MKLROOT}/include/mkl.h" ]; then
            print_info "✓ mkl.h found"
        fi
        if [ -f "${MKLROOT}/include/mkl_vml.h" ]; then
            print_info "✓ mkl_vml.h found"
        fi
    else
        print_warning "MKL include directory not found"
    fi

    echo
    print_info "Installation verification completed"
}

################################################################################
# Usage instructions
################################################################################

print_usage() {
    print_header "Next Steps"

    echo -e "${GREEN}Intel oneAPI Base Toolkit has been installed successfully!${NC}"
    echo
    echo -e "${YELLOW}IMPORTANT: Before using oneMKL, you must load the environment:${NC}"
    echo
    echo -e "  ${BLUE}source ${INSTALL_DIR}/setvars.sh${NC}"
    echo
    echo -e "${YELLOW}You can add this to your ~/.bashrc or ~/.zshrc for automatic loading:${NC}"
    echo
    echo -e "  ${BLUE}echo 'source ${INSTALL_DIR}/setvars.sh' >> ~/.bashrc${NC}"
    echo
    echo -e "${YELLOW}To build Potter with oneMKL:${NC}"
    echo
    echo -e "  ${BLUE}cd /path/to/oneMKL-Potter${NC}"
    echo -e "  ${BLUE}source ${INSTALL_DIR}/setvars.sh${NC}"
    echo -e "  ${BLUE}cmake -B build -DCMAKE_BUILD_TYPE=Release .${NC}"
    echo -e "  ${BLUE}cmake --build build --parallel \$(nproc)${NC}"
    echo
    echo -e "${YELLOW}To run the build and test script:${NC}"
    echo
    echo -e "  ${BLUE}./scripts/build-and-test.sh${NC}"
    echo
    echo -e "${GREEN}For more information, see:${NC}"
    echo -e "  - ${BLUE}ONEMKL-OPTIMIZATION.md${NC}"
    echo -e "  - ${BLUE}${INSTALL_DIR}/documentation/${NC}"
    echo
}

################################################################################
# Main execution
################################################################################

main() {
    install_oneapi
}

# Run main function
main
