"""
HTTrack Website Scraper Actor - Main Module

This Actor uses HTTrack to scrape websites and create ZIP archives.
It reads configuration from Actor input and stores results in the default dataset.
"""

import os
import sys
import subprocess
import zipfile
import shutil
from pathlib import Path
from datetime import datetime
from typing import Dict, Any, Optional

from apify import Actor


class HTTrackScraper:
    """HTTrack website scraper for Apify Actor"""
    
    def __init__(self):
        self.output_base = "/home/myuser/scraped_websites"
        Path(self.output_base).mkdir(parents=True, exist_ok=True)
    
    def check_httrack(self) -> bool:
        """Check if HTTrack is installed"""
        try:
            result = subprocess.run(
                ["httrack", "--version"],
                capture_output=True,
                text=True,
                check=False
            )
            return result.returncode == 0
        except FileNotFoundError:
            return False
    
    def build_httrack_command(
        self,
        url: str,
        output_dir: str,
        config: Dict[str, Any]
    ) -> list:
        """Build HTTrack command with parameters"""
        cmd = ["httrack", url, "-O", output_dir]
        
        # Mirror depth
        cmd.extend([f"-r{config.get('depth', 2)}"])
        
        # External links depth
        cmd.extend([f"%e{config.get('external_depth', 0)}"])
        
        # Stay on domain
        if config.get('stay_on_domain', True):
            cmd.extend(["-a"])  # Stay on same address
            cmd.extend(["-D"])  # Can only go down into subdirs
        
        # Connection settings
        cmd.extend([f"-c{config.get('connections', 4)}"])
        cmd.extend([f"-T{config.get('timeout', 30)}"])
        cmd.extend([f"-R{config.get('retries', 2)}"])
        
        # Download limits
        if config.get('max_rate', 0) > 0:
            cmd.extend([f"-A{config['max_rate'] * 1000}"])
        
        if config.get('max_size', 0) > 0:
            cmd.extend([f"-M{config['max_size'] * 1000000}"])
        
        if config.get('max_time', 0) > 0:
            cmd.extend([f"-E{config['max_time']}"])
        
        # Content settings
        if not config.get('get_images', True):
            cmd.extend(["-*", "+*.html", "+*.css", "+*.js"])
        
        if not config.get('get_videos', True):
            cmd.extend(["-*.mp4", "-*.avi", "-*.mov", "-*.wmv"])
        
        # Robots.txt
        if config.get('follow_robots', True):
            cmd.extend(["-s2"])
        else:
            cmd.extend(["-s0"])
        
        # Additional options
        cmd.extend(["-v"])   # Verbose
        cmd.extend(["-N0"])  # Save structure
        cmd.extend(["-K0"])  # Keep original links
        cmd.extend(["-o"])   # Generate error files
        cmd.extend(["-%P"])  # Extended parsing
        
        return cmd
    
    async def scrape_website(
        self,
        url: str,
        config: Dict[str, Any],
        output_name: Optional[str] = None
    ) -> Optional[str]:
        """Scrape website using HTTrack"""
        
        # Create output directory name
        if not output_name:
            domain = url.replace("http://", "").replace("https://", "")
            domain = domain.split("/")[0].replace(":", "_")
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            output_name = f"{domain}_{timestamp}"
        
        output_dir = os.path.join(self.output_base, output_name)
        os.makedirs(output_dir, exist_ok=True)
        
        await Actor.log.info(f"Starting scrape: {url}")
        await Actor.log.info(f"Output directory: {output_dir}")
        await Actor.log.info(f"Configuration: {config}")
        
        # Build command
        cmd = self.build_httrack_command(url, output_dir, config)
        await Actor.log.info(f"Command: {' '.join(cmd)}")
        
        try:
            # Run HTTrack
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=False
            )
            
            if result.returncode == 0:
                await Actor.log.info("Scraping completed successfully")
                return output_dir
            else:
                await Actor.log.warning(
                    f"Scraping completed with warnings (exit code: {result.returncode})"
                )
                if result.stderr:
                    await Actor.log.warning(f"Errors: {result.stderr}")
                return output_dir
                
        except Exception as e:
            await Actor.log.error(f"Error during scraping: {e}")
            return None
    
    def create_zip(self, source_dir: str, zip_name: Optional[str] = None) -> Optional[str]:
        """Create ZIP archive of scraped content"""
        
        if not zip_name:
            zip_name = f"{os.path.basename(source_dir)}.zip"
        
        zip_path = os.path.join(self.output_base, zip_name)
        
        try:
            with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
                for root, dirs, files in os.walk(source_dir):
                    for file in files:
                        file_path = os.path.join(root, file)
                        arcname = os.path.relpath(file_path, source_dir)
                        zipf.write(file_path, arcname)
            
            size_mb = os.path.getsize(zip_path) / (1024 * 1024)
            return zip_path
            
        except Exception as e:
            return None
    
    def cleanup_directory(self, directory: str):
        """Clean up scraped directory after zipping"""
        try:
            if os.path.exists(directory):
                shutil.rmtree(directory)
        except Exception:
            pass


