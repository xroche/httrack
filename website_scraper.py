#!/usr/bin/env python3
"""
Website Scraper using HTTrack
Scrapes websites and creates ZIP archives of the downloaded content
"""

import os
import sys
import subprocess
import shutil
import zipfile
from datetime import datetime
from pathlib import Path
import argparse
import json


class WebsiteScraper:
    def __init__(self):
        self.output_base = "scraped_websites"
        self.config_file = "scraper_config.json"
        self.default_config = {
            "depth": 2,
            "max_rate": 0,  # 0 = unlimited
            "max_size": 0,  # 0 = unlimited
            "max_time": 0,  # 0 = unlimited
            "connections": 4,
            "retries": 2,
            "timeout": 30,
            "user_agent": "Mozilla/5.0 (compatible; HTTrack)",
            "accept_cookies": True,
            "follow_robots": True,
            "parse_java": True,
            "get_images": True,
            "get_videos": True,
            "get_audio": True,
            "external_depth": 0,
            "stay_on_domain": True,
            "update_existing": False,
            "verbose": True,
        }
        
    def check_httrack(self):
        """Check if HTTrack is installed"""
        try:
            result = subprocess.run(
                ["httrack", "--version"],
                capture_output=True,
                text=True,
                check=False
            )
            if result.returncode == 0:
                print("✓ HTTrack is installed")
                print(f"  Version: {result.stdout.strip().split()[2]}")
                return True
            return False
        except FileNotFoundError:
            print("✗ HTTrack is not installed!")
            print("\nTo install HTTrack:")
            print("  Ubuntu/Debian: sudo apt-get install httrack")
            print("  Fedora/RHEL:   sudo dnf install httrack")
            print("  macOS:         brew install httrack")
            return False

    def load_config(self):
        """Load configuration from file or use defaults"""
        if os.path.exists(self.config_file):
            try:
                with open(self.config_file, 'r') as f:
                    saved_config = json.load(f)
                    return {**self.default_config, **saved_config}
            except:
                pass
        return self.default_config.copy()

    def save_config(self, config):
        """Save configuration to file"""
        try:
            with open(self.config_file, 'w') as f:
                json.dump(config, f, indent=2)
            print(f"✓ Configuration saved to {self.config_file}")
        except Exception as e:
            print(f"⚠ Could not save configuration: {e}")

    def get_user_config(self, url, interactive=True):
        """Get scraping configuration from user"""
        config = self.load_config()
        
        if not interactive:
            return config
        
        print("\n" + "="*70)
        print("HTTRACK WEBSITE SCRAPER - CONFIGURATION")
        print("="*70)
        print(f"\nTarget URL: {url}")
        print("\nPress Enter to use default value [shown in brackets]\n")
        
        # Basic settings
        print("── BASIC SETTINGS ──")
        depth = input(f"Mirror depth (how many links deep to follow) [{config['depth']}]: ").strip()
        if depth:
            config['depth'] = int(depth)
        
        stay = input(f"Stay on same domain? (yes/no) [{'yes' if config['stay_on_domain'] else 'no'}]: ").strip().lower()
        if stay in ['yes', 'y', 'no', 'n']:
            config['stay_on_domain'] = stay in ['yes', 'y']
        
        # Download limits
        print("\n── DOWNLOAD LIMITS ──")
        max_rate = input(f"Max download rate in KB/s (0=unlimited) [{config['max_rate']}]: ").strip()
        if max_rate:
            config['max_rate'] = int(max_rate)
        
        max_size = input(f"Max total size in MB (0=unlimited) [{config['max_size']}]: ").strip()
        if max_size:
            config['max_size'] = int(max_size)
        
        max_time = input(f"Max time in seconds (0=unlimited) [{config['max_time']}]: ").strip()
        if max_time:
            config['max_time'] = int(max_time)
        
        # Connection settings
        print("\n── CONNECTION SETTINGS ──")
        connections = input(f"Number of simultaneous connections [{config['connections']}]: ").strip()
        if connections:
            config['connections'] = int(connections)
        
        retries = input(f"Number of retries on error [{config['retries']}]: ").strip()
        if retries:
            config['retries'] = int(retries)
        
        timeout = input(f"Connection timeout in seconds [{config['timeout']}]: ").strip()
        if timeout:
            config['timeout'] = int(timeout)
        
        # Content settings
        print("\n── CONTENT SETTINGS ──")
        images = input(f"Download images? (yes/no) [{'yes' if config['get_images'] else 'no'}]: ").strip().lower()
        if images in ['yes', 'y', 'no', 'n']:
            config['get_images'] = images in ['yes', 'y']
        
        videos = input(f"Download videos? (yes/no) [{'yes' if config['get_videos'] else 'no'}]: ").strip().lower()
        if videos in ['yes', 'y', 'no', 'n']:
            config['get_videos'] = videos in ['yes', 'y']
        
        # Advanced settings
        print("\n── ADVANCED SETTINGS ──")
        robots = input(f"Follow robots.txt? (yes/no) [{'yes' if config['follow_robots'] else 'no'}]: ").strip().lower()
        if robots in ['yes', 'y', 'no', 'n']:
            config['follow_robots'] = robots in ['yes', 'y']
        
        # Save configuration
        save = input("\nSave this configuration for future use? (yes/no) [yes]: ").strip().lower()
        if save != 'no' and save != 'n':
            self.save_config(config)
        
        return config

    def build_httrack_command(self, url, output_dir, config):
        """Build HTTrack command with parameters"""
        cmd = ["httrack", url, "-O", output_dir]
        
        # Mirror depth
        cmd.extend([f"-r{config['depth']}"])
        
        # External links depth
        cmd.extend([f"%e{config['external_depth']}"])
        
        # Stay on domain
        if config['stay_on_domain']:
            cmd.extend(["-a"])  # Stay on same address
            cmd.extend(["-D"])  # Can only go down into subdirs
        
        # Connection settings
        cmd.extend([f"-c{config['connections']}"])  # Simultaneous connections
        cmd.extend([f"-T{config['timeout']}"])      # Timeout
        cmd.extend([f"-R{config['retries']}"])      # Retries
        
        # Download limits
        if config['max_rate'] > 0:
            cmd.extend([f"-A{config['max_rate'] * 1000}"])  # Convert KB to bytes
        
        if config['max_size'] > 0:
            cmd.extend([f"-M{config['max_size'] * 1000000}"])  # Convert MB to bytes
        
        if config['max_time'] > 0:
            cmd.extend([f"-E{config['max_time']}"])
        
        # Content settings
        if not config['get_images']:
            cmd.extend(["-*", "+*.html", "+*.css", "+*.js"])
        
        if not config['get_videos']:
            cmd.extend(["-*.mp4", "-*.avi", "-*.mov", "-*.wmv", "-*.flv", "-*.webm"])
        
        if not config['get_audio']:
            cmd.extend(["-*.mp3", "-*.wav", "-*.ogg", "-*.m4a"])
        
        # Robots.txt
        if config['follow_robots']:
            cmd.extend(["-s2"])  # Follow robots.txt
        else:
            cmd.extend(["-s0"])  # Ignore robots.txt
        
        # Cookies
        if config['accept_cookies']:
            cmd.extend(["-b1"])  # Accept cookies
        else:
            cmd.extend(["-b0"])  # Don't accept cookies
        
        # Parse Java
        if config['parse_java']:
            cmd.extend(["-j"])
        
        # Update mode
        if config['update_existing']:
            cmd.extend(["-i"])  # Continue interrupted download
        
        # Verbose mode
        if config['verbose']:
            cmd.extend(["-v"])
        
        # User agent
        cmd.extend([f"-F{config['user_agent']}"])
        
        # Additional useful options
        cmd.extend(["-N0"])  # Save structure (original)
        cmd.extend(["-K0"])  # Keep original links (relative)
        cmd.extend(["-o"])   # Generate error files
        cmd.extend(["-%P"])  # Extended parsing
        
        return cmd

    def scrape_website(self, url, config, output_name=None):
        """Scrape website using HTTrack"""
        # Create output directory name
        if not output_name:
            domain = url.replace("http://", "").replace("https://", "").replace("/", "_")
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            output_name = f"{domain}_{timestamp}"
        
        output_dir = os.path.join(self.output_base, output_name)
        
        # Create output directory
        os.makedirs(output_dir, exist_ok=True)
        
        print("\n" + "="*70)
        print("STARTING WEBSITE SCRAPING")
        print("="*70)
        print(f"URL:        {url}")
        print(f"Output:     {output_dir}")
        print(f"Depth:      {config['depth']}")
        print(f"Domain:     {'Same domain only' if config['stay_on_domain'] else 'Follow external links'}")
        print("="*70 + "\n")
        
        # Build and display command
        cmd = self.build_httrack_command(url, output_dir, config)
        print(f"Command: {' '.join(cmd)}\n")
        
        # Save configuration used for this scrape
        config_path = os.path.join(output_dir, "scrape_config.json")
        try:
            with open(config_path, 'w') as f:
                json.dump({
                    'url': url,
                    'timestamp': datetime.now().isoformat(),
                    'config': config,
                    'command': ' '.join(cmd)
                }, f, indent=2)
        except:
            pass
        
        # Run HTTrack
        try:
            print("Scraping in progress...\n")
            result = subprocess.run(cmd, check=False)
            
            if result.returncode == 0:
                print("\n✓ Scraping completed successfully!")
                return output_dir
            else:
                print(f"\n⚠ Scraping completed with warnings (exit code: {result.returncode})")
                return output_dir
        except Exception as e:
            print(f"\n✗ Error during scraping: {e}")
            return None

    def create_zip(self, source_dir, zip_name=None):
        """Create ZIP archive of scraped content"""
        if not zip_name:
            zip_name = f"{os.path.basename(source_dir)}.zip"
        
        zip_path = os.path.join(self.output_base, zip_name)
        
        print("\n" + "="*70)
        print("CREATING ZIP ARCHIVE")
        print("="*70)
        print(f"Source: {source_dir}")
        print(f"Output: {zip_path}")
        
        try:
            with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
                # Walk through directory
                for root, dirs, files in os.walk(source_dir):
                    for file in files:
                        file_path = os.path.join(root, file)
                        arcname = os.path.relpath(file_path, source_dir)
                        zipf.write(file_path, arcname)
                        print(f"  Adding: {arcname}")
            
            # Get file size
            size_mb = os.path.getsize(zip_path) / (1024 * 1024)
            
            print("="*70)
            print(f"✓ ZIP archive created successfully!")
            print(f"  Location: {zip_path}")
            print(f"  Size: {size_mb:.2f} MB")
            print("="*70)
            
            return zip_path
        except Exception as e:
            print(f"\n✗ Error creating ZIP: {e}")
            return None

    def cleanup(self, directory, keep_zip=True):
        """Clean up scraped directory after zipping"""
        if keep_zip and os.path.exists(directory):
            try:
                shutil.rmtree(directory)
                print(f"✓ Cleaned up temporary directory: {directory}")
            except Exception as e:
                print(f"⚠ Could not clean up directory: {e}")

    def run(self, url, interactive=True, cleanup=False, output_name=None):
        """Main execution flow"""
        print("\n" + "="*70)
        print("HTTRACK WEBSITE SCRAPER")
        print("="*70)
        
        # Check HTTrack installation
        if not self.check_httrack():
            return False
        
        # Get configuration
        config = self.get_user_config(url, interactive)
        
        # Scrape website
        output_dir = self.scrape_website(url, config, output_name)
        if not output_dir:
            return False
        
        # Create ZIP
        zip_path = self.create_zip(output_dir)
        if not zip_path:
            return False
        
        # Cleanup if requested
        if cleanup:
            self.cleanup(output_dir)
        
        print("\n" + "="*70)
        print("✓ ALL OPERATIONS COMPLETED SUCCESSFULLY")
        print("="*70)
        print(f"\nZIP Archive: {zip_path}")
        if not cleanup:
            print(f"Source Directory: {output_dir}")
        print()
        
        return True


