# HTTrack Website Scraper - Apify Actor

Download complete websites using HTTrack and get them as ZIP archives. Perfect for creating offline backups, archiving websites, or downloading entire sites with all assets (HTML, CSS, JavaScript, images, videos).

## Features

✅ **Complete Website Downloads** - Downloads entire websites with all assets  
✅ **ZIP Archive Output** - Automatically creates compressed ZIP files  
✅ **Configurable Depth** - Control how deep to follow links (1-10 levels)  
✅ **Rate Limiting** - Respect servers with configurable download rates  
✅ **Domain Filtering** - Stay on same domain or follow external links  
✅ **Content Selection** - Choose to download images, videos, or just HTML/CSS/JS  
✅ **Robots.txt Support** - Optionally respect website's robots.txt  
✅ **Progress Tracking** - Real-time logging of scraping progress  
✅ **Statistics** - File counts, sizes, and compression ratios  

## Input Configuration

### Required

- **Website URL** - The URL to scrape (must include `http://` or `https://`)

### Optional

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `depth` | Integer | 2 | How many links deep to follow (1-10) |
| `stayOnDomain` | Boolean | true | Only download from the same domain |
| `externalDepth` | Integer | 0 | How deep to follow external links |
| `connections` | Integer | 4 | Number of simultaneous downloads |
| `maxRate` | Integer | 0 | Max download rate in KB/s (0 = unlimited) |
| `maxSize` | Integer | 0 | Max total size in MB (0 = unlimited) |
| `maxTime` | Integer | 0 | Max scraping time in seconds (0 = unlimited) |
| `retries` | Integer | 2 | Number of retry attempts on error |
| `timeout` | Integer | 30 | Connection timeout in seconds |
| `getImages` | Boolean | true | Download image files |
| `getVideos` | Boolean | true | Download video files |
| `followRobots` | Boolean | true | Respect robots.txt |
| `outputName` | String | null | Custom output name (auto-generated if empty) |
| `cleanup` | Boolean | true | Remove source files after creating ZIP |

## Output

The Actor provides two types of output:

### 1. Dataset

Statistics and metadata for each scrape:

```json
{
  "url": "https://example.com",
  "outputName": "example.com_20241205_130000",
  "zipFile": "example.com_20241205_130000.zip",
  "fileCount": 156,
  "totalSize": 5242880,
  "zipSize": 2621440,
  "compressionRatio": 50.0,
  "timestamp": "2024-12-05T13:00:00.000Z",
  "config": { ... },
  "status": "success"
}
```

### 2. Key-Value Store

The complete website as a ZIP archive. Access it via:
- Apify Console: Storage → Key-Value Store → [filename].zip
- API: `https://api.apify.com/v2/key-value-stores/{storeId}/keys/{filename}.zip`

## Usage Examples

### Example 1: Basic Website Backup

```json
{
  "url": "https://example.com",
  "depth": 2,
  "stayOnDomain": true
}
```

Downloads the website up to 2 levels deep, staying on the same domain.

### Example 2: Deep Archive with External Links

```json
{
  "url": "https://example.com",
  "depth": 5,
  "externalDepth": 1,
  "stayOnDomain": false
}
```

Downloads 5 levels deep and follows external links 1 level.

### Example 3: Fast Scrape (HTML/CSS/JS Only)

```json
{
  "url": "https://example.com",
  "depth": 3,
  "getImages": false,
  "getVideos": false,
  "connections": 8
}
```

Fast scraping without images or videos, using 8 parallel connections.

### Example 4: Rate-Limited Polite Scrape

```json
{
  "url": "https://example.com",
  "depth": 2,
  "maxRate": 500,
  "connections": 2,
  "followRobots": true
}
```

Polite scraping with rate limiting and respecting robots.txt.

### Example 5: Time-Limited Scrape

```json
{
  "url": "https://example.com",
  "depth": 10,
  "maxTime": 300,
  "maxSize": 100
}
```

Stops after 5 minutes or 100 MB, whichever comes first.

## How It Works

1. **Input Validation** - Validates the URL and configuration
2. **HTTrack Execution** - Runs HTTrack with configured parameters
3. **Progress Monitoring** - Logs progress in real-time
4. **ZIP Creation** - Creates a compressed archive of all files
5. **Storage** - Saves ZIP to Key-Value Store and stats to Dataset
6. **Cleanup** - Optionally removes temporary files

## Technical Details

### Based On

- **HTTrack 3.49+** - Industry-standard website copier
- **Python 3.13** - Modern async Python runtime
- **Apify SDK 2.0** - For Actor integration and storage

### Limitations

- Some JavaScript-heavy SPAs may not download completely
- Websites with aggressive bot protection may block scraping
- Dynamic content loaded after page load may be missed
- Maximum recommended depth is 5-6 for most websites

### Performance

- **Small websites** (< 100 pages): 1-5 minutes
- **Medium websites** (100-1000 pages): 5-30 minutes
- **Large websites** (1000+ pages): 30+ minutes

Performance depends on:
- Website size and structure
- Number of connections
- Network speed
- Rate limiting settings

## Legal and Ethical Considerations

⚠️ **Important**: Always ensure you have permission to scrape websites.

- ✅ Respect `robots.txt` files (enabled by default)
- ✅ Don't overload servers (use rate limiting)
- ✅ Check website Terms of Service
- ✅ Don't scrape copyrighted content without permission
- ✅ Use reasonable connection limits (2-8)

## Troubleshooting

### Scraping Takes Too Long

- Reduce `depth` to 1 or 2
- Disable `getVideos` and `getImages`
- Increase `connections` (but be respectful)
- Set `maxTime` or `maxSize` limits

### ZIP File Too Large

- Reduce `depth`
- Disable `getVideos`
- Set `maxSize` limit
- Use `maxTime` to stop early

### Website Blocks Scraping

- Enable `followRobots`
- Reduce `connections` to 2-4
- Add rate limiting with `maxRate`
- Increase `timeout` if connections are slow

### Missing Content

- Increase `depth`
- Enable `externalDepth` if content is on other domains
- Check if website uses heavy JavaScript (may not work)
- Enable `getImages` and `getVideos` if needed

## Development

### Local Testing

```bash
# Install dependencies
pip install -r requirements.txt

# Run locally
apify run
```

### Building

```bash
# Build Docker image
docker build -t httrack-scraper .

# Run container
docker run httrack-scraper
```

## Support

For issues or questions:
- Check Actor logs for detailed error messages
- Review HTTrack documentation: https://www.httrack.com/
- Contact Apify support through the platform

## License

This Actor uses HTTrack, which is licensed under GPL v3.

## Version History

- **1.0.0** - Initial release with full HTTrack integration
