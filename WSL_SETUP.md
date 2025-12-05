# HTTrack WSL Setup Guide

This guide will help you set up and build HTTrack Website Copier using Windows Subsystem for Linux (WSL).

## Prerequisites

1. **WSL Installation**: Ensure you have WSL installed on your Windows system.
   - For WSL 1 or WSL 2: Open PowerShell as Administrator and run:
     ```powershell
     wsl --install
     ```
   - Or install a specific distribution:
     ```powershell
     wsl --install -d Ubuntu
     ```

2. **Access the Project**: Navigate to the project directory in WSL.
   - The Windows path `C:\Users\asus\OneDrive\Desktop\Shree Swami Smartha\httrack` 
   - Maps to WSL path: `/mnt/c/Users/asus/OneDrive/Desktop/Shree Swami Smartha/httrack`
   - Or use: `cd ~` and copy the project to your WSL home directory

## ⚠️ CRITICAL: Path Requirements

### Issue 1: Spaces in Path (WILL CAUSE BUILD FAILURE)

**Your current path contains spaces:** `Shree Swami Smartha`

**This WILL break the build!** Autotools and libtool cannot handle spaces in paths.

**Error you'll see:**
```
configure: WARNING: Libtool does not cope well with whitespace in `pwd`
configure: error: cannot run /bin/bash ./config.sub
```

### Issue 2: Windows Filesystem Mount Issues

Working from Windows mounts (`/mnt/c/`, `/mnt/d/`) causes:
- ❌ Line ending problems (CRLF vs LF) in ALL scripts
- ❌ File permission and executability issues  
- ❌ Slow build performance
- ❌ Autotools compatibility problems

## ✅ REQUIRED SOLUTION

**YOU MUST copy the project to your WSL home directory:**

```bash
# Copy project to WSL home directory (no spaces, native filesystem)
cp -r "/mnt/c/Users/asus/OneDrive/Desktop/Shree Swami Smartha/httrack" ~/httrack

# Navigate to the copied directory
cd ~/httrack

# Run the setup script
./setup-wsl.sh
```

**This fixes ALL issues automatically:**
- ✓ No spaces in path
- ✓ Native Linux filesystem
- ✓ Correct line endings
- ✓ Proper file permissions
- ✓ Fast build performance

## Quick Setup

### Option 1: Automated Setup Script (Recommended)

1. Open WSL terminal (Ubuntu/Debian)

2. **⚠️ REQUIRED: Copy project to WSL home:**
   ```bash
   cp -r "/mnt/c/Users/asus/OneDrive/Desktop/Shree Swami Smartha/httrack" ~/httrack
   cd ~/httrack
   ```
   
   **DO NOT skip this step!** The path contains spaces and is on a Windows mount, both of which will cause build failures.

3. Make the setup script executable:
   ```bash
   chmod +x setup-wsl.sh
   ```

4. Run the setup script:
   ```bash
   ./setup-wsl.sh
   ```

The script will:
- ✓ Check if you're on a Windows mount or have spaces in path
- ✓ Install all required build dependencies
- ✓ Fix line endings in all autotools scripts automatically
- ✓ Configure the build system with proper options
- ✓ Compile HTTrack using all CPU cores
- ✓ Optionally install it to your home directory
- ✓ Update your PATH automatically

### Option 2: Manual Setup

#### Step 1: Install Build Dependencies

For Ubuntu/Debian:
```bash
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
    dos2unix
```

For Fedora/RHEL/CentOS:
```bash
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
    git
```

#### Step 2: Copy Project to WSL Home (Recommended)

For best results, copy the project to your WSL native filesystem:
```bash
cp -r "/mnt/c/Users/asus/OneDrive/Desktop/Shree Swami Smartha/httrack" ~/httrack
cd ~/httrack
```

This avoids issues with Windows filesystem mounts. If you must work from the Windows mount:
```bash
cd "/mnt/c/Users/asus/OneDrive/Desktop/Shree Swami Smartha/httrack"
```

#### Step 3: Configure the Build

**Important**: If you're accessing files from Windows, the configure script may have Windows line endings. Fix this first:
```bash
dos2unix configure
# Or if dos2unix is not installed:
sudo apt-get install dos2unix
dos2unix configure
```

Make configure executable:
```bash
chmod +x configure
```