def main():
    parser = argparse.ArgumentParser(
        description="Website Scraper using HTTrack - Download websites and create ZIP archives",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive mode with all prompts
  python website_scraper.py https://example.com
  
  # Non-interactive mode with default settings
  python website_scraper.py https://example.com --non-interactive
  
  # Scrape and cleanup source directory
  python website_scraper.py https://example.com --cleanup
  
  # Custom output name
  python website_scraper.py https://example.com --output my_website
        """
    )
    
    parser.add_argument(
        "url",
        help="URL of the website to scrape"
    )
    
    parser.add_argument(
        "-n", "--non-interactive",
        action="store_true",
        help="Use default configuration without prompts"
    )
    
    parser.add_argument(
        "-c", "--cleanup",
        action="store_true",
        help="Remove source directory after creating ZIP"
    )
    
    parser.add_argument(
        "-o", "--output",
        help="Custom output name for the scraped content"
    )
    
    args = parser.parse_args()
    
    # Validate URL
    if not args.url.startswith(('http://', 'https://')):
        print("Error: URL must start with http:// or https://")
        sys.exit(1)
    
    # Create scraper instance
    scraper = WebsiteScraper()
    
    # Run scraper
    success = scraper.run(
        url=args.url,
        interactive=not args.non_interactive,
        cleanup=args.cleanup,
        output_name=args.output
    )
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()

