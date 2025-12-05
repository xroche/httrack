# Website Scraper Using HTTrack

A comprehensive Python script to scrape websites using HTTrack and create ZIP archives of the downloaded content.

## Features

- ✅ Interactive configuration with sensible defaults
- ✅ Non-interactive mode for automation
- ✅ Automatic ZIP archive creation
- ✅ Configuration saving/loading
- ✅ Detailed progress tracking
- ✅ Error handling and validation
- ✅ Customizable download parameters
- ✅ Support for depth control, rate limiting, and content filtering

## Prerequisites

1. **HTTrack** must be installed:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install httrack
   
   # Fedora/RHEL
   sudo dnf install httrack
   
   # macOS
   brew install httrack
   ```

2. **Python 3.6+** (usually pre-installed on Linux)

## Installation

1. Make the script executable:
   ```bash
   chmod +x website_scraper.py
   ```

2. (Optional) Create a virtual environment:
   ```bash
   python3 -m venv venv
   source venv/bin/activate
   ```

## Usage

### Basic Usage (Interactive Mode)

```bash
python website_scraper.py https://example.com
```

This will:
1. Prompt you for configuration options
2. Scrape the website
3. Create a ZIP archive
4. Keep both the scraped directory and ZIP file

### Non-Interactive Mode (Use Defaults)

```bash
python website_scraper.py https://example.com --non-interactive
```

### Cleanup After Scraping

```bash
python website_scraper.py https://example.com --cleanup
```

This removes the scraped directory after creating the ZIP file.

### Custom Output Name

```bash
python website_scraper.py https://example.com --output my_website
```

### Combined Options

```bash
python website_scraper.py https://example.com -n -c -o my_site
```

## Configuration Options

When running in interactive mode, you'll be prompted for:

### Basic Settings
- **Mirror depth**: How many links deep to follow (default: 2)
- **Stay on domain**: Whether to only download from the same domain (default: yes)

### Download Limits
- **Max download rate**: Maximum KB/s (0 = unlimited)
- **Max total size**: Maximum total size in MB (0 = unlimited)
- **Max time**: Maximum scraping time in seconds (0 = unlimited)

### Connection Settings
- **Simultaneous connections**: Number of parallel downloads (default: 4)
- **Retries**: Number of retry attempts on error (default: 2)
- **Timeout**: Connection timeout in seconds (default: 30)

### Content Settings
- **Download images**: Whether to download images (default: yes)
- **Download videos**: Whether to download videos (default: yes)
- **Download audio**: Whether to download audio files (default: yes)

### Advanced Settings
- **Follow robots.txt**: Respect website's robots.txt (default: yes)
- **Accept cookies**: Allow cookies during scraping (default: yes)
- **Parse JavaScript**: Parse JavaScript for links (default: yes)

## Configuration File

The script saves your configuration to `scraper_config.json` for reuse. You can edit this file directly:

```json
{
  "depth": 2,
  "max_rate": 0,
  "connections": 4,
  "stay_on_domain": true,
  "get_images": true,
  "follow_robots": true
}
```

## Output Structure

```
scraped_websites/
├── example.com_20241205_120000/
│   ├── index.html
│   ├── assets/
│   ├── images/
│   ├── scrape_config.json
│   └── hts-log.txt
└── example.com_20241205_120000.zip
```

Each scraped site includes:
- All downloaded files with original structure
- `scrape_config.json`: Configuration used for this scrape
- `hts-log.txt`: HTTrack's detailed log file

## Examples

### Scrape a Blog

```bash
# Scrape all posts, stay on domain
python website_scraper.py https://myblog.com
```

### Scrape Documentation Site

```bash
# Deep scrape for documentation
python website_scraper.py https://docs.example.com --non-interactive
```

Then edit `scraper_config.json`:
```json
{
  "depth": 5,
  "stay_on_domain": true,
  "get_images": true,
  "get_videos": false
}
```

### Quick Scrape (No Prompts)

```bash
python website_scraper.py https://createathon.co -n -c
```

### Scrape Multiple Sites

```bash
#!/bin/bash
# scrape_multiple.sh

sites=(
  "https://site1.com"
  "https://site2.com"
  "https://site3.com"
)

for site in "${sites[@]}"; do
  python website_scraper.py "$site" --non-interactive --cleanup
done
```

## Troubleshooting

### HTTrack Not Found

```
✗ HTTrack is not installed!
```

**Solution**: Install HTTrack using your package manager (see Prerequisites).

### Permission Denied

```
Permission denied: 'scraped_websites'
```

**Solution**: Run with appropriate permissions or change output directory:
```bash
mkdir -p ~/scraped_websites
python website_scraper.py https://example.com
```

### Scraping Takes Too Long

**Solution**: Adjust configuration:
- Reduce `depth` (e.g., from 5 to 2)
- Set `max_time` limit
- Disable video/image download if not needed
- Increase `connections` for faster parallel downloads

### Website Blocks Scraping

**Solution**:
- Enable `follow_robots` to respect robots.txt
- Reduce `connections` to be more polite
- Set `max_rate` to limit bandwidth usage
- Increase `timeout` if connections are slow

### Large ZIP Files

**Solution**:
- Disable video/image downloads
- Reduce `depth`
- Set `max_size` limit
- Filter specific file types

## Advanced Usage

### Custom HTTrack Filters

Edit the script to add custom filters:

```python
# In build_httrack_command method, add:
cmd.extend(["+*.html", "+*.css", "+*.js"])  # Only these types
cmd.extend(["-*/admin/*", "-*/wp-admin/*"]) # Exclude admin paths
```

### Batch Processing

Create a `urls.txt` file:
```
https://site1.com
https://site2.com
https://site3.com
```

Then run:
```bash
while read url; do
  python website_scraper.py "$url" -n -c
  sleep 60  # Wait 60 seconds between scrapes
done < urls.txt
```

### Integration with Other Tools

```python
from website_scraper import WebsiteScraper

scraper = WebsiteScraper()
config = scraper.load_config()
output_dir = scraper.scrape_website("https://example.com", config)
zip_path = scraper.create_zip(output_dir)

# Do something with the ZIP file
upload_to_s3(zip_path)
```

## Legal and Ethical Considerations

⚠️ **Important**: Always ensure you have permission to scrape websites.

- Respect `robots.txt` files
- Don't overload servers with too many connections
- Check website Terms of Service
- Don't scrape copyrighted content without permission
- Be a good internet citizen

## Command-Line Options Reference

```
usage: website_scraper.py [-h] [-n] [-c] [-o OUTPUT] url

positional arguments:
  url                   URL of the website to scrape

optional arguments:
  -h, --help            Show help message and exit
  -n, --non-interactive Use default configuration without prompts
  -c, --cleanup         Remove source directory after creating ZIP
  -o OUTPUT, --output OUTPUT
                        Custom output name for the scraped content
```

## License

This script is provided as-is for educational and legitimate use cases.

## Support

For issues or questions:
1. Check HTTrack documentation: https://www.httrack.com/
2. Verify HTTrack is properly installed
3. Check file permissions and disk space
4. Review the generated `hts-log.txt` for detailed errors

