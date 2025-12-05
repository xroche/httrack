# HTTrack Website Scraper - Complete Apify Actor

## ✅ What's Included

This is a complete, production-ready Apify Actor that scrapes websites using HTTrack.

### Files Structure

```
.
├── Dockerfile                      # Docker configuration with HTTrack
├── requirements.txt                # Python dependencies
├── src/
│   ├── __init__.py                # Package initialization
│   ├── __main__.py                # Entry point
│   └── main.py                    # Main Actor logic
├── .actor/
│   ├── actor.json                 # Actor metadata
│   ├── input_schema.json          # Input configuration schema
│   ├── output_schema.json         # Output schema
│   ├── dataset_schema.json        # Dataset display schema
│   └── INPUT_EXAMPLE.json         # Example input
├── .dockerignore                  # Files to exclude from Docker
├── README.md                      # Actor documentation
├── website_scraper.py             # Standalone Python script
└── README_SCRAPER.md              # Standalone script docs
```

## 🚀 Quick Start

### Option 1: Deploy to Apify Platform

1. **Login to Apify**:
   ```bash
   apify login
   ```

2. **Deploy Actor**:
   ```bash
   apify push
   ```

3. **Run on Apify Console**:
   - Go to https://console.apify.com/
   - Find your Actor
   - Configure input and run

### Option 2: Test Locally

1. **Install Apify CLI**:
   ```bash
   npm install -g apify-cli
   ```

2. **Run Locally**:
   ```bash
   apify run
   ```

3. **Check Output**:
   ```bash
   cd apify_storage/key_value_stores/default/
   ls *.zip
   ```

### Option 3: Use Standalone Script

The `website_scraper.py` can also be used independently:

```bash
cd ~
python3 website_scraper.py https://example.com --non-interactive
```

## 📋 Input Configuration

### Simple Example

```json
{
  "url": "https://example.com"
}
```

Uses all default settings (depth=2, stay on domain, download all content).

### Advanced Example

```json
{
  "url": "https://example.com",
  "depth": 3,
  "stayOnDomain": true,
  "connections": 8,
  "maxRate": 1000,
  "maxSize": 500,
  "maxTime": 600,
  "getImages": true,
  "getVideos": false,
  "followRobots": true,
  "outputName": "my_backup"
}
```

## 📦 Output

### 1. Dataset Entry

Statistics for each scrape:

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
  "status": "success"
}
```

### 2. Key-Value Store

Complete ZIP archive of the website.

**Download via API**:
```bash
curl "https://api.apify.com/v2/key-value-stores/{storeId}/keys/{filename}.zip" > website.zip
```

## 🔧 How It Works

### Dockerfile

The Dockerfile:
1. ✅ Installs HTTrack (system package)
2. ✅ Installs Python dependencies
3. ✅ Copies Actor source code
4. ✅ Sets up proper permissions
5. ✅ Configures environment

Key sections:
- **Root user section**: Installs HTTrack
- **myuser section**: Installs Python packages and copies code
- **Environment**: Sets PATH and HTTrack variables

### Main Actor Logic (src/main.py)

Flow:
1. **Input Validation** - Checks URL is provided
2. **Configuration** - Loads config with defaults
3. **HTTrack Check** - Verifies HTTrack is installed
4. **Scraping** - Runs HTTrack with configured parameters
5. **ZIP Creation** - Compresses all downloaded files
6. **Storage** - Saves ZIP to Key-Value Store
7. **Dataset** - Pushes statistics to Dataset
8. **Cleanup** - Removes temporary files (if enabled)

### Input Schema

Defines the Actor's input form in Apify Console:
- Required fields (url)
- Optional fields with defaults
- Field types and validation
- Descriptions and examples

### Output Schema

Defines output quick links:
- ZIP file in Key-Value Store
- Dataset with statistics

### Dataset Schema

Defines how data is displayed in Apify Console:
- **Overview view**: Key metrics
- **Details view**: Full configuration

## 🎯 Use Cases

### 1. Website Backups

```json
{
  "url": "https://mywebsite.com",
  "depth": 5,
  "stayOnDomain": true,
  "followRobots": true
}
```

### 2. Competitor Analysis

```json
{
  "url": "https://competitor.com",
  "depth": 2,
  "getVideos": false,
  "maxSize": 100
}
```

### 3. Archive Collection

```json
{
  "url": "https://old-site.com",
  "depth": 10,
  "externalDepth": 1,
  "maxTime": 3600
}
```

### 4. Documentation Download

```json
{
  "url": "https://docs.example.com",
  "depth": 3,
  "stayOnDomain": true,
  "getImages": true,
  "getVideos": false
}
```

## 🐛 Debugging

### View Logs

**In Apify Console**:
- Go to Run detail
- Click "Log" tab
- See real-time progress

**Locally**:
```bash
apify run
# Logs appear in terminal
```

### Common Issues

**Issue**: "HTTrack is not installed"
- **Solution**: Docker image should have HTTrack pre-installed
- **Check**: `docker run <image> httrack --version`

**Issue**: "Failed to scrape website"
- **Solution**: Check logs for HTTrack errors
- **Try**: Reduce depth, enable followRobots, increase timeout

**Issue**: "ZIP file too large"
- **Solution**: Disable videos, reduce depth, set maxSize

## 🔐 Security & Ethics

### Best Practices

✅ **Always have permission** to scrape websites  
✅ **Respect robots.txt** (enabled by default)  
✅ **Use rate limiting** to avoid overloading servers  
✅ **Check Terms of Service** before scraping  
✅ **Don't scrape personal data** without consent  

### Rate Limiting

Recommended settings:
- `connections`: 2-8 (4 is safe default)
- `maxRate`: 500-1000 KB/s for polite scraping
- `followRobots`: true (always)

## 📊 Performance Optimization

### Fast Scraping

```json
{
  "connections": 8,
  "getVideos": false,
  "depth": 2
}
```

### Balanced Scraping

```json
{
  "connections": 4,
  "maxRate": 1000,
  "depth": 3
}
```

### Conservative Scraping

```json
{
  "connections": 2,
  "maxRate": 500,
  "depth": 2,
  "timeout": 60
}
```

## 🚢 Deployment Checklist

Before deploying:

- [ ] Test locally with `apify run`
- [ ] Verify HTTrack is in Dockerfile
- [ ] Check input_schema.json has all fields
- [ ] Test with sample URLs
- [ ] Review logs for errors
- [ ] Check ZIP files are created
- [ ] Verify dataset output
- [ ] Update actor.json metadata
- [ ] Set appropriate timeout (default: 3600s)
- [ ] Add README.md with examples

Deploy command:
```bash
apify push
```

## 📝 Maintenance

### Update HTTrack Version

In Dockerfile:
```dockerfile
RUN apt-get update && apt-get install -y httrack=<version>
```

### Update Python Dependencies

In requirements.txt:
```
apify ~= 2.0.0
```

### Monitor Performance

Check Actor runs for:
- Average duration
- Memory usage
- Success rate
- Common errors

## 🆘 Support

**Apify Documentation**: https://docs.apify.com/  
**HTTrack Documentation**: https://www.httrack.com/  
**Actor Console**: https://console.apify.com/

## 🎉 Summary

This Actor provides:
- ✅ Complete website downloads with HTTrack
- ✅ ZIP archive output
- ✅ Configurable parameters (15+ options)
- ✅ Progress tracking and statistics
- ✅ Apify platform integration
- ✅ Production-ready Docker container

Perfect for website backups, archiving, and offline browsing!