async def main():
    """Main Actor entry point"""
    
    async with Actor:
        # Get Actor input
        actor_input = await Actor.get_input() or {}
        
        # Validate input
        url = actor_input.get('url')
        if not url:
            await Actor.fail('Missing required input: url')
            return
        
        # Get configuration with defaults
        config = {
            'depth': actor_input.get('depth', 2),
            'stay_on_domain': actor_input.get('stayOnDomain', True),
            'max_rate': actor_input.get('maxRate', 0),
            'max_size': actor_input.get('maxSize', 0),
            'max_time': actor_input.get('maxTime', 0),
            'connections': actor_input.get('connections', 4),
            'retries': actor_input.get('retries', 2),
            'timeout': actor_input.get('timeout', 30),
            'get_images': actor_input.get('getImages', True),
            'get_videos': actor_input.get('getVideos', True),
            'follow_robots': actor_input.get('followRobots', True),
            'external_depth': actor_input.get('externalDepth', 0),
        }
        
        output_name = actor_input.get('outputName')
        cleanup = actor_input.get('cleanup', True)
        
        await Actor.log.info(f"Starting HTTrack scraper for: {url}")
        
        # Initialize scraper
        scraper = HTTrackScraper()
        
        # Check HTTrack installation
        if not scraper.check_httrack():
            await Actor.fail('HTTrack is not installed in the container')
            return
        
        await Actor.log.info("HTTrack is installed and ready")
        
        # Scrape website
        output_dir = await scraper.scrape_website(url, config, output_name)
        
        if not output_dir:
            await Actor.fail('Failed to scrape website')
            return
        
        await Actor.log.info(f"Scraping completed: {output_dir}")
        
        # Create ZIP archive
        await Actor.log.info("Creating ZIP archive...")
        zip_path = scraper.create_zip(output_dir)
        
        if not zip_path:
            await Actor.fail('Failed to create ZIP archive')
            return
        
        await Actor.log.info(f"ZIP created: {zip_path}")
        
        # Save ZIP to key-value store
        zip_filename = os.path.basename(zip_path)
        with open(zip_path, 'rb') as f:
            zip_data = f.read()
            await Actor.set_value(zip_filename, zip_data, content_type='application/zip')
        
        await Actor.log.info(f"ZIP saved to key-value store: {zip_filename}")
        
        # Calculate statistics
        file_count = sum(len(files) for _, _, files in os.walk(output_dir))
        total_size = sum(
            os.path.getsize(os.path.join(root, file))
            for root, _, files in os.walk(output_dir)
            for file in files
        )
        zip_size = os.path.getsize(zip_path)
        
        # Push results to dataset
        await Actor.push_data({
            'url': url,
            'outputName': output_name or os.path.basename(output_dir),
            'zipFile': zip_filename,
            'fileCount': file_count,
            'totalSize': total_size,
            'zipSize': zip_size,
            'compressionRatio': round((1 - zip_size / total_size) * 100, 2) if total_size > 0 else 0,
            'timestamp': datetime.now().isoformat(),
            'config': config,
            'status': 'success'
        })
        
        # Cleanup if requested
        if cleanup:
            await Actor.log.info("Cleaning up source directory...")
            scraper.cleanup_directory(output_dir)
            os.remove(zip_path)  # Also remove local ZIP after saving to KVS
        
        await Actor.log.info("✓ Scraping completed successfully!")
