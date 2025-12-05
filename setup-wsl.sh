#!/bin/bash

# HTTrack WSL Setup Script
# This script automates the setup and build process for HTTrack in WSL
#
# Features:
# - Automatically detects WSL environment
# - Installs build dependencies for Ubuntu/Debian/Fedora
# - Fixes Windows line endings in configure script
# - Properly configures build with standard autotools options
# - Supports custom configure options via environment variables
# - Builds and installs HTTrack
#
# Usage:
#   ./setup-wsl.sh
#   INSTALL_PREFIX=/custom/path ./setup-wsl.sh
#   ENABLE_HTTPS=no ./setup-wsl.sh
#
# Environment variables:
#   INSTALL_PREFIX - Installation prefix (default: $HOME/usr)
#   ENABLE_HTTPS - Enable HTTPS support: yes|no|auto (default: yes)
#   ENABLE_TESTS - Enable online unit tests: yes|no|auto (default: yes)

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running in WSL
check_wsl() {
    if grep -qi microsoft /proc/version; then
        print_success "Running in WSL environment"
        return 0
    else
        print_warning "This script is designed for WSL, but may work in other Linux environments"
        return 1
    fi
}

# Check if running on Windows mount and suggest copying
check_windows_mount() {
    CURRENT_DIR=$(pwd)
    
    # Check if path contains spaces
    if echo "$CURRENT_DIR" | grep -q " "; then
        print_error "Current path contains spaces: $CURRENT_DIR"
        print_error "This WILL cause build failures with autotools/libtool!"
        print_info ""
        print_info "REQUIRED: Copy the project to a path without spaces:"
        print_info "  cp -r . ~/httrack"
        print_info "  cd ~/httrack"
        print_info "  ./setup-wsl.sh"
        print_info ""
        
        read -p "Do you want to exit and fix this? (y/n) [y]: " EXIT_FOR_SPACES
        EXIT_FOR_SPACES=${EXIT_FOR_SPACES:-y}
        
        if [ "$EXIT_FOR_SPACES" = "y" ] || [ "$EXIT_FOR_SPACES" = "Y" ]; then
            print_info "Please copy to a path without spaces and try again."
            exit 0
        fi
        
        print_error "Continuing with spaces in path - BUILD WILL LIKELY FAIL!"
    fi
    
    # Check if we're on a Windows mount point (e.g., /mnt/c/)
    if echo "$CURRENT_DIR" | grep -q "^/mnt/[a-z]/"; then
        print_warning "======================================================"
        print_warning "WARNING: Running from Windows filesystem mount!"
        print_warning "======================================================"
        print_info ""
        print_info "This can cause serious issues:"
        print_info "  ❌ Line ending problems (CRLF vs LF)"
        print_info "  ❌ File permission and executability issues"
        print_info "  ❌ Slow build performance"
        print_info "  ❌ Autotools compatibility problems"
        print_info ""
        print_info "STRONGLY RECOMMENDED: Copy to WSL home directory:"
        print_info "  cp -r . ~/httrack"
        print_info "  cd ~/httrack"
        print_info "  ./setup-wsl.sh"
        print_info ""
        print_warning "This will fix ALL issues automatically!"
        print_info ""
        
        read -p "Do you want to continue anyway? (y/n) [n]: " CONTINUE_MOUNT
        CONTINUE_MOUNT=${CONTINUE_MOUNT:-n}
        
        if [ "$CONTINUE_MOUNT" != "y" ] && [ "$CONTINUE_MOUNT" != "Y" ]; then
            print_info "Exiting. Please copy project to WSL home and try again."
            print_info ""
            print_info "Quick commands to fix:"
            print_info "  cp -r \"$CURRENT_DIR\" ~/httrack"
            print_info "  cd ~/httrack"
            print_info "  ./setup-wsl.sh"
            exit 0
        fi
        
        print_warning "Continuing on Windows mount at your own risk..."
        print_warning "If build fails, you MUST copy to ~/httrack"
    else
        print_success "Running on WSL native filesystem ✓"
    fi
}

# Detect Linux distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        print_info "Detected distribution: $DISTRO"
    else
        print_error "Cannot detect Linux distribution"
        exit 1
    fi
}