For installation to your home directory (recommended for WSL):
```bash
./configure --prefix=$HOME/usr
```

For system-wide installation (requires sudo):
```bash
./configure --prefix=/usr/local
```

**Available configure options:**
- `--prefix=PREFIX` - Installation prefix (default: /usr/local)
- `--enable-https=yes|no|auto` - Enable HTTPS support (default: yes)
- `--enable-online-unit-tests=yes|no|auto` - Enable online unit tests (default: yes)

To see all configuration options:
```bash
./configure --help
```

**Example with custom options:**
```bash
./configure --prefix=$HOME/usr --enable-https=yes --enable-online-unit-tests=no
```

#### Step 4: Build HTTrack

```bash
make -j$(nproc)
```

The `-j$(nproc)` flag uses all available CPU cores for faster compilation.

#### Step 5: Run Tests (Optional)

```bash
make check
```

#### Step 6: Install HTTrack

For home directory installation:
```bash
make install
```

For system-wide installation:
```bash
sudo make install
```

#### Step 7: Update PATH (if installed to home directory)

Add to your `~/.bashrc` or `~/.zshrc`:
```bash
export PATH="$HOME/usr/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/usr/lib:$LD_LIBRARY_PATH"
```

Then reload your shell:
```bash
source ~/.bashrc
```

## Verification

After installation, verify HTTrack is working:

```bash
httrack --version
```

Or:
```bash
which httrack
```

## Troubleshooting

### Issue: configure script not found or permission denied

**Solution**: Ensure you're in the correct directory and the file is executable:
```bash
chmod +x configure
./configure
```

### Issue: configure script has bad interpreter or line ending errors

**Primary Solution**: Copy the project to WSL native filesystem:
```bash
cd ~
cp -r "/mnt/c/Users/asus/OneDrive/Desktop/Shree Swami Smartha/httrack" ~/httrack
cd ~/httrack
./configure --version  # Should work now
```

**Alternative Fix**: If you must work from Windows mount, try:
```bash
# Method 1: Use sed to fix line endings
sed -i 's/\r$//' configure
chmod +x configure
./configure --version

# Method 2: Use bash directly
bash ./configure --version

# Method 3: Use dos2unix (if available)
dos2unix configure
chmod +x configure
./configure --version
```

If dos2unix is not installed:
```bash
sudo apt-get install dos2unix
```

### Issue: Missing dependencies

**Solution**: Install missing development packages. Common ones:
- `zlib1g-dev` (Ubuntu/Debian) or `zlib-devel` (Fedora)
- `libssl-dev` (Ubuntu/Debian) or `openssl-devel` (Fedora)
- `build-essential` (Ubuntu/Debian) or `gcc gcc-c++ make` (Fedora)

### Issue: Build fails with compiler errors

**Solution**: 
1. Clean previous build attempts:
   ```bash
   make distclean
   ```
2. Reconfigure:
   ```bash
   ./configure --prefix=$HOME/usr
   ```
3. Rebuild:
   ```bash
   make clean && make -j$(nproc)
   ```

### Issue: Permission denied during make install

**Solution**: 
- If installing to system directories, use `sudo make install`
- Or install to home directory: `./configure --prefix=$HOME/usr && make && make install`

### Issue: Command not found after installation

**Solution**: 
- Add installation directory to PATH (see Step 7 above)
- Or use full path: `$HOME/usr/bin/httrack`

## Uninstallation

To remove HTTrack:

```bash
# If installed to home directory
rm -rf $HOME/usr/bin/httrack* $HOME/usr/lib/libhttrack* $HOME/usr/share/httrack*

# If installed system-wide
sudo make uninstall
# Or manually:
sudo rm -rf /usr/local/bin/httrack* /usr/local/lib/libhttrack* /usr/local/share/httrack*
```

## Additional Resources

- See `INSTALL` file for detailed installation instructions
- See `INSTALL.Linux` for Linux-specific notes
- See `README` and `README.md` for general information
- Visit http://www.httrack.com/ for documentation

## Notes

- Building in WSL provides a native Linux environment, which is ideal for compiling Unix/Linux software
- The build process follows standard GNU autotools conventions (configure, make, make install)
- Installing to `$HOME/usr` avoids the need for sudo and keeps your system clean
- The compiled binaries will work within WSL and can be used like any Linux application