# Install dependencies based on distribution
install_dependencies() {
    print_info "Installing build dependencies..."
    
    case $DISTRO in
        ubuntu|debian)
            sudo apt-get update
            sudo apt-get install -y \
                build-essential \
                gcc \
                g++ \
                make \
                autoconf \
                automake \
                libtool \
                zlib1g-dev \
                libssl-dev \
                pkg-config \
                git \
                dos2unix \
                || {
                    print_error "Failed to install dependencies"
                    exit 1
                }
            ;;
        fedora|rhel|centos)
            if command -v dnf &> /dev/null; then
                sudo dnf install -y \
                    gcc \
                    gcc-c++ \
                    make \
                    autoconf \
                    automake \
                    libtool \
                    zlib-devel \
                    openssl-devel \
                    pkgconfig \
                    git \
                    dos2unix \
                    || {
                        print_error "Failed to install dependencies"
                        exit 1
                    }
            else
                sudo yum install -y \
                    gcc \
                    gcc-c++ \
                    make \
                    autoconf \
                    automake \
                    libtool \
                    zlib-devel \
                    openssl-devel \
                    pkgconfig \
                    git \
                    dos2unix \
                    || {
                        print_error "Failed to install dependencies"
                        exit 1
                    }
            fi
            ;;
        *)
            print_warning "Unknown distribution. Please install build dependencies manually:"
            print_info "Required: gcc, g++, make, autoconf, automake, libtool, zlib-dev, openssl-dev, pkg-config, dos2unix"
            read -p "Press Enter to continue anyway, or Ctrl+C to abort..."
            ;;
    esac
    
    print_success "Dependencies installed"
}

# Fix line endings for all autotools scripts
fix_autotools_line_endings() {
    print_info "Checking and fixing line endings in autotools scripts..."
    
    local files_to_fix=(
        "configure"
        "config.sub"
        "config.guess"
        "install-sh"
        "missing"
        "depcomp"
        "ltmain.sh"
        "compile"
    )
    
    local fixed_count=0
    
    for file in "${files_to_fix[@]}"; do
        if [ -f "./$file" ]; then
            # Check if file has CRLF line endings
            if head -1 "./$file" 2>/dev/null | od -c 2>/dev/null | grep -q '\\r'; then
                print_info "Fixing line endings in: $file"
                
                # Use sed to remove carriage returns
                if sed -i 's/\r$//' "./$file" 2>/dev/null; then
                    chmod +x "./$file" 2>/dev/null || true
                    ((fixed_count++))
                else
                    # Try tr as fallback
                    if tr -d '\r' < "./$file" > "./$file.tmp" 2>/dev/null; then
                        mv "./$file.tmp" "./$file"
                        chmod +x "./$file" 2>/dev/null || true
                        ((fixed_count++))
                    fi
                fi
            fi
        fi
    done
    
    if [ $fixed_count -gt 0 ]; then
        print_success "Fixed line endings in $fixed_count file(s)"
    else
        print_info "No line ending issues detected in autotools scripts"
    fi
}

# Check if configure script exists and validate
check_configure() {
    if [ ! -f "./configure" ]; then
        print_error "configure script not found!"
        print_info "If this is a git repository, you may need to run:"
        print_info "  autoreconf -fiv"
        print_info "Or generate configure from configure.ac"
        exit 1
    fi
    
    # Fix line endings in all autotools scripts first
    fix_autotools_line_endings
    
    # Make configure executable
    chmod +x ./configure 2>/dev/null || true
    
    # Try to run configure to check if it works
    if ./configure --version > /dev/null 2>&1; then
        print_success "Configure script validated successfully"
        return 0
    fi
    
    print_error "Configure script validation failed"
    print_info ""
    print_info "Troubleshooting steps:"
    print_info "  1. BEST SOLUTION: Copy to WSL home (avoids all these issues):"
    print_info "     cp -r . ~/httrack && cd ~/httrack"
    print_info "  2. Try running with bash directly:"
    print_info "     bash ./configure --version"
    print_info "  3. Check file type:"
    print_info "     file ./configure"
    print_info ""
    
    exit 1
}

# Run configure
run_configure() {
    print_info "Configuring build system..."
    
    INSTALL_PREFIX=${INSTALL_PREFIX:-$HOME/usr}
    ENABLE_HTTPS=${ENABLE_HTTPS:-yes}
    ENABLE_TESTS=${ENABLE_TESTS:-yes}
    
    # Build configure command
    CONFIGURE_CMD="./configure --prefix=\"$INSTALL_PREFIX\""
    
    # Add HTTPS support option
    if [ "$ENABLE_HTTPS" != "default" ]; then
        CONFIGURE_CMD="$CONFIGURE_CMD --enable-https=$ENABLE_HTTPS"
    fi
    
    # Add online unit tests option
    if [ "$ENABLE_TESTS" != "default" ]; then
        CONFIGURE_CMD="$CONFIGURE_CMD --enable-online-unit-tests=$ENABLE_TESTS"
    fi
    
    print_info "Installation prefix: $INSTALL_PREFIX"
    print_info "HTTPS support: $ENABLE_HTTPS"
    print_info "Online unit tests: $ENABLE_TESTS"
    print_info ""
    print_info "To customize configure options, set environment variables:"
    print_info "  INSTALL_PREFIX=/custom/path"
    print_info "  ENABLE_HTTPS=yes|no|auto (default: yes)"
    print_info "  ENABLE_TESTS=yes|no|auto (default: yes)"
    print_info ""
    print_info "Or see all options with: ./configure --help"
    print_info ""
    print_info "Running: $CONFIGURE_CMD"
    
    # Run configure with proper error handling
    if ! eval "$CONFIGURE_CMD"; then
        print_error "Configuration failed"
        print_info ""
        print_info "Check config.log for detailed error information:"
        print_info "  tail -50 config.log"
        print_info ""
        print_info "Common issues:"
        print_info "  - Missing dependencies: sudo apt-get install zlib1g-dev libssl-dev"
        print_info "  - Line ending issues: Try bash ./configure instead"
        print_info "  - Permission issues: chmod +x configure"
        print_info "  - Working on Windows mount: Copy to WSL home: cp -r . ~/httrack"
        print_info ""
        
        # Try to show relevant part of config.log if it exists
        if [ -f "config.log" ]; then
            print_info "Last few lines from config.log:"
            tail -20 config.log | grep -i "error\|fail\|not found" || tail -10 config.log
        fi
        
        exit 1
    fi
    
    print_success "Configuration completed successfully"
    print_info "Configuration summary saved in config.log"
}

# Build the project
build_project() {
    print_info "Building HTTrack..."
    
    # Use all available CPU cores
    CPU_CORES=$(nproc)
    print_info "Using $CPU_CORES CPU cores for compilation"
    
    make -j$CPU_CORES || {
        print_error "Build failed"
        exit 1
    }
    
    print_success "Build completed"
}

# Run tests (optional)
run_tests() {
    print_info "Running tests..."
    
    if make check; then
        print_success "All tests passed"
    else
        print_warning "Some tests failed, but continuing..."
    fi
}

# Install the project
install_project() {
    print_info "Installing HTTrack..."
    
    make install || {
        print_error "Installation failed"
        exit 1
    }
    
    print_success "Installation completed"
}

# Update PATH in shell config
update_path() {
    INSTALL_PREFIX=${INSTALL_PREFIX:-$HOME/usr}
    
    if [ "$INSTALL_PREFIX" = "$HOME/usr" ] || [[ "$INSTALL_PREFIX" == "$HOME"* ]]; then
        print_info "Updating PATH in shell configuration..."
        
        SHELL_CONFIG=""
        if [ -f "$HOME/.bashrc" ]; then
            SHELL_CONFIG="$HOME/.bashrc"
        elif [ -f "$HOME/.zshrc" ]; then
            SHELL_CONFIG="$HOME/.zshrc"
        fi
        
        if [ -n "$SHELL_CONFIG" ]; then
            BIN_PATH="$INSTALL_PREFIX/bin"
            LIB_PATH="$INSTALL_PREFIX/lib"
            
            # Check if already added
            if ! grep -q "$BIN_PATH" "$SHELL_CONFIG"; then
                echo "" >> "$SHELL_CONFIG"
                echo "# HTTrack paths" >> "$SHELL_CONFIG"
                echo "export PATH=\"$BIN_PATH:\$PATH\"" >> "$SHELL_CONFIG"
                echo "export LD_LIBRARY_PATH=\"$LIB_PATH:\$LD_LIBRARY_PATH\"" >> "$SHELL_CONFIG"
                print_success "PATH updated in $SHELL_CONFIG"
                print_info "Run 'source $SHELL_CONFIG' or restart your terminal to use httrack"
            else
                print_info "PATH already configured in $SHELL_CONFIG"
            fi
        else
            print_warning "Could not find .bashrc or .zshrc to update PATH"
            print_info "Manually add to your shell config:"
            print_info "  export PATH=\"$INSTALL_PREFIX/bin:\$PATH\""
            print_info "  export LD_LIBRARY_PATH=\"$INSTALL_PREFIX/lib:\$LD_LIBRARY_PATH\""
        fi
    fi
}

# Verify installation
verify_installation() {
    INSTALL_PREFIX=${INSTALL_PREFIX:-$HOME/usr}
    
    print_info "Verifying installation..."
    
    if [ -f "$INSTALL_PREFIX/bin/httrack" ]; then
        print_success "HTTrack installed successfully!"
        print_info "Location: $INSTALL_PREFIX/bin/httrack"
        
        # Try to get version
        if "$INSTALL_PREFIX/bin/httrack" --version 2>/dev/null | head -1; then
            print_success "HTTrack is working correctly"
        fi
        
        print_info ""
        print_info "To use httrack:"
        if [[ "$INSTALL_PREFIX" == "$HOME"* ]]; then
            print_info "  1. Run: source ~/.bashrc (or restart terminal)"
            print_info "  2. Then use: httrack --help"
        else
            print_info "  Use full path: $INSTALL_PREFIX/bin/httrack --help"
        fi
    else
        print_error "Installation verification failed - httrack binary not found"
        exit 1
    fi
}

# Show configure help
show_configure_help() {
    print_info "Showing configure help..."
    ./configure --help | head -80
    echo ""
    read -p "Press Enter to continue with default configuration, or Ctrl+C to abort..."
}

# Main execution
main() {
    echo ""
    print_info "=========================================="
    print_info "HTTrack WSL Setup Script"
    print_info "=========================================="
    echo ""
    
    # Check WSL environment
    check_wsl
    
    # Check if on Windows mount
    check_windows_mount
    
    # Detect distribution
    detect_distro
    
    # Ask for dependency installation
    read -p "Install build dependencies? (y/n) [y]: " INSTALL_DEPS
    INSTALL_DEPS=${INSTALL_DEPS:-y}
    
    if [ "$INSTALL_DEPS" = "y" ] || [ "$INSTALL_DEPS" = "Y" ]; then
        install_dependencies
    else
        print_info "Skipping dependency installation"
    fi
    
    # Check configure script
    check_configure
    
    # Ask if user wants to see configure options
    read -p "Show configure options? (y/n) [n]: " SHOW_HELP
    SHOW_HELP=${SHOW_HELP:-n}
    
    if [ "$SHOW_HELP" = "y" ] || [ "$SHOW_HELP" = "Y" ]; then
        show_configure_help
    fi
    
    # Run configure
    run_configure
    
    # Build project
    build_project
    
    # Ask to run tests
    read -p "Run tests? (y/n) [n]: " RUN_TESTS
    RUN_TESTS=${RUN_TESTS:-n}
    
    if [ "$RUN_TESTS" = "y" ] || [ "$RUN_TESTS" = "Y" ]; then
        run_tests
    fi
    
    # Ask to install
    read -p "Install HTTrack? (y/n) [y]: " INSTALL_PROJ
    INSTALL_PROJ=${INSTALL_PROJ:-y}
    
    if [ "$INSTALL_PROJ" = "y" ] || [ "$INSTALL_PROJ" = "Y" ]; then
        install_project
        update_path
        verify_installation
    else
        print_info "Skipping installation"
        print_info "To install later, run: make install"
    fi
    
    echo ""
    print_success "Setup completed successfully!"
    echo ""
}

# Run main function
main

